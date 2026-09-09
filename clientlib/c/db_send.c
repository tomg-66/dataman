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
 *
 *				Sun Jul 19 03:44:27 PM MDT 2026
 *				tomg
 *				doing clean up.  using better send/receive routines
 *
 *				Wed Jul 29 01:53:46 PM MDT 2026
 *				tomg
 *				make call to db_err not exit and return false
 *
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
#include <stdbool.h>

#include <sys/types.h>

#include <arpa/inet.h>		/* for def of ntohl */

#include "globs.h"

DATAMAN_HIDDEN extern void db_err(int, char *, ...);

static bool write_all(int fd, const void *buf, size_t len)
{
	const char *ptr = buf;

	while (len) {
		ssize_t ret = write(fd, ptr, len);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			return(false);
		}
		if (ret == 0)
			return(false);
		ptr += ret;
		len -= ret;
	}
	return(true);
}

static bool read_exact(int fd, void *buf, size_t len)
{
	char *ptr = buf;

	while (len) {
		ssize_t ret = read(fd, ptr, len);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			return(false);
		}
		if (ret == 0)
			return(false);
		ptr += ret;
		len -= ret;
	}
	return(true);
}

DATAMAN_HIDDEN char *db_send_len(char *cmd, int len, char *module, size_t *response_len)
{

	int32_t size;

	int j;

	char *ret;
	if (response_len)
		*response_len = 0;

	if (dm_sock == -1 || len < 0)
		return(NULL);

	size = htonl((int32_t)len);
	if (!write_all(dm_sock, (char *)&size, sizeof(int32_t))) {
		db_err(0, "%s: Can't write wrap to socket", _progname);
		return NULL;
	}

	if (!write_all(dm_sock, cmd, (size_t)len)) {
		db_err(0, "%s: Can't write to socket", _progname);
		return NULL;
	}

/*
 * Read the length wrapper, then exactly that many response bytes.
 */
	if (!read_exact(dm_sock, (char *)&size, sizeof(int32_t))) {
		db_err(0, "%s: Can't read response wrapper", _progname);
		return NULL;
	}

	size = ntohl(size);
	if (size < 0 || size == INT32_MAX) {
		db_err(0, "%s: invalid response length %"PRId32, _progname, size);
		return NULL;
	}

	if (dbgsw) {
		fprintf(stderr, "header says to read %"PRId32" bytes\n", size);
		fflush(stderr);
	}

	if ((ret = malloc(size+1)) == NULL) {
		db_err(0, "%s: Cannot allocate buffer memory in db_send for %s",
					_progname, module);
		return NULL;
	}

	memset(ret, '\0', size+1);
	if (!read_exact(dm_sock, ret, (size_t)size)) {
		db_err(0, "%s: Can't read socket response in %s",
					_progname, module);
		free(ret);
		return NULL;
	}
	j = size;
	if (response_len)
		*response_len = (size_t)size;

	if (dbgsw) {
		fprintf(stderr, "have read %d bytes from socket\n", j);
		fflush(stderr);
	}

	return(ret);
}

DATAMAN_HIDDEN char *db_send(char *cmd, int len, char *module)
{
	return db_send_len(cmd, len, module, NULL);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
