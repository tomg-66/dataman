/* ***************************************************************
 *
 * PROCEDURE:	clear.c
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		legacy, originally written in 1988
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATIONS:
 * 				March, 2002 tomg
 * 				modifications to switch to server.  This is the
 * 				server side, perform the clear.
 *
 *				Tue Jan 18 18:36:41 MST 2005
 *				modified how fl_lock is used for the new file
 *				locking model.
 *				tomg
 *
 *				Thu Aug 11 14:30:08 MDT 2005
 *				made changes to change to 64 bit server.
 *				tomg
 *
 *				Wed Dec 28 17:48:17 MST 2005
 *				getting rid of one of the last vestiges of the
 *				old (non server) system.  user level locks as
 *				set by protect changed a bit in the header for
 *				a record on disk.  now keep all protect info
 *				for a file in a hash table for that file. now,
 *				should the server crash, a protect isn't left
 *				asserted.
 *				tomg
 ************************************************************* */

/*
 * this procedure clears the protect bit set in the protect command.  the
 * calling sequence is:
 * clear(ix_name);
 * where ix_name is the index whose MFRP points to the protected record.  if
 * ix_name is a NULL the function clears the protect bit from the current
 * work file record.
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
#include <string.h>
#include <inttypes.h>

#include <sys/types.h>

#include "srv_index.h"
#include "lock.h"
#include "errors.h"
#include "misc.h"

#define CLEAR	0277			/* the bit mask to clear the protect */

extern int idx_cnt;

extern INDEX *_indices;
extern FILES *_wfiles[];

int clear(char *cmd, int c_off, char **ret)

{
	INDEX *idx;				/* pointer returned from findex() */
	FILES *fptr;
	LOCKS *lptr;
	LOCKS *prev;

	int ixno;
	int fno;
	int hash;
	int i;

	int64_t recno;		/* offset to record to clear */

	char *cptr;

	*ret = NULL;
	ixno = atoi(cmd+c_off);
	if (ixno < 0) {
		ixno *= -1;
		if (ixno > MAX_CONNS)
			return(EINVMSG);
		fptr = _wfiles[ixno-1];
		cptr = cmd+c_off;
		fno = 0;
	} else {
		if (ixno >= idx_cnt)
			return(EINVMSG);
		if ((idx = _indices+ixno) == NULL)
			return(EINVMSG);
		if (!idx->_refcnt)
			return(EIDXNOO);
		if ((cptr = strchr(cmd+c_off, '|') + 1) == (char *)1)
			return(EINVMSG);
		fno = atoi(cptr);
		if (fno < 0 || fno > idx->_f_cnt)
			return(EINVMSG);
		fptr = idx->_files[fno];
	}
	if (fptr == NULL)
		return(EINVMSG);
	if ((cptr = strchr(cptr, '|') + 1) == (char *)1)
		return(EINVMSG);
	recno = strtoll(cptr, NULL, 0);
	fl_lock(&fptr->_lock, LOCK_EX);			/* lock file for update */

	hash = recno % LOCK_TABLE_SIZE;
	if ((lptr = fptr->_locks[hash]) != NULL) {
		prev = NULL;
		while(lptr) {
			if (lptr->_recno == recno) {
				if (prev == NULL) {
					fptr->_locks[hash] = NULL;
					free(lptr);
				} else {
					prev->_next = lptr->_next;
					free(lptr);
				}
				break;
			}
			prev = lptr;
			lptr = lptr->_next;
		}
	}
	
	fl_lock(&fptr->_lock, LOCK_UN);			/* final unlock of file */
	i = sprintf(cmd, "0|%d|%d|%"PRId64"|", ixno, fno, recno);
	return(i);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
