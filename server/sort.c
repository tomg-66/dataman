/* ***************************************************************
 *
 * PROCEDURE:	sort.c
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
 * 				made modifications necessary for being handled
 * 				by the server side... no file locking, since
 * 				when sorting, no one else should have the index.
 ************************************************************* */

/*
 * this is the sort routine,  it takes as its argument the key to be sorted
 * into the index created by mkidx,  it's only function is to insert a key
 * pointing to the current work file record into the current index.  as a rule
 * it should only be used when creating new indexes.  at the very first of
 * the process the root node is a leaf.
 *
 * we don't have to worry about locking and concurrency in this routine
 * because we are creating the index.  no one else can have it open.
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
#include <unistd.h>
#include <stdlib.h>

#include "srv_index.h"
#include "node.h"
#include "misc.h"
#include "errors.h"

extern int idx_cnt;

extern INDEX *_indices;

#define LEAF    0200                            /* leaf node bit mask */

extern int node_search(char *, int, int64_t,int, SPLIT *, NODE *);
extern int get_node(int64_t, NODE *, SPLIT *);
extern int split_node(int64_t, NODE *, SPLIT *);

int sort(char *cmd, int c_off, char **ret)

{
    int nxt,bytes;							/* misc usage */
	int ixno;
	int fileno;
	int i;

	int64_t w_cur;						/* pointer to curr work rec */

	char *key;
	char *cptr;
	char buff[NODESIZE];

	SPLIT cur_index;						/* current working index */
	NODE cur_node;							/* the last accessed node */

	*ret = NULL;
	if ((cptr = strrchr(cmd+c_off, '|')) == NULL)
		return(EINVMSG);
	*cptr = '\0';
	ixno = atoi(cmd+c_off);
	if (ixno < 0 || ixno >= idx_cnt)
		return(EINVMSG);
	if ((cptr = strchr(cmd+c_off, '|') + 1) == (char *)1)
		return(EINVMSG);
	fileno = atoi(cptr);
	if ((cptr = strchr(cptr, '|') + 1) == (char *)1)
		return(EINVMSG);
	w_cur = strtoll(cptr, NULL, 0);
	if ((key = strchr(cptr, '|') + 1) == (char *)1)
		return(EINVMSG);

	cur_index._keylen = _indices[ixno]._keylen;
	cur_index._idxchan = _indices[ixno]._idxchan;
	cur_index._idxno = ixno;
	cur_index._prntnode = 0;
    cur_index._curnode = _indices[ixno]._rootpos;		/* start at the top */
	cur_index._curleaf = 0;
    cur_index._rootpos = _indices[ixno]._rootpos;		/* start at the top */

    while (1) {                                 /* go until a leaf */
        if ((i = get_node(cur_index._curnode, &cur_node, &cur_index)) < 0)
			return(i);
        nxt = node_search(key,fileno,w_cur,cur_node._isleaf & LEAF,
						&cur_index, &cur_node);
        if (cur_node._isleaf & LEAF)
            break;                              /* found the right leaf */
        cur_index._curnode = cur_node._kids[nxt]; /* go to proper kid */
    }

    if ((cur_node._isleaf & ~LEAF) == N_KEYS) {
        if ((i = split_node(cur_index._curnode, &cur_node, &cur_index)) < 0)
			return(i);
		_indices[ixno]._rootpos = cur_index._rootpos;	/* may have changed */
	} else {
		cur_node._isleaf++;
		memset(buff, '\0', NODESIZE);
		bytes = N_KEYS * (cur_index._keylen + MISC_LEN);
		*buff = cur_node._isleaf;
		memcpy(buff+1, cur_node._keys, bytes);
		bytes += 1;
		llseek(cur_index._idxchan, cur_index._curnode, SEEK_SET);
		if (write(cur_index._idxchan, buff, bytes) < bytes)
			return(ENODWRT);
	}
	strcpy(cmd, "0|1|");
	return(4);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
