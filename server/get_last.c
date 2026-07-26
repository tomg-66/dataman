/* ***************************************************************
 *
 * PROCEDURE:	get_last.c
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
 * 				March 2002
 * 				Tom Green
 * 				changes and modiciactions for making it a
 * 				server side function.
 *
 *				Tue Jan 18 18:36:41 MST 2005
 *				modified how fl_lock is used for the new file
 *				locking model.
 *				tomg
 ************************************************************* */

/*
 * this procedure gets the last key from an index.  The calling sequence
 * (using the define is:
 *	get_last(idx_name) else ...
 * or, using the define:
 *	if (g_last(idx_name)) ; else ...
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
#include <stdlib.h>

#include "srv_index.h"
#include "node.h"
#include "lock.h"
#include "errors.h"
#include "misc.h"

#define TRUE    1
#define FALSE   0
#define LEAF    0200                    /* leaf indicator mask */
#define ROOT	1

extern int idx_cnt;

extern INDEX *_indices;			

extern void rm_key(int, int, char *);
extern int read_node(int64_t, NODE *, int);
extern int upd_idx(INDEX *,NODE *,int,int64_t, int64_t, char *, char **);
extern char *substr(char *,int,int);

int get_last(char *cmd, int c_off, char **ret)
{
	int tmp;							/* temporary storage */
	int idxno;

	int64_t node;					/* node to search for */

	char *tkey;
	char *rptr;

	INDEX *idx;							/* the working index */
	NODE cur_node;						/* last accessed node */

	rptr = NULL;
	*ret = NULL;

	idxno = atoi(cmd+c_off);			/* get the index */
	if (idxno < 0 || idxno >= idx_cnt)
		return(EINVMSG);
	if ((idx = _indices+idxno) == NULL)
		return(ENOINDEX);
	if (!idx->_refcnt)
		return(EIDXNOO);

	fl_lock(&idx->_lock, LOCK_SH);
	while (1) {
		node = ROOT;									/* root pos */
		while (1) {
			if ((tmp = read_node(node,&cur_node,idxno)) < 0) {
				goto done;
			}
			if (cur_node._isleaf & LEAF)
				break;									/* at a leaf */
			node = cur_node._kids[cur_node._isleaf];	/* next kid to get */
		}
		if (*(cur_node._keys) == '\0') {
			tmp = FALSE;
			goto done;
		}

		tmp = (cur_node._isleaf & ~LEAF) - 1;			/* last key offset */
		if ((tmp = upd_idx(idx,&cur_node,tmp,node,0,cmd,&rptr)))
			break;										/* worked */
		tmp = (tmp - 1) * (idx->_keylen + MISC_LEN);	/* offset to key */

		tkey = substr(cur_node._keys,tmp,tmp+idx->_keylen+PTR_LENGTH); /* get key */
		fl_lock(&idx->_lock, LOCK_UN);
		rm_key(idxno, NOXACT, tkey);					/* remove key */
		fl_lock(&idx->_lock, LOCK_SH);
		free(tkey);										/* free temp space */

	}
done:
	fl_lock(&idx->_lock, LOCK_UN);
	*ret = rptr;
	return(tmp);										/* evrything worked */
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker ft=c:
 */
