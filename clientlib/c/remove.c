/* ***************************************************************
 *
 * PROCEDURE:	remove.c
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
 *				Tue Jul 18 15:57:30 MDT 2006
 *				added transaction processing switch, and new
 *				param at end of command for transactions.
 *				tomg
 ************************************************************* */

/*
 * this routine removes from the named index the named key
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
#include <malloc.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
//#include <unistd.h>

#include "index.h"
#include "globs.h"
#include "../../server/dbfunc.h"
#include "../../server/misc.h"
#include "proto.h"					/* this is where the typedef of key is */

#define TRUE	1
#define FALSE	0

extern INDEX *findex(char *);
extern char *db_send(char *, int, char *);
extern void db_err(int, char *, ...);

extern int in_xact;

int db_rm_key(key key_val, char *ixname)
{

	int i;
	int tmp;

    char cmd[128];			/* command */
	char *buff;

	INDEX *idx;

    idx = findex(ixname);
	if (!idx->_wrmode)
		db_err(0, "%s: in remove - index %s not opened for update\n",
						_progname, idx->_idxname);
/*
 * what happened to strnchr?  not int the lib any more?
 *
 * this makes it not my favorite way to do this!
 *
 *  if (strnchr(key,'*', idx->_keylen))
 *		db_err(0, "%s: can't use wildcard in remove\n", _progname);
 */
	memset(cmd, '\0', sizeof(cmd));
	memcpy(cmd, key_val, idx->_keylen);
	if (strchr(key_val, '*'))
		db_err(0, "%s: can't use wildcard in remove\n", _progname);

	if (*(key_val+idx->_keylen))
		tmp = idx->_keylen + KEY_HEADER_LENGTH;
	else
		tmp = strlen(key_val);

	i = sprintf(cmd, "%d|%d|%d|", REMOVE, idx->_idxno, in_xact);
	memcpy(cmd+i, key_val, tmp);
	i += tmp;

	buff = db_send(cmd, i, __FILE__);

	i = atoi(buff);
	free(buff);
	if (i < 0)
		db_err(i, "%s: remove error", _progname);
	else if (i == 0)
		return(FALSE);
	return(TRUE);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
