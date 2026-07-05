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

	fd_set rfds;

	int32_t size;

	size_t count = len;
	size_t off;
	size_t num;

	int i;
	int j;
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
	if (write(this->db_sock, (char *)&size, sizeof(int32_t)) != sizeof(int32_t))
		db_err(0, "%s: Can't write wrapper to socket", _progname);

	j = 0;
	while(count) {
		if ((i = write(this->db_sock, cmd+j, count)) < 0)
			db_err(0, "%s: Can't write to socket", _progname);
		count -= i;
		j += i;
	}

	while (1) {
		FD_ZERO(&rfds);
		FD_SET(this->db_sock, &rfds);
		if (select(this->db_sock+1, &rfds, NULL, NULL, NULL) > -1)
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
	if (ioctl(this->db_sock, FIONREAD, &len) < 0 || len == 0)
		db_err(0, "%s: ioctl failed, socket gone", _progname);
	if (len < 4)
		db_err(0, "%s: wrapper length is too small - %d", _progname, len);

	if ((i = read(this->db_sock, (char *)&size, sizeof(int32_t))) != sizeof(int32_t))
		db_err(0, "%s: read only %d header bytes", _progname, i);
	size = ntohl(size);

	char *ret = new char[size+1];
	memset(ret, '\0', size+1);

//
//read until we either get an error or the number of bytes that the wrapper
//tells us we are supposed to, then return the buffer
//
	off = 0;
	count = size;
	while(count > 0) {
		if ((num = read(this->db_sock, ret+off, count)) < 0)
			throw(comm_err);
		off += num;
		count -= num;
	}
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
