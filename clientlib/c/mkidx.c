/* ***************************************************************
 *
 * PROCEDURE:	mkidx.c
 *
 * PROJECT:		dataman client side
 * 
 * DATE:		legacy, originally writtin in 1988
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 * 				March 2002
 *				Tom Green
 *				modified to use call interface to server
 ************************************************************* */
/*
 * this is the initialization for all sort procedures
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
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "index.h"              /* index description */
#include "globs.h"
#include "../../server/dbfunc.h"
#include "../../server/misc.h"
#include "../../config.h"

/* -------- these are globals declared here -------- */
short _maxfil;				/* number of files in index */

int _fileno;				/* the offset to the current file */
char **_fnames;				/* the names of the files in index */
char *_progname;

extern int is_sort;
extern int dbgsw;
extern char _root[];

extern void data_globs(void);
extern int in_rec(int, char *);
extern char *substr(char *,int,int);
extern char *db_send(char *, int, char *);
extern int db_connect(char *);
extern void db_discon(void);
extern void db_err(int, char *, ...);

#ifdef DWINDOW
extern void init_dwin(void);
#endif

static void useage(char *name)
{
    db_err(0,
        "Usage: %s [-len] [-D] [-h host] [-r rootdir] idx_name file1 "
		"[file2 file3 ...]\n\t-len where len is the max key length"
		"\n\t-D   turn on global debugging"
		"\n\t-h   host is the database server host"
		"\n\t-r   rootdir is the database root directory\n",name);
}

/*
 * make the index
 */
void mkidx(int argc, char *argv[])		/* the command line args from main */

{
    int i, j;					/* general usage */
	int h_len;
	int phpsw;					/* php switch */

    char string[256];			/* misc string */
	char host[32];				/* host to connect to */
    char *ptr;
	char *cptr;

	_progname = argv[0];		/* save the program name */
    if (argc < 3)               /* got to get at least the right # of args */
        useage(argv[0]);

    data_globs();               /* init global vars */

/*
 * save the index name, and the rest of the command line arguments.
 */
	*host = '\0';
	*_root = '\0';
	phpsw = 0;
	for (i = 1; i < argc; i++) {
		if (*argv[i] != '-')
			break;
		for(j = 1; j < strlen(argv[i]); j++) {
			switch(argv[i][j]) {
				case 'D':					/* turn on debugging */
					if (dbgsw)
						useage(argv[0]);
					dbgsw++;
					break;
				case 'h':					/* run in daemon mode */
					if (strlen(host))
						useage(argv[0]);
					strcpy(host, argv[++i]);
					j = strlen(argv[i]) + 1;
					break;
				case 'p':
					if (phpsw)
						useage(argv[0]);
					phpsw++;
					break;
				case 'r':
					if (strlen(_root))
						useage(argv[0]);
					strcpy(_root, argv[++i]);
					j = strlen(argv[i]) + 1;
					break;
				case '0': case '1': case '2': case '3': case '4':
				case '5': case '6': case '7': case '8': case '9':
					if (cur_index._keylen)
						useage(argv[0]);
					cur_index._keylen = atoi(argv[i]+j);
        			if (cur_index._keylen > MAX_KEY_SIZE||cur_index._keylen
									< MIN_KEY_SIZE)
						db_err(0, " %s: keysize %d invalid - min %d, "
										"max %d\n", argv[0], cur_index._keylen,
										MIN_KEY_SIZE, MAX_KEY_SIZE);
					j = strlen(argv[i]) + 1;
					break;
				default:
					db_err(0, "unknown switch %c\n", argv[i][j]);
					useage(argv[0]);
			}
		}
	}
	if (i == argc)
		useage(argv[0]);
	strcpy(cur_index._idxname, argv[i++]);
	_maxfil = argc - i;
	if ((_fnames = calloc(_maxfil, sizeof(char *))) == NULL) {
		fprintf(stderr, "Can't allocate new file name space");
		perror("");
		exit(0);
	}
	h_len = 0;
	for(j = 0; i < argc; i++,j++) {
		if (*argv[i] == '-')
			useage(argv[0]);
		if ((_fnames[j] = strdup(argv[i])) == NULL) {
			fprintf(stderr, "Can't allocate filename space for %s", argv[i]);
			perror("");
			exit(0);
		}
		h_len += strlen(argv[i])+1;
	}
/*
 * if root wasn't specified find it somewhere.
 */
	if (!strlen(_root)) {
		ptr = getenv("ROOT");				/* get root env var */
		if (ptr == NULL)					/* is it set? */
			db_err(0, "%s: no database ROOT set\n", argv[0]);

		strcpy(_root, ptr);
	}
/*
 * if host wasn't specified find it somewhere, or default to localhost
 */
	if (!strlen(host)) {
		ptr = getenv("DSERVHOST");
		if (ptr == NULL)
			strcpy(host, "localhost");
		else
			strcpy(host, ptr);
	}
/*
 * put together the command for the server.
 */
	j = sprintf(string, "%d|%d|%s|%s|%d|", MKIDX, cur_index._keylen,
					cur_index._idxname, _root, _maxfil);
	if ((cptr = malloc(j+1+h_len)) == NULL) {
		fprintf(stderr, "Can't allocate command space");
		perror("");
		exit(0);
	}
	strcpy(cptr, string);
	for (i = 0; i < _maxfil; i++) {
		j += sprintf(cptr+j, "%s|", _fnames[i]);
	}
/*
 * dm_sock is a global. connect up to the server, and arrange to
 * disconnect if something screws up.
 */
	if ((dm_sock = db_connect(host)) < 0)
		db_err(dm_sock, "%s: Can't connect to database server: ", argv[0]);
	atexit(db_discon);

/*
 * send the message and wait for response.  the returned string is
 * the response from the server
 */
	ptr = db_send(cptr, j, __FILE__);
	free(cptr);
/*
 * now parse the return for all of the pertinant information.
 */
	cptr = ptr;
	i = atoi(cptr);						/* length of record being retrieved */
/*
 * if the first field returned is negative, there was an error
 */
	if (i < 0)
		db_err(i, "%s: mkidx failed with", argv[0]);

	cptr = strchr(cptr, '|') + 1;
	cur_index._idxno = atoi(cptr);			/* idxno- offset in server */
	cptr = strchr(cptr, '|') + 1;
	cur_index._curnode = strtoll(cptr, NULL, 0);	/* current node */
	cptr = strchr(cptr, '|') + 1;
	w_chan = atoi(cptr);					/* work file offset in server */
	cptr = strchr(cptr, '|') + 1;
	cptr = strchr(cptr, '|') + 1;
	h_len = atoi(cptr);						/* length of work file header */
	cptr = strchr(cptr, '|') + 1;
	w_cur = strtoll(cptr, NULL, 0);			/* current record offset */
	cptr = strchr(cptr, '|') + 1;
	w_fmt = atoi(cptr);						/* format number of record */
	cptr = strchr(cptr, '|') + 1;
	w_next = strtoll(cptr, NULL, 0);		/* pointer to next rec in file */
	cptr = strchr(cptr, '|') + 1;

	if (dbgsw) {
		fprintf(stderr, "Parsed mkidx return, calling in_rec\n");
		fflush(stderr);
	}

	in_rec(WORK, cptr);
	_file = 1;
	free(ptr);
	is_sort = 1;

#ifdef DWINDOW
	if (!phpsw)
		init_dwin();
#endif

	return;
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker ft=c:
 */
