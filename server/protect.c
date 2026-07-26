/* ***************************************************************
 *
 * PROCEDURE:	protect.c
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
 * 				March 2002
 * 				Tom Green
 *				modified to run under the server system.  file
 *				locking, concurrency, and so forth.
 *
 *				Tue Jan 18 18:36:41 MST 2005
 *				modified how fl_lock is used for the new file
 *				locking model.
 *				tomg
 *
 *				Wed Dec 28 17:48:17 MST 2005
 *				getting rid of one of the last vestiges of the
 *				old (non server) system.  user level locks as
 *				set by protect changed a bit in the header for
 *				a record on disk.  now keep all protect info
 *				for a file in a hash table for that file. now,
 *				should the server crash, a protect isn't left
 *				asserted.
 *				tomg
 ************************************************************* */

/*
 * this procedure protects the record currently pointed to by idx_name.  if
 * idx_name is a null the routine protects the current work record.
 * it's calling sequence is (using the #define):
 *	protect(idx_name) else ...
 * internally this is:
 *	if prtct(idx_name) ;
 * where idx name is the index that points to the record to protect.  if the 
 * record is currently protected the routine will pend, then make a new attempt
 * to protect the record.  if the routine fails on three attempts it returns
 * false.  a protected record is cleared with the clear command.  it also
 * assures that the protected record is the most recent available.
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
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <inttypes.h>

#include "srv_index.h"
#include "lock.h"
#include "errors.h"
#include "misc.h"

#define PROTCT	0100			/* protect bit mask */
#define DEL		0200			/* deleted record bit mask */
#define TRUE	1				/* boolean true value */
#define FALSE	0				/* boolean false value */

extern int idx_cnt;

extern INDEX *_indices;			/* last operated on index */
extern FILES *_wfiles[];			/* work files */

extern short get_short(char *);
extern int64_t get_ll(char *);

extern void pend(int, int);
extern int get_blobs (FILES *, int, int64_t, char **, int *);

int protect(char *cmd, int c_off, char **ret)

{
	INDEX *idx;				/* the found index */
	FILES *fptr;

	int i;
	int tmp;				/* temporary var */
	int idxno;
	int fno;
	int hash;
	int locked;

	int64_t recno;		/* the record to protect */

	char buf;				/* input output buffer */
	char *cptr;
	char *rptr;

	LOCKS *lptr;			/* pointer to locks struct */

	*ret = NULL;

	idxno = atoi(cmd+c_off);
	if (idxno < 0) {
		idxno *= -1;
		if (idxno > MAX_CONNS)
			return(EINVMSG);
		fptr = _wfiles[idxno-1];
		if ((cptr = strchr(cmd+c_off, '|') + 1) == (char *)1)
			return(EINVMSG);
		fno = 0;
	} else {
		if (idxno > idx_cnt)
			return(EINVMSG);
		if ((idx = _indices+idxno) == NULL)
			return(ENOINDEX);
		if (!idx->_refcnt)
			return(EIDXNOO);
		if ((cptr = strchr(cmd+c_off, '|') + 1) == (char *)1)
			return(EINVMSG);
		fno = atoi(cptr);
		if (fno < 0 || fno >= idx->_f_cnt)
			return(ENOFILE);
		fptr = idx->_files[fno];
		if ((cptr = strchr(cptr, '|') + 1) == (char *)1)
			return(EINVMSG);
	}
	if (fptr == NULL)
		return(ENOTOPEN);
	recno = strtoll(cptr, NULL, 0);
	hash = recno % LOCK_TABLE_SIZE;

	locked = 0;
	for (tmp = 1;tmp < 4;tmp++) {
/*
 * lock the file exclusively.  so no one else can do anything
 * to it.   find out if the requested record has already been
 * deleted.
 */
		fl_lock(&fptr->_lock, LOCK_EX);
		llseek(fptr->_chan, recno, SEEK_SET);
		if (read(fptr->_chan, &buf, DATARECORD_FLAG_LENGTH) != DATARECORD_FLAG_LENGTH) {
			i = EPRTRD;
			goto done;
		}
		if (buf & DEL) {
			i = EPRCTDEL;			/* deleted record, can't protect */
			goto done;
		}
/*
 * is there is already a hash chain for this hash entry?
 * no? create one, and we're done.
 */
		if (fptr->_locks[hash] == NULL) {
			if ((fptr->_locks[hash] = calloc(1, sizeof(LOCKS))) == NULL) {
				i = EPRTRD;
				goto done;
			}
			fptr->_locks[hash]->_recno = recno;
			locked = 1;
		} else {
/*
 * look to see if this record is already protected.
 */
			lptr = fptr->_locks[hash];
			while(1) {
				if(lptr->_recno == recno) {
/*
 * it is already protected.  if this is the third time
 * we've tried, then give up.
 */
					if (tmp == 3) {
						i = FALSE;
						goto done;
					}
/*
 * we can keep trying.  unlock the file, wait, and go around
 */
					fl_lock(&fptr->_lock, LOCK_UN);
					pend(500,0);
					break;
				}
				if (lptr->_next == NULL) {
/*
 * we are at the end of the chain, and we didn't find this
 * record, so we can assert the protection.
 */
					if ((lptr->_next = calloc(1,sizeof(LOCKS))) == NULL) {
						i = EPRTRD;
						goto done;
					}
					lptr->_next->_recno = recno;
					locked = 1;
					break;
				}
/*
 * check the next link in the chain.
 */
				lptr = lptr->_next;
			} 
		}
		if (locked)
			break;
	}
/*
 * if we get here, we've made a new entry in the protect hash table
 */
	i = buf & ~(DEL);					/* record format number */
	tmp = fptr->_filedesc->record_desc[i-1].rf_len;

/*
 * read the record.  we need to make sure we have the most recent
 * copy for the client.
 */
	rptr = malloc(tmp);
	lseek(fptr->_chan, recno + DATARECORD_HEADER_LENGTH, SEEK_SET);
	if (read(fptr->_chan, rptr, tmp) < tmp ) {
		i = ERECREAD;
		goto done;
	}
	if (fptr->_filedesc->record_desc[i-1].has_blob) {
		if ((tmp = get_blobs(fptr, i, recno, &rptr, NULL)) < 0) {
			free(rptr);
			rptr = NULL;
			i = tmp;
			goto done;
		}
	}
	i = sprintf(cmd, "%d|%d|%d|%d|%"PRId64"|", tmp, i, idxno, fno, recno);

done:
	fl_lock(&fptr->_lock,LOCK_UN);			/* all done, unlock  record */
	*ret = rptr;
	return(i);					/* protect worked */
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker ft=c:
 */
