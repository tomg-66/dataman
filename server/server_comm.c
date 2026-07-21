/* ***************************************************************
 *
 * PROCEDURE:	server_comm
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		Fri Jun 16 21:04:27 MDT 2006
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 *
 *				Sun Jul 19 10:20:28 PM MDT 2026
 *				tomg
 *				hardened the connections, make the copys go directly
 *				to/from shared memory instead of doing extra copys
 *
 ************************************************************* */
/*
 * send data to the server, and get it back.  it makes the
 * serial_service routine much cleaner, and now that I'm writing
 * transaction processing, having this a seperate function saves
 * a lot of code duplication.
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
#include <stdbool.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <arpa/inet.h>

#include <time.h>
#include <errno.h>

#include "msg.h"
#include "misc.h"

#ifndef min
#define min(x,y)	((x)<(y)?(x):(y))
#endif

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

extern int dbgsw;
extern int shmsiz;

/*
 * semaphone ops.  they are analogous to the sem_ functions in dispatch.c
 */
static bool sem_wait_for_zero(int semid, pid_t pid)
{
	struct sembuf sop;

	while (true) {
		sop.sem_num = 0;
		sop.sem_op = 0;
		sop.sem_flg = 0;
		if (semop(semid, &sop, 1) == 0)
			return(true);
		if (errno != EINTR)
			break;
	}
	fprintf(stderr, "pid %d: semaphore wait failed: ", pid);
	perror("");
	return(false);
}

static bool sem_take_slot(int semid, pid_t pid)
{
	struct sembuf sop;

	while (true) {
		sop.sem_num = 0;
		sop.sem_op = -1;
		sop.sem_flg = 0;
		if (semop(semid, &sop, 1) == 0)
			return(true);
		if (errno != EINTR)
			break;
	}
	fprintf(stderr, "pid %d: semaphore take failed: ", pid);
	perror("");
	return(false);
}

static bool sem_publish_chunk(int semid, int value, pid_t pid)
{
	union semun sarg;

	sarg.val = value;
	if (semctl(semid, 0, SETVAL, sarg) == 0)
		return(true);
	fprintf(stderr, "pid %d: semaphore publish failed: ", pid);
	perror("");
	return(false);
}

static bool sem_release_slot(int semid, pid_t pid)
{
	struct sembuf sop;

	sop.sem_num = 0;
	sop.sem_op = 2;
	sop.sem_flg = 0;
	if (semop(semid, &sop, 1) == 0)
		return(true);
	fprintf(stderr, "pid %d: semaphore release failed: ", pid);
	perror("");
	return(false);
}
/* --------------------------------------------------------------- */

/*
 * read data from the socket
 */
static bool read_socket_chunk(int fd, void *buf, size_t len)
{
	char *ptr = buf;
	ssize_t ret;

	while (len) {
		ret = read(fd, ptr, len);
		if (ret < 0 && errno == EINTR)
			continue;
		if (ret <= 0)
			return(false);
		ptr += ret;
		len -= ret;
	}
	return(true);
}

/*
 * write data to the socket
 */
static bool write_socket_chunk(int fd, const void *buf, size_t len)
{
	const char *ptr = buf;
	ssize_t ret;

	while (len) {
		ret = write(fd, ptr, len);
		if (ret < 0 && errno == EINTR)
			continue;
		if (ret <= 0)
			return(false);
		ptr += ret;
		len -= ret;
	}
	return(true);
}

/*
 * send the command portion of the message to the server
 */
