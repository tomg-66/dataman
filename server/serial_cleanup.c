/* ***************************************************************
 *
 * PROCEDURE:	serial_cleanup
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		Fri Jun 16 20:36:27 MDT 2006
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 ************************************************************* */
/*
 * take some of the functions that we could out of serial_service
 * to clean it up a little.
 *
 * these functions allow the serial server to keep track of
 * things that will need to be cleaned up in case of an abnormal
 * termination of the client - indexes and protected records
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

#include <sys/msg.h>

#include "dbfunc.h"
#include "msg.h"

extern int dbgsw;

static struct _ixlist_ {
	int				 _ixno;
	struct _ixlist_	*_next;
} *ixlist = NULL;

/*
 * save the opened index and workfile information for this
 * connection/
 */
void store_ix(char *cmd, int type)
{
	struct _ixlist_ *tmp;
	char *cptr;

	if (dbgsw) {
		fprintf(stderr, "store_ix, cmd = %s\n", cmd);
		fflush(stderr);
	}
	tmp = (struct _ixlist_ *)calloc(1, sizeof(struct _ixlist_));
	tmp->_next = ixlist;
	ixlist = tmp;
	cptr = strchr(cmd, '|') + 1;
	ixlist->_ixno = atoi(cptr);
	if (type == INIT_DAT)
		ixlist->_ixno *= -1;
	if (type == MKIDX) {
		cptr = strchr(cptr, '|') + 1;
		cptr = strchr(cptr, '|') + 1;
		tmp = (struct _ixlist_ *)calloc(1, sizeof(struct _ixlist_));
		tmp->_next = ixlist;
		ixlist = tmp;
		ixlist->_ixno = atoi(cptr);
		ixlist->_ixno *= -1;
	}
}

/*
 * if the user forgot to clean up after himself or the client crashed, 
 * clean up the open indexes/work file
 */
void do_iclose(char *cmd, int msgid)
{
	int ixno;

	pid_t mypid;

	char *cptr;

	struct _ixlist_ *tptr;
	struct _ixlist_ *fptr;

	MSG msg;
/*
 * remove one entry
 */
	if (cmd) {
		cptr = strchr(cmd, '|') + 1;
		ixno = atoi(cptr);
		tptr = NULL;
		fptr = ixlist;
		while (fptr) {
			if (fptr->_ixno == ixno) {
				if (tptr == NULL)
					ixlist = fptr->_next;
				else
					tptr->_next = fptr->_next;
				free(fptr);
				return;
			}
			tptr = fptr;
			fptr = fptr->_next;
		}
		return;
	}
/*
 * remove all entries
 */
	while(ixlist) {
		mypid = getpid();
		if (dbgsw) {
			fprintf(stderr, "ix_closing %d\n", ixlist->_ixno);
			fflush(stderr);
		}
		msg.type = MSG_SRV;
		sprintf(msg.txt, "%d|%d|%d|", mypid, ICLOSE, ixlist->_ixno);
		msgsnd(msgid, (void *)&msg, strlen(msg.txt), 0);
		msgrcv(msgid, &msg, MAXSIZ, (long)mypid, 0);
		tptr = ixlist->_next;
		free(ixlist);
		ixlist = tptr;
	}
}


static struct _prlist_ {
	int				 _ixno;			/* index of protect */
	int				 _fno;			/* file number in index */
	unsigned long	 _recno;		/* record number in file */
	struct _prlist_	*_next;
} *prlist = NULL;

/*
 * keep track of each of the times the user calls protect, so that if
 * he forgets to clean up, we can for him
 */
void store_prot(char *cmd)
{
	int ixno;
	int fno;
	char *ptr;
	unsigned long recno;
	struct _prlist_ *pptr;
/*
 * get the values
 */
	ptr = strchr(cmd, '|') +1;
	ptr = strchr(ptr, '|') +1;
	ixno = atoi(ptr);
	ptr = strchr(ptr, '|') + 1;
	fno = atoi(ptr);
	ptr = strchr(ptr, '|') + 1;
	recno = strtoul(ptr, NULL, 0);
/*
 * make sure it's not already in there
 */
	pptr = prlist;
	while(pptr) {
		if (pptr->_ixno == ixno && pptr->_fno == fno && pptr->_recno == recno)
			return;
		pptr = pptr->_next;
	}
/*
 * add it
 */
	pptr = calloc(1, sizeof(struct _prlist_));
	pptr->_ixno = ixno;
	pptr->_fno = fno;
	pptr->_recno = recno;
	pptr->_next = prlist;
	prlist = pptr;
}

/*
 * if the client terminates with any records protected, clean them up
 * so we don't have a record that is eternally protected.
 */
void do_clear(char *cmd, key_t msgid)
{
	int ixno;
	int fno;

	char *cptr;

	unsigned long recno;

	struct _prlist_ *pptr;
	struct _prlist_ *fptr;

	pid_t mypid;

	MSG msg;

	mypid = getpid();
	if (cmd) {
		cptr = strchr(cmd, '|') + 1;
		ixno = atoi(cptr);
		cptr = strchr(cptr, '|') + 1;
		fno = atoi(cptr);
		cptr = strchr(cptr, '|') + 1;
		recno = strtoul(cptr, NULL, 0);
		fptr = NULL;
		pptr = prlist;
		while(pptr) {
			if (pptr->_ixno == ixno && pptr->_fno == fno &&
							pptr->_recno == recno) {
				if (!fptr)
					prlist = pptr->_next;
				else
					fptr->_next = pptr->_next;
				free(pptr);
				break;
			}
			fptr = pptr;
			pptr = pptr->_next;
		}
		return;
	}

	while(prlist) {
		sprintf(msg.txt, "%d|%d|%d|%d|%lu|", mypid, CLEAR, prlist->_ixno,
						prlist->_fno, prlist->_recno);
		msgsnd(msgid, (void *)&msg, strlen(msg.txt), 0);
		msgrcv(msgid, &msg, MAXSIZ, (long)mypid, 0);
		pptr = prlist;
		prlist = prlist->_next;
		free(pptr);
	}
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
