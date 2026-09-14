/* ***************************************************************
 *
 * PROCEDURE:	flush.c
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		Thu Mar 18 12:10:48 MDT 2002
 * 
 * AUTHOR:		Tom Green
 * 
 * MODIFICATION HISTORY:
 *				Tue Jan 18 18:36:41 MST 2005
 *				modified how fl_lock is used for the new file
 *				locking model.
 *				tomg
 ************************************************************* */

/*
 * this routine writes a data record out to the appropriate file
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


#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

#include "srv_index.h"
#include "lock.h"
#include "errors.h"
#include "misc.h"
#include "storage_io.h"

extern int idx_cnt;

extern INDEX *_indices;
extern FILES *_wfiles[];

extern int put_blobs (FILES *, int , int64_t , char *);

int flush(char *cmd, int c_off, char **data)
{
	int i;
	int len;
	int fmt;

	int64_t offs;

	char *cptr;

	INDEX *idx;
	FILES *fptr;

	i = atoi(cmd+c_off);
	if (i < 0) {
		i *= -1;
		if (i > MAX_CONNS)
			return(EINVMSG);
		fptr = _wfiles[i-1];
		if ((cptr = strchr(cmd+c_off, '|') + 1) == (char *)1)
			return(EINVMSG);
	} else {
		if (i >= idx_cnt)
			return(EINVMSG);
		if ((idx = _indices+i) == NULL)
			return(EINVMSG);
		if (!idx->_refcnt)
			return(EIDXNOO);
		if ((cptr = strchr(cmd+c_off, '|') + 1) == (char *)1)
			return(EINVMSG);

		i = atoi(cptr);
		if (i < 0 || i >= idx->_f_cnt)
			return(EINVMSG);
		fptr = idx->_files[i];
	}
	if (fptr == NULL)
		return(EINVMSG);

	if ((cptr = strchr(cptr, '|') + 1) == (char *)1)
		return(EINVMSG);
	offs = strtoll(cptr, NULL, 0);

	if ((cptr = strchr(cptr, '|') + 1) == (char *)1)
		return(EINVMSG);
	fmt = atoi(cptr);
	if (fmt < 1 || fmt > fptr->_filedesc->n_rformats)
		return(EINVMSG);
	cptr = *data;

	fl_lock(&fptr->_lock, LOCK_EX);
	len = fptr->_filedesc->record_desc[fmt-1].rf_len;
	/* A failed record write must not be hidden by a successful blob write. */
	if (offs < 0 || offs > INT64_MAX - (int64_t)DATARECORD_HEADER_LENGTH || len < 0 ||
			dm_storage_write_at(fptr->_chan, cptr, (size_t)len,
				offs + DATARECORD_HEADER_LENGTH) < 0) {
		i = ERECWRT;
		goto done;
	}
	if (fptr->_filedesc->record_desc[fmt-1].has_blob)
		if ((i = put_blobs(fptr, fmt, offs, cptr)) < 0)
			goto done;

	strcpy(cmd, "0|1|");
	i = 4;
done:
	fl_lock(&fptr->_lock, LOCK_UN);
	free(cptr);
	*data = NULL;
	return(i);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
