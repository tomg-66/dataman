/* ***************************************************************
 *
 * PROCEDURE:	iclose.c
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
 * this procedure closes the named index and if the current record is
 * from the current index, flushes the record, and closes the data file.
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
#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include "index.h"
#include "m_params.h"
#include "globs.h"
#include "../../server/dbfunc.h"

extern void out_rec(int);
extern INDEX *findex(char *);
extern char *db_send(char *, int, char *);
extern void db_err(int, char *, ...);

void iclose(char *ixname)
{
	INDEX *idx;			/* pointer to index structure to close */

	int tmp;			/* misc usages */

	char cmd[128];
	char *buff;

	if (cur_index._wrmode)
		out_rec(MASTER);					/* flush the record */

/*
 * don't need to check the return of this, because if the named
 * index isn't open, then findex will message and terminate
 */
	idx = findex(ixname);
	sprintf(cmd, "%d|%d|", ICLOSE, idx->_idxno);
	buff = db_send(cmd, strlen(cmd), __FILE__);

	tmp = atoi(buff);
	if (tmp < 0)
		db_err(tmp, "%s: iclose error", _progname);

	if (idx->_savptr != NULL) {				/* has a save been done? */
		if (idx->_savptr->_savkey != NULL)	/* is the saved key ok? */
			free(idx->_savptr->_savkey);	/* free the saved key */
		free(idx->_savptr);					/* free the save structure */
    }
    if (idx->_curkey != NULL)				/* initial GET attempted? */
		free(idx->_curkey);					/* free the key */

    for(tmp = 0;tmp < idx->_fno;tmp++) {
		free(idx->_files[tmp]._fname);			/* free the file name */
		if (idx->_files[tmp]._desc != NULL)		/* is a file here? */
			free(idx->_files[tmp]._desc);		/* free the description */
	}
	free(idx->_files);

    if (idx->_idxno == cur_index._idxno) {
		m_fdesc = NULL;
		m_head = 0;
		m_cur = 0;
		m_chan = 0;
		m_fmt = 0;
		memset((char *)&cur_index, '\0', sizeof(INDEX));
	}
	memset((char *)idx, '\0', sizeof(INDEX));
	free(buff);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker ft=c:
 */
