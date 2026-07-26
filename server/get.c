/* ***************************************************************
 *
 * PROCEDURE:	get
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
 *				Feb 26 2002
 *				modified to include file locking and other stuff
 *				as needed for making it a server routine.
 *
 *				Tue Jan 18 18:36:41 MST 2005
 *				modified how fl_lock is used for the new file
 *				locking model.
 *				tomg
 ************************************************************* */
/*
 * this routine will retreive from the named index the key passed
 * the calling sequence (using the #define) is:
 *		get(index_name,key)
 * the internal call is:
 *		if g_key(index_name,key) ;
 * where index_name is an index that was previously opened with
 * a call to iopen.  if the key is found the key is read into the
 * index structure, and the master file record is read into memory.
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
#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

#include "node.h"
#include "srv_index.h"
#include "lock.h"
#include "errors.h"
#include "misc.h"

#define TRUE	1
#define FALSE	0
#define ROOT	1
#define LEAF	0200					/* mask to identify leaf nodes */

extern void rm_key(int, int, char *);
extern int read_node(int64_t, NODE *, int);
extern int match(char *,char *, int);
extern int upd_idx(INDEX *,NODE *, int, int64_t, int64_t, char *, char **);
extern char *substr(char *,int,int);

extern int dbgsw;
extern int idx_cnt;

extern INDEX *_indices;

int get(char *cmd, int c_off, char **ret)
{

	NODE curnode;	/* the current working node */
	INDEX *index;	/* the current index description */

	int tmp;				/* temporary */
	int cnt;				/* loop counter */
	int stuff;				/* returned from match */
	int offs;
	int len;				/* internal length of key */
	int i;
	int template_len;

	int idxno;				/* the index number */

	int64_t node;		/* which node to get */
	int64_t tnode;

	char *key;
	char *tkey;				/* pointer to test key */
	char *rptr;				/* return pointer from upd_idx */
	char bsw;				/* boolean switch */

	short keylen;			/* the index keylen */

	if (dbgsw) {
		fprintf(stderr, "enter GET, cmd = %s\n", cmd);
		fflush(stderr);
	}
	rptr = NULL;
	*ret = NULL;

	idxno = atoi(cmd+c_off);
	if (idxno < 0 | idxno >= idx_cnt)
		return(EINVMSG);
	if ((key = strchr(cmd+c_off, '|') + 1) == (char *)1)
		return(EINVMSG);
	if ((tkey = strchr(key, '|')) == NULL)
		return(EINVMSG);
	template_len = tkey-key;
	*tkey = '\0';

	if (dbgsw) {
		fprintf(stderr, "in GET, idxno = %d\n", idxno);
		fflush(stderr);
	}

	if (!_indices)
		return(ENOINDEX);

	if ((index = _indices+idxno) == NULL)					/* find the index number */
		return(ENOINDEX);
	if (dbgsw) {
		fprintf(stderr, "idx->_keylen = %d\n"
						"idx->_idxchan = %d\n"
						"idx->_f_cnt = %d\n"
						"idx->_refcnt = %d\n"
						"idx->_rootpos = %"PRId64"\n"
						"idx->_idxname = %s\n"
						"idx->_rootdir = %s\n",
						index->_keylen, index->_idxchan, index->_f_cnt,
						index->_refcnt, index->_rootpos, index->_idxname,
						index->_rootdir);
		fflush(stderr);
	}

	if (!index->_refcnt)
		return(EIDXNOO);
	keylen = index->_keylen;				/* the current key length */

	fl_lock(&index->_lock, LOCK_SH);

	while (1) {								/* do until we get a key */
		node = index->_rootpos;				/* first get the root node */
		bsw = FALSE;						/* haven't found anything yet */
		len = keylen + MISC_LEN;			/* the internal length of the key */

		while (1) {							/* loop until found or leaf */
			if (dbgsw) {
				fprintf(stderr, "getting node %"PRId64"\n", node);
				fflush(stderr);
			}
			if ((stuff = read_node(node,&curnode,idxno)) < 0) {
				i = stuff;
				goto done;
			}
			if (dbgsw) {
				fprintf(stderr, "read node %"PRId64"\n", node);
				fflush(stderr);
			}
			for (cnt = 0;cnt < N_KEYS;cnt++) {
				tmp = cnt * len;			/* save one calculation */
				if (*(curnode._keys+tmp) == '\0')
					break;
				if (dbgsw) {
					tkey = substr(curnode._keys,tmp,tmp+keylen-1);
					fprintf(stderr, "testing key ->%s<-\n", tkey);
					fflush(stderr);
					free(tkey);
				}
				if ((stuff = match(curnode._keys+tmp,key,template_len)) <= 0) {	/* found that sucker? */
					if (stuff == 0) {
						bsw = TRUE;				/* yup! */
						tnode = node;
						offs = cnt;
					}
					break;
				}
			}
			if (curnode._isleaf & LEAF)
				break;							/* we're at a leaf */
			if (dbgsw) {
				fprintf(stderr, "count = %d, node = %"PRId64"\n", cnt, curnode._kids[cnt]);
				fflush(stderr);
			}
			node = curnode._kids[cnt];			/* pointer to next node to read */
		}
		if (!bsw) {
			if (dbgsw) {
				fprintf(stderr, "bsw is false\n");
				fflush(stderr);
			}
			*ret = NULL;
			i = FALSE;						/* didn't find anything */
			goto done;
		}

		if (dbgsw) {
			fprintf(stderr, "found key %s\n", key);
			fflush(stderr);
		}

		if (tnode != node) {
			read_node(tnode,&curnode,idxno);	/* get the found node */
			cnt = offs;							/* the offset */
			node = tnode;						/* the node */
		}
		if (!(i = upd_idx(index,&curnode,cnt,node, 0, cmd, &rptr))) {
			tkey = substr(curnode._keys,(int)cnt*len,(int)((cnt+1)*len-1));
			fl_lock(&index->_lock, LOCK_UN);
			rm_key(idxno, NOXACT, tkey);
			fl_lock(&index->_lock, LOCK_SH);
			free(tkey);
			continue;
		}
done:
		*ret = rptr;
		fl_lock(&index->_lock, LOCK_UN);
		return(i);								/* return value */
	}
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker ft=c:
 */
