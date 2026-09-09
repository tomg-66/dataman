/* ***************************************************************
 *
 * PROCEDURE:	commit.c
 *
 * PROJECT:		dataman client side
 * 
 * DATE:		Wed Jul  5 21:46:27 MDT 2006
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 *
 ************************************************************* */
/*
 * this routine commits a transaction.  it has to end a block
 * that began with a start_transaction.
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

#include "index.h"
#include "globs.h"
#include "m_params.h"

#include "../../server/dbfunc.h"
#include "../../server/errors.h"
#include "../../server/misc.h"

#define TRUE    1
#define FALSE   0

DATAMAN_HIDDEN extern char *db_send(char *, int, char *);
DATAMAN_HIDDEN extern void db_err(int, char *, ...);
DATAMAN_HIDDEN extern int out_rec(int);

DATAMAN_HIDDEN extern int in_xact;

DATAMAN_HIDDEN extern char *_progname;

DATAMAN_API int db_commit(void)
{
    int i;								/* temporary */

	char cmd[128];
	char *buff;
	char *cptr;

/*
 * if need be, flush the current record before the commit
 */
	if (cur_index._wrmode) {			/* do only if in update mode */
		if (mfld && *mfld) {			/* has the array been allocated yet? */
			if (!out_rec(MASTER)) {		/* write it */
				db_err(EOUTREC, "In %s: error in out_rec", _progname);
				return FALSE;
			}
		}
	}

	i = sprintf(cmd, "%d|", COMMIT);
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
	if (i < 0) {
		db_err(i, "In %s: Error during COMMIT", _progname);
		i = FALSE;
	}
	free(buff);
	in_xact = FALSE;
	return(i);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
