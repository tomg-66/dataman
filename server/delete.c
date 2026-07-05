/* ***************************************************************
 *
 * PROCEDURE:	delete.c
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
 *				Thu Jul 13 18:07:03 MDT 2006
 *				there are now four cases where this function can
 *				be called:
 *					1) just a plain delete.  get rid of the rec.
 *					2) inside a transaction - check the rec and
 *						return the appropriate one to the user
 *					3) finalize the deletion in the commit.
 *					4) clean up the blobs after a successful commit.
 *				tomg
 *
 *				Wed Apr 16 17:01:34 MDT 2008
 *				added changes to keep a deleted record in a free
 *				list so that empty space can be reused.
 *				tomg
 *
 *				Wed Aug 26 20:36:02 MDT 2008
 *				removed the freelist implementation.  turned out
 *				to not be such a good idea.
 *				tomg
 *
 ************************************************************* */
/*
 * this routine deletes the current record pointed to by the named index.
 * the calling sequence is:
 *      delete(idx_name);
 * where idx_name is any index that is open and has a record.  the current
 * record usually then becomes the one logically prior to the deleted one.
 * if the deleted record is the first of the file, the current record
 * becomes the record that logically followed the deleted record.
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
#include <inttypes.h>

#include "srv_index.h"
#include "lock.h"
#include "errors.h"
#include "misc.h"

#define DEL     0200                    /* bit mask for deleted record */

extern int idx_cnt;

extern INDEX *_indices;

extern short int get_short(char *);

extern int64_t get_ll(char *);
extern void put_ll(char *, int64_t);
extern int get_blobs (FILES *, int , int64_t , char **, int *);
extern void blob_ctl(char *, char *, int, int64_t, int);

int delete(char *cmd, int c_off, char **ret)

