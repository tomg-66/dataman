/* ***************************************************************
 *
 * PROCEDURE:	collapse_node.c
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		legacy, originaly written in 1988
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATIONS:
 * 				March, 2002
 *
 *				Thu Aug 11 15:05:19 MDT 2005
 *				modified to handle defines and offsets for
 *				64 bit offsets
 *				tomg
 ************************************************************* */

/*
 * This procedure collapses nodes.  If necessary, it calls itself
 * recursively until all nodes on this branch (up to and including the root)
 * have been collapsed.
 *
 * the index has already been write-locked before enterence to this
 * routine, so we don't need to worry about mutexes around the seek/read,write
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
#include <stddef.h>
#include <unistd.h>

#include <sys/types.h>

#include "srv_index.h"
#include "node.h"
#include "errors.h"
#include "misc.h"

#define TRUE	1
#define FALSE	0

extern void put_ll(void *, int64_t);
extern int node_search(char *, int, int64_t, int, SPLIT *, NODE *);
extern int read_node(int64_t, NODE *, int);
extern int split_node(int64_t, NODE *, SPLIT *);

int collapse_node(int64_t num, NODE *cur_node, SPLIT *cur_index)

{
	NODE prnt_node;						/* the parent of the node */

	int cnt,i,offs,bytes;				/* misc usage */
	int kcount;							/* # of keys in the parent node */
	int len;							/* keylength */
	int idx;

	int64_t trec;						/* temporary record pointer */
	int64_t prnt;						/* position of parent node */

	char tfile;							/* temoprary file name offset */
	char tkey[KEY_BUFFER_SIZE];			/* temporary key */
	char stuff[NODESIZE];				/* output buffer */

	prnt = cur_node->_parent;							/* parent pos */
	len = cur_index->_keylen + KEY_HEADER_LENGTH;		/* internal len of key */
	memset((char *)&prnt_node, '\0', sizeof(NODE));		/* zero this out */
	read_node(prnt, &prnt_node, cur_index->_idxno);		/* read parent node */
	for(cnt = 0; cnt < N_KIDS; cnt++)					/* look at each kid */
		if (prnt_node._kids[cnt] == num)				/* found it? */
			break;										/* good */

	if (cnt > N_KEYS)							/* deep problems */
		return(ENOPARENT);

	kcount = prnt_node._isleaf & ~LEAF;			/* # of keys in node */
	i = cnt * len;								/* offs to key */
	memcpy(tkey, prnt_node._keys+i, len);		/* save the key */

	memset(stuff,'\0',NODESIZE);					/* blank the buffer */
	*stuff = --prnt_node._isleaf;				/* start building the output */
	memcpy(stuff+NODE_FLAG_LENGTH, prnt_node._keys, i);			/* two lines of keys */
	memcpy(stuff+NODE_FLAG_LENGTH+i, prnt_node._keys+len+i, (N_KEYS-1)*len-i);

	offs = N_KEYS * len + 1;
	for(idx = 0; idx < cnt; idx++) {
		put_ll(stuff+offs, prnt_node._kids[idx]);
		offs += sizeof(int64_t);
	}
//	offs += (sizeof(long) * cnt);

	for (idx=cnt+1; idx <= kcount; idx++) {
		put_ll(stuff+offs, prnt_node._kids[idx]);
		offs += sizeof(int64_t);
	}

	offs = N_KIDS * sizeof(int64_t) + len * N_KEYS + 1;
	put_ll(stuff+offs, prnt_node._parent);
	offs += sizeof(int64_t);

	llseek(cur_index->_idxchan, cur_node->_parent, SEEK_SET);		/* get to node */
	if (write(cur_index->_idxchan, stuff, offs) != offs)
		return(ENODWRT);

	num = (cnt == kcount)? prnt_node._kids[cnt-1] : prnt_node._kids[cnt+1];

	while (1) {
		read_node(num, cur_node, cur_index->_idxno);
		if (cur_node->_isleaf & LEAF)
			break;
		num = (cnt != kcount)? cur_node->_kids[0] :
					cur_node->_kids[(cur_node->_isleaf & ~LEAF) + 1];
	}

	i = node_search(tkey, 0, 0ll, 1, cur_index, cur_node);			/* insert the key */

	if ((cur_node->_isleaf & ~LEAF) == N_KEYS) {
		if ((i = split_node(num, cur_node, cur_index)) < 0)		/* this node is full, split */
			return(i);
		read_node(prnt, &prnt_node, cur_index->_idxno);			/* re read parent */
		kcount = prnt_node._isleaf & ~LEAF;						/* # of keys */
	} else {
		*stuff = ++cur_node->_isleaf;
		bytes = N_KEYS * len + 1;							/* first offset into buffer */
		memcpy(stuff+1, (char *)cur_node->_keys, bytes);	/* put keys into output buff */
		llseek(cur_index->_idxchan, num, SEEK_SET);			/* position on current node */
		if (write(cur_index->_idxchan, stuff, bytes) < bytes)
			return(ENODWRT);
	}

	if (!kcount) {
		if (!prnt_node._parent) {
			*cur_node = prnt_node;				/* parent node is now current */
			if ((i = collapse_node(prnt, cur_node, cur_index)) < 0)
				return(i);
		} else {
			llseek(cur_index->_idxchan,prnt_node._kids[0],1);	/* get to the new root */

			if (read(cur_index->_idxchan,&tfile,1) != 1) /* get first byte */
				return(ENODERD);

			trec = N_KEYS * (len + 5)+ (tfile & LEAF)? 0: N_KIDS * 4;
			llseek(cur_index->_idxchan,trec,1);   /* position ahead */
			trec = 0;                           /* what to write */

			if (write(cur_index->_idxchan,(char *)&trec,4) != 4)
				return(ENODWRT);

			put_ll(stuff, prnt_node._kids[0]);			/* what to write this time */
			cur_index->_rootpos = prnt_node._kids[0];	/* update pointer to root */
			llseek(cur_index->_idxchan, INDEX_HEADER_LENGTH, 0);			/* get to root offset */

			if (write(cur_index->_idxchan,stuff,sizeof(int64_t)) != sizeof(int64_t))
				return(ENODWRT);
		}
	}
	return(TRUE);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
