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

#include "globs.h"
#include "index.h"              /* index description */
#include "../../server/dbfunc.h"
#include "../../server/misc.h"
#include "../../server/errors.h"
#ifdef DATAMAN_CMAKE_BUILD
#include "config.h"
#else
#include "../../config.h"
#endif

/* -------- these are globals declared here -------- */
DATAMAN_HIDDEN short _maxfil;				/* number of files in index */

DATAMAN_HIDDEN int _fileno;				/* the offset to the current file */
DATAMAN_HIDDEN char **_fnames;				/* the names of the files in index */
DATAMAN_HIDDEN char *_progname;

DATAMAN_HIDDEN extern int is_sort;
DATAMAN_HIDDEN extern int dbgsw;
DATAMAN_HIDDEN extern char _root[];

DATAMAN_HIDDEN extern void data_globs(void);
DATAMAN_HIDDEN extern int in_rec(int, char *, size_t, INDEX *, int, int);
DATAMAN_HIDDEN extern char *db_send(char *, int, char *);
DATAMAN_HIDDEN extern char *db_send_len(char *, int, char *, size_t *);
DATAMAN_HIDDEN extern int db_connect(char *);
DATAMAN_HIDDEN extern void db_discon(void);
DATAMAN_HIDDEN extern void db_err(int, char *, ...);

DATAMAN_API extern char *substr(const char *,const int,const int);

#ifdef DWINDOW
extern void init_dwin(void);
#endif


static void useage(const char *name)
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
DATAMAN_API int mkidx(int argc, const char *argv[])		/* the command line args from main */
{
    int i, j;					/* general usage */
	int h_len;
	int phpsw;					/* php switch */

    char string[256];			/* misc string */
	char host[32];				/* host to connect to */
    char *ptr;
	char *cptr;
	size_t response_len;

	_progname = strdup(argv[0]);	/* save the program name */
    if (argc < 3) {					/* got to get at least the right # of args */
        useage(argv[0]);
		return FALSE;
	}

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
					if (dbgsw) {
						useage(argv[0]);
						return FALSE;
					}
					dbgsw++;
					break;
				case 'h':					/* run in daemon mode */
					if (strlen(host)) {
						useage(argv[0]);
						return FALSE;
					}
					strcpy(host, argv[++i]);
					j = strlen(argv[i]) + 1;
					break;
				case 'p':
					if (phpsw) {
						useage(argv[0]);
						return FALSE;
					}
					phpsw++;
					break;
				case 'r':
					if (strlen(_root)) {
						useage(argv[0]);
						return FALSE;
					}
					strcpy(_root, argv[++i]);
					j = strlen(argv[i]) + 1;
					break;
				case '0': case '1': case '2': case '3': case '4':
				case '5': case '6': case '7': case '8': case '9':
					if (cur_index._keylen) {
						useage(argv[0]);
						return FALSE;
					}
					cur_index._keylen = atoi(argv[i]+j);
        			if (cur_index._keylen > MAX_KEY_SIZE||cur_index._keylen
									< MIN_KEY_SIZE) {
						db_err(0, " %s: keysize %d invalid - min %d, "
										"max %d\n", argv[0], cur_index._keylen,
										MIN_KEY_SIZE, MAX_KEY_SIZE);
						return FALSE;
					}
					j = strlen(argv[i]) + 1;
					break;
				default:
					db_err(0, "unknown switch %c\n", argv[i][j]);
					useage(argv[0]);
					return FALSE;
			}
		}
	}
	if (i == argc) {
		useage(argv[0]);
		return FALSE;
	}
	strcpy(cur_index._idxname, argv[i++]);
	_maxfil = argc - i;
	if ((_fnames = calloc(_maxfil, sizeof(char *))) == NULL) {
		fprintf(stderr, "Can't allocate new file name space");
		perror("");
		exit(0);
	}
	h_len = 0;
	for(j = 0; i < argc; i++,j++) {
		if (*argv[i] == '-') {
			useage(argv[0]);
			return FALSE;
		}
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
		if (ptr == NULL) {					/* is it set? */
			db_err(0, "%s: no database ROOT set\n", argv[0]);
			return FALSE;
		}

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
	if ((dm_sock = db_connect(host)) < 0) {
		db_err(dm_sock, "%s: Can't connect to database server: ", argv[0]);
		return FALSE;
	}
	atexit(db_discon);

/*
 * send the message and wait for response.  the returned string is
 * the response from the server
 */
	ptr = db_send_len(cptr, j, __FILE__, &response_len);

	if (!ptr)
		return FALSE;

	free(cptr);
/*
 * now parse the return for all of the pertinant information.
 */
	cptr = ptr;
	i = atoi(cptr);						/* length of record being retrieved */
/*
 * if the first field returned is negative, there was an error
 */
	if (i < 0) {
		db_err(i, "%s: mkidx failed with", argv[0]);
		return FALSE;
	}
/*
 * we can just go straight to the globals here because if the in_rec fails
 * everything fails, and nothing will sort anyway
 */
	if (!dm_next_field(&cptr))
		goto invalid_response;
	cur_index._idxno = atoi(cptr);			/* idxno- offset in server */
	if (!dm_next_field(&cptr))
		goto invalid_response;
	cur_index._curnode = strtoull(cptr, NULL, 0);	/* current node */
	if (!dm_next_field(&cptr))
		goto invalid_response;
	w_chan = atoi(cptr);					/* work file offset in server */
	if (!dm_next_field(&cptr))
		goto invalid_response;
	if (!dm_next_field(&cptr))
		goto invalid_response;
	h_len = atoi(cptr);						/* length of work file header */
	if (!dm_next_field(&cptr))
		goto invalid_response;
	w_cur = strtoll(cptr, NULL, 0);			/* current record offset */
	if (!dm_next_field(&cptr))
		goto invalid_response;
	w_fmt = atoi(cptr);						/* format number of record */
	if (!dm_next_field(&cptr))
		goto invalid_response;
	w_next = strtoll(cptr, NULL, 0);		/* pointer to next rec in file */
	if (!dm_next_field(&cptr))
		goto invalid_response;

	if (dbgsw) {
		fprintf(stderr, "Parsed mkidx return, calling in_rec\n");
		fflush(stderr);
	}

	if ((size_t)(cptr-ptr) > response_len ||
			!in_rec(WORK, cptr, response_len-(size_t)(cptr-ptr),
				NULL, w_fmt, w_chan)) {
		db_err(EINREC, "%s: MKIDX", _progname);
		free(ptr);
		return FALSE;
	}

	_file = 1;
	free(ptr);
	is_sort = 1;

#ifdef DWINDOW
	if (!phpsw)
		init_dwin();
#endif

	return TRUE;

invalid_response:
	db_err(EINVMSG, "%s: invalid MKIDX response", _progname);
	free(ptr);
	return FALSE;
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
