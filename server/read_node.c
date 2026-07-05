/* ***************************************************************
 *
 * PROCEDURE:	read_node.c
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
 * 				modified to run in the new server system.
 ************************************************************* */

/*
 * this routine reads in the specified node from the specified index
 * the arguments are:
 *		node	--> the node number to read
 *	   *curnode --> pointer to the node to return
 *		idxno   --> the index number to search
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
#include <stdio.h>
#include <inttypes.h>

#include "node.h"
#include "srv_index.h"
#include "errors.h"
#include "misc.h"

#define ROOT	1
#define LEAF	0200
#define TRUE	1

extern INDEX *_indices;
extern int dbgsw;

extern int64_t get_ll(char *);

int  read_node(int64_t node, NODE *curnode, int idxno)
{
	register short keylen;					/* index key length */

	register int chan;						/* index file descriptor */
	register int bytes;						/* bytes to read */
	register int tmp;

	int64_t seekpt;						/* point to seek to */

	char buff[NODESIZE];					/* read buffer */

	if (dbgsw) {
		fprintf(stderr, "enter read_node, node = %"PRId64", idxno = %d\n", node, idxno);
		fflush(stderr);
	}
	chan = _indices[idxno]._idxchan;		/* get channel */
	keylen = _indices[idxno]._keylen;		/* get keylen */

	bytes = N_KEYS * (MISC_LEN + keylen)	/* length of keys + overhead */
		  + N_KIDS * PTR_LENGTH				/* length of pointers to child nodes */
		  + MISC_LEN;						/* flag byte and parent pointer */

	if (node == ROOT)
		seekpt = _indices[idxno]._rootpos;
	else
		seekpt = node;
/*
 * at this point the index could have either an exclusive or
 * shared lock.  so, we need to make sure that no other thread
 * can reset the file pointer on the seek/read
 */
	pthread_mutex_lock(&(_indices[idxno]._mutex));
	llseek(chan, seekpt, SEEK_SET);			/* seek to node to read */
	tmp = read(chan,buff,bytes);			/* read in the node */
	pthread_mutex_unlock(&(_indices[idxno]._mutex));
	if (dbgsw) {
		fprintf(stderr, "in read_node, bytes to read = %d, bytes actually read = %d\n", bytes, tmp);
		fprintf(stderr, "*buff&LEAF = %d\n", *buff&LEAF);
		fflush(stderr);
	}
	if (tmp != bytes) {
		if(tmp == (bytes - N_KIDS*PTR_LENGTH) && (*buff & LEAF)) {
			;
		} else {
			return(ENODERD);
		}
	}

	curnode->_isleaf = *buff;					/* copy the leaf indicator */
	memcpy(curnode->_keys, buff+1, N_KEYS*(keylen+MISC_LEN));  /* copy the keys */
	bytes = 1 + N_KEYS * (keylen+MISC_LEN);					/* # of bytes into buffer */
	if (!(curnode->_isleaf & LEAF)) {
		for (tmp = 0;tmp < N_KIDS;tmp++)
			*(curnode->_kids+tmp) = get_ll(buff+bytes+PTR_LENGTH*tmp);
		bytes += N_KIDS*PTR_LENGTH;
	}
	curnode->_parent = get_ll(buff+bytes);	/* copy the parent position */
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