{

	int i;
	int m_fmt;
	int m_len;
	int xsw;

	int64_t bof;				/* new begining record of file */
	int64_t recno, m_cur;		/* offset into file */
	int64_t m_prev;
	int64_t m_next;

	char *rptr;
	char *cptr;
	char buff[DATARECORD_HEADER_LENGTH];	/* output buffer */
	char file_name[512];
	char *path_name, *data_name;

	INDEX *index;				/* the insert index structure */
	FILES *fptr;

	RFDESC *rfptr;

	rptr = NULL;
	*ret = NULL;

	i = atoi(cmd+c_off);
	if (i < 0 || i >= idx_cnt)
		return(EINVMSG);
	if ((index = _indices+i) == NULL)
		return(EINVMSG);
	if (!index->_refcnt)
		return(EIDXNOO);

	if ((cptr = strchr(cmd+c_off, '|') + 1) == (char *)1)
		return(EINVMSG);
	i = atoi(cptr);
	if (i < 0 || i >= index->_f_cnt)
		return(EINVMSG);
	if ((fptr = index->_files[i]) == NULL)
		return(EINVMSG);
	if ((cptr = strchr(cptr, '|') + 1) == (char *)1)
		return(EINVMSG);
	m_cur = recno = strtoll(cptr, NULL, 0);
	if ((cptr = strchr(cptr, '|') + 1) == (char *)1)
		return(EINVMSG);
	xsw = atoi(cptr);

	if (fptr->_chan == 0)
		return(ENOTOPEN);

	fl_lock(&fptr->_lock, LOCK_EX);
/*
 * get to current record then read the header
 *
 * this file now has an exclusive lock on the file, so don't
 * need to put a mutex around the lseek/read,write * (as long
 * as no one else tries anything without locking it)
 */
	llseek(fptr->_chan, recno, SEEK_SET);
	if (read(fptr->_chan, buff, DATARECORD_HEADER_LENGTH) != DATARECORD_HEADER_LENGTH) {
		i = ERHREAD;
		goto done;
	}
	if ((*buff & DEL) && xsw != CLEANUP) {
		i = ENOREC;
		goto done;
	}

	m_fmt = *buff & ~(char)DEL;				/* on CLEANUP del bit will be set */
	m_prev = get_ll(buff+OFFSET_TO_PREV);	/* save prev record pointer */
	m_next = get_ll(buff+OFFSET_TO_NEXT);	/* save next record pointer */

	if (m_next == 0 && m_prev == 0) {
		i = ENODEL;							/* can't del only rec in file! */
		goto done;
	}
/*
 * get rid of any blobs associated with this record
 */
	rfptr = fptr->_filedesc->record_desc+m_fmt-1;
	if (xsw != XACT && rfptr->has_blob) {
		strcpy(file_name, fptr->_fname);
		path_name = dirname(file_name);				/* this will return /path/to/root/files */
		strcpy(file_name, path_name);
		path_name = dirname(path_name);			/* this will return /path/to/root */
		strcpy(path_name, fptr->_fname);
		data_name = basename(path_name);			/* this will be datafile_name */
		blob_ctl(path_name, data_name, m_fmt, recno, xsw);
		if (xsw == CLEANUP) {
			i = 0;
			goto done;
		}
	}
/*
 * if the case is we are deleting the record or in a commit and deleting
 * rewrite the 'flag byte' for this record, then rewrite the headers for
 * the prev and next records.
 */
	if (xsw == NOXACT || xsw == INCOMMIT) {
		bof = 0;
		*buff = m_fmt | DEL;							/* set deleted bit */
		llseek(fptr->_chan, recno, SEEK_SET);			/* get to current record */
		if (write(fptr->_chan, buff, sizeof(char)) != sizeof(char)) {
			i = EHDRWRT;
			goto done;
		}
/*
 * rewrite the back pointer of the next record if there is one
 */
		if (m_next != 0) {
			llseek(fptr->_chan, m_next+OFFSET_TO_PREV, SEEK_SET);
			put_ll(buff, m_prev);
			if (write(fptr->_chan, buff, PTR_LENGTH) != PTR_LENGTH) {
				i = EHDRWRT;
				goto err_1;
			}
			recno = m_next;
			if (m_prev == 0)
				bof = m_next;
		}
/*
 * rewrite the next pointer of the prev record if there is one
 */
		if (m_prev != 0) {
			llseek(fptr->_chan, m_prev+OFFSET_TO_NEXT, SEEK_SET);
			put_ll(buff, m_next);
			if (write(fptr->_chan, buff, PTR_LENGTH) != PTR_LENGTH) {
				i = EHDRWRT;
				goto err_2;
			}
			if (recno != m_next)
				recno = m_prev;
		}
/*
 * if we deleted the first record, update in the file where the
 * new first record is to be found
 */
		if (bof) {
			llseek(fptr->_chan, fptr->_hlen+2, SEEK_SET);	/* set pos to first rec */
			put_ll(buff, bof);
			if (write(fptr->_chan, buff, PTR_LENGTH) != PTR_LENGTH) {
				i = EBEGWRT;
				goto err_3;
			}
		}
		i = 0;
	} else {
		if (m_next == 0)
			recno = m_prev;
		else
			recno = m_next;
	}
/*
 * read the record that we need to return to the user
 */
	if (xsw == NOXACT || xsw == XACT) {
		llseek(fptr->_chan, recno, SEEK_SET);
		if (read(fptr->_chan, buff, DATARECORD_HEADER_LENGTH) != DATARECORD_HEADER_LENGTH) {
			i = ERHREAD;
			goto err_4;
		}
		m_fmt = *buff;

		m_len = fptr->_filedesc->record_desc[m_fmt-1].rf_len;

		rptr = malloc(m_len);
		i = m_len;
		if (read(fptr->_chan, rptr, m_len) != m_len) {
			i = ERECREAD;
			free(rptr);
			rptr = NULL;
			goto err_4;
		}
		if (fptr->_filedesc->record_desc[m_fmt-1].has_blob) {
			if ((m_len = get_blobs(fptr, m_fmt, recno, &rptr, NULL)) < 0) {
				free(rptr);
				rptr = NULL;
				i = m_len;		/* gets an error if get_blobs() failed */
				goto err_4;
			}
		}
		i = sprintf(cmd, "%d|%"PRId64"|%d|", m_len, recno, m_fmt);
	}
	goto done;
/*
 * these error labes put things back the way they were before
 * we got into here
 */
err_4:
	if (bof) {
		llseek(fptr->_chan, fptr->_hlen+2, SEEK_SET);	/* set pos to first rec */
		put_ll(buff, m_cur);
		if (write(fptr->_chan, buff, PTR_LENGTH) < PTR_LENGTH) {
			i = EMULTIPLE;
			goto done;
		}
	}
err_3:
	if (m_prev != 0) {
		llseek(fptr->_chan, m_prev+OFFSET_TO_NEXT, SEEK_SET);
		put_ll(buff, m_cur);
		if (write(fptr->_chan, buff, PTR_LENGTH) < PTR_LENGTH) {
			 i = EMULTIPLE;
			 goto done;
		}
	}
err_2:
	if (m_next != 0) {
		llseek(fptr->_chan, m_next+OFFSET_TO_PREV, SEEK_SET);
		put_ll(buff, m_cur);
		if (write(fptr->_chan, buff, PTR_LENGTH) < PTR_LENGTH) {
			i = EMULTIPLE;
			goto done;
		}
	}
err_1:
	*buff = m_fmt;
	llseek(fptr->_chan, m_cur, SEEK_SET);			/* get to current record */
	if (write(fptr->_chan, buff, 1) < 1)			/* re-mark the record as good */
		i = EMULTIPLE;
done:
	fl_lock(&fptr->_lock, LOCK_UN);
	*ret = rptr;
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
