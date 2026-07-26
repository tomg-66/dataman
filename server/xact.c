/* ***************************************************************
 *
 * PROCEDURE:	start_xact.c
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		Tue Jun 20 19:32:18 MDT 2006
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 *				 Thu Mar 21 16:04:49 MDT 2013
 *				 tomg
 *				 fixed bug not setting the return message length
 *
 ************************************************************* */
/*
 * this function starts a dataman transaction.  it records
 * everything that is to be done to the database, then when
 * the user calls commit, it commits it to the database.
 * if any of the operations fail, then all of the operations
 * to that point are rolled back.  Things that may be be
 * difficult to roll back are record deletions and key
 * removals.
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "dbfunc.h"

#include "msg.h"
#include "errors.h"
#include "misc.h"

#define START_XACT_C
#include "xact.h"

xact_t *xact_list = NULL;
xact_t *xact_curr = NULL;

extern bool recv_from_server(context_t *, MSG *, char **, int *, size_t *);
extern bool send_to_server(context_t *, char *, char *, int, int);

int n_inserts;
int64_t *inserts;

/*
 * add a transaction to the list
 * (or, start a new list if we need to)
 */
int store_xact(context_t *ctx, char **rcvbuf, int rlen, MSG *msgbuf, char **sndbuf, int *slen, size_t *msglen)
{

	int i, j;
	int len;
	int ret;

	int64_t recno;

	char buff[MAXSIZ];
	char *cptr;

	xact_t *xptr;

	if ((xptr = calloc(1, sizeof(xact_t))) == NULL) {
//
//do an error here
		return(FALSE);
	}
	xptr->data = *rcvbuf;
	if ((*rcvbuf = malloc(rlen)) == NULL) {
//
// do an error here
		return(FALSE);
	}

	if (!xact_list)
		xact_list = xptr;
	else {
		xptr->prev = xact_curr;
		xact_curr->next = xptr;
	}
	xact_curr = xptr;
	i = atoi(xptr->data);
	xptr->cmd = i;
/*
 * now we have the command stored, we need to build a return for
 * the client.  the command -can only be- one of these five
 */
	switch(i) {
/*
 * delete needs to retrieve the record following the current one,
 * unless it is the last in the file, then the prior one.
 */
		case DELETE:
/*
 * delete is supposed to return the record following it after it
 * is done.  if the deleted record is the last in the file, then
 * it returns the record prior
 */
			cptr = strrchr(xptr->data, '|') + 1;
			len = cptr - xptr->data;
			if (!send_to_server(ctx, xptr->data, NULL, len, 0))
				return(FALSE);
			if (!recv_from_server(ctx, msgbuf, sndbuf, slen, msglen))
				return(FALSE);
			if (*slen < 0)
				return(FALSE);
			break;
/*
 * save the insert, which insert this is in the xact
 */
		case INSERT:
			n_inserts++;
			*msglen = sprintf(msgbuf->txt, "0|%d|%d|", i, -n_inserts);
			*slen = 0;
			break;
/*
 * save the include and return successful
 */
		case INCLUDE:
			*msglen = sprintf(msgbuf->txt, "0|0|0|");
			*slen = 0;
			break;

		case REMOVE:
			cptr = strchr(xptr->data, '|') + 1;
			i = atoi(cptr);
			cptr = xptr->data;
			len = msgbuf->type;
			if (!send_to_server(ctx, xptr->data, NULL, len, 0))
				return(FALSE);
			if (!recv_from_server(ctx, msgbuf, sndbuf, slen, msglen))
				return(FALSE);
			if (*slen < 0)
				return(FALSE);
			cptr = strchr(msgbuf->txt, '|') + 1;
			len = atoi(cptr);
			cptr = strchr(cptr, '|') + 1;
			i = sprintf(xptr->data, "%d|%d|1|", REMOVE, i);
			memcpy(xptr->data+i, cptr, len);
			xptr->len = len;
			break;

		case FLUSH:
			*msglen = sprintf(msgbuf->txt, "0|1|");
			*slen = 0;
			xptr->len = rlen;
			break;
	}
	return(TRUE); 
}

/*
 * clean up the entire xaction list
 */
void xact_del_list()
{
	xact_t *ptr;

	if (!xact_list)
		return;
	
	while(xact_list) {
		ptr = xact_list->next;
		if (xact_list->data)
			free(xact_list->data);
		free(xact_list);
		xact_list = ptr;
	}
	xact_curr = NULL;
	if (inserts)
		free(inserts);
	n_inserts = 0;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker ft=c:
 */
