/* ***************************************************************
 *
 * PROCEDURE:	get.c
 *
 * PROJECT:		dataman client side
 * 
 * DATE:		Wed Jul  5 21:24:04 MDT 2006
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 *
 ************************************************************* */
/*
 * this routine starts a transaction.  it should be bounded
 * by either a rollback or commit call.
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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <malloc.h>

#include "../../server/dbfunc.h"
#include "../../server/misc.h"

#define TRUE    1
#define FALSE   0

extern char *db_send(char *, int, char *);
extern void db_err(int, char *, ...);

extern char *_progname;

int in_xact;

int start_transaction(void)
{
    int i;								/* temporary */

	char cmd[128];
	char *buff;
	char *cptr;


	i = sprintf(cmd, "%d|", START_XACT);
/*
 * send the command and deal with the return.
 */
	buff = db_send(cmd, i, __FILE__);

	if (!buff)
		return FALSE;
/*
 * the first field of the return is an error code if necessary,
 * otherwise the command worked.
 */
	i = atoi(buff);
	if (i < 1) {
		if (i < 0)
			db_err(i, "%s: Error during START_XACT", _progname);
		free(buff);
		return FALSE;
	}

	in_xact = TRUE;
	free(buff);
	return i;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
