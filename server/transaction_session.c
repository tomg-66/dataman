/* ***************************************************************
 *
 * PROCEDURE:	transaction_session.c
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		Thu Sep 17 08:17:11 PM MDT 2026
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 ************************************************************* */
/*
 * dataman transaction processing
 */
/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * The GNU General Public License is contained in the file COPYING.
 */

#include "transaction_session.h"
#include "session_root.h"
#include "undo_journal.h"
#include "errors.h"
#include "storage_io.h"
#include "srv_index.h"
#include "index_v2.h"
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

static pthread_mutex_t tx_mutex = PTHREAD_MUTEX_INITIALIZER;
static char *journal_directory;
static char *database_root;
static dm_undo *journal;
static int owner = -1, failed, blocked;
static uint64_t generation;
static size_t direct_writers, active_requests;
static int owner_request;
static _Thread_local int request_active;
static _Thread_local struct {
	int active, shmid;
	uint64_t generation;
	const dm_tx_target *targets;
	size_t count;
} scope;

typedef struct cached_index {
	struct cached_index *next;
	INDEX *index;
	int fd;
	dev_t device;
	ino_t inode;
	uint64_t root, generation;
} cached_index;

static cached_index *cached_indices;

int dm_tx_track_index(INDEX *index)
{
	int result = 0;
	struct stat st;
	pthread_mutex_lock(&tx_mutex);
	if (blocked)
		result = EROLLBACK;
	else if (owner < 0)
		result = scope.active ? ENOXACT : 0;
	else if (!scope.active || scope.shmid != owner || scope.generation != generation)
		result = ENOLOCK;
	else if (failed)
		result = ERECWRT;
	else {
		cached_index *entry;
		for (entry = cached_indices; entry; entry = entry->next)
			if (entry->index == index) break;
		if (!index || fstat(index->_idxchan, &st) < 0 || !S_ISREG(st.st_mode))
			result = EIDXNOO;
		else if (entry) {
			if (entry->fd != index->_idxchan || entry->device != st.st_dev || entry->inode != st.st_ino)
				result = EIDXNOO;
		} else if (!(entry = calloc(1, sizeof(*entry))))
			result = ENOALLOC;
		else {
			entry->index = index; entry->fd = index->_idxchan;
			entry->device = st.st_dev; entry->inode = st.st_ino;
			entry->next = cached_indices; cached_indices = entry;
		}
		if (result < 0) failed = 1;
	}
	pthread_mutex_unlock(&tx_mutex);
	return result;
}

/* Admission is still closed, with no owner handler running. Validate every
 * descriptor/header before publishing any cache values. No index mutex is
 * acquired under tx_mutex, avoiding the handler's index->transaction lock order. */
static int refresh_indices(void)
{
	for (cached_index *entry = cached_indices; entry; entry = entry->next) {
		struct stat st;
		uint16_t keylen, count;
		uint32_t crc;
		if (entry->index->_idxchan != entry->fd || fstat(entry->fd, &st) < 0 ||
			entry->device != st.st_dev || entry->inode != st.st_ino ||
			!index_v2_read_header(entry->fd, &keylen, &count, &entry->root, &crc, &entry->generation) ||
			keylen != entry->index->_keylen || count != entry->index->_f_cnt || entry->root > INT64_MAX)
			return -1;
	}
	while (cached_indices) {
		cached_index *entry = cached_indices;
		entry->index->_rootpos = (int64_t)entry->root;
		entry->index->_generation = entry->generation;
		cached_indices = entry->next;
		free(entry);
	}
	return 0;
}

int dm_tx_request_enter(void)
{
	int result = 0;
	pthread_mutex_lock(&tx_mutex);
	if (request_active)
		result = EINVMSG;
	else if (blocked)
		result = EROLLBACK;
	else if (owner >= 0)
		result = ENOLOCK;
	else {
		active_requests++;
		request_active = 1;
	}
	pthread_mutex_unlock(&tx_mutex);
	return result;
}

void dm_tx_request_leave(void)
{
	pthread_mutex_lock(&tx_mutex);
	if (request_active) {
		active_requests--;
		request_active = 0;
	}
	pthread_mutex_unlock(&tx_mutex);
}

