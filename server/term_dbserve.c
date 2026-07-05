/* ***************************************************************
 *
 * PROCEDURE:	term_dbserve.c
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		Sun Jun 27 21:53:17 MDT 2004
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 ************************************************************* */

/*
 * this function reads the /tmp/.dataman_srv.pid file to
 * get the pid of the running dataman server, and then
 * terminates it.
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

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>

#include <signal.h>

void term_dbserve(void)
{

	FILE *fp;

	char pid_str[16];

	pid_t pid;

	if ((fp = fopen("/tmp/.dataman_srv.pid", "r")) == NULL) {
		fprintf(stderr, "dataman_con: Can't open .dataman_srv.pid file: ");
		perror("");
		return;
	}
	if (fgets(pid_str, sizeof(pid_str), fp) == NULL) {
		fprintf(stderr, "dataman_con: Can't read .dataman_srv.pid: ");
		perror("");
		return;
	}
	fclose(fp);
	pid = atoi(pid_str);
	kill(pid, SIGTERM);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
