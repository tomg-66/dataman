/* ***************************************************************
 *
 * PROCEDURE:	get_first.c
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
 *				Tom Green
 *				Sun Mar 17 2002
 *				modified to include file locking and other stuff
 *				as needed for making it a server routine.
 *
 *				Tue Jan 18 18:36:41 MST 2005
 *				modified how fl_lock is used for the new file
 *				locking model.
 *				tomg
 ************************************************************* */

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
#include <stdlib.h>
#include <malloc.h>

#include "srv_index.h"
#include "node.h"
#include "lock.h"
#include "errors.h"
#include "misc.h"

#define TRUE    1
#define FALSE   0
#define LEAF    0200
#define ROOT	1

extern int idx_cnt;

extern INDEX *_indices;

extern void rm_key(int, int, char *);
extern int read_node(int64_t, NODE *, int);
extern int upd_idx(INDEX *, NODE *, int, int64_t, int64_t, char *, char **);
extern char *substr(char *, int, int);

int get_first(char *cmd, int c_off, char **ret)
{
	int tmp;					/* misc usage */
	int idxno;

	int64_t node;				/* node to search */

	char *tkey;					/* temporary key */
	char *rptr;

	INDEX *idx;					/* the current index */
	NODE cur_node;				/* the last accessed node */

	*ret = NULL;
	idxno = atoi(cmd+c_off);	/* get the current index */
	if (idxno < 0 || idxno >= idx_cnt)
		return(EINVMSG);
	if ((idx = _indices+idxno) == NULL)		/* this index */
		return(ENOINDEX);
	if (!idx->_refcnt)
		return(EIDXNOO);

	rptr = NULL;
	fl_lock(&idx->_lock, LOCK_SH);
	while(1) {
		node = ROOT;							/* pointer to root pos */
		while (1) {								/* go until leaf found */
			if ((tmp = read_node(node,&cur_node,idxno)) < 0) {
				goto done;
			}
			if (cur_node._isleaf & LEAF)		/* at a leaf? */
				break;							/* quit loop */
			node = cur_node._kids[0];			/* next kid to check */
		}
		if (*(cur_node._keys) == '\0') {
			tmp = FALSE;
			goto done;
		}

		if ((tmp = upd_idx(idx,&cur_node,0,node, 0, cmd, &rptr)))
			break;
		tkey = substr(cur_node._keys,0,idx->_keylen+PTR_LENGTH);
		fl_lock(&idx->_lock, LOCK_UN);
		rm_key(idxno, NOXACT, tkey);
		fl_lock(&idx->_lock, LOCK_SH);
		free(tkey);
	}
done:
	fl_lock(&idx->_lock, LOCK_UN);
	*ret = rptr;
	return(tmp);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
