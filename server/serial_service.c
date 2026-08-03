/* ***************************************************************
 *
 * PROCEDURE:	serial_service
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		Tue Feb 19 18:01:50 MST 2002
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 *				Thu Mar 21 10:03:47 MST 2002
 *				added shared memory and semaphore for passing
 *				the actual data records around, since they
 *				-may- be (much) larger than the message queue
 *				tomg
 *
 *				Mon Mar 25 08:45:15 MST 2002
 *				added a list of open indices so that if the
 *				user terminates or the socket goes away before
 *				all of the indices have been closed, this will
 *				take care of it.
 *				tomg
 *
 *				Sat Oct 18 23:26:19 MDT 2003
 *				Changed to make the send and receive indicate
 *				the message size as well.  also loops on the
 *				read until the complete message is received.
 *				tomg
 *
 *				Thu Oct 23 20:48:18 MDT 2003
 *				added some code to enhance security.  no dos
 *				and no buffer overflows
 *				tomg
 *
 *				Mon Jun 28 21:51:21 MDT 2004
 *				changed so that this routine calls _exit() so
 *				that termination functions are not called.
 *				tomg
 *
 * 				Thu Aug 5 09:06:36 MDT 2004
 * 				fixed a race condition with the semaphore that
 * 				would let data overwrite itself in the shared
 * 				mem segment
 * 				tomg
 *
 * 				Fri May  5 18:02:49 MDT 2006
 * 				tried compiling on a 64 bit machine.  need to be
 *				careful of declarations.  with gcc
 *										32 bit cpu		64 bit cpu
 *					sizeof(char)			1				1
 *					sizeof(short)			2				2
 *					sizeof(int)				4				4
 *					sizeof(long)			4				8
 *					sizeof(long long)		8				8
 *					sizeof(void *)			4				8
 *					sizeof(size_t)			4				8
 *					sizeof(ssize_t)			4				8
 *					sizeof(pid_t)			4				4
 *					sizeof(off_t)			4				8
 *					sizeof(loff_t)			8				8
 * 				tomg
 *
 *				Fri Jun 16 19:53:10 MDT 2006
 *				this file got too freaking big, so i needed to make
 *				this function smaller and more	modular.  broke it
 *				up into smaller parts so that the transaction
 *				processing stuff would work better
 *				tomg
 *
 *				Thu Mar 21 16:06:40 MDT 2013
 *				tomg
 *				fixed bug not setting message buffer length when
 *				adding transaction
 *
 *				Sun Jul 19 10:30:23 PM MDT 2026
 *				tomg
 *				did some hardening better error checking
 *
 ************************************************************* */

/*
 * this is the routine that services the connection from the
 * remote program.  There are two types of messages.  The
 * first is a 'short' message.  this says do something, and
 * there is no additional data associated with it.  The
 * second type is a 'long' message.  This is used when data
 * records are transfered back and forth.  instead of using
 * a message queue for this, use shared memory.  it's faster
 * and doesn't suffer serialization problems.
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
#include <string.h>
#include <limits.h>
#include <stdbool.h>

#include <signal.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#if defined(__GNU_LIBRARY__) && !defined(_SEM_SEMUN_UNDEFINED)
/* union semun is defined by including <sys/sem.h> */
#else
/* according to X/OPEN we have to define it ourselves */
union semun {
	int val;                    /* value for SETVAL */
	struct semid_ds *buf;       /* buffer for IPC_STAT, IPC_SET */
	unsigned short int *array;  /* array for GETALL, SETALL */
	struct seminfo *__buf;      /* buffer for IPC_INFO */
};
#endif

#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>			/* for def of ntohl */

#include <errno.h>

#include "msg.h"
#include "dbfunc.h"
#include "errors.h"
#include "misc.h"

#define TRUE	1
#define FALSE	0

extern void put_long (void *, int32_t);

extern void store_ix(char *, int);
extern void do_iclose(char *, int);
extern void store_prot(char *);
extern void do_clear(char *, key_t);
extern int soc_setup(int, pid_t);
extern int rollback(context_t *);
extern int commit(context_t *);
extern void xact_del_list(void);
extern int store_xact(context_t *, char **, int, MSG *, char **, int *, size_t *);
extern int msg_setup(int , pid_t);
extern int sem_setup(int, pid_t);
extern int shm_setup(int, pid_t, char **);
extern bool send_to_server(context_t *, char *, char *, int, int);
extern bool send_to_server_stream(context_t *, char *, int, int, int);
extern bool recv_from_server_stream(context_t *, MSG *, int);
extern bool recv_from_server(context_t *, MSG *, char **, int *, size_t *);