/* No fallback to direct writes while a transaction owns the journal. */
static int route_write(int fd, const void *data, size_t length, int64_t offset)
{
	int result = -1;
	pthread_mutex_lock(&tx_mutex);
	if (blocked) {
		errno = EIO;
	} else if (owner < 0 && !scope.active) {
		/* Preserve concurrency between ordinary writes, but do not allow
		 * begin to create a journal until all admitted writes finish. */
		direct_writers++;
		pthread_mutex_unlock(&tx_mutex);
		result = dm_storage_write_at(fd, data, length, offset);
		int saved = errno;
		pthread_mutex_lock(&tx_mutex);
		direct_writers--;
		pthread_mutex_unlock(&tx_mutex);
		errno = saved;
		return result;
	} else if (!scope.active || owner != scope.shmid || generation != scope.generation) {
		errno = EBUSY;
	} else if (failed) {
		errno = EIO;
	} else {
		const char *path = NULL;
		for (size_t i = 0; i < scope.count; i++) {
			if (scope.targets[i].fd == fd) {
				path = scope.targets[i].path;
				break;
			}
		}
		if (!path)
			errno = EINVAL;
		else
			result = dm_undo_write_fd(journal, path, fd, data, length, offset);
		if (result < 0)
			failed = 1;
	}
	int saved = errno;
	pthread_mutex_unlock(&tx_mutex);
	errno = saved;
	return result;
}

int dm_tx_enter(int shmid, const dm_tx_target *targets, size_t count)
{
	int result = 0;
	if (scope.active || (count && !targets))
		return EINVMSG;
	for (size_t i = 0; i < count; i++) {
		if (targets[i].fd < 0 || !targets[i].path || !targets[i].path[0])
			return EINVMSG;
		for (size_t j = 0; j < i; j++)
			if (targets[i].fd == targets[j].fd)
				return EINVMSG;
	}
	pthread_mutex_lock(&tx_mutex);
	if (blocked)
		result = EROLLBACK;
	else if (owner < 0 || owner != shmid)
		result = ENOXACT;
	else if (failed)
		result = ERECWRT;
	else if (owner_request)
		result = ENOLOCK;
	else {
		owner_request = 1;
		scope.active = 1;
		scope.shmid = shmid;
		scope.generation = generation;
		scope.targets = targets;
		scope.count = count;
	}
	pthread_mutex_unlock(&tx_mutex);
	return result;
}

int dm_tx_bind(const dm_tx_target *targets, size_t count)
{
	if (!scope.active || (count && !targets)) return EINVMSG;
	for (size_t i = 0; i < count; i++) {
		if (targets[i].fd < 0 || !targets[i].path || !targets[i].path[0]) return EINVMSG;
		for (size_t j = 0; j < i; j++)
			if (targets[i].fd == targets[j].fd) return EINVMSG;
	}
	scope.targets = targets;
	scope.count = count;
	return 0;
}

void dm_tx_complete(int result)
{
	pthread_mutex_lock(&tx_mutex);
	if (scope.active) {
		if (result < 0)
			failed = 1;
		owner_request = 0;
		memset(&scope, 0, sizeof(scope));
	}
	pthread_mutex_unlock(&tx_mutex);
}

void dm_tx_leave(void)
{
	dm_tx_complete(0);
}

static int namespace_check(void)
{
	int result = 0;
	pthread_mutex_lock(&tx_mutex);
	if (blocked || owner >= 0) {
		if (scope.active && scope.shmid == owner)
			failed = 1;
		errno = blocked ? EIO : EBUSY;
		result = -1;
	}
	pthread_mutex_unlock(&tx_mutex);
	return result;
}

static const char *relative_blob(const char *path)
{
	size_t length = strlen(database_root);
	if (!path || path[0] != '/' || strncmp(path, database_root, length)) return NULL;
	if (length == 1) return path + 1;
	return path[length] == '/' ? path + length + 1 : NULL;
}

static int route_blob(int operation, const char *path, const char *dest,
		const void *data, size_t length)
{
	int result = -1;
	pthread_mutex_lock(&tx_mutex);
	if (blocked)
		errno = EIO;
	else if (owner < 0 && !scope.active)
		result = 0;
	else if (!scope.active || scope.shmid != owner || scope.generation != generation)
		errno = EBUSY;
	else if (failed)
		errno = EIO;
	else {
		const char *name = relative_blob(path);
		const char *destination = dest ? relative_blob(dest) : NULL;
		if (!name || (dest && !destination))
			errno = EINVAL;
		else if (dm_undo_blob(journal, operation, name, destination, data, length) == 0)
			result = 1;
		if (result < 0) failed = 1;
	}
	int saved = errno;
	pthread_mutex_unlock(&tx_mutex);
	errno = saved;
	return result;
}

