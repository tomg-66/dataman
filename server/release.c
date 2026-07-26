/* ***************************************************************
 *
 * PROCEDURE:	release.c
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
 * 				modified for server side.  doesn't need file
 * 				locking, because it is only for sorts.
 *
 *				Tue Jan 18 18:36:41 MST 2005
 *				modified how fl_lock is used for the new file
 *				locking model.  the new locking model made it
 *				necessary to use different file handling.  this
 *				way is much cleaner too!
 *				tomg
 *
 ************************************************************* */

/*
 * this routine is very similar to forward.  The first defference is that
 * when this routine comes to the end of a data file it tries to get the
 * first record in the next named data file, if it is at the en of the data
 * file list it returns false.  The second difference is that it works only
 * on a work file.
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <inttypes.h>

#include <pthread.h>

#include "srv_index.h"
#include "errors.h"
#include "lock.h"
#include "misc.h"

#define FALSE   0
#define TRUE    1

extern int64_t get_ll(char *);
extern FILES * get_file(char *);

extern void rm_file(char *);
extern int get_blobs (FILES *, int, int64_t, char **, int *);
extern int get_datafile_desc(FILES *);

extern FILES *_wfiles[];
extern pthread_mutex_t w_mutex;

extern int dbgsw;

int release(char *cmd, int c_off, char **ret)

{
	int i;
	int fno;
	int tmp;
	int fmt;

	int64_t recno;
	int64_t w_prev;
	int64_t w_next;

	char path[132];                     /* path to file */
	char *cptr;
	char *rptr;

	FILES *fptr;

	rptr = NULL;
	*ret = NULL;

	fno = atoi(cmd+c_off) - 1;
	if (fno < 0 || fno >= MAX_CONNS)
		return(EINVMSG);
	if ((cptr = strchr(cmd+c_off, '|') + 1) == (char *)1)
		return(EINVMSG);
	fptr = _wfiles[fno];
	if (fptr == NULL)
		return(ENOWFILE);
	recno = strtoll(cptr, NULL, 0);
	if ((cptr = strchr(cptr, '|') + 1) == (char *)1)
		return(EINVMSG);
/*
 * if recno is zero, then there is a file name in the command as well
 * that tells us we are at the end of the current work file, and to
 * close it, and open the next.
 */
    if (!recno) {
		char *tptr;
		if ((tptr = strchr(cptr, '|')) == NULL)
			return(EINVMSG);
		*tptr = '\0';
		pthread_mutex_lock(&w_mutex);
		rm_file(fptr->_fname);
		fptr = _wfiles[fno] = get_file(cptr);

		if (fptr->_desc == NULL) {
			if ((i = get_datafile_desc(fptr)) < 0) {
				rm_file(fptr->_fname);
				goto err;
			}
		}

		if (read(fptr->_chan, path, PTR_LENGTH) < PTR_LENGTH) {			/* move 1st ptr to w_next */
			i = ERHREAD;
			goto done;
		}
		recno = get_ll(path);

		pthread_mutex_unlock(&w_mutex);
	}

	fl_lock(&fptr->_lock, LOCK_SH);
	llseek(fptr->_chan, recno, SEEK_SET);
	if (read(fptr->_chan, path, DATARECORD_HEADER_LENGTH) != DATARECORD_HEADER_LENGTH) {
		i = ERHREAD;
		goto done;
	}
	fmt = *path & 077;
	tmp = fptr->_filedesc->record_desc[fmt-1].rf_len;
	rptr = malloc(tmp);							/* get space for record */
	if (read(fptr->_chan, rptr, tmp) != tmp) {
		i = ERECREAD;
		goto done;
	}
	w_prev = get_ll(path+OFFSET_TO_PREV);
	w_next = get_ll(path+OFFSET_TO_NEXT);
	
	if(dbgsw) {
		fprintf(stderr, "release return buffer ->%s<-\n", rptr);
		fflush(stderr);
	}
	if (fptr->_filedesc->record_desc[fmt-1].has_blob) {
		if ((tmp = get_blobs(fptr, fmt, recno, &rptr, NULL)) < 0) {
			free(rptr);
			rptr = NULL;
			i = tmp;
			goto done;
		}
	}

	i = sprintf(cmd, "%d|%d|%d|%"PRId64"|%"PRId64"|%"PRId64"|", tmp, fptr->_longest, fmt, recno, w_prev, w_next);

done:
	fl_lock(&fptr->_lock, LOCK_UN);
	*ret = rptr;
	return(i);

err:
	fptr->_fname = NULL;				/* free up the work file entry */
	fptr->_desc = NULL;
	*ret = NULL;
	pthread_mutex_unlock(&w_mutex);
	return(i);

}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker ft=c:
 */