extern int dbgsw;						/* debugging on? */
extern int shmsiz;						/* size of shared mem seg */


void sigprint(int sig)
{
	fprintf(stderr, "caught signal %d\n", sig);
}

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

/*
 * Long commands have five fixed fields before their decimal payload length.
 * Verify that the declared length exactly consumes the bytes left in the
 * socket frame before handing the payload to shared-memory transfer code.
 */
static int shared_payload(char *buf, int frame_len, char **payload, int *payload_len,
						int *command_len)
{
	char *field;
	char *sep;
	char *end;
	char saved;
	long value;
	int i;

	end = buf + frame_len;
	field = buf;
	for (i = 0; i < 5; i++) {
		sep = memchr(field, '|', (size_t)(end - field));
		if (sep == NULL)
			return(0);
		field = sep + 1;
	}
	sep = memchr(field, '|', (size_t)(end - field));
	if (sep == NULL || sep == field)
		return(0);

	saved = *sep;
	*sep = '\0';
	errno = 0;
	value = strtol(field, &field, 10);
	*sep = saved;
	if (errno == ERANGE || *field != '|' || value < 0 || value > INT_MAX)
		return(0);

	*payload = sep + 1;
	*payload_len = (int)value;
	*command_len = (int)(*payload - buf);
	return((size_t)*payload_len == (size_t)(end - *payload));
}

static int shared_payload_header(char *buf, int header_len, int frame_len,
						int *payload_len)
{
	char *field = buf;
	char *sep;
	char *end = buf + header_len;
	char saved;
	long value;
	int i;

	for (i = 0; i < 5; i++) {
		sep = memchr(field, '|', (size_t)(end - field));
		if (sep == NULL)
			return(0);
		field = sep + 1;
	}
	sep = memchr(field, '|', (size_t)(end - field));
	if (sep == NULL || sep == field || sep + 1 != end)
		return(0);
	saved = *sep;
	*sep = '\0';
	errno = 0;
	value = strtol(field, &field, 10);
	*sep = saved;
	if (errno == ERANGE || *field != '|' || value < 0 || value > INT_MAX)
		return(0);
	*payload_len = (int)value;
	return(*payload_len == frame_len - header_len);
}

/* Read just enough of a long command to locate and validate its payload. */
static int read_command_prefix(int fd, char *buf, int frame_len, int *cmd,
						int *prefix_len, int *payload_len)
{
	int fields = 0;
	int len = 0;
	char ch;

	while (fields < 6) {
		if (len >= MAXSIZ || len >= frame_len || !read_exact(fd, &ch, 1))
			return(-1);
		buf[len++] = ch;
		if (ch != '|')
			continue;
		fields++;
		if (fields == 1) {
			buf[len - 1] = '\0';
			*cmd = atoi(buf);
			buf[len - 1] = '|';
			if (*cmd != FLUSH) {
				*prefix_len = len;
				return(0);
			}
		}
	}
	buf[len] = '\0';
	*prefix_len = len;
	if (!shared_payload_header(buf, len, frame_len, payload_len))
		return(-1);
	return(1);
}

/*
 * this is where things really start
 */
