/* ***************************************************************
 *
 * PROCEDURE:	serial_setup
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		Fri Jun 16 19:42:27 MDT 2006
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MOMDIFICATION HISTORY:
 *
 *			Sun Jul 19 10:23:58 PM MDT 2026
 *			tomg
 *			do a little better error checking and clean
 *			up the code some
 *
 ************************************************************* */
/*
 * take some of the basic setup stuff out of serial_service so
 * that function is a little cleaner.  set the socket option,
 * set up the connection to the message queue, semaphore, and
 * shared memory segment.
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
#include <stdio.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>

#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>			/* for def of ntohl */

#include <errno.h>

#include "msg.h"
#include "errors.h"

extern int shmsiz;

/*
 * get the msgid that will be necessary for communication with the  server
 */
int msg_setup(int sock, pid_t mypid)
{
	int retval;
	int i;
	char buff[1024];

	if ((retval = msgget((key_t)MSGKEY, PERMS|IPC_CREAT)) < 0) {
		i = sprintf(buff, "%d", ENOMSGQ);
		if (write(sock, buff, i) != i) {
			fprintf(stderr, "pid %d: error writing ENOSMGQ to socket: ", mypid);
			perror("");
		}
		fprintf(stderr, "pid %d: error getting message queue", mypid);
		perror("");
		return(-1);
	}
/*
 * first thing we need to do, is to make sure there are no messages
 * sitting in the queue from an old instance of us, that might be
 * destined for this pid!
 */
	while (msgrcv(retval, &buff, 1024, (long)mypid, MSG_NOERROR|IPC_NOWAIT) > 0)
		;
	return(retval);
}

/*
 * we now need to get the semid for the synchronization with the shared
 * memory operations
 */
int sem_setup(int sock, pid_t mypid)
{
	int retval;
	int i;
	char buff[32];

	int perms;

	perms = IPC_CREAT|IPC_EXCL|PERMS;
	if ((retval = semget((key_t)mypid, 1, perms)) < 0) {
		if (errno != EEXIST)
			goto err;
		retval = semget((key_t)mypid, 1, 0);		/* it was already there */
		if (retval < 0 || semctl(retval, 0, IPC_RMID, 0) < 0) {
			i = sprintf(buff, "%d", ENOSEM);
			if (write(sock, buff, i) != i) {
				fprintf(stderr, "pid %d: error writing ENOSEM to socket: ", mypid);
				perror("");
			}
			fprintf(stderr, "pid %d: error removing stale semaphore: ", mypid);
			perror("");
			return(-1);
		}
		if ((retval = semget((key_t)mypid, 1, perms)) < 0) {
			goto err;
		}
	}
	return(retval);

err:
	i = sprintf(buff, "%d", ENOSEM);
	if (write(sock, buff, i) != i) {
		fprintf(stderr, "pid %d: error writing ENOSEM to socket: ", mypid);
		perror("");
	}
	fprintf(stderr, "pid %d: error creating semaphore: ", mypid);
	perror("");
	return(-1);
}

/*
 * setup the shared memory segment, then attach to it
 */
int shm_setup(int sock, pid_t mypid, void**shptr)
{
	int retval;
	int i;
	char buff[32];

	int perms;

	void *ptr;

	perms = IPC_CREAT|IPC_EXCL|PERMS;
	if ((retval = shmget((key_t)mypid, (size_t)shmsiz, perms)) < 0) {
		if (errno != EEXIST || (retval = shmget((key_t)mypid, 1, 0)) < 0 ||
				shmctl(retval, IPC_RMID, NULL) < 0 ||
				(retval = shmget((key_t)mypid, (size_t)shmsiz, perms)) < 0) {
			fprintf(stderr, "error creating shared memory: ");
			goto err;
		}
	}
	if ((ptr = shmat(retval, NULL, 0)) == (void *)-1) {
		fprintf(stderr,"error attaching shared memory: ");
		shmctl(retval, IPC_RMID, NULL);
		goto err;
	}
	*shptr = ptr;
	return(retval);

err:
	i = sprintf(buff, "%d", ENOSHM);
	if (write(sock, buff, i) != i) {
		fprintf(stderr, "pid %d: error writing ENOSHM to socket: ", mypid);
		perror("");
	}
	fprintf(stderr, "pid %d: error creating/attaching shared memory: ", mypid);
	perror("");
	return(-1);
}

/*
 * set the socket sommunication option to turn off Nagle's algorythm.
 */
int soc_setup(int sock, pid_t mypid)
{
	int i;
	char errbuf[32];

	i = 1;
	if (setsockopt(sock, SOL_TCP, TCP_NODELAY, (void *)&i, sizeof(i)) < 0) {
		i = sprintf(errbuf, "%d", ESOCKOPT);
		if (write(sock, errbuf, i) != i) {
			fprintf(stderr, "pid %d: error writing ESOCKOPT to socket: ", mypid);
			perror("");
		}
		fprintf(stderr, "pid %d: error setting socket option", mypid);
		perror("");
		return(0);
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
