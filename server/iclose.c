/* ***************************************************************
 *
 * PROCEDURE:	iclose
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
 * 				Tom Green
 *				Feb 26 2002  modified to include changes for
 *				making a server routine.
 *
 *				Sun Jan 16 10:13:19 MST 2005
 *				changed calls from my_open to open because we
 *				changed how files are kept track of, mostly
 *				because of the new locking model
 *				tomg
 ************************************************************* */
/*
 * this procedure decrements the reference count for the given index,
 * then if the reference count has reached zero, closes files, frees
 * the space, closes the index, and cleans up the open index entry in
 * the array.
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
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>

#include <pthread.h>

#include "srv_index.h"
#include "errors.h"
#include "misc.h"

extern int idx_cnt;

extern INDEX *_indices;
extern FILES *_wfiles[];

extern pthread_mutex_t w_mutex;			/* work file mutex */
extern pthread_mutex_t i_mutex;			/* index mutex */

extern void rm_file(char *);

int iclose(char *cmd, int c_off, char **ret)
{
	int idx;			/* index into array of indices */
	int tmp;			/* misc usages */

	char *ptr;

	*ret = NULL;

	idx = atoi(cmd+c_off);

	if (idx < 0) {
		idx *= -1;
		if (idx > MAX_CONNS)
			return(EINVMSG);
		tmp = idx-1;
		if (_wfiles[tmp] != NULL) {
			pthread_mutex_lock(&w_mutex);
			rm_file(_wfiles[tmp]->_fname);
			_wfiles[tmp] = NULL;
			pthread_mutex_unlock(&w_mutex);
		}
		goto done;
	}

	if (idx >= idx_cnt)
		return(EINVMSG);
	if (!_indices[idx]._refcnt)
		return(EIDXNOO);

	pthread_mutex_lock(&(_indices[idx]._mutex));
	_indices[idx]._refcnt--;
	if (_indices[idx]._refcnt < 1) {
		pthread_mutex_lock(&i_mutex);
		if (_indices[idx]._files) {
			for(tmp = 0; tmp < _indices[idx]._f_cnt; tmp++) {
				rm_file(_indices[idx]._files[tmp]->_fname);
			}
			free(_indices[idx]._files);
			_indices[idx]._files = NULL;
		}
		close(_indices[idx]._idxchan);
		free(_indices[idx]._idxname);
		free(_indices[idx]._rootdir);
		pthread_mutex_destroy(&_indices[idx]._lock.mutex);
		pthread_cond_destroy(&_indices[idx]._lock.cond);
		pthread_mutex_unlock(&(_indices[idx]._mutex));
		memset((char *)(_indices+idx), '\0', sizeof(INDEX));
		pthread_mutex_unlock(&i_mutex);
	} else
		pthread_mutex_unlock(&(_indices[idx]._mutex));
done:
	tmp = sprintf(cmd, "%d|%d|", 0, idx);
	return(tmp);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
