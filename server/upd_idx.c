/* ***************************************************************
 *
 * PROCEDURE:	upd_idx
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
 *				March 2002
 *				Tom Green
 *				added file locking, concurrency, and argument
 *				handling as required for the server side.
 *
 *				Tue Jan 18 18:36:41 MST 2005
 *				modified how fl_lock is used for the new file
 *				locking model.
 *				tomg
 ************************************************************* */

/*
 * this procedure updates the current index structure
 * calling sequence is upd_idx(idx,nod,off,numb)
 * where:
 *      idx  is a pointer to the new index
 *      node is a pointer to the new node information to be put in idx
 *      off  is the offset into the node for this key
 *      numb is the current node number
 *
 * this is the heart of returning data for all of the get_* routines
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
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <inttypes.h>

#include <pthread.h>

#include "srv_index.h"
#include "node.h"
#include "lock.h"
#include "errors.h"
#include "misc.h"

#define DEL		0200                    /* deleted mask */

#define MAXSIZ	256

extern int dbgsw;

extern short get_short(char *);
extern int64_t get_ll(char *);
extern char *substr(char *,int,int);

extern int get_datafile_desc(FILES *);
extern int get_blobs (FILES *, int, int64_t, char **, int *);

/*
 * check to see if a record has been marked deleted.
 *
 * you mask off the two bits at the top because they are control
 * information, and if you don't you pass back an impossible
 * record format number, and it goes -crash-
 *
 * the file still has a mutex on it when it successfully returns.
 */
static int check_rec(FILES *file, int64_t offs)
{
	int64_t tmp;
	char name[DATARECORD_HEADER_LENGTH];

	pthread_mutex_lock(&file->_mutex);
	llseek(file->_chan, offs, SEEK_SET);				/* get to record */
	if ((tmp=read(file->_chan, name, DATARECORD_HEADER_LENGTH)) != DATARECORD_HEADER_LENGTH) {
		pthread_mutex_unlock(&file->_mutex);
		return(ERHREAD);							/* error gracefully? */
	}
	if (*name & DEL) {						/* high bit set means deleted record */
		pthread_mutex_unlock(&file->_mutex);
		return(0);							/* points to deleted record */
	}
	return(*name & ~DEL);					/* good record return the format number */
}

/*
 * do the data update stuff.
 */
int upd_idx(register INDEX *idx,
			register NODE *nod,
			int off,
			int64_t numb,
			int64_t spec,			/* specific rec, not from key */
			char *cmd,				/* originally command buf, holds return */
			char **buff)			/* holds shared memory portion of return */
{

	int len;							/* misc length usage */
	int ret;							/* return value */
	int offs;
	int i;

	int64_t msc;						/* misc usage */

	char *inkey;						/* temporary key storage */
	char *tptr;

	FILES *fptr;

	msc = idx->_keylen + MISC_LEN;
	inkey = substr(nod->_keys, (int)msc*off, (int)((off+1)*msc-1));
	offs = *(inkey+idx->_keylen) - 1;
	fptr = idx->_files[offs];
	if (dbgsw) {
		fprintf(stderr, "enter upd_idx\n");
		fflush(stderr);
	}

/*
 * check this index file.  if it has never been opened, need to
 * do so, read the file header, and other stuff
 */
	pthread_mutex_lock(&(idx->_mutex));
	if (!fptr->_desc) {
		if ((ret = get_datafile_desc(fptr)) < 0) {
			pthread_mutex_unlock(&(idx->_mutex));
			goto err;
		}
	}
	pthread_mutex_unlock(&(idx->_mutex));

/*
 * find out if the pointed to record has been deleted
 */
	if (spec == 0) 
		msc = get_ll(inkey+idx->_keylen+1);       /* get record pointer */
	else
		msc = spec;
/*
 * remember that when when we return good from check_rec() that the mutex
 * for that file (fptr->_mutex) is locked!
 */
	fl_lock(&fptr->_lock, LOCK_SH);					/* lock the file for reading */
	if ((ret = check_rec(fptr, msc)) <= 0)
		goto err;
	if (dbgsw) {
		fprintf(stderr, "record format to get is %d\n", ret);
		fflush(stderr);
	}
	if (ret < 1 || ret > fptr->_filedesc->n_rformats) {
		ret = EBADFMT;
		goto err;
	}
/*
 * ret receives the record format number if check_rec indicated that
 * the record is ok.  it is the true rf number, not the offset!
 */
	len = fptr->_filedesc->record_desc[ret-1].rf_len;
	if (len < 1 || len > fptr->_longest) {
		ret = EBADFMT;
		goto err;
	}

	if ((tptr = malloc(len)) == NULL) {
		ret = ENOALLOC;
		goto err;
	}
	if (read(fptr->_chan, tptr, len) < len) {
		free(tptr);
		ret = ERECREAD;
		goto err;
	}
	pthread_mutex_unlock(&fptr->_mutex);

	if (fptr->_filedesc->record_desc[ret-1].has_blob) {
		if ((len = get_blobs(fptr, ret, msc, &tptr, NULL)) < 0) {
			ret = len;
			free(tptr);
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
	if (dbgsw) {
		fprintf(stderr, "in upd_idx, len = %d, ret = %d, numb = %"PRId64", off = %d, cmd = 0x%p\n", len, ret, numb, off, cmd);
		fflush(stderr);
	}
	i = sprintf(cmd, "%d|%d|%"PRId64"|%d|", len, ret, numb, off);
	memcpy(cmd+i, inkey, idx->_keylen+MISC_LEN);
	i += idx->_keylen+MISC_LEN;
	if (dbgsw) {
		fprintf(stderr, "at end of updidx msg = ");
		fwrite(cmd, 1, i, stderr);
		fprintf(stderr, "\n\ttptr = %s", tptr);
		fwrite(tptr, 1, len, stderr);
		fprintf(stderr, "\n");
		fflush(stderr);
	}
	*buff = tptr;
	free(inkey);
	return(i);

err:
/*
 * these unlocks don't hurt to do again if it was already done, and
 * can come here for other errors
 */
	pthread_mutex_unlock(&fptr->_mutex);
	fl_lock(&fptr->_lock, LOCK_UN);
	*buff = NULL;
	free(inkey);									/* deleted record */
	return(ret);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
