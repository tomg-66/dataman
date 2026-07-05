/* ***************************************************************
 *
 * PROCEDURE:	split_node.c
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
 * 				Modified to use new arguments and concurrency
 * 				handling for server side inclusion
 ************************************************************* */

/*
 * the argument to this routine is the node being split.  it's purpose is to
 * split the current node, and, if necessary, call itself to split parents
 * up to and including the root node.
 *
 * the index already has an exclusive lock before enterance to this routine.
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
#include <stdarg.h>

#include "srv_index.h"
#include "node.h"
#include "errors.h"
#include "misc.h"

extern int node_search(char *, int, int64_t, int, SPLIT *, NODE *);
extern int get_node(int64_t, NODE *, SPLIT *);
extern void put_ll(char *, int64_t);

int split_node(int64_t node, NODE *cur_node, SPLIT *cur_index)

{
	int idx;						/* loop counter */
	int bytes;						/* the number of bytes to write */
	int leaf,root;					/* booleans */
	int i;

	int64_t offs;					/* misc usage offset */
	int64_t parent;					/* temp parent node */
	int64_t tnode;					/* temp node pos */

	char buff[NODESIZE];			/* output buffer */
	char tkey[64];					/* temporary key */

	memset(buff, '\0', sizeof(buff));		/* blank out the buffer */
	root = cur_index->_prntnode == 0;		/* is this the root node? */
	leaf = cur_index->_curleaf & LEAF;		/* save as a switch */

	*buff = leaf == 0 ?MIDDLE:MIDDLE|LEAF;		/* mask including LEAF and */
												/* half the keys for a node */

	tnode = llseek(cur_index->_idxchan, (loff_t)0, SEEK_END);	/* save for later use */
/*
 * this is the length of the node.  different if leaf
 */
	bytes = (cur_index->_keylen + MISC_LEN) * N_KEYS + MISC_LEN;
	if (!leaf)
		bytes += N_KIDS * PTR_LENGTH;

	if (root)
		cur_index->_rootpos = parent = bytes + tnode;
	else
		parent = cur_index->_prntnode;
/*
 * build a new node out of the first half of the current node
 * then write it out.
 */
	offs = MIDDLE * (cur_index->_keylen + MISC_LEN);		/* length of new stuff */
	memcpy(buff+1, cur_node->_keys, offs);					/* save the keys */
	offs = N_KEYS * (cur_index->_keylen + MISC_LEN) + 1;	/* offset into node */
	if (!leaf) {
		for (idx = 0; idx <= MIDDLE; idx++)
			put_ll(buff+offs+PTR_LENGTH*idx, *(cur_node->_kids+idx));
		offs += PTR_LENGTH * N_KIDS;
	}
	put_ll(buff+offs,parent);						/* save in the parent node */
	llseek(cur_index->_idxchan, node, SEEK_SET);		/* back up to current node */
	if(write(cur_index->_idxchan, buff, bytes) != bytes)
		return(ENODWRT);
/*
 * now build a new node from the second half of the current node
 * and write it out
 */
	offs = (MIDDLE+1) * (cur_index->_keylen+MISC_LEN);
	memcpy(buff+1, cur_node->_keys+offs, MIDDLE*(cur_index->_keylen+MISC_LEN));
	offs = N_KEYS * (cur_index->_keylen + MISC_LEN) + 1;	/* offset into node */
	if(!leaf)
		for (idx = 0;idx <= MIDDLE;idx++)
			put_ll(buff+offs+PTR_LENGTH*idx, *(cur_node->_kids+MIDDLE+1+idx));

	llseek(cur_index->_idxchan, (loff_t)0, SEEK_END);				/* get to the eof */
	if(write(cur_index->_idxchan, buff, bytes) != bytes)	/* write the node */
		return(ENODWRT);
/*
 * if the current node being split isn't a leaf, then the second half of
 * the children will need thier parent nodes updated to the new node.
 */
	if (!leaf) {
		llseek(cur_index->_idxchan, cur_node->_kids[0], SEEK_SET);
		if (read(cur_index->_idxchan,buff,1) != 1)
			return(ENODERD);
		offs = N_KEYS * (cur_index->_keylen + MISC_LEN) + 1;
		if (!(*buff & LEAF))
			offs += N_KIDS * PTR_LENGTH;
		put_ll(buff, tnode);
		for(idx = MIDDLE+1; idx <= N_KIDS; idx++) {
			llseek(cur_index->_idxchan, cur_node->_kids[idx]+offs, SEEK_SET);
			if (write(cur_index->_idxchan, buff, PTR_LENGTH) != PTR_LENGTH)
				return(ENODWRT);
		}
	}       

	offs = MIDDLE * (cur_index->_keylen + MISC_LEN);
	memset(tkey, '\0', sizeof(tkey));
	memcpy(tkey, (cur_node->_keys)+offs, cur_index->_keylen+MISC_LEN);

	if (root) {
		memset(buff, '\0', sizeof(buff));	/* blank out buffer */
		*buff = 1;							/* new roots contain only one key */
		memcpy(buff+1, tkey, cur_index->_keylen+MISC_LEN);		/* put the key in the new node */
		offs = N_KEYS * (cur_index->_keylen + MISC_LEN) + 1;
		put_ll(buff+offs, node);
		put_ll(buff+offs+PTR_LENGTH, tnode);
		offs += (N_KIDS+1) * PTR_LENGTH;
		llseek(cur_index->_idxchan, (loff_t)0, SEEK_END);
		if(write(cur_index->_idxchan, buff, offs) != offs)
			return(ENODWRT);
		llseek(cur_index->_idxchan, INDEX_HEADER_LENGTH, SEEK_SET);
		put_ll(buff, parent);
		if(write(cur_index->_idxchan, buff, PTR_LENGTH) != PTR_LENGTH)    /* write out info */
			return(ENODWRT);
	} else {
		if ((i = get_node(cur_index->_prntnode, cur_node, cur_index)) < 0)
			return(i);
		idx = node_search(tkey, 0, 0ll, 1, cur_index, cur_node);
		for(offs = cur_node->_isleaf&~LEAF; offs > idx; offs--)
			*(cur_node->_kids+offs+1) = *(cur_node->_kids+offs);
		*(cur_node->_kids+offs+1) = tnode;

		if ((cur_node->_isleaf & ~LEAF) == N_KEYS)
			split_node(cur_index->_curnode, cur_node, cur_index);
		else {
			memset(buff, '\0', sizeof(buff));
			*buff = ++cur_node->_isleaf;                 /* save misc info */
			offs = N_KEYS * (cur_index->_keylen + MISC_LEN);
			memcpy(buff+1,cur_node->_keys,offs);
			offs++;
			for (idx = 0; idx < N_KIDS; idx++)
				put_ll(buff+offs+PTR_LENGTH*idx, *(cur_node->_kids+idx));
			offs += N_KIDS * PTR_LENGTH;
			llseek(cur_index->_idxchan, cur_index->_curnode, SEEK_SET);
			if(write(cur_index->_idxchan, buff, offs) != offs)
				return(ENODWRT);
		}
    }
	return(1);			/* yea!!!!! it worked!!!!!!!!!!! */
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
