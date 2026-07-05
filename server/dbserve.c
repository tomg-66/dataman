/* ***************************************************************
 *
 * PROCEDURE:	dbserv
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		Wed Feb 20 09:03:37 MST 2002
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 ************************************************************* */

/*
 * ok, so this is the real database server.  it is what performs
 * all of the functions to actually get and store data records.
 * all it does is read messages off of the queue, and dispatch
 * the proper thread.
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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include <signal.h>
#include <errno.h>
#include <string.h>

#include <pthread.h>

#include "msg.h"			/* need for SHMSIZ */
#include "errors.h"			/* dataman error numbers */
#include "misc.h"			/* this is for various things */

#define MAX_CONNS	256

extern void print_version(char *);
extern void err_sys(char *, char *);
extern int verify_pid(char *);

int shmsiz;
int dbgsw;

char *_progname;

/*
 * handle the signal that will turn on/off debugging during runtime
 */
void handle_usr1(int sig)
{
	FILE *tfile;

	if (sig != SIGUSR1)
		return;
	if (!dbgsw) {
		if ((tfile = freopen("/tmp/dbserv.log", "w+", stderr)) == NULL) {
			fprintf(stderr, "Can't (re)open /tmp/serial.log: ");
			perror("");
		} else
			dbgsw = 1;
	} else
		dbgsw = 0;
}

void term_handle(int sig)
{exit(0);}

void useage(char *name)
{
	fprintf(stderr, "useage: %s -dDsvh? [-m size] [-n count]\n"
					"    -d run in daemon mode\n"
					"    -D turn on debugging\n"
					"    -s this is run by the server\n"
					"    -v print version number and exit\n"
					"    -m size   where size is the size in kilobytes \n"
					"            of the shared memory segments\n"
					"    -n count  where count is the number of worker \n"
					"            threads to start\n"
					"    -h/? prints this useful message\n",
					name);
	exit(0);
}

int main(int argc, char *argv[])
{

	int i, j;				/* counters */
	int dmnsw;				/* daemon mode switch */
	int srvsw;				/* started by server... */
	int msgid;				/* message queue id */
	int threads;			/* number of worker threads to spin up */

	pid_t pid;

	MSG msgbuf;

	pthread_t thr;
	pthread_attr_t attr;

	struct sigaction act;

	extern void *dispatch(void *);

	_progname = strdup(argv[0]);
	dmnsw = 0;
	dbgsw = 0;
	srvsw = 0;
	shmsiz = 0;
	threads = 0;
	for (i = 1; i < argc; i++) {
		if (*argv[i] != '-')
			useage(argv[0]);
		for (j=1; j < strlen(argv[i]); j++) {
			switch(argv[i][j]) {
				case 'd':
					if (dmnsw || srvsw)
						useage(argv[0]);
					dmnsw++;
					break;
				case 'D':
					if (dbgsw)
						useage(argv[0]);
					dbgsw++;
					break;
				case 'm':
					if (shmsiz != 0 || ++i >= argc)
						useage(argv[0]);
					if ((shmsiz = atoi(argv[i])) == 0 || shmsiz > 4)
						useage(argv[0]);
					shmsiz *= SHMSIZ;
					j += strlen(argv[i]);
					break;
				case 'n':
					if (threads != 0 || ++i >= argc)
						useage(argv[0]);
					threads = atoi(argv[i]);
					if (threads == 0)
						useage(argv[0]);
					j = strlen(argv[i]) + 1;
					break;
				case 's':
					if (dmnsw || srvsw)
						useage(argv[0]);
					srvsw++;
					break;
				case 'v':
					print_version(argv[0]);
				default:
					fprintf(stderr, "%s: unknown switch '%c'\n",
							argv[0], argv[i][j]);
				case '?':
				case 'h':
					useage(argv[0]);
			}
		}
	}
	if (shmsiz && !srvsw) {
		fprintf(stderr, "\nThe '-m' switch should not be set manually\n\n");
		exit(0);
	}
	if (shmsiz == 0)
		shmsiz = SHMSIZ;
/*
 * attach to the message queue and clean out any messages
 * that might be there.  This means we need to be up and
 * running before the db connection server!
 */
	if ((msgid = msgget((key_t)MSGKEY, PERMS|IPC_CREAT)) < 0)
		err_sys("%s: Can't create message queue: ", argv[0]);

	while(msgrcv(msgid, &msgbuf, MAXSIZ, MSG_ANY, MSG_NOERROR|IPC_NOWAIT) > 0)
		;

/*
 * ok, run in daemon mode if necessary
 */
	if (dmnsw) {
		if ((pid = fork()) < 0)
			err_sys("%s: Can't fork daemon process: ", argv[0]);
		if (pid > 0)
			exit(0);
	}
/*
 * do this after we fork (if we're going to) so that the child (daemon)
 * can hold the lock over it's lifetime.
 */
	if (verify_pid(basename(argv[0])) < 0) {
		fprintf(stderr, "%s: process already running\n", argv[0]);
		exit(0);
	}
/*
 * now finish up becoming the daemon.
 */
	if (dmnsw) {
		fclose(stdin);
		if (freopen("/tmp/dbserv.log", "w+", stderr) == NULL)
			err_sys("%s: Can't open system log: ", argv[0]);

		fprintf(stdout, "%s: Running in daemon mode!\n", argv[0]);
		fclose(stdout);
	}

	if (srvsw && dbgsw) {
		if (freopen("/tmp/dbserv.log", "w+", stderr) == NULL)
			err_sys("%s: Can't open system log: ", argv[0]);
	}

	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = term_handle;
	if (sigaction(SIGINT, &act, NULL) < 0) {
		fprintf(stderr, "Can't install int handler\n");
		perror("");
		exit(0);
	}
	if (sigaction(SIGTERM, &act, NULL) < 0) {
		fprintf(stderr, "Can't install term handler\n");
		perror("");
		exit(0);
	}
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = handle_usr1;
	if (sigaction(SIGUSR1, &act, NULL) < 0) {
		fprintf(stderr, "Can't install USR1 handler\n");
		perror("");
		exit(0);
	}
#if !defined __gnu_linux__
	check_endian();					/* check endian_ness if needed */
#endif

/*
 * now spin up worker threads.  the default is 1 if not specified on the
 * command line.
 */
	if (threads == 0)
		threads = 1;
	for (i = 0; i < threads; i++) {
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
		if (pthread_create(&thr, &attr, dispatch, NULL) != 0) {
			fprintf(stderr, "%s: Can't create new dispatch thread %d: ", argv[0], i);
			perror("");
			exit(0);
		}
	}
	while(1) {
		pause();
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
