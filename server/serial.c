/* ***************************************************************
 *
 * PROCEDURE:	serial.c
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		Tue Feb 19 15:23:18 MST 2002
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 *
 * 				Sun Jun 27 21:52:35 MDT 2004
 *				added a call to verify_pid() so that we can
 *				make sure there is only one of these running.
 *				also, we want to do an intelligent shutdown of
 *				the system.  don't accept new connections after
 *				we get a SIGQUIT, and when there are no more
 *				kids, shutdown the database server.
 *				tomg
 ************************************************************* */

/*
 * this program is the one that receives attachments from the
 * remote programs and services/serializes their requests
 * through the database message queue.
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
#include <string.h>
#include <unistd.h>
#include <libgen.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <netinet/in.h>

#include <signal.h>
#include <errno.h>

#include "errors.h"
#include "msg.h"			/* need for SHMSIZ */

#define PORTNO		8758
#define MAX_CONNS	256

int dbgsw;					/* debugging? */
int shmsiz;					/* size of shared mem seg */

static int accepting;		/* are we currently accepting new connections? */
static int con_count;		/* how many connections are curently active? */

extern void term_dbserve(void);
extern void print_version(char *name);
extern void err_sys(char *, char *);
extern int verify_pid(char *);
extern void serial_service(int);


/*
 * kill -all- child pids because this server is hosed
 * ignore all of the kids that die on us as we kill them....
 */
static pid_t pid_tab[MAX_CONNS];
void serial_stop(void)
{
	int i;
	struct sigaction act;

	sigemptyset(&act.sa_mask);
	act.sa_handler = SIG_IGN;
	act.sa_flags = SA_NOCLDSTOP;
	sigaction(SIGCHLD, &act, NULL);

	for (i = 0; i < MAX_CONNS; i++) {
		if (pid_tab[i] != 0)
			kill(pid_tab[i], SIGKILL);
	}
}

void useage(char *name)
{
	fprintf(stderr, "usage: %s -Ddsvh? [-m size]\n"
			"    -d    run in daemon mode\n"
			"    -D    start with Debugging turned on\n"
			"    -s    this is being run by the server\n"
			"    -v    print version information and quit\n"
			"    -m size  where size is the size (in kilobyets)\n"
			"             of the shared memory segments\n"
			"    -h/v  print this helpful message\n",
			name);
	exit(0);
}

/*
 * handle the signal that will turn on/off debugging during runtime
 */
void handle_usr1(int sig)
{
	FILE *tfile;

	if (sig != SIGUSR1)
		return;
	if (!dbgsw) {
		if ((tfile = freopen("/tmp/serial.log", "w+", stderr)) == NULL) {
			fprintf(stderr, "Can't (re)open /tmp/serial.log: \n");
			perror("");
		} else
			dbgsw = 1;
	} else
		dbgsw = 0;
}

/*
 * handle getting a terminate signal.  struct sigaction defines that the
 * handler function must take an int argument (not sig_t).
 */
void handle_term(int sig)
{
	if (sig != SIGTERM)
		return;
	serial_stop();
	exit(0);
}

/*
 * with SIGQUIT if there are are no active connections
 * terminate immediately.  if so, set a flag to not
 * allow any new connections.
 */
void handle_quit(int sig)
{
	if (sig != SIGQUIT)
		return;
	if (!con_count) {
		term_dbserve();
		exit(0);
	}
	accepting = 0;			/* not accepting new connections */
}


