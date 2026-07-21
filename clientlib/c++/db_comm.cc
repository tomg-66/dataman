/* ***************************************************************
 *
 * PROCEDURE:	db_comm.cc
 *
 * PROJECT:		dataman client side c++ routine
 * 
 * DATE:		Mon Jun 14 07:42:26 MDT 2004
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 * 				Thu Mar 21 15:49:21 MDT 2013
 * 				Tom Green
 * 				added name space
 *
 *				 Sun Jul 19 03:43:26 PM MDT 2026
 *				 Tom Green
 *				 doing clean up.  used better send/receive routines
 *
 ************************************************************* */
/*
 * db_comm.cc
 *
 * These routines define the communication that occurs between an application
 * and the server.
 * The communication constructor will establish the connection to the server.
 * for all transactions back and forth the db_send routine is used.  it
 * defines new memory that needs to be deleted when the buffer is through.
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
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>

#include <sys/ioctl.h>

#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <endSort.hh>
#include <db_comm.hh>

#include "../server/errors.h"

#define DBSOCK 8758

using namespace Dataman;

int db_comm::db_sock = -1;

static bool write_all(int fd, const void *buf, size_t len)
{
	const char *ptr = (const char *)buf;

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
	char *ptr = (char *)buf;

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

/*
 * constructors for the db_conn class
 */
db_comm::db_comm(const char *host)
{
	int ret;

	if (this->db_sock == -1) {
		if ((ret = db_connect(host)) < 0)
			throw(ret);
		else
			this->db_sock = ret;
	}
}

db_comm::db_comm(void)
{
	const char *host;
	int ret;

	if (this->db_sock == -1) {
		if (!(host = getenv("DSERVHOST")))
			host = "localhost";
		if ((ret = db_connect(host)) < 0)
			throw(ret);
	}
}

db_comm::~db_comm(void)
{
}

/*
 * connect to the database server.  the only returns if the socket 
 * call succeeds is either a small negative number or the string "ok"
 */
int db_comm::db_connect(const char *host)
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
		return(ESOCKOPT);
	}
	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(sock);
		return(ENOCONN);
	}
//
// we have made a connection.  send them a magic string so that the
// server knows that we are to be trusted.  (christy's birthday)
//
	if (write(sock, "9-30-1966", 9) != 9)
		return(ENORESP);
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
			}
		}
		break;
	}
	if (ioctl(sock, FIONREAD, &i) < 0 || i == 0)
		db_err(0, "%s: ioctl failed, socket gone", _progname);

	if (read(sock, resp, i) != i)
		return(ENORESP);
/*
 * return the response
 */
	if (strcmp(resp, "ok"))
		sock = atoi(resp);
	return(sock);
}

/*
 * send a message to the server then get the response
 */
char *db_comm::db_send(char *cmd, int len)
{

	int32_t size;

/*
 * the reason I chose to do two writes is that it would take much less
 * time and effort than to allocate a new buffer len+4 bytes long, copy
 * in the length, then the buffer, then call write once, and then de-
 * allocate the buffer.
 */
	if (this->db_sock < 0) {
		fprintf(stderr, "%s: DB operation attempted without communications established\n", _progname);
		fprintf(stderr, "command ->");
		fwrite(cmd, 1, len, stderr);
		fprintf(stderr, "<-\n");
		fflush(stderr);
		exit(0);
	}

//	size = htonl(s_len);
	size = htonl(len);
	if (!write_all(this->db_sock, (char *)&size, sizeof(int32_t)))
		db_err(0, "%s: Can't write wrapper to socket", _progname);

	if (!write_all(this->db_sock, cmd, (size_t)len))
		db_err(0, "%s: Can't write to socket", _progname);

/*
 * Read the length wrapper, then exactly that many response bytes.
 */
	if (!read_exact(this->db_sock, (char *)&size, sizeof(int32_t)))
		db_err(0, "%s: Can't read response wrapper", _progname);
	size = ntohl(size);
	if (size < 0)
		db_err(0, "%s: invalid response length %d", _progname, size);

	char *ret = new char[size+1];
	memset(ret, '\0', size+1);

	if (!read_exact(this->db_sock, ret, (size_t)size))
		throw(comm_err);
	return(ret);
}


#include <index.hh>
#include "../server/dbfunc.h"

void db_comm::db_discon(void)
{
	char msg[128];
	char *s;

	if (this->db_sock < 0)
		return;
	if (is_sort) {
		sprintf(msg, "%d|%d|", ICLOSE, cur_index->get_idxno());
		s = this->db_send(msg, strlen(msg));
		delete[] s;
		sprintf(msg, "%d|%d|", ICLOSE, -(workfile.getchan()));
		s = this->db_send(msg, strlen(msg));
		delete[] s;
	} else {
/*
 * do something here to clean up any records that are protected
 */
//		for (i = 0; i < 6; i++) {
//			if (strlen(_indices[i]._idxname)) {
//				sprintf(msg, "%d|%d|", ICLOSE, _indices[i]._idxno);
//				s = comm.db_send(msg, strlen(msg));
//				delete s;
//			}
//		}
		if (!dataman_has_php) {
			if (workfile.getdirty())
				workfile.out_rec();
			sprintf(msg, "%d|%d|", ICLOSE, -(workfile.getchan()));
			s = this->db_send(msg, strlen(msg));
			delete[] s;
		}
	}
	sprintf(msg, "%d", DISCON);
	s = this->db_send(msg, strlen(msg));
	if (memcmp(s, "ok", 2))
		fprintf(stderr, "Didn't get proper shutdown reply\n");
	close(this->db_sock);
	this->db_sock = -1;
	delete[] s;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
