/* ***************************************************************
 *
 * PROCEDURE:	add_protect.c
 *
 * PROJECT:		dataman client side
 * 
 * DATE:		Fri May 17 21:45:27 MDT 2002
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 *
 ************************************************************* */

/*
 * this function keeps a list of records that have been
 * protected.  this is so that when we shut down, if there are
 * any the user forgot to do we can do it.
 *
 * just do a list, because there shouldn't be many protects
 * around at any given time, so it will be a quick search.
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
#include <malloc.h>

#include "globs.h"

extern void db_err(int, char *, ...);

typedef struct _pchain_ {
	int idxno;					/* index number */
	int fileno;					/* file number */
	int64_t recno;				/* record number */
	struct _pchain_ *next;		/* pointer to next */
} PCHAIN;

static PCHAIN *phead = NULL;	/* head of chain */

void add_protect(int idxno, int fileno, int64_t recno)
{

	PCHAIN *tmp;

	tmp = phead;

	if (tmp == NULL) {
		if ((tmp = phead = (PCHAIN *)calloc(1, sizeof(PCHAIN))) == NULL)
			db_err(0, "%s: Can't allocate list header in add_protect", _progname);
	} else {
		while (tmp->next) {
			if (tmp->idxno == idxno && tmp->fileno == fileno && tmp->recno == recno)
				return;
			tmp = tmp->next;
		}
		if ((tmp->next  = (PCHAIN *)calloc(1, sizeof(PCHAIN))) == NULL)
			db_err(0, "%s: Can't allocate list element in add_protect", _progname);
		tmp = tmp->next;
	}
	tmp->idxno = idxno;
	tmp->fileno = fileno;
	tmp->recno = recno;
}

void del_protect(int idxno, int fileno, int64_t recno)
{


	PCHAIN *tmp;
	PCHAIN *ptr;

	tmp = phead;
	ptr = NULL;

	while (tmp) {
		if ((idxno == tmp->idxno && fileno == tmp->fileno && recno == tmp->recno) ||
					(!idxno && !fileno && !recno)) {
			if (!ptr) {
				phead = phead->next;
				free(tmp);
				tmp = phead;
			} else {
				ptr->next = tmp->next;
				free(tmp);
				tmp = ptr->next;
			}
			continue;
		}
		tmp = tmp->next;
	}
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker ft=c:
 */
