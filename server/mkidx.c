/* ***************************************************************
 *
 * PROCEDURE:	mkidx.c
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
 * 				tomg
 * 				made the appropriate changes for making this a
 * 				server side routine.
 *
 *				Tue Jan 18 18:36:41 MST 2005
 *				modified how fl_lock is used for the new file
 *				locking model.  along with changing how the locking
 *				model works, it also made it necessary to change
 *				how files are opened and referred to.  this makes
 *				for more efficient usage of resources.
 *				tomg
 *
 *				Mon Jul 27 08:21:37 PM MDT 2026
 *				tomg
 *				update to use the V2 index model.  This also sets
 *				a flag to make sure no file edit routine can call
 *				iopen on and index that is being built
 *
 ************************************************************* */

/*
   this routine is the header of any sort program.  It creates the index
   file, writes the index header information to the file, creates the
   first empty node, opens the first work file named, and reads in the
   first record of the work file.  the routine 'sort' is used to place keys
   into the index in a sort prog, the routine 'include' puts keys into
   the index in file edit routines.  the number of keys and kids in the
   index is defined in the index_v2.h header file.

   the index header description is to be found in index_v2.h

   we are -not- worried about concurrency so much in here, because
   we are creating a new index.  if anyone is running an app that
   accesses the index or the work files while we are building, well,
   that's their problem
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
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <inttypes.h>

#include <pthread.h>

#include "srv_index.h"			/* index description */
#include "errors.h"
#include "misc.h"
#include "index_v2.h"

#define LEAF	0200			/* bit mask for the leaf node */
#define PMODE   0666			/* default file creation mode */
#define MAX_CONNS	256

extern INDEX *_indices;			/* currently open indices */
extern FILES *_wfiles[];			/* work file entries */

extern pthread_mutex_t i_mutex;
extern pthread_mutex_t w_mutex;

extern int idx_cnt;
extern int dbgsw;

extern char *substr(char *,int, int);

extern void put_short(char *, short int);
extern short get_short(char *);

extern void put_ll(void *, int64_t);
extern int64_t get_ll(void *);

extern FILES *get_file(char *);

extern int get_blobs (FILES *, int, int64_t, char **, int *);
extern int get_datafile_desc(FILES *);
extern void rm_file(char *);

