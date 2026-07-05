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

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>

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
int send_to_server(context_t *ctxt, char *fptr, char *vptr, int fsize, int vsize)
{
	int i;

	MSG msgbuf;

	struct timeval tv;

	struct sembuf sop;			/* semaphore operation */

	union semun sarg;

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
		if (msgsnd(ctxt->msgid, (void *)&msgbuf, (size_t)fsize, 0) < 0) {
			if (dbgsw) {
				fprintf(stderr, "msgsnd is failing - %d\n", errno);
				fflush(stderr);
			}
			tv.tv_sec = 0;
			tv.tv_usec = 500;
			select(1, NULL, NULL, NULL, &tv);
			continue;
		} else
			break;
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
		while(1) {
			sop.sem_num = 0;
			sop.sem_op = -1;
			sop.sem_flg = 0;
			if (semop(ctxt->semid, &sop, 1) > -1)
				break;
			if (errno != EINTR) {
				fprintf(stderr, "pid %d: semop failed: ", ctxt->mypid);
				perror("");
				return(0);
			}
		}
		i = min(vsize, shmsiz);
		memcpy(ctxt->shptr, vptr, i);
		vptr += i;
		vsize -= i;
		sarg.val = 0;
		semctl(ctxt->semid, 0, SETVAL, sarg);
	}
	return(1);
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
int recv_from_server(context_t *ctx, MSG *msgbuf, char **ptr, int *len, size_t *buflen)
{

	static int maxsize = MAXSIZ;		/* default maxsize of send buff */

	int i;
	int j;
	int size;

	char *sndbuf;

	struct sembuf sop;			/* semaphore operation */

	sndbuf = *ptr;
	while (1) {
		memset((char *)msgbuf, '\0', sizeof(msgbuf));
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
			return(0);
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
				fprintf(stderr, "pid %d: failed realloc 2 - size = %d: ",
								ctx->mypid, j);
				perror("");
				return(0);
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
			while (1) {
				sop.sem_num = 0;
				sop.sem_op = 0;
				sop.sem_flg = 0;
				if (semop(ctx->semid, &sop, 1) > -1)
					break;
				if (errno != EINTR) {
					fprintf(stderr, "pid %d, semop error %d\n",
							ctx->mypid, errno);
					_exit(0);
				}
			}
			size = min(j, shmsiz);
			memcpy(sndbuf+i, ctx->shptr, size);
			i += size;
			j -= size;
			sop.sem_num = 0;
			sop.sem_op = 2;
			sop.sem_flg = 0;
			semop(ctx->semid, &sop, 1);
		}
		if (dbgsw) {
			fprintf(stderr, "done with shared mem\n");
			fflush(stderr);
		}
	}
	return(1);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
