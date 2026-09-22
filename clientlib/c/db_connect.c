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

#include "../../server/protocol.h"
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

DATAMAN_HIDDEN extern void db_err(int, char*, ...);

DATAMAN_HIDDEN extern int dbgsw;

/*
 * Connect and validate the wire version. Return a socket or a negative error.
 */
DATAMAN_HIDDEN int db_connect(char *host)
{

	int i;
	int sock;

	struct in_addr iadd;
	struct hostent *haddr;
	struct sockaddr_in addr;

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
 * Check wire compatibility before sending any database commands.
 */
	i = dm_protocol_connect(sock);
	if (i < 0) {
		close(sock);
		return i;
	}
	return sock;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
