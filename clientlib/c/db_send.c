/* ***************************************************************
 *
 * PROCEDURE:	db_send
 *
 * PROJECT:		dataman client side
 * 
 * DATE:		Mar 7 14:41:30 MDT 2002
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 * 				Sat Oct 18 22:12:18 MDT 2003
 * 				tomg -
 * 				modified so that this sends and receives data
 * 				based on a 'wrapper'.  this and the server now
 * 				send the packet length along with the packet
 * 				to assure complete transmission.  Also, this
 * 				function now allocates the read buffer, reads
 * 				the socket, and returns the pointer to the
 * 				newly allocated data.
 *
 *				Wed Nov  2 12:26:34 MST 2005
 *				check for dm_sock == -1 just in case, so we
 *				don't send data on a closed socket.
 *				tomg
 ************************************************************* */

/*
 * this sends a request to the db server, then waits for a return.
 * then we get how many bytes are on the socket, and return that.
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
#include <errno.h>
#include <string.h>
#include <inttypes.h>

#include <sys/ioctl.h>
#include <sys/types.h>

#include <arpa/inet.h>		/* for def of ntohl */

#include "globs.h"
/*
 * this is what is was!
int db_send(char *cmd, int len)
 */

extern void db_err(int, char *, ...);

char * db_send(char *cmd, int len, char *module)
{

	int32_t size;

	int i, j;

	char *ret;

	fd_set rfds;

	if (dm_sock == -1)
		return(NULL);

	size = htonl((int32_t)len);
	if (write(dm_sock, (char *)&size, sizeof(int32_t)) != sizeof(int32_t))
		db_err(0, "%s: Can't write wrap to socket", _progname);

	j = 0;
	while (len) {
		if ((i = write(dm_sock, cmd+j, (size_t)len)) < 0)
			db_err(0, "%s: Can't write to socket", _progname);
		j += i;
		len -= i;
	}

	while (1) {
		FD_ZERO(&rfds);
		FD_SET(dm_sock, &rfds);
		if (select(dm_sock+1, &rfds, NULL, NULL, NULL) > -1)
			break;
		if (errno != EINTR)
			db_err(0, "%s: select failed", _progname);
	}
/*
 * get the number of bytes available on the socket
 * if the ioctl fails or returns 0 bytes, that indicates that the socket
 * went away for some reason.  if the socket goes away between the
 * ioctl and the read, the read will fail as well.
 */
	if (ioctl(dm_sock, FIONREAD, &i) < 0 || i == 0)
		db_err(0, "%s: ioctl failed, socket gone", _progname);
	if (i < sizeof(int32_t))
		db_err(0, "%s: wrapper length is too small!", _progname);
	if ((i = read(dm_sock, (char *)&size, sizeof(int32_t))) != sizeof(int32_t))
		db_err(0, "%s: read only %d header bytes", _progname, i);

	size = ntohl(size);

	if (dbgsw) {
		fprintf(stderr, "header says to read %"PRId32" bytes\n", size);
		fflush(stderr);
	}

	if ((ret = malloc(size+1)) == NULL)
		db_err(0, "%s: Cannot allocate buffer memory in db_send for %s",
					_progname, module);
	memset(ret, '\0', size+1);
	j = 0;
/*
 * read until we either get an error or the number of bytes that we
 * are supposed to, and return the buffer
 */
	while(size > 0) {
		if ((i = read(dm_sock, ret+j, size)) < 0)
			db_err(0, "%s: Can't read socket response in %s",
						_progname, module);
		j += i;
		size -= i;
	}

	if (dbgsw) {
		fprintf(stderr, "have read %d bytes from socket\n", j);
		fflush(stderr);
	}

	return(ret);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
