/* ***************************************************************
 *
 * PROCEDURE:	dispatch
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		Thu Feb 21 13:21:01 MST 2002
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * REVISION HISTORY:
 * 				Thu Aug 5 09:06:36 MDT 2004
 * 				fixed a race condition in the semaphore that
 * 				reared it's ugly head.
 * 				tomg
 *
 *				Mon May  1 20:29:47 MDT 2006
 *				changed the structure so that a new thread wasn't
 *				started for each call, but that each dispatch
 *				thread will listen on the message queue.
 ************************************************************* */
/*
 * this is the newly created thread.  it's purpose is to perform
 * the appropriate database operation and return the response to
 * the appropriate connection server via the message queue.
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

#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>

#include <stdio.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>

#include <pthread.h>

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

#define DISPATCH_C

#if !defined min
#define min(x,y)	((x)<(y)?(x):(y))
#endif

#include "msg.h"
#include "dbfunc.h"
#include "errors.h"

extern int dbgsw;				/* debugging on? */
extern int shmsiz;				/* size of shared mem seg */
extern char *_progname;

extern void err_sys(char *, char *);

void *dispatch(void *dummy)
{

	size_t size;

	int cmd;				/* command to perform */
	int ret;				/* the return value */
	int len;				/* length of this message */
	int i;
	int offs;

	char msg[MAXSIZ];		/* message to operate on */
	char *ptr;				/* parsing pointers */
	char *sptr;
	char *shptr;			/* shared memory seg pointer */

	pid_t pid;				/* pid of waiting process */
	int msgid;				/* the message queue id */
	int semid;				/* semaphore id */
	int shmid;				/* shared memory id */

	MSG msgbuf;

	struct sembuf sop;

	union semun sarg;

/*
 * get the id of the message queue
 */
	if (dbgsw) {
		fprintf(stderr, "Enter dispatch, thread = %d\n", pthread_self());
		fflush(stderr);
	}
	if ((msgid = msgget((key_t)MSGKEY, PERMS|IPC_CREAT)) < 0)
		err_sys("%s: Can't create message queue: ", _progname);
/*
 * now what we want to do is spin up a bunch of threads that are all are
 * just waiting to execute a function.  That way we don't have the over-
 * head of starting a new thread -every- time we want to service a request
 */
	while(1) {
		memset((void *)&msgbuf, '\0', sizeof(MSG));
		if ((i = msgrcv(msgid, &msgbuf, MAXSIZ, MSG_SRV, 0)) < 0) {
			switch(errno) {
				case EIDRM:			/* someone removed the queue! */
					fprintf(stderr, "Some twit removed the message queue"
									"while we were running\n");
					exit(0);
				case EINTR:			/* interrupted call */
					continue;
				default:
					fprintf(stderr, "msgrcv failed: ");
					perror("");
					exit(0);
			}
		}
		*(msgbuf.txt+i) = '\0';
		if (dbgsw) {
			fprintf(stderr, "received message ->%s<-\n", msgbuf.txt);
			fflush(stderr);
		}

/*
 * get the passed in argument.  save the message queue id, and the message.
 * parse the return pid which is message type for the queue, and the
 * command to be executed.
 */
		pid = atoi(msgbuf.txt);
		sptr = strchr(msgbuf.txt, '|') + 1;		/* point past pid */
		cmd = atoi(sptr);
		if (cmd < FUNC_MIN || cmd > FUNC_MAX) {
			ret = EINVMSG;
			goto err_jump;
		}
		sptr = strchr(sptr, '|') + 1;		/* point past cmd */
		if (sptr == (char *)1) {
			ret = EINVMSG;
			goto err_jump;
		}
/*
 * if the client is sending us data, we are the consumer process
 * and need to get data out of the shared memory segment.
 */
		shmid = -1;
		if (cmd >= FLUSH) {
			ptr = sptr;
			for (i = 0; i < 4; i++) {
				ptr = strchr(ptr, '|') + 1;
				if (ptr == (char *)1) {
					ret = EINVMSG;
					goto err_jump;
				}
			}
			len = atoi(ptr);				/* this is the len of the data */
			if (len < 1 || len > INT_MAX) {
				ret = EINVMSG;
				goto err_jump;
			}
			if ((ptr = malloc((size_t)len)) == NULL) {		/* get space for it */
				ret = ENOALLOC;
				goto err_jump;
			}
			offs = 0;
/*
 * get the semaphore id, then the shared memory segment.  len tells us
 * how much data we need to get out of the shared memory.  since the
 * shared memory segment is 1K, loop around as we need to using the
 * semaphore as a gatekeeper.
 */
			semid = semget((key_t)pid, 0, 0666);
			shmid = shmget((key_t)pid, (size_t)shmsiz, 0666);
			if (dbgsw) {
				fprintf(stderr, "len = %d, semid = %d, shmid = %d\n", len, semid, shmid);
				fflush(stderr);
			}
			shptr = shmat(shmid, NULL, 0);
			while(len) {
				while (1) {
					sop.sem_num = 0;
					sop.sem_op = 0;
					sop.sem_flg = 0;
					if (semop(semid, &sop, 1) > -1)
						break;
					if (errno != EINTR) {
						fprintf(stderr, "pid %d error during semop - %d\n",
								getpid(), errno);
						exit(0);
					}
				}
				size = min(shmsiz, len);	
				memcpy(ptr+offs, shptr, size);		// copy out the data
				offs += size;						// increment the pointer
				len -= size;						// decrement the count
				sop.sem_num = 0;
				sop.sem_op = 2;
				sop.sem_flg = 0;
				semop(semid, &sop, 1);
			}
		}
/*
 * find the offset to where the dbfunc will need info...
 */
		i = sptr-msgbuf.txt;
		if (*(msgbuf.txt+i-1) != '|') {
			ret = ENOALLOC;
			goto err_jump;
		}
/*
 * call the appropriate database routine.
 * ret is the length of the return standard buffer (msg);
 * len gets the length of the shared memory portion of the return.
 * (if any)
 */
		ret = dbfunc[cmd](msgbuf.txt, i, &ptr);

		if (dbgsw) {
			fprintf(stderr, "dbfunc[%d] returns %d - ", cmd, ret);
			if (ret > 0)
				fwrite(msgbuf.txt, 1, ret, stderr);
			fprintf(stderr, "\n");
			fflush(stderr);
		}
err_jump:
		msgbuf.type = pid;
		if (ret < 0) {
			i = sprintf(msgbuf.txt, "%d|", ret);
			msgsnd(msgid, &msgbuf, i, 0);
			goto done;
		} else if (ret == 0) {
			sprintf(msgbuf.txt, "0|0|");
			msgsnd(msgid, &msgbuf, 5, 0);
			goto done;
		}
/*
 * len is the length of the data to return besides the 'standard
 * response'.  so what we need to do is transfer it to the conn
 * server via shared memory.  set up the semaphore, shared mem,
 * and make the semaphore available only to us so we can copy
 * data in first.
 */
		len = atoi(msgbuf.txt);
		if (len) {
			semid = semget((key_t)pid, 0, 0666);
			shmid = shmget((key_t)pid, (size_t)shmsiz, 0666);
			shptr = shmat(shmid, NULL, 0);
			sarg.val = 2;
			semctl(semid, 0, SETVAL, sarg);
		}
/*
 * send the message to serial_service that we have some
 * sort of return for them
 */
		msgsnd(msgid, &msgbuf, ret, 0);

/*
 * copy the data to the shared memory segment 1024 bytes at a
 * time.  remember the semaphore is available to us only on the
 * first pass.
 */
		offs = 0;
		while (len) {
			while (1) {
				sop.sem_num = 0;
				sop.sem_op = -1;
				sop.sem_flg = 0;
				if (semop(semid, &sop, 1) > -1)
					break;
				if (errno != EINTR) {
					fprintf(stderr, "pid %d error during semop - %d\n",
						getpid(), errno);
					exit(0);
				}
			}
			size = min(len, shmsiz);
			memcpy(shptr, ptr+offs, size);
			len -= size;
			offs += size;
			sarg.val = 0;
			semctl(semid, 0, SETVAL, sarg);
		}

done:
		if (shmid > -1) {
			shmdt(shptr);
			shmid = -1;
		}
		if (dbgsw) {
			fprintf(stderr, "ptr is returned 0x%p\n", ptr);
			fflush(stderr);
		}
		if (ptr)
			free(ptr);
	}
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
