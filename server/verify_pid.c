/* ***************************************************************
 *
 * PROCEDURE:	verify_pid
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		Thu Feb 21 08:57:20 MST 2002
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 ************************************************************* */
/*
 * this procedure is passed the name of the program being run.
 * it is a generic procedure to make sure that this process is
 * only being run once.  it takes the name of the program being
 * run as it's argument.
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

#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>

#include <sys/types.h>

#include <fcntl.h>

extern void err_sys(char *, char *);

static char pidfile_name[64];		/* path to pid file */
void termit(void)
{
	unlink(pidfile_name);
}

int verify_pid(char *prog_name)
{
	
	int chan;				/* chan to open up */
	int ret;

	char str[32];
	char *ptr;				/* used to find basename */

	struct flock lock;

	ptr = basename(prog_name);
	if (*ptr == '.' || *ptr == '/')
		return -1;

	sprintf(pidfile_name, "/tmp/.%s.pid", ptr);

	atexit(termit);

	if ((chan = open(pidfile_name, O_WRONLY|O_CREAT, 0666)) < 0)
		return(-1);
/*
 * now put a write lock on the whole file.  this will make sure
 * only we will have access.
 */
	lock.l_type = F_WRLCK;
	lock.l_start = 0;
	lock.l_whence = SEEK_SET;
	lock.l_len = 0;

	if (fcntl(chan, F_SETLK, &lock) < 0) {
		if (errno == EACCES || errno == EAGAIN)
			return(-1);
		err_sys("%s: can't get lock on pid file: ", prog_name);
	}
/*
 * put our pid in the file.
 */
	if (ftruncate(chan, 0) < 0)
		err_sys("%s: Can't truncate pid file: ", prog_name);

	sprintf(str, "%d", getpid());
	if (write(chan, str, strlen(str)) != strlen(str))
		err_sys("%s: Can't write pid to file: ", prog_name);

/*
 * set close on exec, so this doesn't get copied around.
 */
	if ((ret = fcntl(chan, F_GETFD, 0)) < 0)
		err_sys("%s: Can't get file attr: ", prog_name);
	ret |= FD_CLOEXEC;
	if (fcntl(chan, F_SETFD, ret < 0))
		err_sys("%s: Can't set file attr: ", prog_name);
	return(0);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
