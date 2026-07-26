/* ***************************************************************
 *
 * PROCEDURE:	init_dataman.c
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
 *
 *				Wed Nov  2 12:22:39 MST 2005
 *				added dataman_has_php because with php
 *				we don't want to have a work file.  also
 *				will need to do some different initialization
 *				and shutdown things.
 *				tomg
 *
 *				Mon Jul 17 18:01:53 MDT 2006
 *				added a new global variable (in_xact) and init
 *				it for transaction processing.
 *				tomg
 ************************************************************* */

/*
 * this routine initializes (declares) the global dataman variables.
 * the calling sequence is:
 *      init_dataman(argc,argv);
 * where argc, and argv are the arguments to main()
 *
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

#include <string.h>     /* strcat function */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <ctype.h>

#include "globs.h"
#include "../../server/dbfunc.h"
#include "../../config.h"

INDEX _indices[6];
int dataman_has_php;

extern int in_xact;

extern int db_connect(char *);
extern char *db_send(char *, int, char *);
extern void db_err(int, char *, ...);
extern void in_rec(int, char *);
extern void db_discon(void);
extern void flush(void);

extern void data_globs(void);

#ifdef DWINDOW
extern void init_dwin(void);
#endif

void useage()
{
	db_err(0, "%s: useage: %s [-n] [-h host] [-r root] workfile\n"
					"\t-n non-traditional (don't use workfile)\n"
					"\t-h host is database server host to connect to\n"
					"\t-r root is database root directory on server\n",
					_progname, _progname);
}

void init_dataman(int argc, char *argv[])
{
	int i, j;			/* argument handling */

	char cmd[256];		/* command to send */
	char *host;			/* host to connect to */
	char *ptr;
	char *cptr;

	data_globs();
	is_sort = 0;
	dataman_has_php = 0;
	host = NULL;
	*_root = '\0';

	_progname = argv[0];
	for (i = 1; i < argc; i++) {
		if (*argv[i] != '-')
			break;
		for (j = 1; j < strlen(argv[i]); j++) {
			switch(argv[i][j]) {
				case 'D':
					if (dbgsw)
						useage();
					dbgsw++;
					break;
				case 'h':
					if (host)
						useage();
					host = strdup(argv[++i]);
					j = strlen(argv[i]) + 1;
					break;
				case 'n':
					if (!traditional)
						useage();
					traditional = 0;
					break;
				case 'p':
					if (dataman_has_php)
						useage();
					dataman_has_php = 1;
					break;
				case 'r':
					if (strlen(_root))
						useage();
					strcpy(_root, argv[++i]);
					j = strlen(argv[i]) + 1;
					break;
				default:
					db_err(0, "unknown switch '%c'\n", argv[i][j]);
					useage();
			}
		}
	}
/*
 * this is anachronistic.  still requireing the use of the 'work file'
 * for now for backwards compatibility.  in the future i might take
 * that out, or make it a switch
 */
	in_xact = 0;
	if (dataman_has_php) {
		if (i > argc)
			useage();
	} else {
		if (i >= argc)
			useage();
	}
/*
 * if we need to get the root of the database directory on the host
 */
	if (!strlen(_root)) {
		ptr = getenv("ROOT");
		if (ptr == NULL)
			db_err(0, "ROOT not defined\n");

		strcpy(_root, ptr);
	}
/*
 * and if we need to get the host to connect to, else default to
 * localhost
 */
	if (!host) {
		if ((ptr = getenv("DSERVHOST")) == NULL)
			host = strdup("localhost");
		else
			host = strdup(ptr);
	}

	if ((dm_sock = db_connect(host)) < 0)
		db_err(dm_sock, "%s: Can't connect to host", _progname);
/*
 * for php, we don't use a work file, so we skip all of that
 * part.  when we do non-traditional, that will be the case
 * as well.
 */
	if (dataman_has_php)
		goto php_done;
/*
 * ok, we're connected, initialize our connection on the server.
 */
	sprintf(cmd, "%d|%s/files/%s|", INIT_DAT, _root, argv[i]);
	if (dbgsw) {
		fprintf(stderr, "file to open is %s\n", cmd);
		fflush(stderr);
	}
	ptr = db_send(cmd, strlen(cmd), __FILE__);

	i = atoi(ptr);
	if (i < 0)
		db_err(i, "%s: Error during INIT_DATAMAN", _progname);

	cptr = strchr(ptr, '|') + 1;
	w_chan = atoi(cptr);
	cptr = strchr(cptr, '|') + 1;
	cptr = strchr(cptr, '|') + 1;
	w_fmt = atoi(cptr);
	cptr = strchr(cptr, '|') + 1;
	w_cur = strtoll(cptr, NULL, 0);
	cptr = strchr(cptr, '|') + 1;
	w_prev = strtoll(cptr, NULL, 0);
	cptr = strchr(cptr, '|') + 1;
	w_next = strtoll(cptr, NULL, 0);
	cptr = strchr(cptr, '|') + 1;

	in_rec(WORK, cptr);			/* read the work record */
	free(ptr);
php_done:
	atexit(db_discon);			/* clean up on exit */
	atexit(flush);				/* flush any modified record */

#ifdef DWINDOW
	if (!dataman_has_php)
		init_dwin();
#endif
}

void dataman_disconnect()
{
	if (dm_sock < 0)
		return
	db_discon();
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
