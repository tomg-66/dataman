/* ***************************************************************
 *
 * PROCEDURE:	findex.c
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
 *				March 2002
 *				Tom Green
 *				modified for use as a client side function.
 ************************************************************* */

/*
 * this procedure finds the proper index
 * the calling sequence is:
 *      idx = findex(name)
 *  name is the name of an open index
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
#include <stdlib.h>
#include <string.h>

#include "globs.h"
#include "index.h"
#include "../server/errors.h"

extern INDEX _indices[6];                /* pointers to each index */
extern void db_err(int, char *, ...);

INDEX *findex(char *name)
{
    register int tmp;           /* the index to return */

	if (!name)
		db_err(0, "%s: findex: looking for unnamed index\n", _progname);

    for (tmp = 0;tmp < 6;tmp++) {
        if (strcmp(name,_indices[tmp]._idxname) == 0) {
            return(_indices+tmp);
        }
    }
/*
 * db_err never returns, so a warning about 
 * "control reaches end of non-void function" is ok
 */
	db_err(0, "%s: index named %s is not open\n", _progname, name);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker ft=c:
 */
