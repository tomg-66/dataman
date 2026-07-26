/* ***************************************************************
 *
 * PROCEDURE:	rollback.c
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		Fri Jul 14 19:49:18 MDT 2006
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 *
 ************************************************************* */
/*
 * a transaction had a failure. we need to put everything in the
 * database back to where it was.
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
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>

#include <sys/sem.h>

#include "dbfunc.h"
#include "xact.h"
#include "misc.h"
#include "msg.h"

extern xact_t *xact_list;			/* the list of transactions */
extern xact_t *xact_curr;			/* the instruction that failed */

extern int64_t *inserts;

extern bool send_to_server(context_t *, char *, char *, int, int);
extern bool recv_from_server(context_t *, MSG *, char **, int *, int *);
extern int64_t get_ll(char *);
extern void put_ll(char *, int64_t);

int rollback(context_t *ctxt)
{

	int i, j;
	int idxno;
	int fno;
	int fmt;
	int i_count;					/* insert count */

	int64_t recno;

	char *cptr;
	char key[64];

	MSG msgbuf;

	i_count = 0;
	while(xact_curr) {
		cptr = xact_curr->data;
		cptr = strchr(cptr, '|') + 1;
//		ptr = NULL;
		j = 0;
		switch(xact_curr->cmd) {
			case DELETE:
				idxno = atoi(cptr);
				cptr = strchr(cptr, '|') + 1;
				fno = atoi(cptr);
				cptr = strchr(cptr, '|') + 1;
				recno = strtoll(cptr, NULL, 0);
				j = sprintf(msgbuf.txt, "%d|%d|%d|%"PRId64, UNDELETE, idxno,
								fno, recno);
				i = 0;
				cptr = NULL;
				break;

			case REMOVE:
				idxno = atoi(cptr);						/* index we're doing */
				cptr = strchr(cptr, '|') + 1;			/* point to xact flag */
				cptr = strchr(cptr, '|') + 1;			/* point to key */
				j = xact_curr->len - KEY_HEADER_LENGTH;		/* find out stuff! */
				memset(key, '\0', sizeof(key));
				memcpy(key, cptr, j);
				fno = *(cptr+j) - 1;					/* the array offset, not fileno */
				j++;
				recno = get_ll(cptr+j);
				sprintf(msgbuf.txt, "%d|%d|%d|%d|%d|%"PRId64"|%s|", INCLUDE, idxno,
								fno, idxno, fno, recno, key);
				j = strlen(msgbuf.txt);
				i = 0;
				cptr = NULL;
				break;

			case INSERT:
				cptr = strchr(cptr, '|') + 1;			/* skip format number */
				cptr = strchr(cptr, '|') + 1;			/* skip mode */
				idxno = atoi(cptr);
				cptr = strchr(cptr, '|') + 1;
				fno = atoi(cptr);
				recno = inserts[++i_count];
				sprintf(msgbuf.txt, "%d|%d|%d|%"PRId64"|%d|", DELETE, idxno,
								fno, recno, 0);
				j = strlen(msgbuf.txt);
				i = 0;
				cptr = NULL;
				break;

			case INCLUDE:
				cptr = strchr(cptr, '|') + 1;			/* skip idx1->_idxno */
				cptr = strchr(cptr, '|') + 1;			/* skip idx1->_fno */
				idxno = atoi(cptr);
				cptr = strchr(cptr, '|') + 1;
				fno = atoi(cptr);
				cptr = strchr(cptr, '|') + 1;
				recno = strtoll(cptr, NULL, 0);
				cptr = strchr(cptr, '|') + 1;
				memset(key, '\0', sizeof(key));
				strcpy(key, cptr);
				i  = strlen(key)-1;
				*(key+i) = fno;
				put_ll(key+i+1, recno);
				i += KEY_HEADER_LENGTH;
				j = sprintf(msgbuf.txt, "%d|%d|%d|", REMOVE, idxno, NOXACT);
				memcpy(msgbuf.txt+j, key, i);
				j += i;
				i = 0;
				cptr = NULL;
				break;

			case FLUSH:
				i = atoi(xact_curr->data);
				idxno = atoi(cptr);
				cptr = strchr(cptr, '|') + 1;
				fno = atoi(cptr);
				cptr = strchr(cptr, '|') + 1;
				recno = strtoll(cptr, NULL, 0);
				cptr = strchr(cptr, '|') + 1;
				fmt = atoi(cptr);
				cptr = strchr(cptr, '|') + 1;
				j = sprintf(msgbuf.txt, "%d|%d|%d|%"PRId64"|%d|%d|", FLUSH, idxno, fno, recno, fmt, i);
				break;

		}

		if (!send_to_server(ctxt, msgbuf.txt, cptr, j, i))
			return(FALSE);
		cptr = NULL;
		i = 0;
		if (!recv_from_server(ctxt, &msgbuf, &cptr, &i, &j))
			return(FALSE);

/*
 * point to the prior instruction to undo
 */
		xact_curr = xact_curr->prev;
	}
	return(TRUE);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker ft=c:
 */
