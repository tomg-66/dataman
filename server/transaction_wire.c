/* ***************************************************************
 *
 * PROCEDURE:	transaction_wire.c
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
 * Server-owned transactions for the existing wire protocol.
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

#include "transaction_wire.h"
#include "transaction_dispatch.h"
#include "session_root.h"
#include "srv_index.h"
#include "dbfunc.h"
#include "misc.h"
#include "errors.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

extern INDEX *_indices;
extern int idx_cnt;
extern FILES *_wfiles[];
extern int32_t get_long(char *);

typedef struct request {
	int shmid, command, offset;
	char *message, **data;
	size_t payload;
	int (*handler)(char *, int, char **);
	dm_tx_target *targets;
	size_t count;
	char *root;
} request;

/* Numeric fields must end at a separator, before any binary key. */
static int number(char **cursor, long long *value)
{
	char *end;
	errno = 0;
	*value = strtoll(*cursor, &end, 10);
	if (errno || end == *cursor || *end != '|')
		return EINVMSG;
	*cursor = end + 1;
	return 0;
}

static int below(const char *root, const char *path)
{
	size_t n = strlen(root);
	return !strncmp(root, path, n) && (n == 1 || path[n] == '/');
}

static int bind_file(request *r, int fd, const char *name)
{
	char *path = realpath(name, NULL);
	if (!path)
		return ENOTOPEN;
	if (!below(r->root, path)) {
		free(path);
		return EINVMSG;
	}

	for (size_t i = 0; i < r->count; i++) {
		if (r->targets[i].fd == fd) {
			free(path);
			return 0;
		}
	}

	dm_tx_target *targets = realloc(r->targets, (r->count + 1) * sizeof(*targets));
	if (!targets) {
		free(path);
		return ENOALLOC;
	}
	r->targets = targets;
	size_t n = strlen(r->root);
	memmove(path, path + n + (n != 1), strlen(path + n + (n != 1)) + 1);
	r->targets[r->count++] = (dm_tx_target){fd, path};
	return 0;
}

static int bind_index(request *r, long long id)
{
	if (id < 0) {
		if (id < -MAX_CONNS || !_wfiles[-id-1])
			return EINVMSG;
		FILES *f = _wfiles[-id-1];
		return bind_file(r, f->_chan, f->_fname);
	}
	if (id >= idx_cnt || !_indices[id]._refcnt)
		return EIDXNOO;

	INDEX *index = _indices + id;
	int result = bind_file(r, index->_idxchan, index->_idxname);

	if (result < 0)
		return result;

	for (int i = 0; i < index->_f_cnt; i++) {
		FILES *f = index->_files[i];
		/* Read handlers may lazily open a data file. Index maintenance during
		 * those reads only writes the already-bound index descriptor. */
		if (f && f->_hlen && (result = bind_file(r, f->_chan, f->_fname)) < 0)
			return result;
	}
	return 0;
}

static int validate_flush(request *r, long long id, char *cursor)
{
	long long file, record, format, length;

	if (number(&cursor, &file) || number(&cursor, &record) ||
			number(&cursor, &format) || number(&cursor, &length) ||
			record < 0 || length < 0 || (unsigned long long)length != r->payload) {
		return EINVMSG;
	}

	FILES *f;
	if (id < 0)
		f = _wfiles[-id-1];
	else {
		if (file < 0 || file >= _indices[id]._f_cnt)
			return EINVMSG;
		f = _indices[id]._files[file];
	}

	if (!f || !f->_filedesc || format < 1 || format > f->_filedesc->n_rformats)
		return EINVMSG;

	RFDESC *desc = f->_filedesc->record_desc + format - 1;
	if (desc->rf_len < 0 || (size_t)desc->rf_len > r->payload || !*r->data)
		return EINVMSG;

	size_t at = desc->rf_len;
	int blobs = 0;
	for (int i = 0; i < desc->n_fields; i++) {
		if (desc->field_sizes[i])
			continue;
		blobs++;
		if (r->payload - at < 4)
			return EINVMSG;
		int32_t size = get_long(*r->data + at);
		at += 4;
		if (size >= 0) {
			if ((size_t)size > r->payload - at)
				return EINVMSG;
			at += size;
		}
	}
	return blobs == desc->has_blob && at == r->payload ? 0 : EINVMSG;
}

