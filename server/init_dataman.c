/* ***************************************************************
 *
 * PROCEDURE:	init_dataman.c
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
 *				changes and modifications to be server side
 *				responding to client.
 *
 *				Tue Jan 18 18:36:41 MST 2005
 *				modified how fl_lock is used for the new file
 *				locking model.  with the new locking model, i
 *				also changed how the files were opened and
 *				referred to.  this makes more efficient use of
 *				resources.
 *				tomg
 *
 *				Sat Jul 30 12:19:18 MDT 2005
 *				modified to use misc.h in the move to 64 bit
 *				offsets for the database.
 *				tomg
 ************************************************************* */

/*
 * this routine initializes (declares) the global dataman variables.
 * the calling sequence is:
 *      init_dataman(argc,argv);
 * where argc, and argv are the arguments to main()
 *
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
#include <string.h>			/* strcat function */
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <ctype.h>
#include <inttypes.h>

#include <pthread.h>

#include "srv_index.h"		/* the index description */
#include "errors.h"
#include "lock.h"
#include "misc.h"

FILES *_wfiles[MAX_CONNS];		/* the open work files */
pthread_mutex_t w_mutex = PTHREAD_MUTEX_INITIALIZER;

extern short get_short(char *);
extern int64_t get_ll(char *);
extern FILES *get_file(char *);

extern int get_datafile_desc(FILES *);
extern void rm_file(char *);
extern int get_blobs (FILES *, int, int64_t, char **, int *);

extern int dbgsw;

int init_dataman(char *cmd, int c_off, char **ret)

{
	int i, j;
	int fmt;
	int tmp;

	short w_longest;

	char buff[DATARECORD_HEADER_LENGTH];		/* read buffer */
	char path[256];		/* path to file */
	char *ptr;
	char *rptr;

	int64_t w_cur;
	int64_t w_prev, w_next;

	FILES *fptr;

	if ((ptr = strrchr(cmd+c_off, '|')) == NULL)
		return(EINVMSG);
	*ptr = '\0';
	strcpy(path, cmd+c_off);
	ptr = path;
	rptr = NULL;

	pthread_mutex_lock(&w_mutex);
/*
 * find an open slot for another work file.
 */
	for (j = 0; j < MAX_CONNS; j++)
		if (_wfiles[j] == NULL)
			break;
	if (j == MAX_CONNS)
		return(ENOWSP);

	fptr = _wfiles[j] = get_file(path);

	if (fptr->_desc == NULL) {
		if ((i = get_datafile_desc(fptr)) < 0) {
			rm_file(path);
			goto done;
		}
	}
	w_cur = fptr->_hlen + sizeof(short);

	fl_lock(&fptr->_lock, LOCK_SH);
/*
 * having a shared lock on the file means that other threads can
 * access the file while we 'sleep', put a mutex around any
 * seek/read
 */
	pthread_mutex_lock(&fptr->_mutex);
	llseek(fptr->_chan, w_cur, SEEK_SET);
	if (read(fptr->_chan, buff, PTR_LENGTH) != PTR_LENGTH) {
		pthread_mutex_unlock(&fptr->_mutex);
		rm_file(path);
		i = EFHDRD;
		goto done;
	}
	pthread_mutex_unlock(&fptr->_mutex);

	w_cur = get_ll(buff);				/* offset to first record */
	w_longest = fptr->_longest;			/* longes record */

	if (dbgsw) {
		fprintf(stderr, "in init_dataman, reading record %"PRId64"\n", w_cur);
		fflush(stderr);
	}
	pthread_mutex_lock(&fptr->_mutex);
	llseek(fptr->_chan, w_cur, SEEK_SET);
	if (read(fptr->_chan, buff, DATARECORD_HEADER_LENGTH) != DATARECORD_HEADER_LENGTH) {
		pthread_mutex_unlock(&fptr->_mutex);
		if (dbgsw) {
			fprintf(stderr, "Can't read record header");
			perror("");
			fflush(stderr);
		}
		i = ERHREAD;
		rm_file(path);
		goto done;
	}
	fmt = *buff & 077;						/* save format number */
	tmp = fptr->_filedesc->record_desc[fmt-1].rf_len;

	if ((rptr = malloc(tmp)) == NULL) {
		i = ENOALLOC;
		pthread_mutex_unlock(&fptr->_mutex);
		rm_file(path);
		goto done;
	}
	if (read(fptr->_chan, rptr, tmp) != tmp) {
		pthread_mutex_unlock(&fptr->_mutex);
		free(rptr);
		rptr = NULL;
		rm_file(path);
		i = ERECREAD;
		goto done;
	}
	pthread_mutex_unlock(&fptr->_mutex);
	if (fptr->_filedesc->record_desc[fmt-1].has_blob) {
		if ((tmp = get_blobs(fptr, fmt, w_cur, &rptr, NULL)) < 0) {
			free(rptr);
			rptr = NULL;
			i = tmp;
			goto done;
		}
	}

	w_prev = get_ll(buff+OFFSET_TO_PREV);
	w_next = get_ll(buff+OFFSET_TO_NEXT);

	i = sprintf(cmd, "%d|%d|%d|%d|%"PRId64"|%"PRId64"|%"PRId64"|", tmp, j+1,
					w_longest, fmt, w_cur, w_prev, w_next);

done:
	fl_lock(&fptr->_lock, LOCK_UN);
	pthread_mutex_unlock(&w_mutex);
	*ret = rptr;
	return(i);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker ft=c:
 */