int dm_tx_configure(const char *path)
{
	int result = 0;
	pthread_mutex_lock(&tx_mutex);
	if (!path || path[0] != '/' || owner >= 0 || blocked || direct_writers || active_requests) {
		result = EINVMSG;
	} else {
		char *copy = strdup(path);
		if (!copy)
			result = ENOALLOC;
		else {
			free(journal_directory);
			journal_directory = copy;
			dm_storage_set_router(route_write, namespace_check);
			dm_storage_set_blob_router(route_blob);
		}
	}
	pthread_mutex_unlock(&tx_mutex);
	return result;
}

static int begin_registered(const char *root, void *context)
{
	int shmid = *(int *)context;
	int result = 0;

	pthread_mutex_lock(&tx_mutex);

	if (blocked)
		result = EROLLBACK;
	else if (!journal_directory || shmid < 0)
		result = EINVMSG;
	else if (owner == shmid)
		result = EINXACT;
	else if (owner >= 0 || direct_writers || active_requests)
		result = ENOLOCK;
	else if (generation == UINT64_MAX)
		result = EINVMSG;
	else if (!(database_root = strdup(root)))
		result = ENOALLOC;
	else if (dm_undo_begin(journal_directory, root, &journal) < 0) {
		/*
		 * An unsuccessful begin may have created an incomplete header, or
		 * discovered a journal requiring recovery. Do not admit another owner.
		 */
		owner = shmid;
		blocked = 1;
		result = EROLLBACK;
	} else {
		owner = shmid;
		failed = 0;
		generation++;
	}

	pthread_mutex_unlock(&tx_mutex);

	return result;
}

int dm_tx_begin(int shmid)
{
/*
 * Pin the registered session through journal creation. Otherwise a cleanup
 * racing between lookup and begin could forget a newly created journal.
 */
	return session_root_use(shmid, begin_registered, &shmid);
}

static int write_target(int shmid, const char *path, int fd, int check_fd,
		const void *data, size_t length, int64_t offset)
{
	int result = 0;

	pthread_mutex_lock(&tx_mutex);

	if (blocked)
		result = EROLLBACK;
	else if (owner < 0 || owner != shmid)
		result = ENOXACT;
	else if (owner_request && !scope.active)
		result = ENOLOCK;
	else if (failed)
		result = ERECWRT;
	else if ((check_fd ? dm_undo_write_fd(journal, path, fd, data, length, offset) :
			dm_undo_write(journal, path, data, length, offset)) < 0) {
		failed = 1;
		result = ERECWRT;
	}

	pthread_mutex_unlock(&tx_mutex);

	return result;
}

int dm_tx_write(int shmid, const char *path, const void *data, size_t length, int64_t offset)
{
	return write_target(shmid, path, -1, 0, data, length, offset);
}

int dm_tx_write_fd(int shmid, const char *path, int fd, const void *data,
		size_t length, int64_t offset)
{
	return write_target(shmid, path, fd, 1, data, length, offset);
}

static int finish(int shmid, int commit, int disconnect)
{
	int result = 0;

	pthread_mutex_lock(&tx_mutex);

	if (blocked)
		result = EROLLBACK;
	else if (owner < 0 || owner != shmid)
		result = disconnect ? 0 : ENOXACT;
	else if (owner_request)
		result = ENOLOCK;
	else if (commit && failed)
		result = ERECWRT;
	else {
		int status = commit ? dm_undo_commit(journal) : dm_undo_abort(journal);
		dm_undo_close(journal);
		journal = NULL;
		if (status == 0)
			status = refresh_indices();
		if (status < 0) {
			/*
			 * A failed commit has an unknown outcome. Keep admission blocked
			 * until restart recovery, even if its resolved marker survived.
			 */
			blocked = 1;
			result = commit ? EMULTIPLE : EROLLBACK;
		} else {
			owner = -1;
			failed = 0;
			free(database_root); database_root = NULL;
		}
	}

	pthread_mutex_unlock(&tx_mutex);

	return result;
}

int dm_tx_commit(int shmid) {
	return finish(shmid, 1, 0);
}
int dm_tx_abort(int shmid) {
	return finish(shmid, 0, 0);
}
int dm_tx_disconnect(int shmid) {
	return finish(shmid, 0, 1);
}
int dm_tx_blocked(void)
{
	int result;
	pthread_mutex_lock(&tx_mutex);
	result = blocked;
	pthread_mutex_unlock(&tx_mutex);
	return result;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