void serial_service(int sock)
{

	int32_t size;

	size_t len;
	size_t buflen;

	int maxread = MAXSIZ;		/* default maxsize of read buff */
	int i, j;
	int prefix_len;
	int stream_input;
	int frame_len;

	int cmd;					/* received command */
	int isw;
	int in_xact;				/* are we in a transaction */

	char *rcvbuf;				/* socket receive buffer */
	char *sndbuf;				/* send buffer */
	char *ptr;

	 context_t context;


	MSG msgbuf;

	fd_set readfds;				/* readable fds for select */

	struct timeval tv;
	struct sembuf sop;			/* semaphore operation */
	union semun sarg;

	if (dbgsw) {
		char buff[128];
		sprintf(buff, "/tmp/serial.%d.log", getpid());
		if (freopen(buff, "w+", stderr) == NULL)
			dbgsw = 0;
	}

	context.mypid = getpid();
	context.msgid = -1;
	context.semid = -1;					/* semaphore id returned by semget() */
	context.shmid = -1;					/* shared memory id returned by shmget() */
	context.shptr = NULL;
	in_xact = 0;						/* don't start in a transaction */

	if (!soc_setup(sock, context.mypid))
		goto done;

	sndbuf = malloc(MAXSIZ);
	rcvbuf = malloc(MAXSIZ);
	if (sndbuf == NULL || rcvbuf == NULL) {
		char errstr[8];
		i = sprintf(errstr, "%d", ENOALLOC);
		if (!write_all(sock, errstr, (size_t)i)) {
			fprintf(stderr, "pid %d: error writing ENOALLOC to socket: ", context.mypid);
			perror("");
		}
		fprintf(stderr, "pid %d: error allocating system memory", context.mypid);
		perror("");
		goto done;
	}
/*
 * here we want to let the remote client identify itself.  if it doesn't
 * within a second or know what we are expecting, then boot it!
 * 		(christy's birthday)
 */
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	FD_ZERO(&readfds);
	FD_SET(sock, &readfds);
	if (select(sock+1, &readfds, NULL, NULL, &tv))
		if (read(sock,rcvbuf,9) == 9)
			if (!memcmp(rcvbuf,"9-30-1966", 9))
/*
 * FIONREAD requires 'int *' as arg.  I don't think we'll be compiling
 * on 16 bit systems, but by definition an 'int' can be 16, 32, or 64 bits.
 */
				if (ioctl(sock, FIONREAD, &i) > -1)
					goto ok_conn;

	fprintf(stderr, "Attempt to connect from non dataman!\n");
	close(sock);
	return;

ok_conn:
	while(i--)
		if (read(sock, rcvbuf, 1) < 1)	/* if there is an error, the socket is empty */
			break;

	if ((context.msgid = msg_setup(sock, context.mypid)) < 0)
		goto done;
	if ((context.semid = sem_setup(sock, context.mypid)) < 0)
		goto done;
	if ((context.shmid = shm_setup(sock, context.mypid, &context.shptr)) < 0)
		goto done;
/*
 * finally, everything is set up, notify the remote client
 */
	if (!write_all(sock, "ok", 2)) {	/* let client know we are ok */
		fprintf(stderr, "pid %d: failed notification: ", context.mypid);
		perror("");
		goto done;
	}
/*
 * go for ever reading requests from this process.
 */
	while (1) {
		isw = -1;
		FD_ZERO(&readfds);
		FD_SET(sock, &readfds);
		if (select(sock+1, &readfds, NULL, NULL, NULL) < 1) {
			fprintf(stderr, "pid %d: select failed: ", context.mypid);
			perror("");
			break;
		}
		if (dbgsw) {
			fprintf(stderr, "select succeded\n");
			fflush(stderr);
		}
/*
 * a message has at least 4 bytes.  this is the wrapper that indicates the
 * the length of the following message.  it is network byte order.
 */
		if (!read_exact(sock, (char *)&size, sizeof(int32_t))) {
			fprintf(stderr, "pid %d: failed reading command header\n", context.mypid);
			close(sock);
			break;
		}
		size = ntohl(size);
		if (size < 1 || size == INT_MAX) {
			fprintf(stderr, "pid %d: invalid command length %d\n", context.mypid, size);
			close(sock);
			break;
		}
		if (dbgsw) {
			fprintf(stderr, "read header length %d\n", size);
			fflush(stderr);
		}
		frame_len = size;

		stream_input = read_command_prefix(sock, rcvbuf, size, &cmd, &prefix_len, &j);
		if (stream_input < 0) {
			fprintf(stderr, "pid %d: invalid command prefix\n", context.mypid);
			break;
		}
		if (stream_input == 0) {
			j = size + 1;
			if (j > maxread) {
				char *newbuf = realloc(rcvbuf, (size_t)j);

				if (newbuf == NULL) {
					fprintf(stderr, "pid %d: failed realloc 1 - size = %d: ", context.mypid, j);
					perror("");
					break;
				}
				rcvbuf = newbuf;
				maxread = j;
			}
			if (!read_exact(sock, rcvbuf + prefix_len, (size_t)(size - prefix_len))) {
				fprintf(stderr, "pid %d: command read failed: ", context.mypid);
				perror("");
				break;
			}
			rcvbuf[size] = '\0';
			cmd = atoi(rcvbuf);
		} else {
			size = prefix_len;
		}
		if (dbgsw) {
			fprintf(stderr, "read socket ->");
			fwrite(rcvbuf, sizeof(char), (size_t)size, stderr);
			fprintf(stderr, "<-\n");
			fflush(stderr);
		}
/*
 * check the command.  if it is a FLUSH command, we are receiveing
 * a database record with it.  ummm... this might be big, so don't
 * use a message queue for it.  terminate the message at the end
 * of the command buffer, and copy the data record into shared
 * memory.  if there is more than one segment worth of data then
 * after we send the message we will need to loop on copying more
 * into the shared memory segment.
 */
		if (cmd == START_XACT) {
			if (in_xact) {
				i = sprintf(sndbuf+sizeof(int32_t), "%d|", EINXACT);
			} else {
				in_xact = 1;
				i = sprintf(sndbuf+sizeof(int32_t), "1|");
			}
			put_long(sndbuf, (int32_t)i);
			i += sizeof(int32_t);
			if (!write_all(sock, sndbuf, (size_t)i)) {
				fprintf(stderr, "Can't write XACT response to socket:");
				perror("");
				exit(0);
			}
			if (i > 6)
				goto done;
			continue;
		}
		if (cmd == COMMIT || cmd == ROLLBACK) {
			if (!in_xact) {
				i = sprintf(sndbuf+sizeof(int32_t), "%d|", ENOXACT);
				put_long(sndbuf, (int32_t)i);
				i += sizeof(int32_t);
				if (!write_all(sock, sndbuf, (size_t)i)) {
					fprintf(stderr, "Can't write ENOXACT to socket:");
					perror("");
					exit(0);
				}
				goto done;
			}
			in_xact = 0;
			i = TRUE;
			if (cmd == COMMIT) {
				i = commit(&context);
				if (!i)
					if (!rollback(&context))
						i = EROLLBACK;
			}
			xact_del_list();
			i = sprintf(sndbuf+sizeof(int32_t), "%d|", i);
			put_long(sndbuf, (int32_t)i);
			i += sizeof(int32_t);
			if (!write_all(sock, sndbuf, (size_t)i)) {
				fprintf(stderr, "Can't write COMMIT response to socket:");
				perror("");
				exit(0);
			}
			continue;
		}

		if (cmd == DISCON) {
			if (dbgsw) {
				fprintf(stderr, "got disconnect message,closing down\n");
				fflush(stderr);
			}
			break;					/* got disconnect message from client */
		}

		if (cmd >= FLUSH) {
			sarg.val = 2;
			if (semctl(context.semid, 0, SETVAL, sarg) < 0) {
				fprintf(stderr, "pid %d: error setting semaphore value", context.mypid);
				perror("");
				break;
			}
			if (!stream_input && !shared_payload(rcvbuf, size, &ptr, &j, &size)) {
				fprintf(stderr, "pid %d: invalid shared-memory payload frame\n", context.mypid);
				break;
			}
			if (stream_input)
				ptr = NULL;
		} else {
			j = 0;
			ptr = NULL;
		}
		isw = cmd;
		if (stream_input && in_xact) {
			if (frame_len + 1 > maxread) {
				if ((rcvbuf = realloc(rcvbuf, (size_t)frame_len + 1)) == NULL) {
					fprintf(stderr, "pid %d: failed realloc for transaction payload: ", context.mypid);
					perror("");
					break;
				}
				maxread = frame_len + 1;
			}
			if (!read_exact(sock, rcvbuf + size, (size_t)j)) {
				fprintf(stderr, "pid %d: transaction payload read failed: ", context.mypid);
				perror("");
				break;
			}
			rcvbuf[frame_len] = '\0';
			if (!shared_payload(rcvbuf, frame_len, &ptr, &j, &size))
				break;
			stream_input = 0;
		}
/*
 * verify that the base command we got wasn't bigger than the
 * buffer we have to store it in.  if it is, we can bet it is
 * malicious and we had better close this connection!
 */
		if (size > MAXSIZ) {
			sprintf(sndbuf+sizeof(int32_t), "%d", EINVMSG);
			put_long(sndbuf, (int32_t)sizeof(int32_t));
			i = strlen(sndbuf+sizeof(int32_t)) + sizeof(int32_t);
			if (!write_all(sock, sndbuf, (size_t)i)) {
				fprintf(stderr, "pid %d: error writing EINVMSG to socket: ", context.mypid);
				perror("");
			}
			fprintf(stderr, "pid %d: received bad message ->%s<-", context.mypid, rcvbuf);
			break;
		}
/*
 * if we are in a transaction, these commands are the ones that still
 * modify the database, so we need to save them until we do a commit.
 * otherwise we send the command to the server, and get the  response.
 */
		if (stream_input && !in_xact) {
			if (!send_to_server_stream(&context, rcvbuf, size, j, sock))
				break;
			if (!recv_from_server_stream(&context, &msgbuf, sock))
				break;
			continue;
		} else if (in_xact && (cmd ==  DELETE || cmd == INSERT || cmd == INCLUDE
								|| cmd == REMOVE || cmd == FLUSH)) {
			msgbuf.type = size;
			if (!store_xact(&context, &rcvbuf, maxread, &msgbuf, &sndbuf, &i, &buflen))
				break;
		} else {
			if (!send_to_server(&context, rcvbuf, ptr, size, j))
				break;
			if (!recv_from_server(&context, &msgbuf, &sndbuf, &i, &buflen))
				break;
		}
/*
 * if the first field of the response (returned in i) is negative it is
 * an error return.  if it is positive there is a shared memory portion to
 * the response and if it is zero, then you can just send the response back.
 * i is set in recv_from_server.  if it is < 0 there was a database error.
 * if it is == 0 it is a simple response.  if > 0 then the return is already
 * setup
 */
		if (dbgsw) {
			fprintf(stderr, "back from recv_from_server, i = %d\n", i);
			fflush(stderr);
		}
/*
 * if the command sent was an iopen or iclose, we need to save
 * or remove the index from the list. the same with protect and
 * clear;
 */
		if (i > -1) {
			switch(isw) {
				case IOPEN:
				case INIT_DAT:
				case MKIDX:
					store_ix(msgbuf.txt, isw);
					break;
				case ICLOSE:
					/* The response no longer needs to expose the server slot. */
					do_iclose(rcvbuf, context.msgid);
					break;
				case PROTECT:
					store_prot(msgbuf.txt);
					break;
				case CLEAR:
					do_clear(msgbuf.txt, context.msgid);
					break;
				default:
					break;
			}
			len = i;
		}
		if (i < 0) {
			len = sprintf(sndbuf, "%d|", i);
		} else if (i == 0) {
/*
 * 0 length return means just the command buffer has the answer.
 */
			ptr = strchr(msgbuf.txt, '|') + 1;
			len = buflen - (ptr-msgbuf.txt);
			memcpy(sndbuf, ptr, len+1);
		}
		if (dbgsw && i >= 0) {
			fprintf(stderr, "final receipt is\n");
			fwrite(sndbuf, 1, i, stderr);
			fprintf(stderr, "\nwriting %d bytes\n", (int)i);
			fflush(stderr);
		}
		size = htonl((int32_t)len);
		if (!write_all(sock, (char*)&size, sizeof(int32_t))) {
			if (dbgsw) {
				fprintf(stderr, "failed header size write!, errno = %d\n", errno);
				fflush(stderr);
			}
			break;
		}
		if (!write_all(sock, sndbuf, len)) {
			if (dbgsw) {
				fprintf(stderr, "failed sending buffer to client, errno = %d\n", errno);
				fflush(stderr);
			}
			break;
		}
		sarg.val = 0;
		semctl(context.semid, 0, SETVAL, sarg);
	}

done:
	do_clear(NULL, context.msgid);				/* clear any recs left protected */
	do_iclose(NULL, context.msgid);				/* close any indices left open */
	if (context.semid > -1)
		semctl(context.semid, 0, IPC_RMID, 0);	/* remove the semaphore */
	if (context.shmid > -1)
		shmctl(context.shmid, IPC_RMID, 0);		/* remove the shared memory */
	put_long(sndbuf, (int32_t)2);
	memcpy(sndbuf+sizeof(int32_t), "ok", 2);
	i = write_all(sock, sndbuf, 2+sizeof(int32_t));	/* shutting down the socket, don't care if it fails */
	shutdown(sock, SHUT_WR);			/* shutdown the socket */
	if (dbgsw) {
		fprintf(stderr, "sent shutdown message from pid %d\n", context.mypid);
		fflush(stderr);
	}
	_exit(0);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