int mkidx(char *cmd, int c_off, char **ret)
{
	int i;						/* general usage */
	int idxno;					/* index number returned */
	int fileno;					/* work file number returned */
	int fmt;					/* record format number for rec */
	int ixchan;
	int w_chan;
	int w_longest;
	int tmp;

	short fhead;                /* length of file header */
	short maxfil;				/* number of files in index */
	short keylen;				/* max key length */

	int64_t rootpos;
	int64_t w_cur;			/* current work record */
	int64_t w_next;			/* next work record */
	uint64_t generation;
	uint64_t v2_root;

	char string[256];			/* misc string */
	char ixname[512];			/* pathname to index */
	char filename[512];			/* pathanme to work file to open */
	char root[512];				/* root directory */
	char **fnames;			/* the names of the files in index */

	char *buff;					/* output buffer */
	char *ptr;
	char *rptr;					/* pointer to hold data rec on return */

	INDEX *tptr;
	FILES *fptr;

	keylen = atoi(cmd+c_off);		/* max key length for index */
	if (keylen == 0)
		keylen = 20;				/* if not set, default to 20 */

	if ((ptr = strchr(cmd+c_off, '|') + 1) == (char *)1)
		return(EINVMSG);
	if ((buff = strchr(ptr, '|')) == NULL)
		return(EINVMSG);
	*buff = '\0';
	strcpy(ixname, ptr);			/* save the index name */

	ptr = buff+1;
	if ((buff = strchr(ptr, '|')) == NULL)
		return(EINVMSG);
	*buff = '\0';
	strcpy(root, ptr);				/* save the root directory */

	ptr = buff+1;
	maxfil = atoi(ptr);					/* the number of files in the index */
	if ((ptr = strchr(ptr, '|') + 1) == (char *)1)
		return(EINVMSG);
	w_chan = -1;
/*
 * make sure that these are initialized
 */
	rptr = NULL;
	*ret = NULL;
/*
 * save file names for inclusion in the index later
 */
	if (dbgsw) {
		fprintf(stderr, "received %d file names\n", maxfil);
		fflush(stderr);
	}
	if ((fnames = (char **)calloc(maxfil, sizeof(char *))) == NULL)
		return(ENOALLOC);
	tmp = 0;
	for (i = 0; i < maxfil; i++) {
		if ((buff = strchr(ptr, '|')) == NULL)
			return(EINVMSG);
		*buff = '\0';
		fnames[i] = strdup(ptr);
		ptr = buff+1;
		tmp += strlen(fnames[i])+1;
	}

	if (dbgsw) {
		for (i = 0; i < maxfil; i++)
			fprintf(stderr, "file %d named %s\n", i, fnames[i]);
		fflush(stderr);
	}
/*
 * this hasn't been allocated yet?
 */
	pthread_mutex_lock(&i_mutex);
	if (idx_cnt == 0 && _indices == NULL) {
		if ((_indices = (INDEX *)calloc(8, sizeof(INDEX))) == NULL) {
			pthread_mutex_unlock(&i_mutex);
			return(EINITINDEX);
		}
		idx_cnt = 8;
		for (i = 0; i < 8; i++)
			pthread_mutex_init(&_indices[i]._mutex, NULL);
	}
	pthread_mutex_unlock(&i_mutex);
	sprintf(string, "%s/index/%s", root, ixname);

retry:
	idxno = -1;
	for (i = 0; i < idx_cnt; i++) {
		pthread_mutex_lock(&_indices[i]._mutex);
		if (!_indices[i]._idxname) {
			if (idxno == -1)
				idxno = i;
			else
				pthread_mutex_unlock(&_indices[i]._mutex);
			continue;
		}
		if (!strcmp(string, _indices[i]._idxname)) {
			if (!_indices[i]._refcnt) {
				idxno = i;
				break;
			}
			if (idxno != -1)
				pthread_mutex_unlock(&_indices[idxno]._mutex);
			pthread_mutex_unlock(&_indices[i]._mutex);
			return(EIDXOPN);
		}
		pthread_mutex_unlock(&_indices[i]._mutex);
	}
/*
 * ok, if we get to here, the index isn't already open.  if idxno is still
 * -1, then there wasn't any room in the currently allocated array to
 *  store an index and we have to increase the size of that array.
 */
	if (idxno == -1) {
		if (pthread_mutex_trylock(&i_mutex) < 0)
			goto retry;
		idxno = idx_cnt;
		i = idx_cnt * 2 * sizeof(INDEX);
		if ((tptr = realloc(_indices, i)) == NULL) {
			pthread_mutex_unlock(&i_mutex);
			return(ENOIXSP);
		}
		memset((char *)(tptr+idx_cnt), '\0', idx_cnt * sizeof(INDEX));
		_indices = tptr;
		idx_cnt *= 2;
		for (i = idxno; i < idx_cnt; i ++) {
			pthread_mutex_init(&_indices[i]._mutex, NULL);
			if (i == idxno)
				pthread_mutex_lock(&_indices[i]._mutex);
		}
		pthread_mutex_unlock(&i_mutex);
	}

    i = O_CREAT | O_RDWR | O_TRUNC|O_LARGEFILE;
	if (dbgsw) {
		fprintf(stderr, "index to open is %s\n", string);
		fflush(stderr);
	}
    if ((ixchan = open(string, i, 0666)) < 0) {
		pthread_mutex_unlock(&_indices[idxno]._mutex);
        return(EIDXCREAT);
	}
	strcpy(ixname, string);			/* save the built name */

	if (!index_v2_build_begin(ixchan, keylen, maxfil,
			(const char *const *)fnames, &v2_root)) {
		i = EHDRWRT;
		goto err;
	}
	generation = 0;
	rootpos = (int64_t)v2_root;
/*
 * now the index file is created and has a null node residing therein.  Now,
 * the first file designated must be opened and the contents of the first
 * file read into memory
 */
	sprintf(filename, "%s/files/%s", root, *fnames);
	pthread_mutex_lock(&w_mutex);
	for (fileno = 0; fileno < MAX_CONNS; fileno++)
		if (_wfiles[fileno] == NULL)
			break;
	if (fileno == MAX_CONNS) {
		pthread_mutex_unlock(&w_mutex);
		i = ENOWSP;
		goto err;
	}
	fptr = _wfiles[fileno] = get_file(filename);
	pthread_mutex_unlock(&w_mutex);
/*
 * open the work file and start getting the header info, then read the
 * very first record in the file.
 */
	pthread_mutex_lock(&fptr->_mutex);
	if (fptr->_desc == NULL) {
		if ((i = get_datafile_desc(fptr)) < 0) {
			pthread_mutex_unlock(&fptr->_mutex);
			rm_file(filename);
			goto err;
		}
	} else {
		w_cur = fptr->_hlen + sizeof(short);
		llseek(fptr->_chan, w_cur, SEEK_SET);
	}

	if (read(fptr->_chan, string, PTR_LENGTH) != PTR_LENGTH) {
		rm_file(filename);
		i = EFHDRD;
		goto err;
	}
	pthread_mutex_unlock(&fptr->_mutex);
    w_cur = get_ll(string);
    w_longest = fptr->_longest;
/*
 * get the record header, find the record format number and length
 * of this record.  allocate space for it, and read it
 */
	pthread_mutex_lock(&fptr->_mutex);
	llseek(fptr->_chan, w_cur, SEEK_SET);
	if (read(fptr->_chan, string, DATARECORD_HEADER_LENGTH) != DATARECORD_HEADER_LENGTH) {
		pthread_mutex_unlock(&fptr->_mutex);
		rm_file(filename);
		i = ERHREAD;
		goto err;
	}
	pthread_mutex_unlock(&fptr->_mutex);
	w_next = get_ll(string+OFFSET_TO_NEXT);

	fmt = *string & 077;
	tmp = fptr->_filedesc->record_desc[fmt-1].rf_len;

	if ((rptr = malloc(tmp)) == NULL) {
		pthread_mutex_unlock(&fptr->_mutex);
		rm_file(filename);
		i = ENOALLOC;
		goto err;
	}
	if (read(fptr->_chan, rptr, tmp) != tmp) {
		pthread_mutex_unlock(&fptr->_mutex);
		rm_file(filename);
		i = ERECREAD;
		goto err;
	}
	pthread_mutex_unlock(&fptr->_mutex);
	if (fptr->_filedesc->record_desc[fmt-1].has_blob) {
		if ((tmp = get_blobs(fptr, fmt, w_cur, &rptr, NULL)) < 0) {
			free(rptr);
			rptr = NULL;
			i = tmp;
			goto err;
		}
	}
/*
 * put together the index and file structure for this stuff.
 */
	tptr = _indices+idxno;
	tptr->_keylen = keylen;
	tptr->_idxchan = ixchan;
	tptr->_f_cnt = maxfil;
	tptr->_refcnt = 1;
	tptr->_idxname = strdup(ixname);
	tptr->_rootdir = strdup(root);
	tptr->_rootpos = rootpos;
	tptr->_generation = generation;
/*
 * put together the return buffer:
 */
	pthread_mutex_unlock(&_indices[idxno]._mutex);

	i = sprintf(cmd, "%d|%d|%" PRId64 "|%d|%d|%hd|%" PRId64 "|%d|%" PRId64 "|",
					tmp, idxno, rootpos, fileno + 1,
					w_longest, fhead, w_cur, fmt,
					w_next);

	*ret = rptr;
/*
 * free up the allocated file names
 */
	for (tmp = 0; tmp < maxfil; tmp++)
		free(fnames[tmp]);
	free(fnames);
	return(i);

err:
	pthread_mutex_unlock(&_indices[idxno]._mutex);
	_wfiles[fileno] = NULL;
	close(ixchan);
	unlink(ixname);
	for (tmp = 0; tmp < maxfil; tmp++)
		free(fnames[tmp]);
	free(fnames);
	return(i);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