static int execute(void *context)
{
	request *r = context;
	char *cursor = r->message + r->offset;
	long long id, ignored;
	int result;

	r->root = session_root_copy(r->shmid);
	if (!r->root)
		return ENOCONN;
	if (r->command == INSERT && (number(&cursor, &ignored) || number(&cursor, &ignored)))
		return EINVMSG;
	if (number(&cursor, &id))
		return EINVMSG;

	if (id < 0 && r->command != FLUSH && r->command != PROTECT &&
			r->command != CLEAR && r->command != GET_REC && r->command != GET_DESC &&
			r->command != FORWARD && r->command != BACK && r->command != RELEASE)
		return EINVMSG;

	if ((result = bind_index(r, id)) < 0)
		return result;

	if (r->command == INCLUDE) {
		long long destination;
		if (number(&cursor, &ignored) || number(&cursor, &destination) || destination < 0)
			return EINVMSG;
		if ((result = bind_index(r, destination)) < 0)
			return result;
	}

	if (r->command == FLUSH && (result = validate_flush(r, id, cursor)) < 0)
		return result;
	/* The old queue used preview flags. Live journaled operations execute now;
	 * retain field width so offsets and binary keys remain unchanged.
	 */
	if (r->command == DELETE || r->command == REMOVE) {
		if (r->command == DELETE && (number(&cursor, &ignored) || number(&cursor, &ignored)))
			return EINVMSG;
		char *end = strchr(cursor, '|');
		if (!end || end == cursor)
			return EINVMSG;
		memset(cursor, '0', (size_t)(end - cursor));
	}

	/* Track aliases of bound indexes, not unrelated handles (which may still
	 * be under construction in an index-building session). */
	for (int i = 0; i < idx_cnt; i++) {
		struct stat index_status;
		if (!_indices[i]._refcnt || fstat(_indices[i]._idxchan, &index_status) < 0)
			continue;
		for (size_t j = 0; j < r->count; j++) {
			struct stat target_status;
			if (fstat(r->targets[j].fd, &target_status) < 0)
				return ENOTOPEN;
			if (index_status.st_dev == target_status.st_dev &&
					index_status.st_ino == target_status.st_ino) {
				if ((result = dm_tx_track_index(_indices + i)) < 0)
					return result;
				break;
			}
		}
	}

	if ((result = dm_tx_bind(r->targets, r->count)) < 0)
		return result;
	return r->handler(r->message, r->offset, r->data);
}

int dm_tx_wire(int shmid, int command, char *message, int offset, char **data,
		size_t payload, int (*handler)(char *, int, char **))
{
	int result;
	if (command < 0) {
		if (message[offset])
			return EINVMSG;
		if (command == START_XACT)
			result = dm_tx_begin(shmid);
		else if (command == COMMIT)
			result = dm_tx_commit(shmid);
		else if (command == ROLLBACK)
			result = dm_tx_abort(shmid);
		else
			return EINVMSG;
		if (result < 0)
			return result;
		strcpy(message, "0|1|");
		return 4;
	}
	/* These commands manage object lifetimes or perform maintenance. Their
	 * ordinary admission excludes them for the entire explicit transaction. */
	if (command == INIT_DAT || command == DEF_ROOT || command == MKIDX ||
			command == IOPEN || command == ICLOSE || command == SORT || command == RELEASE) {
		result = dm_tx_request_enter();
		if (result < 0)
			return result;
		result = handler(message, offset, data);
		dm_tx_request_leave();
		return result;
	}

	result = dm_tx_begin(shmid);
	int implicit = result == 0;
	if (result < 0 && result != EINXACT)
		return result;
	request r = {shmid, command, offset, message, data, payload, handler, NULL, 0, NULL};
	result = dm_tx_dispatch(shmid, NULL, 0, execute, &r);

	for (size_t i = 0; i < r.count; i++)
		free((char *)r.targets[i].path);
	free(r.targets);
	free(r.root);

	if (implicit) {
		int finish = result < 0 ? dm_tx_abort(shmid) : dm_tx_commit(shmid);
		if (finish == ERECWRT) {
			int aborted = dm_tx_abort(shmid);
			if (aborted < 0)
				return aborted;
		}
		if (finish < 0)
			return finish;
	}
	return result;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
