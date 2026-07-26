/* ***************************************************************
 *
 * PROCEDURE:	save.c
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
 * this procedure saves the state of an index.
 * the calling sequence is:
 *      save(index_name);
 * where index_name is the name of the index whose state you want to save.
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

#include <malloc.h>
#include <stdio.h>

#include "m_params.h"
#include "index.h"
#include "globs.h"
#include "../../server/errors.h"
#include "../../server/misc.h"

extern char *substr(char *,int, int);
extern INDEX *findex(char *);
extern void db_err(int, char *, ...);

void save(idx_name)
char *idx_name;                         /* the index name to save info from */

{
    INDEX *idx;                         /* pointer to the index to change */

    idx = findex(idx_name);				/* get the index */
    if (idx->_curkey == 0)
		db_err(ENOGET, "%s: error in save", _progname);

    if (idx->_savptr)
        free(idx->_savptr->_savkey);					/* free the substring */
    else
        idx->_savptr = (SAVE *)malloc(sizeof(SAVE));	/* get new space */

    idx->_savptr->_savnode = idx->_curnode;
    idx->_savptr->_savrec = idx->_rptr;
    idx->_savptr->_savkey = substr(idx->_curkey,0,idx->_keylen+KEY_HEADER_LENGTH);
    idx->_savptr->_savfile = idx->_fno;
	idx->_savptr->_savfmt = m_fmt;
	idx->_savptr->_savoffs = idx->_offs;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker ft=c:
 */
