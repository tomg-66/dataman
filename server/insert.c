/* ***************************************************************
 *
 * PROCEDURE:	insert.c
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		legacy, originally written in 1988
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 *				Tom Green
 *				Mar 18 2002
 *				modified to include file locking and other stuff
 *				as needed for making it a server routine.
 *
 *				Tue Jan 18 18:36:41 MST 2005
 *				modified how fl_lock is used for the new file
 *				locking model.
 *				tomg
 *
 *				Sat Jul 30 14:24:15 MDT 2005
 *				modified to use 64 bit offsets described in misc.h
 *				tomg
 *
 *				Wed Apr 16 18:03:13 MDT 2008
 *				modified to look first at the free list for space
 *				to insert a new record, instead of just adding it
 *				at the end of the file.
 *				tomg
 *
 *				Sat Jul 25 08:08:44 MDT 2009
 *				removed references to the freelist, and fixed a
 *				constant.
 *				tomg
 *
 ************************************************************* */
/*
 * this procedure inserts a new record into a data file logically 
 * before or after the current master record in that index.  The calling
 * sequence is:
 *      insert(fmt,mode,idx);
 * where fmt is the format number to insert
 *       mode is BEFORE or AFTER
 *       idx is the index to do the insert on
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
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdlib.h>

#include <stdio.h>

#include "srv_index.h"			/* index description */
#include "lock.h"
#include "errors.h"
#include "misc.h"

#define BEFORE  0				/* flag to insert before record */
#define AFTER   1				/* flag to insert after record */
#define DEL     0200			/* bit mask for deleted record */

extern int idx_cnt;

extern INDEX *_indices;			/* the current operating index */

extern void in_rec(int);
extern void put_ll(void *, int64_t);
extern short int get_short(char *);
extern int64_t get_ll(void *);

extern int dbgsw;

/*
void insert(int fmt,int mode,char *idx)
*/

