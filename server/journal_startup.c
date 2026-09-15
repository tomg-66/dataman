/* GPL-2.0-or-later. */
#include "journal_startup.h"
#include "undo_journal.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

struct dm_journal_guard {
	int directory_fd, lock_fd;
	char *path;
};
static dm_journal_guard *active_guard;
static pid_t owner_pid;

static int sync_directory(int fd)
{
	int result;
	do { result = fsync(fd); } while (result < 0 && errno == EINTR);
	return result;
}

void dm_journal_close(dm_journal_guard *guard)
{
	int saved = errno;
	if (guard) {
		if (active_guard == guard) active_guard = NULL;
		if (guard->lock_fd >= 0) close(guard->lock_fd);
		if (guard->directory_fd >= 0) close(guard->directory_fd);
		free(guard->path);
		free(guard);
	}
	errno = saved;
}

int dm_journal_open(const char *directory, dm_journal_guard **out)
{
	dm_journal_guard *guard;
	struct stat status;
	struct flock lock = {0};
	if (!out) { errno = EINVAL; return -1; }
	*out = NULL;
	/* POSIX locks belong to the process. A second open/close of this inode
	 * in the same process could silently release its existing lock. */
	if (active_guard && owner_pid == getpid()) { errno = EALREADY; return -1; }
	if (!directory || directory[0] != '/') { errno = EINVAL; return -1; }
	guard = calloc(1, sizeof(*guard));
	if (!guard) return -1;
	guard->directory_fd = guard->lock_fd = -1;
	if (mkdir(directory, 0700) < 0 && errno != EEXIST) goto fail;
	guard->directory_fd = open(directory, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
	if (guard->directory_fd < 0 || fstat(guard->directory_fd, &status) < 0) goto fail;
	if (status.st_uid != geteuid() || (status.st_mode & 077)) { errno = EACCES; goto fail; }
	guard->path = realpath(directory, NULL);
	if (!guard->path) goto fail;
	/* Never unlink this lock file: replacing its inode would permit two owners. */
	guard->lock_fd = openat(guard->directory_fd, ".server.lock",
		O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK, 0600);
	if (guard->lock_fd < 0 || fstat(guard->lock_fd, &status) < 0) goto fail;
	if (!S_ISREG(status.st_mode) || status.st_uid != geteuid() ||
		(status.st_mode & 077) || status.st_nlink != 1) { errno = EACCES; goto fail; }
	lock.l_type = F_WRLCK;
	lock.l_whence = SEEK_SET;
	if (fcntl(guard->lock_fd, F_SETLK, &lock) < 0) goto fail;
	/* Persist creation before journal use, including the directory's parent. */
	if (sync_directory(guard->directory_fd) < 0) goto fail;
	int parent = openat(guard->directory_fd, "..", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (parent < 0) goto fail;
	int result = sync_directory(parent), saved = errno;
	close(parent); errno = saved;
	if (result < 0) goto fail;
	*out = guard;
	active_guard = guard;
	owner_pid = getpid();
	return 0;
fail:
	dm_journal_close(guard);
	return -1;
}

int dm_journal_recover(dm_journal_guard *guard)
{
	if (!guard) { errno = EINVAL; return -1; }
	return dm_undo_recover(guard->path);
}

const char *dm_journal_directory(const dm_journal_guard *guard)
{
	return guard ? guard->path : NULL;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