int main(int argc, char *argv[])
{

	int cnt;				/* counter */
	int i;
	int dmn;				/* do we daemonize? */
	int srv;				/* were we started from the server? */

	int msock;				/* master open socket */
	int nsock;				/* new socket conn */

	char resp[5];			/* err response to conn */

	void reap_child(int);
	int addpid(pid_t);

	pid_t pid;				/* pid of program */

//	sigset_t set;			/* signal set */

	struct sigaction act;	/* signal action stuff */

	struct sockaddr_in addr;	/* socket 'address' */

	FILE *tfile;			/* temp file for freopen */

/*
 * do any handling of command line arguments here
 */
	dbgsw = 0;
	dmn = 0;
	srv = 0;
	for (cnt = 1; cnt < argc; cnt++) {
		if (*argv[cnt] != '-')
			useage(argv[0]);
		for(i = 1; i < strlen(argv[cnt]); i++) {
			switch(argv[cnt][i]) {
				case 'D':					/* turn on debugging */
					if (dbgsw)
						useage(argv[0]);
					dbgsw++;
					break;
				case 'd':					/* run in daemon mode */
					if (dmn || srv)
						useage(argv[0]);
					dmn++;
					break;
				case 'm':
					if (shmsiz)
						useage(argv[0]);
					if ((shmsiz = atoi(argv[++cnt])) <= 0 || shmsiz > 4)
						useage(argv[0]);
					shmsiz *= SHMSIZ;
					i += strlen(argv[cnt]);
					break;
				case 's':					/* started from server */
					if (dmn || srv)
						useage(argv[0]);
					srv++;
					break;
				case 'v':
					print_version(argv[0]);
				default:
					fprintf(stderr, "unknown switch %c\n", argv[cnt][i]);
				case '?':
				case 'h':
					useage(argv[0]);
			}
		}
	}
	if (shmsiz && !srv) {
		fprintf(stderr, "\nThe '-m' switch should not be set manually\n\n");
		exit(0);
	}
	if (shmsiz == 0)
		shmsiz = SHMSIZ;
/*
 * are we running as a daemon?
 * if so, fork and disassociate with controlling terminal.
 */
	if (dmn) {
		if ((pid = fork()) < 0)
			err_sys("%s: Can't fork daemon process: ", argv[0]);
		else if (pid > 0)
			exit(0);
		fclose(stdin);
		if ((tfile = freopen("/tmp/serial.log", "w+", stderr)) == NULL)
			err_sys("%s: Can't redirect stderr: ", argv[0]);
		fprintf(stdout, "\n%s: running in daemon mode!\n", argv[0]);
		fclose(stdout);
	}

	if (srv && dbgsw) {
		if ((tfile = freopen("/tmp/serial.log", "w+", stderr)) == NULL)
			err_sys("%s: Can't redirect stderr: ", argv[0]);
	}

	if (verify_pid(basename(argv[0])) < 0) {
		fprintf(stderr, "%s: process already running!\n", basename(argv[0]));
		exit(0);
	}
	accepting = 1;				/* say that we are accepting new connections */

/*
 * ok now, we are ready to start accepting connections from
 * client processes
 */
	if ((msock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		err_sys("%s: Can't initialize socket: ", argv[0]);
/*
 * set up the socket so we can just re-start the program
 */
	i = 1;
	if (setsockopt(msock,SOL_SOCKET,SO_REUSEADDR, (char *)&i, sizeof(int)) < 0)
		err_sys("%s: Can't set socket option: ", argv[0]);
/*
 * set up our port
 */
	memset((char *)&addr, '\0', sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(PORTNO);
/*
 * and bind it, then listen
 */
	if (bind(msock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
		err_sys("%s: Can't bind local socket: ", argv[0]);

	if (listen(msock, 5) < 0)
		err_sys("%s: Can't listen for new connections: ", argv[0]);

/*
 * set up a signal handler to reap any children that terminate.
 */
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = reap_child;
	if (sigaction(SIGCHLD, &act, NULL) < 0)
		err_sys("%s: Can't install reaper: ", argv[0]);

	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = handle_term;
	if (sigaction(SIGTERM, &act, NULL) < 0)
		err_sys("%s: Can't install term handler: ", argv[0]);

	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = handle_quit;
	if (sigaction(SIGQUIT, &act, NULL) < 0)
		err_sys("%s: Can't install quit handler: ", argv[0]);
/*
 * signal handler to toggle debugging
 */
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = handle_usr1;
	if (sigaction(SIGUSR1, &act, NULL) < 0)
		err_sys("%s: Can't install USR1 handler: ", argv[0]);

/*
 * now go for ever accepting new connections
 */
	while (1) {
		memset((char *)&addr, '\0', sizeof(addr));
		i = 0;
		if ((nsock = accept(msock, (struct sockaddr *)&addr, &i)) < 0) {
			switch(errno) {
				case ENETDOWN:			/* according to the linux documentation */
				case EPROTO:			/* accept can return already pending */
				case ENOPROTOOPT:		/* errors on an accept call, and should */
				case EHOSTDOWN:			/* treat these particular errors as */
				case ENONET:			/* EAGAIN.  (man accept) */
				case EHOSTUNREACH:
				case EOPNOTSUPP:
				case ENETUNREACH:
					continue;
				case EINTR:
					if (dbgsw) {
						fprintf(stderr, "after accept, interrupted\n");
						fprintf(stderr, "accepting = %d, con_count = %d\n",
										accepting, con_count);
						fflush(stderr);
					}
					continue;			/* interrupted - reaped child? */
				default:
					fprintf(stderr, "%s, Can't accept new connection: ", argv[0]);
					perror("");
					serial_stop();
					exit(0);
			}
		}
		if (dbgsw) {
			fprintf(stderr, "received new connection\n");
			fflush(stderr);
		}
		if (!accepting) {
			if (dbgsw) {
				fprintf(stderr, "but we are not accepting new connections\n");
				fflush(stderr);
			}
			sprintf(resp, "%d", ESHUT);
			i = write(nsock, resp, strlen(resp));  // don't care if we can't write back to the socket, we're shutting it down anyway.
			shutdown(nsock, SHUT_RDWR);
			continue;
		}
		if ((pid = fork()) < 0) {
			fprintf(stderr, "%s: Can't fork child server: ", argv[0]);
			perror("");
			serial_stop();
			exit(0);
		} else if (pid == 0) {
			close(msock);
			serial_service(nsock);
			exit(0);
		}
		if (addpid(pid) < 0) {
			sprintf(resp, "%d", -1);
			i = write(nsock, resp, strlen(resp));	// don't care if this fails, killing the kid anyway.
			kill(pid, SIGKILL);	/* kill the child */
		}
		con_count++;			/* increment the connection count */
		close(nsock);			/* close the new, child socket */
	}
	return(0);
}

/*
 * add a child pid to a table so that when we decide to die we can
 * kill all of our children that are left.
 */
int addpid(pid_t pid)
{
	int offs;
	int i;

	offs = pid % MAX_CONNS;
	if (dbgsw) {
		fprintf(stderr, "in addpid, offs = %d\n", offs);
		fflush(stderr);
	}
	if (pid_tab[offs] == 0) {
		pid_tab[offs] = pid;
		return(0);
	}
	for(i = offs+1; i < MAX_CONNS; i++) {
		if (pid_tab[i] == 0) {
			pid_tab[i] = pid;
			return(0);
		}
	}
	for(i = 0; i < offs; i++) {
		if (pid_tab[i] == 0) {
			pid_tab[i] = pid;
			return(0);
		}
	}
	return(-1);
}

/*
 * catch all of the kids that terminate.  could have
 * another terminate while we are reaping an earlier
 * one so loop on the waitpid untill it returns nothing
 */
void reap_child(int sig)
{
	int i;
	int offs;
	int status;

	pid_t pid;

	if (dbgsw)
		fprintf(stderr, "entering reaper!\n");
	while ((pid = waitpid(-1, &status, WNOHANG)) != 0) {
		if (pid == -1) {
			if (dbgsw){
				fprintf(stderr, "error in reap_child - %d\n", errno);
			}
			return;
		}
		if (dbgsw) {
			fprintf(stderr, "reaping child pid %d\n", pid);
			fflush(stderr);
		}
		con_count--;						/* hosting one less connection */
/*
 * if we aren't accepting new connections, that means we've
 * received a SIGQUIT which indicates we are shutting down
 * when all connections are done.
 */
		if (!accepting && !con_count) {
			term_dbserve();
			exit(0);
		}

    	offs = pid % MAX_CONNS;
		if (pid_tab[offs] == pid) {
			pid_tab[offs] = 0;
			return;
		}
		for(i = offs+1; i <MAX_CONNS; i++) {
			if (pid_tab[i] == pid) {
				pid_tab[i] = 0;
				return;
			}
		}
		for(i = 0; i < offs; i++) {
			if (pid_tab[i] == pid) {
				pid_tab[i] = 0;
				return;
			}
		}
	}
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker ft=c:
 */