int insert(char *cmd, int c_off, char **ret)
{

    int tmp;					/* misc usage */
	int fmt,					/* format number to insert */
		mode;					/* BEFORE or AFTER */
	int fileno;					/* file number we are working on */
	int i;
	int m_len;
//	int m_fmt;

	short *m_desc;

	int64_t bof;				/* new begining record of file */
	int64_t offs;				/* offset into file */
	int64_t m_new;				/* offset to end of file */
	int64_t m_cur;				/* offset to current record */
	int64_t m_prev;				/* offset to current record */
	int64_t m_next;				/* offset to current record */

	char *rptr;					/* this is the return pointer */
	char *cptr;					/* just a misc usage char ptr */
	char header[DATARECORD_HEADER_LENGTH];				/* record header */

	FILES *fptr;

    INDEX *index;				/* the insert index structure */

/*
 * parse through the received command.
 * format number to insert
 * BEFORE or AFTER
 * index number
 * file number in index
 * offset of record to insert around
 */
	*ret = NULL;
	fmt = atoi(cmd+c_off);				/* format number to insert */
	if ((cptr = strchr(cmd+c_off, '|') + 1) == (char *)1)
		return(EINVMSG);
	mode = atoi(cptr);				/* insert mode */
	if ((cptr = strchr(cptr, '|') + 1) == (char *)1)
		return(EINVMSG);
	tmp = atoi(cptr);				/* index number */
	if (tmp < 0 || tmp >= idx_cnt)
		return(EINVMSG);
	if ((cptr = strchr(cptr, '|') + 1) == (char *)1)
		return(EINVMSG);
	fileno = atoi(cptr);			/* file number */
	if ((cptr = strchr(cptr, '|') + 1) == (char *)1)
		return(EINVMSG);
	m_cur = strtoll(cptr, NULL, 0);

	if (dbgsw) {
		fprintf(stderr, "enter insert, format = %d\n"
						"mode = %s\n"
						"index = %d\n"
						"file = %d\n"
						"m_cur = %"PRId64"\n",
						fmt, mode == BEFORE ? "BEFORE" : "AFTER",
						tmp, fileno, m_cur);
		fflush(stderr);
	}

	rptr = NULL;
	if ((index = _indices+tmp) == NULL)		/* point at the right index */
		return(ENOINDEX);
	if (!index->_refcnt)
		return(EIDXNOO);
	if (fileno < 0 || fileno >= index->_f_cnt)
		return(EINVMSG);
	if ((fptr = index->_files[fileno]) == NULL)
		return(ENOTOPEN);
	if (fptr->_chan == 0)
		return( ENOTOPEN);
	m_desc = fptr->_desc;					/* master description */
	if (fmt > *m_desc || fmt < 1)
		return(EBADFMT);

/*
 * get an exclusive lock on the file, cause we're updating it
 * this means that no one else can be reading the file, so we
 * don't have to worry about a mutex around the seek/read,write.
 */
	fl_lock(&fptr->_lock, LOCK_EX);

	llseek(fptr->_chan, m_cur, SEEK_SET);		/* get to the current record */
	if(read(fptr->_chan ,header, DATARECORD_HEADER_LENGTH) != DATARECORD_HEADER_LENGTH) {	/* read the record header */
		i = ERHREAD;
		goto done;
	}
	if (*header & DEL) {
		i = ENOREC;							/* deleted record */
		goto done;
	}
	m_prev = get_ll(header+OFFSET_TO_PREV);		/* save the previous rec ptr */
	m_next = get_ll(header+OFFSET_TO_NEXT);		/* save pointer to next rec */

	bof = 0;									/* default to false */

	m_new = llseek(fptr->_chan, (loff_t)0, SEEK_END);		/* get the eof position */
	put_ll(header,m_new);

	switch(mode) {
		case BEFORE:
			llseek(fptr->_chan,m_cur+OFFSET_TO_PREV,SEEK_SET);	/* get to current record */
			if (write(fptr->_chan,header,PTR_LENGTH) < PTR_LENGTH) {	/* pointer to prev record */
				i = EHDRWRT;
				goto done;
			}
			if (m_prev != 0) {
				llseek(fptr->_chan,m_prev+OFFSET_TO_NEXT,SEEK_SET);		/* get to previous record */
				if (write(fptr->_chan, header, PTR_LENGTH) < PTR_LENGTH) {		/* pointer to next rec */
					i = EHDRWRT;
					put_ll(header, m_prev);
					llseek(fptr->_chan,m_cur+OFFSET_TO_PREV,SEEK_SET);	/* get to current record */
					if (write(fptr->_chan,header,PTR_LENGTH) < PTR_LENGTH) /* re-point to current */
						i = EMULTIPLE;
					goto done;
				}
			} else
				bof = m_new;					/* this is now the first rec */
			m_next = m_cur;						/* save next record offset */
			break;								/* done here */

		case AFTER:
			llseek(fptr->_chan,m_cur+OFFSET_TO_NEXT,SEEK_SET);			/* get to current record */
			if (write(fptr->_chan,header,PTR_LENGTH) < PTR_LENGTH) {	/* write pointer to next */
				i = EHDRWRT;
				goto done;
			}
			if (m_next != 0) {
				llseek(fptr->_chan,m_next+OFFSET_TO_PREV,SEEK_SET);	/* get to next record */
				if (write(fptr->_chan,header,PTR_LENGTH) < PTR_LENGTH) {	/* write prev pointer */
					i = EHDRWRT;
					put_ll(header, m_next);
					llseek(fptr->_chan, m_cur+OFFSET_TO_NEXT, SEEK_SET);
					if (write(fptr->_chan, header, PTR_LENGTH) < PTR_LENGTH)
						i = EMULTIPLE;
					goto done;
				}
			}
			m_prev = m_cur;							/* save prev record offset */
	}

	m_len = fptr->_filedesc->record_desc[fmt-1].rf_len;
	m_cur = m_new;									/* save pointer to cur rec */

	rptr = malloc(m_len+DATARECORD_HEADER_LENGTH);
	*rptr = fmt;									/* save the format number */
	put_ll(rptr+OFFSET_TO_PREV,m_prev);				/* save the prev pointer */
	put_ll(rptr+OFFSET_TO_NEXT,m_next);				/* save the next pointer */
	memset(rptr+DATARECORD_HEADER_LENGTH, ' ', m_len);
	llseek(fptr->_chan, m_new, SEEK_SET);			/* get to new record offset */

	if (write(fptr->_chan,rptr,m_len+DATARECORD_HEADER_LENGTH) != m_len+DATARECORD_HEADER_LENGTH) {
		i = ERECWRT;
		goto done;
	}
	free(rptr);

	if (bof) {
		offs = fptr->_hlen + 2;				/* where to seek to */
		llseek(fptr->_chan,offs,SEEK_SET);	/* get to file position */
		put_ll(header,m_new);
		if (write(fptr->_chan,header,PTR_LENGTH) != PTR_LENGTH) {
			i = EBEGWRT;
			goto done;
		}
    }
	i = sprintf(cmd, "0|%d|%"PRId64"|", m_len, m_cur);

done:
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
