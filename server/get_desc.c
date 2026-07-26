/* ***************************************************************
 *
 * PROCEDURE:	get_desc.c
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		Sun Mar 24 11:16:20 MST 2002
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 ************************************************************* */
/*
 * this returns the description of a particular file in the
 * selected index
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
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include <pthread.h>

#include "srv_index.h"
#include "errors.h"
#include "misc.h"

extern INDEX *_indices;
extern FILES *_wfiles[];

extern int idx_cnt;
extern int dbgsw;

int get_desc(char *cmd, int c_off, char **ret)
{

	int i;

	char *cptr;
	char *rptr;

	INDEX *idx;
	FILES *fptr;

	*ret = NULL;
	i = atoi(cmd+c_off);

	if (i < 0) {
		if (i > MAX_CONNS)
			return(EINVMSG);
		fptr = _wfiles[(-i)-1];
	} else {
		if (i >= idx_cnt)
			return(EINVMSG);
		if ((idx = _indices+i) == NULL)
			return(ENOINDEX);
		if (!idx->_refcnt)
			return(EIDXNOO);

		if ((cptr = strchr(cmd+c_off, '|') + 1) == (char *)1)
			return(EINVMSG);
		i = atoi(cptr);
/*
 * none of these cases really should happen
 */
		if (i > idx->_f_cnt || !idx->_files[i]->_desc)
			return(ENOTOPEN);
		fptr = idx->_files[i];
	}

	if (!fptr)
		return(ENOFILE);
	if ((rptr = malloc(fptr->_hlen)) == NULL)
		return(ENOALLOC);

	memcpy(rptr, (char *)(fptr->_desc), fptr->_hlen);
	i = sprintf(cmd, "%d|1|", fptr->_hlen);

done:
	*ret = rptr;
	if (dbgsw) {
		fprintf(stderr, "at end of get_desc, cmd = %s, i = %d\n", cmd, i);
		fflush(stderr);
	}
	return(i);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
