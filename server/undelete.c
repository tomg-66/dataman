/* ***************************************************************
 *
 * PROCEDURE:	undelete.c
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		Mon Jul 31 18:42:32 MDT 2006
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 *				Fri May  2 19:27:12 MDT 2008
 *				delete a record from the free list if need be.
 *				tomg
 *
 *				Sat Jul 25 08:43:29 MDT 2009
 *				removed references to the free list.
 *				tomg
 *
 ************************************************************* */
/*
 * a transaction had a failure.  we need to undo a delete the
 * user had performed.
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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <libgen.h>

#include "srv_index.h"
#include "lock.h"
#include "errors.h"
#include "misc.h"

#define DEL     0200                    /* bit mask for deleted record */

extern int idx_cnt;

extern INDEX *_indices;
extern int64_t get_ll(void *);
extern void put_ll(void *, int64_t);
extern void blob_ctl(char *, char *, int, int64_t, int);

int undelete(char *cmd, int c_off, char **ret)
{

	int i;
	int fmt;

	char *cptr;
	char buff[DATARECORD_HEADER_LENGTH];

	int64_t recno;
	int64_t next;
	int64_t prev;

	FILES *fptr;
	INDEX *iptr;

/*
 * set default return for this...
 */
	*ret = NULL;
/*
 * parse the command
 */
	i = atoi(cmd+c_off);
	if (i < 0 || i >= idx_cnt)
		return(EINVMSG);
	if ((cptr = strchr(cmd+c_off, '|') + 1) == (char *)1)
		return(EINVMSG);
	if ((iptr = _indices+i) == NULL)
		return(ENOINDEX);
	if (!iptr->_refcnt)
		return(EIDXNOO);

	i = atoi(cptr);
	if (i < 0 || i >= iptr->_f_cnt)
		return(EINVMSG);
	if ((fptr = iptr->_files[i]) == NULL)
		return(EINVMSG);
	if (fptr->_chan == 0)
		return(ENOTOPEN);
	if ((cptr = strchr(cptr, '|') + 1) == (char *)1)
		return(EINVMSG);

	recno = strtoll(cptr, NULL, 0);
/*
 * having an exclusive lock means that no one else can read
 * or write this file during our undelete.
 */
	fl_lock(&fptr->_lock, LOCK_EX);

	llseek(fptr->_chan, recno, SEEK_SET);
	if (read(fptr->_chan, buff, DATARECORD_HEADER_LENGTH) != DATARECORD_HEADER_LENGTH) {
		i = ERHREAD;
		goto done;
	}
	fmt = *buff & ~DEL;				/* strip off the DELETED bit */
	prev = get_ll(buff+OFFSET_TO_PREV);
	next = get_ll(buff+OFFSET_TO_NEXT);
/*
 * modify the previous records next pointer to point at this record
 */
	while (prev) {
		llseek(fptr->_chan, prev, SEEK_SET);
		if (read(fptr->_chan, buff, DATARECORD_HEADER_LENGTH) != DATARECORD_HEADER_LENGTH) {
			i = ERHREAD;
			goto done;
		}
		if (!(*buff&DEL))
			break;
		prev = get_ll(buff+OFFSET_TO_PREV);
	}
/*
 * if we found a previous record, we have that point to the one we are
 * undeleting.  if we didn't, this becomes the first record in the file
 * and we need to modify the file header
 */
	if (prev) {
		put_ll(buff, recno);
		llseek(fptr->_chan, prev+OFFSET_TO_NEXT, SEEK_SET);
		if (write(fptr->_chan, buff, PTR_LENGTH) < PTR_LENGTH) {
			i = EHDRWRT;
			goto done;
		}
	} else {
		prev = fptr->_hlen + 2;					/* where to seek to */
		llseek(fptr->_chan, prev, SEEK_SET);	/* get to file position */
		put_ll(buff, recno);
		if (write(fptr->_chan, buff, PTR_LENGTH) != PTR_LENGTH) {
			i = EBEGWRT;
			goto done;
		}
	}
/*
 * modify the next records prev pointer to point at this record
 */
	while (next) {
		llseek(fptr->_chan, next, SEEK_SET);
		if (read(fptr->_chan, buff, DATARECORD_HEADER_LENGTH) != DATARECORD_HEADER_LENGTH) {
			i = ERHREAD;
			goto done;
		}
		if (!(*buff&DEL))
			break;
		next = get_ll(buff+OFFSET_TO_NEXT);
	}
	if (next) {
		put_ll(buff, recno);
		llseek(fptr->_chan, next+OFFSET_TO_PREV, SEEK_SET);
		if (write(fptr->_chan, buff, PTR_LENGTH) < PTR_LENGTH) {
			i = EHDRWRT;
			goto done;
		}
	}
/*
 * now mark this record as not deleted
 */
	llseek(fptr->_chan, recno, SEEK_SET);
	*buff = fmt;
	if (write(fptr->_chan, buff, DATARECORD_FLAG_LENGTH) < DATARECORD_FLAG_LENGTH) {
		i = EBEGWRT;
		goto done;
	}
/*
 * now we need to perhaps unhide any blobs that might have been
 * associated with this record.
 */
	blob_ctl(iptr->_rootdir, fptr->_fname, fmt, recno, UNHIDE);
	i = 0;

done:
	fl_lock(&fptr->_lock, LOCK_UN);
	i = sprintf(cmd, "%d|", i);
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