static bool queue_command(context_t *ctxt, char *fptr, int fsize)
{
	MSG msgbuf;
	struct timeval tv;
	int i;

	msgbuf.type = MSG_SRV;
	memset(msgbuf.txt, '\0', MAXSIZ);
	i = sprintf(msgbuf.txt, "%d|", ctxt->mypid);
	if (fsize < 0 || i + fsize > MAXSIZ)
		return(false);
	memcpy(msgbuf.txt + i, fptr, fsize);
	fsize += i;

	/*
	 * the msgsnd will block if the message queue is full.  other errors can terminate
	 * the send, or restart it.
	 */
	while (true) {
		if (msgsnd(ctxt->msgid, (void *)&msgbuf, (size_t)fsize, 0) > -1) {
			break;
		} else {
			if (errno == EIDRM) {
				fprintf(stderr, "some twit removed the message queue while open!\n");
				return(false);
			} else if (errno == EINTR) {
				tv.tv_sec = 0;
				tv.tv_usec = 500;				/* wait 500 microsec */
				select(1, NULL, NULL, NULL, &tv);
			} else {
				perror("Error sending command to dispatch");
				return(false);
			}
		}
	}
	return(true);
}

/* Queue the fixed command, then stream payload chunks directly into shm. */
bool send_to_server_stream(context_t *ctxt, char *fptr, int fsize, int vsize, int sock)
{
	int size;

	if (!queue_command(ctxt, fptr, fsize))
		return(false);
	while (vsize) {
		if (!sem_take_slot(ctxt->semid, ctxt->mypid))
			return(false);
		size = min(vsize, shmsiz);
		if (!read_socket_chunk(sock, ctxt->shptr, (size_t)size))
			return(false);
		vsize -= size;
		if (!sem_publish_chunk(ctxt->semid, 0, ctxt->mypid))
			return(false);
	}
	return(true);
}

/* Receive a response and write each large shared-memory chunk to the client. */
bool recv_from_server_stream(context_t *ctx, MSG *msgbuf, int sock)
{
	int fixed_len;
	int payload_len;
	int size;
	int32_t frame_len;

	while (true) {
		memset(msgbuf, '\0', sizeof(*msgbuf));
		fixed_len = (int)msgrcv(ctx->msgid, msgbuf, (size_t)MAXSIZ, (long)ctx->mypid, 0);
		if (fixed_len >= 0)
			break;
		if (errno != EINTR)
			return(false);
	}
	msgbuf->txt[fixed_len] = '\0';
	payload_len = atoi(msgbuf->txt);
	if (payload_len < 0)
		payload_len = 0;
	frame_len = htonl((int32_t)(fixed_len + payload_len));
	if (!write_socket_chunk(sock, &frame_len, sizeof(frame_len)) ||
			!write_socket_chunk(sock, msgbuf->txt, (size_t)fixed_len))
		return(false);
	while (payload_len) {
		if (!sem_wait_for_zero(ctx->semid, ctx->mypid))
			return(false);
		size = min(payload_len, shmsiz);
		if (!write_socket_chunk(sock, ctx->shptr, (size_t)size) ||
				!sem_release_slot(ctx->semid, ctx->mypid))
			return(false);
		payload_len -= size;
	}
	return(true);
}

/*
 * the context contains the various things that define this connection.
 * fptr is the pointer to the fixed part of the command.  vptr points to
 * the shared memory stuff, fsize is the length of the fixed part, and
 * vsize is the length of the shared memory part.
 * 
 * ctxt is the context of -this- connection
 * fptr is a pointer to the command string
 * vptr is a pointer to the data that goes to shared memory
 * fsize is the length of the command string
 * vsize is the length of the shared memory data
 */
