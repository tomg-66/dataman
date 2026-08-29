/* ***************************************************************
 *
 * PROCEDURE:	db_connect
 *
 * PROJECT:		dataman client side
 * 
 * DATE:		Mar 18 14:37:34 MDT 2002
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 ************************************************************* */

/*
 * this routine connects to the database server.
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
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <sys/ioctl.h>

#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "globs.h"
#include "../server/errors.h"

#define DBSOCK 8758

extern void db_err(int, char*, ...);

extern int dbgsw;

/*
 * connect to the database server.  the only returns if the socket 
 * call succeeds is either a small negative number or the string "ok"
 */
int db_connect(char *host)
{

	int i;
	int sock;

	char resp[8];

	fd_set rfds;

	struct in_addr iadd;
	struct hostent *haddr;
	struct sockaddr_in addr;

	memset(resp, '\0', sizeof(resp));
	bzero((char *)&addr, sizeof(addr));
/*
 * did we receive an address or host name
 */
	if (!inet_aton(host, &iadd)) {
		if ((haddr = gethostbyname(host)) == NULL)
			return(ENOHOST);
		iadd = *((struct in_addr *)(haddr->h_addr));
	}
/*
 * build the socket info
 */
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = iadd.s_addr;
	addr.sin_port = htons(DBSOCK);
/*
 * make the connection.
 */
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return(ENOSOCK);

	i = 1;
	if (setsockopt(sock, SOL_TCP, TCP_NODELAY, (char *)&i, sizeof(int)) < 0) {
			fprintf(stderr, "Can't set socket option in db_connect\n");
			perror("");
			close(sock);
			return(ESOCKOPT);
	}
	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(sock);
		return(ENOCONN);
	}
/*
 * ok, we are connected. let them know we are to be trusted
 * 		(christy's birthday)
 */
	if (write(sock, "9-30-1966", 9) != 9) {
		close(sock);
		return(ENORESP);
	}
/*
 * wait for a response
 */
	while (1) {
		FD_ZERO(&rfds);
		FD_SET(sock, &rfds);
		if (select(sock+1, &rfds, NULL, NULL, NULL) < 0) {
			if (errno == EINTR) {
				if (dbgsw) {
					fprintf(stderr, "after select, interrupted\n");
					fflush(stderr);
				}
				continue;			/* interrupted - reaped child? */
			} else {
				db_err(0, "%s, Can't accept new connection: ", _progname);
				close(sock);
				return -1;
			}
		}
		break;
	}
	if (ioctl(sock, FIONREAD, &i) < 0 || i == 0) {
		db_err(0, "%s: ioctl failed, socket gone", _progname);
		close(sock);
		return -1;
	}

	if (i < sizeof(resp)) {
		if (read(sock, resp, i) != i) {
			close(sock);
			return(ENORESP);
		}
	} else {
		db_err(EINVMSG, "%s: invalid wrapper length in connect", _progname);
		close(sock);
		return -1;
	}
/*
 * return the response - "ok" if good, negative int if not
 */
	if (strcmp(resp, "ok")) {
		close(sock);
		sock = atoi(resp);
	}
	return(sock);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
