/* ***************************************************************
 *
 * PROCEDURE:	get_rec
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		Wed Jul  5 20:30:42 MDT 2006
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 *
 ************************************************************* */

/*
 * this procedure simply retrieves a specific record from the
 * database. it is only ever called from commit, so the index
 * and file -must- already be set up.
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

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <inttypes.h>

#include <pthread.h>

#include "srv_index.h"
#include "lock.h"
#include "errors.h"
#include "misc.h"
#include "dbfunc.h"

#define DEL		0200                    /* deleted mask */

#define MAXSIZ	256

extern int idx_cnt;
extern int dbgsw;

extern INDEX *_indices;
extern int get_blobs(FILES *, int, int64_t, char **, int *);


int get_rec(char *cmd, int c_off, char **ret)
{

	int len;							/* misc length usage */
	int i;
	int ixno;
	int fno;
	int fmt;

	int64_t recno;						/* misc usage */

	char buff[DATARECORD_HEADER_LENGTH];
	char *cptr;

	FILES *fptr;
	INDEX *idx;
/*
 * validate the index they want to operate on
 */
	ixno = atoi(cmd+c_off);
	if (ixno < 0 || ixno > idx_cnt)
		return(EINVMSG);
	if ((idx = _indices+ixno) == NULL)
		return(ENOINDEX);
	if (!idx->_refcnt)
		return(EIDXNOO);
/*
 * validate the file number they want to operate on
 */
	if ((cptr = strchr(cmd+c_off, '|') + 1) == (char *)1)
		return(EINVMSG);
	fno = atoi(cptr);
	if (fno < 0 || fno >= idx->_f_cnt)
		return(EINVMSG);
	if ((fptr = idx->_files[fno]) == NULL)
		return(EINVMSG);
/*
 * get the record number
 */
	if ((cptr = strchr(cptr, '|') + 1) == (char *)1)
		return(EINVMSG);
	recno = strtoll(cptr, NULL, 0);

	*ret = NULL;

	fl_lock(&fptr->_lock, LOCK_SH);
	pthread_mutex_lock(&fptr->_mutex);
	llseek(fptr->_chan, recno, SEEK_SET);
	if (read(fptr->_chan, buff, DATARECORD_HEADER_LENGTH) < DATARECORD_HEADER_LENGTH) {
		i = ERHREAD;
		goto err;
	}
	if (*buff & DEL) {
		i = ENOREC;
		goto err;
	}
	fmt = *buff & ~DEL;
	if (dbgsw) {
		fprintf(stderr, "record format to get is %d\n", fmt);
		fflush(stderr);
	}
	
	len = fptr->_filedesc->record_desc[fmt-1].rf_len;

	if ((cptr = malloc(len)) == NULL) {
		i = ENOALLOC;
		goto err;
	}
	if (read(fptr->_chan, cptr, len) < len) {
		free(cptr);
		i = ERECREAD;
		goto err;
	}
	pthread_mutex_unlock(&fptr->_mutex);

	if (fptr->_filedesc->record_desc[fmt-1].has_blob) {
		if ((len = get_blobs(fptr, fmt, recno, &cptr, NULL)) < 0) {
			i = len;
			free(cptr);
			goto err;
		}
	}
	fl_lock(&fptr->_lock, LOCK_UN);
	if (dbgsw) {
		fprintf(stderr, "read %d bytes for record\n", len);
		fflush(stderr);
	}
/*
 * put together the return buffer.  the information being returned is:
 * len = length of shared memory portion (data record)
 * ret = record format number
 * numb = node number
 * off = key offset in node
 * found system key (no terminating '|')
 * the node number and offset in node are saved and returned for things like
 * get_next and so on as a hint for where to start looking for the original key
 */
	i = sprintf(cmd, "%d|%d|%d|%"PRId64"|%d|", len, ixno, fno, recno, fmt);

	*ret = cptr;
	return(i);

err:
/*
 * these unlocks don't hurt to do again if it was already done, and
 * can come here for other errors
 */
	pthread_mutex_unlock(&fptr->_mutex);
	fl_lock(&fptr->_lock, LOCK_UN);
	*ret = NULL;
	return(i);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
