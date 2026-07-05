/* ***************************************************************
 *
 * PROCEDURE:	get_node.c
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
 * 				made changes to accomidate changing to a server
 * 				routine.
 *
 *				Wed Aug  3 18:14:48 MDT 2005
 *				modified to use misc.h header for offsets in the
 *				change to 64 bit databases.
 *				tomg
 ************************************************************* */

/*
 * this routine takes as it's argument a position on an index file and
 * gets the node at that position.
 *
 * get_node is only called by routines that have the index already
 * exclusively locked, so there doesn't need to be a mutex around
 * the seek/read.
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
#include <stddef.h>

#include "srv_index.h"
#include "node.h"
#include "errors.h"
#include "misc.h"

extern int64_t get_ll(void *);

int get_node(int64_t node, NODE *cur_node, SPLIT *cur_index)

{

	int bytes;				/* misc usage */
	int tmp;
	int i;

	char buff[sizeof(NODE)];		/* read buffer */

	cur_index->_curnode = node;
	bytes = N_KEYS * (cur_index->_keylen + MISC_LEN) + (N_KIDS * PTR_LENGTH) + MISC_LEN;

	llseek(cur_index->_idxchan, node, SEEK_SET);           /* get to node */

	if ((tmp = read(cur_index->_idxchan, buff, bytes)) < bytes)
		if (tmp != bytes - (N_KIDS * PTR_LENGTH))
			return(ENODERD);
	memset((char *)cur_node, '\0', sizeof(NODE));
	cur_index->_curleaf = *buff;                 /* save misc info */
	cur_node->_isleaf = *buff;                   /* save misc info */

	bytes -= ((N_KIDS + 1) * PTR_LENGTH);
	memcpy(cur_node->_keys,buff+1,bytes);        /* save the keys */

	if (!(cur_index->_curleaf & LEAF)) {
		i = (cur_index->_curleaf & ~LEAF)+1;
		for (tmp = 0;tmp < i;tmp++)
			*(cur_node->_kids+tmp) = get_ll((void *)(buff+bytes+tmp*PTR_LENGTH));
		bytes += (N_KIDS * PTR_LENGTH);
	}
	cur_node->_parent = 0;
	cur_node->_parent = get_ll((void *)(buff+bytes));    /* save pointer to parent */
	cur_index->_prntnode = cur_node->_parent;
	return(1);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
