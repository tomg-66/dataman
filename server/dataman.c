/* ***************************************************************
 *
 * PROCEDURE:	dataman.c
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		Fri Jun 25 11:07:49 MDT 2004
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 ************************************************************* */

/*
 * this is the main executive procedure for the entire dataman system.
 * it checks to see if the connection server and database server are
 * running and starts them when necessary.  if either go down it
 * will exec a mail to the dataman user and restart the appropriate
 * subsystem.
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
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <libgen.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

extern void err_sys(char *, char *);
extern void print_version(char *);
extern int verify_pid(char *);

static pid_t con_pid,				/* connection manager pid*/
		srv_pid,					/* db server pid */
		my_pid;						/* dataman pid */
static int term_sw,					/* termination switch */
		dbgsw,						/* debugging switch */
		threads,					/* number of dbserve worker threads */
		memsize;					/* size of shared memory segment */

void reap_child(int sig)
{
	int status;
	int j;

	pid_t pid;
	pid_t newpid;

	FILE *fp;

	char *args[7];
	char nbuff[32];
	char mbuff[32];
	static int i;

	time_t now;

	if (sig != SIGCLD)
		return;

again:
	pid = waitpid(-1, &status, WNOHANG);
	if (pid != srv_pid && pid != con_pid)
		return;						/* what are we doing getting this anyway? */
	if (term_sw) {
		i++;
		if (i == 2) {
			fprintf(stderr, "Dataman exiting\n");
			exit(0);
		}
		goto again;
	}
	now = time(NULL);
/*
 * ok, notify the dataman user that we are restarting something
 */
	if ((fp = popen("mail -s \"dataman restarts\"  dataman", "w")) == NULL) {
		fprintf(stderr, "Can't open mail to dataman: ");
		perror("");
		exit(0);
	}
	fprintf(fp, "The dataman executive process caught the termination\n");
	fprintf(fp, "of the %s server process at %s\n",
				pid == srv_pid?"database":"connection",
				ctime(&now));
	if(WIFEXITED(status)) {
		fprintf(fp, "The termination status was a normal exit.\n");
		fprintf(fp, "The termination value was %d\n", WEXITSTATUS(status));
	}
	if (WIFSIGNALED(status))
		fprintf(fp, "The process received signal %d which was not caught.\n",
				WTERMSIG(status));
#ifdef WCOREDUMP
	if (WCOREDUMP(status))
		fprintf(fp, "A core file was left on the system.\n");
#endif
	pclose(fp);

	j = 2;
	memset(args, '\0', sizeof(args));
	if (pid == con_pid) {
		args[0] = "dataman_con";
	} else {
		args[0] = "dataman_srv";
		if (threads) {
			args[j++] = "-n";
			sprintf(nbuff, "%d", threads);
			args[j++] = nbuff;
		}
	}
	if (memsize) {
		args[j++] = "-m";
		sprintf(mbuff, "%d", memsize);
		args[j++] = mbuff;
	}
	args[1] = "-s";
	if (dbgsw)
		args[j] = "-D";
	if ((newpid = fork()) < 0) {
		fprintf(stderr, "Can't fork a new process for system restart: ");
		perror("");
		if (pid == con_pid)
			kill(srv_pid, SIGKILL);
		else
			kill(con_pid, SIGKILL);
		exit(0);
	} else if (newpid == 0) {
		if (execvp(args[0], args) < 0) {
			fprintf(stderr, "Can't exec process %s: ", args[0]);
			perror("");
			if (pid == con_pid)
				kill(srv_pid, SIGKILL);
			else
				kill(con_pid, SIGKILL);
			kill(getppid(), SIGKILL);
			exit(0);
		}
	}
	if (pid == con_pid)
		con_pid = newpid;
	else
		srv_pid = newpid;
}


void shutdown_handler(int sig)
{
	if (sig != SIGTERM && sig != SIGQUIT)
		return;					/* what in the heck was this? */

	term_sw = 1;
	if (sig == SIGTERM) {
		kill(con_pid, SIGTERM);		/* forcefully term connection server */
		kill(srv_pid, SIGTERM);		/* and the database server */
		exit(0);					/* and go away */
	}
/*
 * tell con server to stop accepting.  when the last connection goes away
 * it will shutdown the database server
 */
	kill(con_pid, SIGQUIT);
}

	
void useage(char *name)
{
	fprintf(stderr, "%s: useage: %s [-D|m size|n num|q|s|t]\n"
					"    -D  start with debugging turned on\n"
					"            if dataman is already running will toggle the\n"
					"            state of debugging\n"
					"    -m size where size is the number of Kilobytes to\n"
					"           use for shared memory segments (1,2,3...)\n"
					"    -n num where num is the numer of worker threads\n"
					"           in the database server\n"
					"    -q  query status of dataman\n"
					"    -s  controlled shutdown of dataman processes\n"
					"    -t  forced termination of dataman processes\n"
					"    -v  print version number and exit\n",
					name, name);
	exit(1);
}

