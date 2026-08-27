/* ***************************************************************
 *
 * PROCEDURE:	init_dataman.cc
 *
 * PROJECT:		dataman client side, c++ client library
 * 
 * DATE:		Tue May 27 21:14:05 MDT 2003
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 *				Mon Mar  9 14:39:06 MDT 2009
 *				added dataman_has_php to match with the
 *				C version when don't want to have a work file.
 *				also will need to do some different initialization
 *				and shutdown things.
 *				tomg
 *
 * 				Thu Mar 21 15:49:21 MDT 2013
 * 				Tom Green
 * 				added name space
 *
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

#include "endSort.hpp"
#include "db_comm.hpp"
#include "datamanError.hpp"

#include "../../server/dbfunc.h"
#ifdef DATAMAN_CMAKE_BUILD
#include "config.h"
#else
#include "../../config.h"
#endif

#include <memory>

#define FALSE 0
#define TRUE  1

namespace Dataman {
	Dataman::datarecord masterRecord(MASTER);
	bool in_xact;
};

using namespace Dataman;

extern void flush(void);

#ifdef DWINDOW
extern void init_dwin(void);
#endif

static void useage()
{
	db_err(0, "%s: useage: %s [-n] [-h host] [-r root] workfile\n"
					"\t-n non-traditional (don't use workfile)\n"
					"\t-h host is database server host to connect to\n"
					"\t-r root is database root directory on server\n",
					_progname, _progname);
}

void dataman_disconnect(void)
{
	if (db_comm::get_sock() < 0)
		return;
	db_comm comm;
	comm.db_discon();
}


int init_dataman(int argc, char **argv)

{
	int i, j;			/* argument handling */
	int traditional = 1; /* default to using traditional */

	char cmd[256];		/* command to send */
	char *host;			/* host to connect to */
	char *cptr;

	is_sort = false;
	in_xact = false;
	dataman_has_php = false;
	workRecord.init();
	host = NULL;

	_progname = argv[0];
	for (i = 1; i < argc; i++) {
		if (*argv[i] != '-')
			break;
		for (j = 1; j < strlen(argv[i]); j++) {
			switch(argv[i][j]) {
				case 'D':
					if (dbgsw) {
						useage();
						return FALSE;
					}
					dbgsw = true;
					break;
				case 'h':
					if (host) {
						useage();
						return FALSE;
					}
					host = strdup(argv[++i]);
					j = strlen(argv[i]) + 1;
					break;
				case 'n':
					if (!traditional) {
						useage();
						return FALSE;
					}
					traditional = 0;
					break;
				case 'p':
					if (dataman_has_php)
						useage();
					dataman_has_php = true;
					break;
				case 'r':
					if (_root) {
						useage();
						return FALSE;
					}
					_root = strdup(argv[++i]);
					j = strlen(argv[i]) + 1;
					break;
				default:
					db_err(0, "unknown switch '%c'\n", argv[i][j]);
					useage();
					return FALSE;
			}
		}
	}
/*
 * this is anachronistic.  still requireing the use of the 'work file'
 * for now for backwards compatibility.  in the future i might take
 * that out, or make it a switch
 */
	if (dataman_has_php) {
		if (i > argc) {
			useage();
			return FALSE;
		}
	} else {
		if (i >= argc) {
			useage();
			return FALSE;
		}
	}
/*
 * if we need to get the root of the database directory on the host
 */
	if (!_root) {
		cptr = getenv("ROOT");
		if (cptr == NULL) {
			db_err(0, "ROOT not defined\n");
			return FALSE;
		}

		_root = strdup(cptr);
	}
/*
 * and if we need to get the host to connect to, else default to
 * localhost
 */
	if (!host) {
		if ((cptr = getenv("DSERVHOST")) == NULL)
			host = strdup("localhost");
		else
			host = strdup(cptr);
	}
//
// set up the communications.  this is 'static' so just the declaration is ok
//
	try {
		db_comm comm(host);
	}
	catch(const datamanError &) {
		return FALSE;
	}
	db_comm comm;
	if (!traditional)
		goto done;
	if (dataman_has_php)
		goto done;
/*
 * ok, we're connected, initialize our connection on the server.
 */
	sprintf(cmd, "%d|%s/files/%s|", INIT_DAT, _root, argv[i]);
	if (dbgsw) {
		fprintf(stderr, "file to open is %s\n", cmd);
		fflush(stderr);
	}

// send the message. db_send allocates space for the answer or it
// will throw an error.  let the user deal with catching it.
	std::unique_ptr<char[]> response(comm.db_send(cmd, strlen(cmd)));

	cptr = response.get();
	i = atoi(cptr);
	if (i < 0)
		throw makeError(i, "%s: Error during INIT_DATAMAN", _progname);

	workRecord.len = i;
	cptr = strchr(cptr, '|') + 1;
	workRecord.chan = atoi(cptr);
	cptr = strchr(cptr, '|') + 1;
	workRecord.longest = atoi(cptr);
	cptr = strchr(cptr, '|') + 1;
	workRecord.fmt = atoi(cptr);
	cptr = strchr(cptr, '|') + 1;
	workRecord.cur = strtoll(cptr, NULL, 0);
	cptr = strchr(cptr, '|') + 1;
	workRecord.prev = strtoll(cptr, NULL, 0);
	cptr = strchr(cptr, '|') + 1;
	workRecord.next = strtoll(cptr, NULL, 0);
	cptr = strchr(cptr, '|') + 1;

	if (!workRecord.in_rec(cptr)) {			/* read the work record */
		return FALSE;
	}

done:
	masterRecord.init();
	atexit(dataman_disconnect);			/* clean up on exit */
	atexit(flush);				/* flush any modified recs */

#ifdef DWINDOW
	if (!dataman_has_php)
		init_dwin();
#endif
	return TRUE;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
