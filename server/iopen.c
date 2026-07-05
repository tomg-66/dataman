/* ***************************************************************
 *
 * PROCEDURE:	iopen
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		legacy, originally written in 1988
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY
 * 				Feb 21 2002
 * 				changed to do meet the needs of the database
 *				server side 
 * 				Tom Green
 *
 *				Sun Jan 16 09:22:46 MST 2005
 *				since 2002 and now, linux has fixed it's
 *				broken threading model, and threads now all
 *				(properly) have the same pid.  This broke
 *				how I did file locking, and required a major
 *				redo on how open indices and files are handled.
 *				tomg
 *
 *				Sat Jul 30 15:25:52 MDT 2005
 *				modified to use 64 bit offsets described in mis
 *				tomg
 *
 ************************************************************* */
/*
 * this opens and initializes an index for use.  first off,
 * the reason that we do an array (that grows with need)
 * to store the index information (as opposed to a hash tab)
 * is that because calls from the clients can use just an
 * offset, and we don't have to do a look up.  The only
 * time we waste is in the initial open, a one time shot.
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
#include <malloc.h>
#include <string.h>
#include <libgen.h>
#include <errno.h>

#include <pthread.h>

#include "srv_index.h"
#include "errors.h"
#include "misc.h"

INDEX *_indices;				/* the currently opened indices */

int idx_cnt = 0;				/* count of indexes */

pthread_mutex_t i_mutex = PTHREAD_MUTEX_INITIALIZER;

extern int dbgsw;

extern short int get_short(char *);
extern int64_t get_ll(char *);
extern FILES *get_file(char *);