int main(int argc, char *argv[])
{

	int i, j;						/* loop counters */
	int ssw, tsw;					/* control switches */
	int qsw;						/* query switch */
	int file_opened;				/* another switch */

	pid_t chk_this, chk_parent;

	char path[128];					/* path to temp file */
	char cmd[256];
	char pid[16];					/* pid read from file */
	char *vec[9];
	char *p;

	FILE *fp;						/* for popen */
	FILE *dp;

	struct sigaction act;			/* signal action stuff */

	my_pid = con_pid = srv_pid = 0;
	dbgsw = ssw = tsw = 0;
	qsw = 0;
	threads = 0;
	memsize = 0;
/*
 * let's process our arguments.
 */
	for (i = 1; i < argc; i++) {
		if (*argv[i] != '-')
			useage(argv[0]);
		for (j = 1; j < strlen(argv[i]); j++) {
			switch(argv[i][j]) {
				case 'D':
					if (dbgsw ||qsw || ssw || tsw)
						useage(argv[0]);
					dbgsw = 1;
					break;
				case 'm':
					if (memsize || qsw || ssw || tsw)
						useage(argv[0]);
					if (++i >= argc)
						useage(argv[0]);
					if ((memsize = atoi(argv[i])) <= 0 || memsize > 4) {
						fprintf(stderr, "shared memory size is currently capped at 4K\n"
										"(size may be 1, 2, 3, or 4)\n");
						useage(argv[0]);
					}
					j += strlen(argv[i]);
					break;

				case 'n':
					if (threads || qsw || ssw || tsw)
						useage(argv[0]);
					if (++i >= argc)
						useage(argv[0]);
					threads = atoi(argv[i]);
					if (threads == 0)
						useage(argv[0]);
					j += strlen(argv[i]);
					break;
				case 'q':
					if (threads || memsize || dbgsw ||qsw || ssw || tsw)
						useage(argv[0]);
					qsw = 1;
					break;
				case 's':
					if (threads || memsize || dbgsw ||qsw || ssw || tsw)
						useage(argv[0]);
					ssw = 1;
					break;
				case 't':
					if (threads || memsize || dbgsw ||qsw || ssw || tsw)
						useage(argv[0]);
					tsw = 1;
					break;
				case 'v':
					print_version(argv[0]);	/* this function calls exit */
				default:
					fprintf(stderr, "%s: unknown switch '%c'\n",
							argv[0], argv[i][j]);
				case '?':
				case 'h':
					useage(argv[0]);
			}
		}
	}
	if (i < argc)
		useage(argv[0]);
/*
 * find out what of the other routines are running
 */
	file_opened = 0;
	if ((fp = popen("ls /tmp/.dataman*.pid 2>/dev/null", "r")) == NULL) {
		fprintf(stderr, "Can't popen for current status: ");
		perror("");
		exit(0);
	}
/*
 * I'd like to rely upon /sbin/pidof being on the system, but that is
 * not always the case.  I would also like to rely on the existence of
 * the /var/run directory to put pid files in, but can't do that either
 * not every system has all linux programs and directory structure.
 */
	while(fgets(path, sizeof(path), fp) != NULL) {
		if ((p = strchr(path, '\n')) != NULL)
			*p = '\0';
		if ((dp = fopen(path, "r")) == NULL) {
			fprintf(stderr, "Can't open pid file %s\n", path);
			perror("");
			exit(0);
		}
		if (!fgets(pid, sizeof(pid), dp)) {
			fclose(dp);
			unlink(path);
			continue;
		}
		fclose(dp);
		if (!strcmp(path, "/tmp/.dataman_con.pid"))
			con_pid = atoi(pid);
		else if (!strcmp(path, "/tmp/.dataman_srv.pid"))
			srv_pid = atoi(pid);
		else
			my_pid = atoi(pid);
		file_opened = 1;
	}
	pclose(fp);
/*
 * this command should within a -very- small percentage give us a fairly
 * sure way to tell if it really is the dataman executive process running.
 * yes, it assumes ps, grep and awk are in the path. that is generally
 * a safe assumption.
 */
	if (my_pid != 0) {
		sprintf(cmd, "ps -ef|grep dataman|grep -v dataman_|grep %d|awk '{if($NR == 1) next; print $2 \" \" $3}'", my_pid);
		if ((fp = popen(cmd, "r")) == NULL) {
			fprintf(stderr, "ps command failed - are ps, grep, and awk in your path?\n");
			perror("Can't open ps command\n");
			exit(0);
		}
/*
 * re-use the path variable here, and i as a boolean switch;
 */
		i = 0;
		while(fgets(path, sizeof(path), fp) != NULL) {
			p = path;
			chk_this = atoi(p);
			if ((p = strchr(path, ' ')) == NULL)
				continue;
			chk_parent = atoi(p+1);
			if (chk_this == my_pid && chk_parent == 1) {
				i = 1;
				break;
			}
		}
		fclose(fp);
		if (!i)
			my_pid = 0;
	}
/*
 * ok, deal with the results of what we just did.
 */
	if (!my_pid) {
		if (qsw) {
			fprintf(stderr, "Dataman is -NOT- running\n");
			if (file_opened)
				fprintf(stderr, "But /tmp/.dataman*.pid files exist\n");
			exit(2);
		}
		if (tsw || ssw) {
			fprintf(stderr, "%s: can't perform operation, dataman not"
						    " running\n", argv[0]);
			if (file_opened)
				fprintf(stderr, "But /tmp/.dataman*.pid files exist\n");
			exit(0);
		}
	} else {
		if (dbgsw) {
			kill(con_pid, SIGUSR1);
			kill(srv_pid, SIGUSR1);
			exit(0);
		}
		if (qsw) {
			fprintf(stderr, "Dataman -IS- running\n");
			exit(1);
		}
		if (tsw)
			kill(my_pid, SIGTERM);
		else if (ssw)
			kill(my_pid, SIGQUIT);
		else
			fprintf(stderr, "%s: dataman running, what do you want me"
							" to do?\n",argv[0]);
		exit(0);
	}
/*
 * ok, main executive isn't running.  we need to make sure that neither
 * of the kids are running.  if they are, kill them, so we can be the
 * parent that controls them.
 */
	if (con_pid)
		kill(con_pid, SIGKILL);
	if (srv_pid)
		kill(srv_pid, SIGKILL);
/*
 * ok, now we fork ourselves, become the process leader, and start up
 * the kids.  we don't start them with the daemon switch, because we
 * want to be their parent.
 */
	if ((my_pid = fork()) < 0) {
		fprintf(stderr, "%s: Can't fork to become a daemon: ", argv[0]);
		perror("");
		exit(errno);
	} else if (my_pid > 0)
		exit(errno);

	setsid();				/* become the session leader */
	if (chdir("/tmp") < 0) {
		fprintf(stderr, "%s: can't change to /tmp: ", argv[0]);
		perror("");
		exit(errno);
	}
	umask(0);				/* don't want to modify open permissions */
/*
 * at this point verify_pid should always return true, because we have
 * already determined that we aren't running.
 */
	verify_pid(basename(argv[0]));
/*
 * ok, now we are a daemon, set up the signal catchers for the child
 * processes
 */
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = reap_child;
	if (sigaction(SIGCHLD, &act, NULL) < 0)
		err_sys("%s: Can't install reaper: ", argv[0]);
/*
 * now set up the handlers to do the system shutdowns
 */
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = shutdown_handler;
	if (sigaction(SIGTERM, &act, NULL) < 0)
		err_sys("%s: Can't install term handler: ", argv[0]);

	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = shutdown_handler;
	if (sigaction(SIGQUIT, &act, NULL) < 0)
		err_sys("%s: Can't install quit handler: ", argv[0]);
/*
 * ok, now we need to exec our kid processes
 */
	term_sw = 1;
	memset(vec, '\0', sizeof(vec));
	if ((srv_pid = fork()) < 0) {
		err_sys("%s: Can't fork database server: ", argv[0]);
	} else if (srv_pid == 0) {
		vec[0] = "dataman_srv";
		vec[1] = "-s";
		i = 2;
		if (dbgsw) {
			vec[i] = "-D";
			i++;
		}
		if (threads) {
			vec[i++] = "-n";
			sprintf(pid, "%d", threads);
			vec[i++] = pid;
		}
		if (memsize) {
			vec[i++] = "-m";
			sprintf(cmd, "%d", memsize);
			vec[i++] = cmd;
		}
		if (execvp(vec[0], vec) < 0) {
			fprintf(stderr, "%s: Could not exec dataman_srv: ", argv[0]);
			perror("");
			kill(getppid(), SIGKILL);
			exit(0);
		}
	}

	if ((con_pid = fork()) < 0) {
		i = errno;
		kill(srv_pid, SIGKILL);
		errno = i;
		err_sys("%s: Can't fork connection server: ", argv[0]);
	} else if (con_pid == 0) {
		vec[0] = "dataman_con";
		vec[1] = "-s";
		i = 2;
		if (dbgsw)
			vec[i++] = "-D";
		if (memsize) {
			vec[i++] = "-m";
			sprintf(cmd, "%d", memsize);
			vec[i++] = cmd;
		}
		if (execvp(vec[0], vec) < 0) {
			kill(srv_pid, SIGKILL);
			fprintf(stderr, "%s: Could not exec dataman_con: ", argv[0]);
			perror("");
			kill(getppid(), SIGKILL);
			exit(0);
		}
	}
/*
 * this sleep allows time for there to be a problem with the exec of
 * the children.  if something fails, then they will terminate and
 * the child reaper will get it and we will exit from there.
 */
	sleep(1);
/*
 * set up a logfile, close stdio channels and let the user know
 * that we are in good shape!
 */
	fclose(stdin);
	if ((fp = freopen("/tmp/dataman.log", "w+", stderr)) == NULL) {
		kill(con_pid, SIGKILL);
		kill(srv_pid, SIGKILL);
		err_sys("%s: Can't redirect stderr: ", argv[0]);
	}
	fprintf(stdout, "\n%s: running...\n", argv[0]);
	fclose(stdout);
/*
 * ok, now do nothing for ever
 */
	term_sw = 0;
	while(1) {
		pause();
	}
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker ft=c:
 */
