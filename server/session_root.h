/* Connection root metadata; GPL-2.0-or-later. */
#ifndef DATAMAN_SESSION_ROOT_H
#define DATAMAN_SESSION_ROOT_H

int def_root(char *command, int offset, char **data);
int init_session(char *command, int offset, char **data);
int mkidx_session(char *command, int offset, char **data);
/*
 * Returns a caller-owned canonical path, or NULL with errno. The shared-memory
 * identifier includes the kernel's IPC generation; it is not merely a PID.
 */
char *session_root_copy(int shmid);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