int iopen(char *cmd, int c_off, char **ret)
{
	int idx;				/* loop counter */
	int chan;				/* save the chan num */
	int numb;				/* the index number */
	int loop;
	int size;
	int tmp;				/* short temporary stuff */
	int i;

	char stuff[256];		/* general purpose buffer */
	char root[256];			/* database root directory */
	char ixname[32];		/* ok, could be longer, but why? */
	char *ptr;
	char *f_buff;			/* read the file names from the index */
	char *cptr;

	INDEX *tptr;

	if (dbgsw) {
		fprintf(stderr, "enter iopen, cmd = %s\n", cmd+c_off);
		fflush(stderr);
	}
	*ret = NULL;
	pthread_mutex_lock(&i_mutex);
/*
 * if this is the first time called, allocate index space
 */
	if (idx_cnt == 0) {
		if ((_indices = (INDEX *)calloc(8, sizeof(INDEX))) == NULL) {
			pthread_mutex_unlock(&i_mutex);
			return(EINITINDEX);
		}
		idx_cnt = 8;
		for (tmp = 0; tmp < 8; tmp++)
			pthread_mutex_init(&_indices[tmp]._mutex, NULL);
	}
	pthread_mutex_unlock(&i_mutex);

	if ((ptr = strchr(cmd+c_off, '|')) == NULL)
		return(EINVMSG);
	*ptr = '\0';
	strcpy(ixname, cmd+c_off);
	ptr++;
	if ((f_buff = strchr(ptr, '|')) == NULL)
		return(EINVMSG);
	*f_buff = '\0';
	strcpy(root, ptr);
	strcpy(stuff, root);
	strcat(stuff, "/index/");
	strcat(stuff, ixname);
	f_buff = NULL;

/*
 * find out if this index has already been opened
 */
retry:
	idx = -1;
	for (numb = 0; numb < idx_cnt; numb++) {
		pthread_mutex_lock(&(_indices[numb]._mutex));
		if (!_indices[numb]._idxname) {
			if (idx == -1)
				idx = numb;
			else
				pthread_mutex_unlock(&(_indices[numb]._mutex));
			continue;
		}
		if (!strcmp(stuff, _indices[numb]._idxname)) {
			_indices[numb]._refcnt++;
			if (idx > -1)
				pthread_mutex_unlock(&(_indices[idx]._mutex));
			pthread_mutex_unlock(&(_indices[numb]._mutex));
			idx = numb;
/* assume max of 63 byte index basenames */
			f_buff = malloc(64*_indices[idx]._f_cnt);
			memset(f_buff, '\0', 64*_indices[idx]._f_cnt);
			for (tmp = 0, i = 0; tmp < _indices[idx]._f_cnt; tmp++) {
				ptr = strdup(_indices[idx]._files[tmp]->_fname);
				strcpy(f_buff+i, basename(ptr));
				free(ptr);
				i += strlen(f_buff+i) + 1;
			}
			size = i;
			goto done;
		}
		if (_indices[numb]._refcnt == 0 && idx == -1) {
			free(_indices[numb]._idxname);
			idx = numb;
		} else
			pthread_mutex_unlock(&(_indices[numb]._mutex));
	}
/*
 * nope, is there room in the current array for a new entry?  if not
 * realloc the array of entries and use the new space
 */
	if (idx == -1) {
		int k;
/*
 * basically, this can only return EBUSY, so try again if it is
 */
		if ((tmp = pthread_mutex_trylock(&i_mutex)) < 0)
			goto retry;
		idx = idx_cnt;
		size = idx_cnt * 2 * sizeof(INDEX);
		if ((tptr = realloc(_indices, size)) == NULL) {
			pthread_mutex_unlock(&i_mutex);
			return(ENOIXSP);
		}
		memset((char *)(tptr+idx_cnt), '\0', idx_cnt * sizeof(INDEX));
		_indices = tptr;
		for(k = idx;k < idx_cnt*2; k++)
			pthread_mutex_init(&(_indices[k]._mutex), NULL);
		pthread_mutex_lock(&(_indices[idx]._mutex));
		idx_cnt *= 2;
		pthread_mutex_unlock(&i_mutex);
	}
/*
 * open the index file.  all this assures that we have only one
 * instance of the index open at one time.
 */
	if ((chan = open(stuff, O_RDWR|O_LARGEFILE)) < 0) {
		pthread_mutex_unlock(&(_indices[idx]._mutex));
		return(ENOINDEX);
	}

	_indices[idx]._idxchan = chan;
	_indices[idx]._idxname = strdup(stuff);

	if (read(chan, stuff, INDEX_FILE_OFFSET) != INDEX_FILE_OFFSET)
		goto err;

	_indices[idx]._keylen = get_short(stuff);					/* capture the keylength */
	_indices[idx]._f_cnt = get_short(stuff+sizeof(short)) + 1;	/* capture # of files */
	_indices[idx]._rootpos = get_ll(stuff+INDEX_HEADER_LENGTH);			/* capture the root offset */
	_indices[idx]._refcnt = 1;					/* there is one reference! */
	_indices[idx]._files = calloc(_indices[idx]._f_cnt,
					sizeof(FILES *));
	_indices[idx]._rootdir = strdup(root);
	_indices[idx]._lock.value = 0;
	pthread_cond_init(&_indices[idx]._lock.cond, NULL);
	pthread_mutex_init(&_indices[idx]._lock.mutex, NULL);

/*
 * assume the file names will be long... up to 64 bytes.  should we think longer?
 * and remember this is the 'basename' not the full path name.
 * at any rate, if we assume that, we can get enough space for all we need
 * the file names will have a name that is null terminated in the index.  so the
 * 'minimum' length of a file name stored there is 2.
 */
	if ((f_buff = malloc(_indices[idx]._f_cnt*64)) == NULL)
		goto err;
	if (read(chan,f_buff,_indices[idx]._f_cnt*64) < _indices[idx]._f_cnt*2)
		goto err;

	ptr = f_buff;
	size = 0;
	memset(stuff, '\0', sizeof(stuff));
	strcpy(stuff,root);
	strcat(stuff, "/files/");
	cptr = stuff+strlen(stuff);
	for (loop = 0; loop < _indices[idx]._f_cnt; loop++) {
		strcpy(cptr,ptr);
		size += strlen(ptr)+1;
		_indices[idx]._files[loop] = get_file(stuff);
		ptr += strlen(ptr) + 1;
	}
	pthread_mutex_unlock(&(_indices[idx]._mutex));

done:
	*ret = f_buff;
	i = sprintf(cmd, "%d|%d|%d|%d|", size, idx,
					_indices[idx]._keylen, _indices[idx]._f_cnt);
	return(i);

err:
	close(chan);
	if (f_buff != NULL)
		free(f_buff);
	if (_indices[idx]._files != NULL)
		free(_indices[idx]._files);
	memset((char *)(_indices+idx), '\0', sizeof(INDEX));
	pthread_mutex_unlock(&(_indices[idx]._mutex));
	return(EINITINDEX);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
