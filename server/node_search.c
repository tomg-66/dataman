/* ***************************************************************
 *
 * PROCEDURE:	node_search.c
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
 * 				modified to run in the server side system.
 *
 *				Thu Mar 21 16:07:37 MDT 2013
 *				tomg
 *				changed how match strings with no wild cards.  it
 *				runs -much- faster now.
 ************************************************************* */

/*
 * this routine searches the current key buffer and finds where the key
 * fits into it, inserts it into the buffer, and returns the position in the
 * node where the key went.
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

#include <malloc.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>		/* byteorder functions */

#include "srv_index.h"
#include "node.h"
#include "misc.h"

extern void put_ll(char *, int64_t);

int node_search(char *key, char fno, int64_t rptr, int do_it, SPLIT *cur_index, NODE *cur_node)

{
	int idx,inc,max;								/* misc usage vars */
	int diff;
	int size;

	uint16_t compare16_value;
	uint32_t compare32_value;

	char kbuff[64];									/* key to actually insert */
	char *ikey;
	char buff[N_KIDS*(MAX_KEY_SIZE + MISC_LEN)];	/* temporary buffer */

	max = N_KEYS * (cur_index->_keylen + MISC_LEN);	/* max buff size */
	memset(buff,'\0',max);							/* make sure buffer is empty */
	inc = cur_index->_keylen + MISC_LEN;			/* distance to next key */
	if (fno != 0 || rptr != 0) {
		memcpy(kbuff,key,cur_index->_keylen);				/* save the key */
		*(kbuff+cur_index->_keylen) = fno + 1;				/* the file number */
		put_ll(kbuff+cur_index->_keylen+1,rptr);
		ikey = kbuff;
	} else
		ikey = key;


	if (cur_index->_keylen < 4)
		compare16_value = htons(*(uint16_t *)ikey);
	else
		compare32_value = htonl(*(uint32_t *)ikey);

	for (idx = 0; idx < max;idx += inc) {
		if (*(cur_node->_keys+idx)=='\0') {
			if (do_it)
				memcpy(cur_node->_keys+idx, ikey, inc);
			return(idx/inc);
		}
/*
 * we use byteorder functions here because on little endian it is -very- efficient
 * assembly code and on big endian it's a no op. most compares will be different
 * in the first 4 chars, so this will speed up the search a lot.
 */
		switch (cur_index->_keylen) {
			case 1:
				diff = *ikey - *(cur_node->_keys+idx);
				size = sizeof(char);
				break;
			case 2:
				diff = compare16_value - htons(*(uint16_t *)(cur_node->_keys+idx));
				size = sizeof(uint16_t);
				break;
			case 3:
				diff = compare16_value - htons(*(uint16_t *)(cur_node->_keys+idx));
				if (!diff)
					diff = *(ikey+sizeof(int16_t)) - *(cur_node->_keys+idx+sizeof(int16_t));
				size = sizeof(uint16_t)+sizeof(char);
				break;
			default:
				diff = compare32_value - htonl(*(uint32_t *)(cur_node->_keys+idx));
				size = sizeof(uint32_t);
				break;
		}
		if (!diff)
			diff = memcmp(ikey+size, cur_node->_keys+idx+size, inc-size);
		if (diff < 0) {
			if (do_it) {
				memmove(cur_node->_keys+idx+inc, cur_node->_keys+idx, max-idx);
				memcpy(cur_node->_keys+idx, ikey, inc);
			}
			return(idx/inc);
		}
	}
	if (do_it)
		memcpy(cur_node->_keys+max,ikey,inc);
	return(N_KEYS);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
