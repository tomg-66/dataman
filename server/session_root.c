/* Root registration alongside the existing transaction path. GPL-2.0-or-later. */
#include "session_root.h"
#include "transaction_session.h"
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include "errors.h"
#include "misc.h"

extern int init_dataman(char *, int, char **);
extern int mkidx(char *, int, char **);

static struct root_entry {
	char *path;
	int shmid;
	pid_t pid;
	dev_t device;
	ino_t inode;
} roots[MAX_CONNS];
static pthread_mutex_t roots_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * Existing connection cleanup removes its shared-memory segment. Reap metadata
 * on registration/lookup so the registry does not outlive that IPC session.
 * Abort the owned journal before releasing metadata. Unattached orphan segments
 * are also dead sessions, even if their creator died before IPC_RMID.
 */
static int close_slot(int i)
{
	int result = dm_tx_disconnect(roots[i].shmid);
	if (result < 0)
		return result;
	free(roots[i].path);
	roots[i].path = NULL;
	return 0;
}

static int reap(void)
{
	struct shmid_ds status;
	for (int i = 0; i < MAX_CONNS; ++i) {
		if (roots[i].path) {
			int ret = shmctl(roots[i].shmid, IPC_STAT, &status);
			int gone = (ret < 0 && (errno == EINVAL || errno == EIDRM)) ||
				(ret == 0 && status.shm_nattch == 0);
			/* A worker may still be attached when its connection process dies. */
			if (!gone && kill(roots[i].pid, 0) < 0 && errno == ESRCH)
				gone = 1;
			if (gone) {
				ret = close_slot(i);
				if (ret < 0)
					return ret;
			}
		}
	}
	return 0;
}

int session_root_reap(void)
{
	int result;
	pthread_mutex_lock(&roots_mutex);
	result = reap();
	pthread_mutex_unlock(&roots_mutex);
	return result;
}

int session_root_close(int shmid)
{
	int result = 0;
	pthread_mutex_lock(&roots_mutex);
	for (int i = 0; i < MAX_CONNS; ++i) {
		if (roots[i].path && roots[i].shmid == shmid) {
			result = close_slot(i);
			break;
		}
	}
	pthread_mutex_unlock(&roots_mutex);
	return result;
}

char *session_root_copy(int shmid)
{
	char *path = NULL;
	pthread_mutex_lock(&roots_mutex);
	if (reap() < 0) {
		pthread_mutex_unlock(&roots_mutex);
		errno = EIO;
		return NULL;
	}
	errno = ENOENT;
	for (int i = 0; i < MAX_CONNS; ++i) {
		if (roots[i].path && roots[i].shmid == shmid) {
			struct stat status;
			if (stat(roots[i].path, &status) == 0) {
				if (status.st_dev != roots[i].device || status.st_ino != roots[i].inode)
					errno = ESTALE;
				else
					path = strdup(roots[i].path);
			}
			break;
		}
	}
	pthread_mutex_unlock(&roots_mutex);
	return path;
}

int session_root_use(int shmid, int (*operation)(const char *, void *), void *context)
{
	int result;
	struct stat status;
	if (!operation)
		return EINVMSG;
	pthread_mutex_lock(&roots_mutex);
	result = reap();
	if (result < 0)
		goto done;
	result = ENOCONN;
	for (int i = 0; i < MAX_CONNS; ++i) {
		if (roots[i].path && roots[i].shmid == shmid) {
			if (stat(roots[i].path, &status) == 0 && status.st_dev == roots[i].device &&
				status.st_ino == roots[i].inode)
				result = operation(roots[i].path, context);
			break;
		}
	}
done:
	pthread_mutex_unlock(&roots_mutex);
	return result;
}

/* mode: 0 DEF_ROOT, 1 INIT_DAT, 2 MKIDX. */
static int register_root(char *cmd, int offset, char **data, int mode)
{
	char *end,
		 *field,
		 *path,
		 *canonical,
		 *last,
		 *parent;
	long pid;
	int shmid,
		slot = -1,
		result = EINVMSG;
	struct stat status;
	struct shmid_ds ipc;

	if (!cmd || !data || offset < 1 || (size_t)offset >= strlen(cmd))
		return EINVMSG;
	/* Internal requests carry the connection process PID before the command. */
	errno = 0;
	pid = strtol(cmd, &end, 10);
	if (errno || end == cmd || *end != '|' || pid < 1 || pid > INT_MAX)
		return EINVMSG;
	shmid = shmget((key_t)pid, 0, 0);
	if (shmid < 0 || shmctl(shmid, IPC_STAT, &ipc) < 0)
		return ENOSHM;
	field = cmd + offset;
	if (mode == 2) {
		/* MKIDX: key length | index name | database root | file count | ... */
		for (int i = 0; i < 2; ++i) {
			end = strchr(field, '|');
			if (!end || end == field)
				return EINVMSG;
			field = end + 1;
		}
	}
	end = strchr(field, '|');
	if (!end || end == field || (mode != 2 && end[1]))
		return EINVMSG;
	path = strndup(field, (size_t)(end - field));
	if (!path)
		return ENOALLOC;
	if (mode == 1) {
		/* Match the trailing /files/<workfile>, not an arbitrary /files/
		 * substring somewhere in a root's own name. */
		last = strrchr(path, '/');
		if (!last || !last[1])
			goto bad_path;
		*last = 0;
		parent = strrchr(path, '/');
		if (!parent || strcmp(parent + 1, "files"))
			goto bad_path;
		if (parent == path)
			parent[1] = 0;
		else
			*parent = 0;
	}
	canonical = realpath(path, NULL);
	free(path);
	if (!canonical)
		return errno == ENOMEM ? ENOALLOC : ENOFILE;
	if (stat(canonical, &status) < 0 || !S_ISDIR(status.st_mode)) {
		free(canonical);
		return ENOFILE;
	}
	pthread_mutex_lock(&roots_mutex);
	result = reap();
	if (result < 0)
		goto done;
	result = EINVMSG;
	for (int i = 0; i < MAX_CONNS; ++i) {
		if (roots[i].path && roots[i].shmid == shmid) {
			slot = i;
			break;
		}
		if (!roots[i].path && slot < 0)
			slot = i;
	}
	if (slot < 0) {
		result = ENOWSP;
		goto done; }
	if (roots[slot].path && (strcmp(roots[slot].path, canonical) ||
		roots[slot].device != status.st_dev || roots[slot].inode != status.st_ino))
		goto done;
	if (mode) {
		result = mode == 1 ? init_dataman(cmd, offset, data) : mkidx(cmd, offset, data);
		if (result <= 0)
			goto done;
	} else {
		*data = NULL;
		memcpy(cmd, "0|1|", 5);
		result = 4;
	}
	if (!roots[slot].path) {
		roots[slot].path = canonical;
		canonical = NULL;
		roots[slot].shmid = shmid;
		roots[slot].pid = (pid_t)pid;
		roots[slot].device = status.st_dev;
		roots[slot].inode = status.st_ino;
	}

done:
	pthread_mutex_unlock(&roots_mutex);
	free(canonical);
	return result;

bad_path:
	free(path);
	return EINVMSG;
}

int def_root(char *cmd, int offset, char **data)
{
	return register_root(cmd, offset, data, 0);
}

int init_session(char *cmd, int offset, char **data)
{
	return register_root(cmd, offset, data, 1);
}

int mkidx_session(char *cmd, int offset, char **data)
{
	return register_root(cmd, offset, data, 2);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
