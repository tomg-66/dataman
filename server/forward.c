/* ***************************************************************
 *
 * PROCEDURE:	forward.c
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		legacy, originally writtin in 1988
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 *				March 2002
 *				tomg
 *				changes to make a server side routine, locking,
 *				command response, and so forth.
 *
 *				Tue Jan 18 18:36:41 MST 2005
 *				modified how fl_lock is used for the new file
 *				locking model.
 *				tomg
 *
 *				Tue Aug 02 20:40:31 MST 2005
 *				modified to include 64 bit offsets in the code
 *				and header as we change to 64 bit databases.
 *				tomg
 ************************************************************* */

/*
 * this routine releases the current record in the database and moves
 * to the previous logical record in the database.
 * the calling sequence (using the #define) is:
 *      forward(idx_name)
 * where idx_name is either the name of an IOPENed index or a NULL (meaning
 * to back the work file).  the internal sequence is:
 *      if (bck(idx_name)) ;
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
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <inttypes.h>

#include "srv_index.h"
#include "lock.h"
#include "errors.h"
#include "misc.h"

#define TRUE    1
#define FALSE   0
#define MASK 077				/* mask off the upper two bits */

extern int idx_cnt;

extern INDEX *_indices;
extern FILES *_wfiles[];

extern short int get_short(char *);
extern int64_t get_ll(void *);
extern int get_blobs (FILES *, int, int64_t, char **, int *);


int forward(char *cmd, int c_off, char **ret)

{
	int i;
	int len;
	int fmt;

	int64_t curr;
    int64_t next;

	char *cptr;
	char *rptr;
	char buff[DATARECORD_HEADER_LENGTH];

    INDEX *idx;                         /* the index structure */
	FILES *fptr;

	rptr = NULL;
	*ret = NULL;
/*
 * get the index or work file
 */
	i = atoi(cmd+c_off);
	if (i < 0) {
		i *= -1;
		if (i > MAX_CONNS)
			return(EINVMSG);
		fptr = _wfiles[i-1];
		cptr = cmd+c_off;
	} else {
		if (i >= idx_cnt)
			return(EINVMSG);
		if ((idx = _indices+i) == NULL)
			return(EINVMSG);
		if (!idx->_refcnt)
			return(EIDXNOO);
		if ((cptr = strchr(cmd+c_off, '|') + 1) == (char *)1)
			return(EINVMSG);
/*
 * get the file
 */
		i = atoi(cptr);
		if (i < 0 || i >= idx->_f_cnt)
			return(EINVMSG);
		fptr = idx->_files[i];
	}
	if (!fptr)
		return(EINVMSG);
	if ((cptr = strchr(cptr, '|') + 1) == (char *)1)
		return(EINVMSG);
/*
 * this is the record pointer.
 */
	curr = strtoll(cptr, NULL, 0);

	fl_lock(&fptr->_lock, LOCK_SH);
/*
 * get to the current record and get the pointer to the next record.
 * if the next is 0, this is the last record, and we cant forward.
 *
 * this has only a shared lock on the datafile, so we need to put a
 * mutex around the seek/read
 *
 */
	pthread_mutex_lock(&fptr->_mutex);
	llseek(fptr->_chan, curr, SEEK_SET);
	if (read(fptr->_chan, buff, DATARECORD_HEADER_LENGTH) != DATARECORD_HEADER_LENGTH) {
		pthread_mutex_unlock(&fptr->_mutex);
		i = ERHREAD;
		goto done;
	}
	pthread_mutex_unlock(&fptr->_mutex);
    next = get_ll(buff+OFFSET_TO_NEXT);
	if (next == 0) {
		i = FALSE;
		goto done;
	}
/*
 * get to the new current record, read the header, get the format, find
 * the record length, then read the record. don't need to worry about
 * going to a deleted record, because of file locking.  we can't be
 * reading this while someone else is deleting.
 */
	pthread_mutex_lock(&fptr->_mutex); llseek(fptr->_chan, curr, SEEK_SET);
	llseek(fptr->_chan, next, SEEK_SET);
	if (read(fptr->_chan, buff, DATARECORD_HEADER_LENGTH) != DATARECORD_HEADER_LENGTH) {
		pthread_mutex_unlock(&fptr->_mutex);
		i = ERHREAD;
		goto done;
	}
	fmt = *buff & MASK;

	len = fptr->_filedesc->record_desc[fmt-1].rf_len;

	rptr = malloc(len);
	if (read(fptr->_chan, rptr, len) != len) {
		pthread_mutex_unlock(&fptr->_mutex);
		i = ERECREAD;
		goto done;
	}
	pthread_mutex_unlock(&fptr->_mutex);
	if (fptr->_filedesc->record_desc[fmt-1].has_blob) {
		if ((len = get_blobs(fptr, fmt, next, &rptr, NULL)) < 0) {
			free(rptr);
			rptr = NULL;
			i = len;
			goto done;
		}
	}
	i = sprintf(cmd, "%d|%"PRId64"|%d|", len, next, fmt);

done:
	*ret = rptr;
	fl_lock(&fptr->_lock, LOCK_UN);
	return(i);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
