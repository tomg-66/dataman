/* ***************************************************************
 *
 * PROCEDURE:	mkidx.c
 *
 * PROJECT:		dataman client side c++ library routine
 * 
 * DATE:		Mon Jun 14 16:40:13 MDT 2004
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 * 				Thu Mar 21 15:49:21 MDT 2013
 * 				Tom Green
 * 				added name space
 *
 ************************************************************* */

/*
 * this routine is the header of any sort program.  It creates the index
 * file, writes the index header information to the file, creates the
 * first empty node, opens the first work file named, and reads in the
 * first record of the work file.  the routine 'sort' is used to place keys
 * into the index in a sort prog, the routine 'include' puts keys into
 * the index in file edit routines. 
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

#include <index.hh>
#include <datarecord.hh>
#include <db_comm.hh>

#include "../../server/dbfunc.h"
#include "../../config.h"

/* -------- these are globals declared here -------- */
short _maxfil;					/* number of files in index */

extern void data_globs(void);
extern void db_err(int, const char *, ...);

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

extern void dataman_disconnect(void);

namespace Dataman {

	Dataman::datarecord workfile(WORK);

	char _file;					// the when_file flag

	index *cur_index = NULL;

	bool dbgsw = false;			// debug switch
	bool is_sort = false;
	bool dataman_has_php = false;

	char *_progname = NULL;		// name of currently running program
	char *_root = NULL;			// pointer to ROOT dir

	char * index::_onames[6];	// this is static - should be inited to NULLs

    char **_fnames = NULL;		// the names of the files in index */
    char _fileno = 0;			// the offset to the current file */
};

using namespace Dataman;

void mkidx(int argc, char *argv[])		/* the command line args from main */

{

	_progname = argv[0];		/* save the program name */
    if (argc < 3)               /* got to get at least the right # of args */
        useage(argv[0]);

	cur_index = new class index;
	cur_index->my_mkidx(argc, argv);

}

void index::my_mkidx(int argc, char *argv[])
{
    int i, j;					/* general usage */
	int h_len;					/* header length */

    char string[256];			/* misc string */
	char host[32];				/* host to connect to */
    char *ptr;
	char *cptr;

	*host = '\0';
	for (i = 1; i < argc; i++) {
		if (*argv[i] != '-')
			break;
		for(j = 1; j < strlen(argv[i]); j++) {
			switch(argv[i][j]) {
				case 'D':					/* turn on debugging */
					if (dbgsw)
						useage(argv[0]);
					dbgsw = true;
					break;
				case 'h':					/* run in daemon mode */
					if (strlen(host))
						useage(argv[0]);
					strcpy(host, argv[++i]);
					j = strlen(argv[i]) + 1;
					break;
				case 'r':
					if (_root)
						useage(argv[0]);
					_root = strdup(argv[++i]);
					j = strlen(argv[i]) + 1;
					break;
				case '0': case '1': case '2': case '3': case '4':
				case '5': case '6': case '7': case '8': case '9':
					if (cur_index->_keylen)
						useage(argv[0]);
					cur_index->_keylen = atoi(argv[i]+j);
        			if (cur_index->_keylen > MAX_KEY_SIZE ||
								cur_index->_keylen < MIN_KEY_SIZE)
						db_err(0, " %s: keysize %d invalid - min %d, "
								"max %d\n", argv[0], cur_index->_keylen,
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
	cur_index->_onames[0] = strdup(argv[i++]);
	cur_index->_idxname = cur_index->_onames[0];
	_maxfil = argc - i;
	_fnames = new char *[_maxfil];
	h_len = 0;
	for(j = 0; i < argc; i++,j++) {
		if (*argv[i] == '-')
			useage(argv[0]);
		_fnames[j] = new char[strlen(argv[i])+1];
		strcpy(_fnames[j], argv[i]);
		h_len += strlen(argv[i]) + 1;
	}
/*
 * if root wasn't specified find it somewhere.
 */
	if (!_root) {
		ptr = getenv("ROOT");				/* get root env var */
		if (ptr == NULL)					/* is it set? */
			db_err(0, "%s: no database ROOT set\n", argv[0]);

		_root = strdup(ptr);
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
	j = sprintf(string, "%d|%d|%s|%s|%d|", MKIDX, cur_index->_keylen,
					cur_index->_idxname, _root, _maxfil);
	cptr = new char[j+h_len+1];
	strcpy(cptr, string);
	for (i = 0; i < _maxfil; i++) {
		j += sprintf(cptr+j, "%s|", _fnames[i]);
	}
/*
 * dm_sock is a global. connect up to the server, and arrange to
 * disconnect if something screws up.
 */
	try {
		db_comm comm(host);
	}
	catch (int i) {
		db_err(i, "%s: Can't connect to database server: ", _progname);
	}
	db_comm comm;
	atexit(dataman_disconnect);

/*
 * send the message and wait for response.  the returned string is
 * the response from the server
 */
	try {
		ptr = comm.db_send(cptr, j);
	}
	catch (int i) {
		comm.db_err(0, "%s: Can't read MKIDX response from socket", _progname);
	}
	delete[] cptr;
/*
 * now parse the return for all of the pertinant information.
 */
	cptr = ptr;
	workfile.len = atoi(cptr);		/* length of record being retrieved */
/*
 * if the first field returned is negative, there was an error
 */
	if (workfile.len < 0)
		comm.db_err(workfile.len, "%s: mkidx failed with", _progname);

	cptr = strchr(cptr, '|') + 1;
	cur_index->_idxno = atoi(cptr);			/* idxno- offset in server */
	cptr = strchr(cptr, '|') + 1;
	cur_index->_curnode = strtoll(cptr, NULL, 0);	/* current node */
	cptr = strchr(cptr, '|') + 1;
	workfile.chan = atoi(cptr);			/* work file offset in server */
	cptr = strchr(cptr, '|') + 1;
	cptr = strchr(cptr, '|') + 1;
	workfile.head = atoi(cptr);			/* length of work file header */
	cptr = strchr(cptr, '|') + 1;
	workfile.cur = strtoll(cptr, NULL, 0);	/* current record offset */
	cptr = strchr(cptr, '|') + 1;
	workfile.fmt = atoi(cptr);			/* format number of record */
	cptr = strchr(cptr, '|') + 1;
	workfile.next = strtoll(cptr, NULL, 0);/* pointer to next rec in file */
	cptr = strchr(cptr, '|') + 1;

	if (dbgsw) {
		fprintf(stderr, "Parsed mkidx return, calling in_rec\n");
		fflush(stderr);
	}

	workfile.in_rec(cptr);
	workfile._file = 1;
	delete[] ptr;
	is_sort = true;

#ifdef DWINDOW
	init_dwin();
#endif

	return;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