bool send_to_server(context_t *ctxt, char *fptr, char *vptr, int fsize, int vsize)
{
	int i;

	MSG msgbuf;

	struct timeval tv;

/*
 * the message we send to the database server consists of our pid
 * and the message we received.  this is so that the database
 * server knows to send the data back to us.
 */
	msgbuf.type = MSG_SRV;
	memset(msgbuf.txt, '\0', MAXSIZ);
	i = sprintf(msgbuf.txt, "%d|", ctxt->mypid);
	memcpy(msgbuf.txt+i, fptr, fsize);

	fsize += i;
	if (dbgsw) {
		fprintf(stderr, "fsize = %d, msg = %s\n", fsize, msgbuf.txt);
		fflush(stderr);
	}
	while (1) {
		if (msgsnd(ctxt->msgid, (void *)&msgbuf, (size_t)fsize, 0) > -1) {
			break;
		}
		if (dbgsw) {
			fprintf(stderr, "%s: line %d: msgsnd is failing: %s\n", __func__, __LINE__, strerror(errno));
			fflush(stderr);
		}
		if (errno == EIDRM) {
			return (false);
		} else if (errno == EINTR) {
			tv.tv_sec = 0;
			tv.tv_usec = 500;
			select(1, NULL, NULL, NULL, &tv);
		} else {
			return (false);
		}
	}

	if (dbgsw) {
		fprintf(stderr, "msgsnd is done\n");
		fflush(stderr);
	}
/*
 * if we are sending a large data record, the server will set up the
 * semaphore for us to start copying in more data to the shared memory
 * segment as that process gets it out.
 */
	while(vsize) {
		if (!sem_take_slot(ctxt->semid, ctxt->mypid))
			return(false);
		i = min(vsize, shmsiz);
		memcpy(ctxt->shptr, vptr, i);
		vptr += i;
		vsize -= i;
		if (!sem_publish_chunk(ctxt->semid, 0, ctxt->mypid))
			return(false);
	}
	return(true);
}

/*
 * receive a message back from the server
 *
 * ctx is the context for -this- connection
 * msgbuf contains the basic portion of the command return
 * ptr is the address of the buffer that will get the shared memory data
 * len receives:
 * 		< 0 it is an error return
 * 		= 0 it is a true return but no shared memory data
 * 		> 0 length of the basic return + length of shared memory
 *
 * 	buflen gets the size of the message returned from the server
 */
bool recv_from_server(context_t *ctx, MSG *msgbuf, char **ptr, int *len, size_t *buflen)
{

	static int maxsize = MAXSIZ;		/* default maxsize of send buff */

	int i;
	int j;
	int size;

	char *sndbuf;

	sndbuf = *ptr;
	while (1) {
		memset((char *)msgbuf, '\0', sizeof(MSG));
		i = (int)msgrcv(ctx->msgid, msgbuf, (size_t)MAXSIZ, (long)ctx->mypid, 0);
		if (dbgsw) {
			fprintf(stderr, "msgrcv returns, size = %d\n", i);
			fflush(stderr);
		}
		if (i < 0) {
			if (errno == EINTR) {
				fprintf(stderr, "it was interrupted\n");
				continue;
			}
			fprintf(stderr, "pid %d: msgrcv failed: ", ctx->mypid);
			perror("");
			return(false);
		}
		break;
	}
	*buflen = i;
	*(msgbuf->txt+i) = '\0';
/*
 * if the first field of the response is negative it is an error return
 * if it is positive there is a shared memory portion to the response and
 * if it is zero, then you can just send the response back.
 */
	*len = atoi(msgbuf->txt);
	if (dbgsw) {
		fprintf(stderr, "len = %d\n", *len);
		fflush(stderr);
	}
/*
 * long data return -
 */
	if (*len > 0) {
		j = i + *len;
		if (j > maxsize || !sndbuf) {
			if ((sndbuf = realloc(sndbuf, j)) == NULL) {
				fprintf(stderr, "pid %d: failed realloc 2 - size = %d: ", ctx->mypid, j);
				perror("");
				return(false);
			}
			if (j > maxsize)
				maxsize = j;
			*ptr = sndbuf;
		}
/*
 * copy the data back out of the shared memory segment.  the semaphore
 * acts as a gatekeeper between us and the thread if there is more than
 * one segment worth of data.
 */
		memcpy(sndbuf, msgbuf->txt, i);
		j = *len;
		*len += i;
		if (dbgsw) {
			fprintf(stderr, "going to copy out %d bytes from shared mem\n", j);
			fflush(stderr);
		}
		while(j) {
			if (!sem_wait_for_zero(ctx->semid, ctx->mypid))
				return(false);
			size = min(j, shmsiz);
			memcpy(sndbuf+i, ctx->shptr, size);
			i += size;
			j -= size;
			if (!sem_release_slot(ctx->semid, ctx->mypid))
				return(false);
		}
		if (dbgsw) {
			fprintf(stderr, "done with shared mem\n");
			fflush(stderr);
		}
	}
	return(true);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
