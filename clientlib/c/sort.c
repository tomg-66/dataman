/* ***************************************************************
 *
 * PROCEDURE:	sort.c
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
 * this is the sort routine,  it takes as its argument the key to be sorted
 * into the index created by mkidx,  it's only function is to insert a key
 * pointing to the current work file record into the current index.  as a rule
 * it should only be used when creating new indexes.  at the very first of
 * the process the root node is a leaf.
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

#include <string.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "globs.h"
#include "../../server/dbfunc.h"

extern INDEX cur_index;                         /* current working index */

extern int  _fileno;                           /* current file number */

extern char *db_send(char *, int, char *);
extern void db_err(int, char *, ...);

void sort(char *pkey)
{

	int i;

	char cmd[128];
	char *buff;

	sprintf(cmd, "%d|%d|%d|%"PRId64"|%s|", SORT, cur_index._idxno,
					_fileno, w_cur, pkey);
	buff = db_send(cmd, strlen(cmd), __FILE__);
	if (dbgsw) {
		fprintf(stderr, "sort returns %s\n", buff);
		fflush(stderr);
	}
	i = atoi(buff);
	if (i < 0)
		db_err(i, "%s: error during sort", _progname);
	free(buff);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
