/* ***************************************************************
 *
 * PROCEDURE:	commit.c
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
 *
 ************************************************************* */
/*
 * commit all of the peices of a transaction to the database
 */

#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include <sys/sem.h>

#include "dbfunc.h"
#include "xact.h"
#include "msg.h"
#include "misc.h"

extern int recv_from_server(context_t *, MSG *, char **, int *, int *);
extern int send_to_server(context_t *, char *, char *, int, int);

extern xact_t *xact_list;
extern xact_t *xact_curr;

extern int n_inserts;
extern int64_t *inserts;

#if defined(__GNU_LIBRARY__) && !defined(_SEM_SEMUN_UNDEFINED)
/* union semun is defined by including <sys/sem.h> */
#else
/* according to X/OPEN we have to define it ourselves */
union semun {
	int val;                    /* value for SETVAL */
	struct semid_ds *buf;       /* buffer for IPC_STAT, IPC_SET */
	unsigned short int *array;  /* array for GETALL, SETALL */
	struct seminfo *__buf;      /* buffer for IPC_INFO */
};
#endif

#define abs(x)	((x)<0?(-x):(x))

int commit(context_t *ctx)
{
	int cmd;
	int i, j;
	int size;
	int inserted;
	int fileno;
	int fmt;
	int len;
	int diff;
	int mode;

	int64_t recno;

	char *cptr;					/* misc char ptr */
	char *tptr;					/* temp pointer */
	char *rcvbuf;
	char *sndbuf;
	char rec_buff[MAXSIZ];

	int32_t b_size;

	union semun sarg;

	MSG msgbuf;

	if (n_inserts)
		if ((inserts = (int64_t *)calloc(n_inserts+1,sizeof(int64_t))) == NULL)
			return(FALSE);

	inserted = 0;
	xact_curr = xact_list;
	while(xact_curr) {
		cptr = xact_curr->data;
		switch(xact_curr->cmd) {
			case FLUSH:
/*
 * get the current record as it exists in the database
 * we'll need to save it in case we need to roll it back.
 */
				cptr = strchr(cptr, '|') + 1;
				j = atoi(cptr);					// index number
				cptr = strchr(cptr, '|') + 1;
				fileno = atoi(cptr);			// file number
				cptr = strchr(cptr, '|') + 1;
				recno = strtoll(cptr, NULL, 0);	// record number
				if (recno < 0)
					recno = -inserts[-recno];
				cptr = strchr(cptr, '|') + 1;
				fmt = atoi(cptr);				// format numer
				cptr = strchr(cptr, '|') + 1;
				b_size = atoi(cptr);			// binsize
				size = sprintf(msgbuf.txt, "%d|%d|%d|%"PRId64"|", GET_REC, j, fileno, abs(recno));
				if (!send_to_server(ctx, msgbuf.txt, NULL, size, 0)) {
					xact_curr = xact_curr->prev;
					return(FALSE);
				}
				sndbuf = NULL;
				if (!recv_from_server(ctx, &msgbuf, &sndbuf, &len, &i)) {
					xact_curr = xact_curr->prev;
					return(FALSE);
				}
/*
 * now we have the original record, save it then send off the command
 */
				rcvbuf = xact_curr->data;
				xact_curr->data = sndbuf;
				xact_curr->len = atoi(sndbuf);
				sarg.val = 2;
				if (semctl(ctx->semid, 0, SETVAL, sarg) < 0) {
					fprintf(stderr, "pid %d: error setting semaphore value", 
									ctx->mypid);
					perror("");
					return(FALSE);
				}
/*
 * if we are flushing a newly inserted record, we need to give it the
 * real record number, so rebuild the command.
 */
				if (recno < 0) {
					i = sprintf(rec_buff, "%d|%d|%d|%"PRId64"|%d|%d|", FLUSH, j,
									fileno, -recno, fmt, b_size);
					if (xact_curr->len < b_size+i+1) {
						tptr = malloc(b_size+i+1);
						memcpy(tptr, rec_buff, i);
						cptr = strchr(cptr, '|') + 1;
						memcpy(tptr+i, cptr, b_size);
						cptr = tptr+i;
						free(rcvbuf);
						rcvbuf = tptr;
					} else {
						diff = strlen(rec_buff) - (cptr-xact_curr->data);
						memmove(rcvbuf+diff, rcvbuf, b_size);
						memcpy(rcvbuf, rec_buff, strlen(rec_buff));
					}
					j = b_size;
					size = i;
				} else {
					j = atoi(cptr);						// j is now the size of the shared mem portion
					cptr = strchr(cptr, '|') + 1;		// ptr points at the shared mem portion
					size = cptr - rcvbuf;				// size is the length of the basic command
				}
				break;

			case INSERT:
				cptr = strchr(cptr, '|') + 1;		/* point past command */
				fmt = atoi(cptr);					/* format number to insert */
				cptr = strchr(cptr, '|') + 1;
				mode = atoi(cptr);					/* insert mode */
				cptr = strchr(cptr, '|') + 1;
				i = atoi(cptr);					/* index number */
				cptr = strchr(cptr, '|') + 1;
				fileno = atoi(cptr);				/* file number */
				cptr = strchr(cptr, '|') + 1;
				recno = strtoll(cptr, NULL, 0);
				if (recno < 0) {					/* ok, we need to rewrite */
					recno = inserts[-recno];
					size = sprintf(xact_curr->data, "%d|%d|%d|%d|%d|%"PRId64"|", INSERT, fmt, mode,
								i, fileno, recno);
				} else
					size = strlen(xact_curr->data);
				rcvbuf = xact_curr->data;
				cptr = NULL;
				j = 0;
				break;

			case INCLUDE:
				cptr = strchr(cptr, '|') + 1;
				i = atoi(cptr);						/* source index number */
				cptr = strchr(cptr, '|') + 1;
				fileno = atoi(cptr);				/* source file number */
				cptr = strchr(cptr, '|') + 1;
				j = atoi(cptr);						/* dest index number */
				cptr = strchr(cptr, '|') + 1;
				size = atoi(cptr);					/* dest file number */
				cptr = strchr(cptr, '|') + 1;
				recno = strtoll(cptr, NULL, 0);		/* record pointer */
				cptr = strchr(cptr, '|') + 1;
				if (recno < 0) {
					char tbuff[MAXSIZ];
					recno = inserts[-recno];
					size = sprintf(tbuff, "%d|%d|%d|%d|%d|%"PRId64"|%s|", INCLUDE, i,
						fileno, j, size, recno, cptr);
					size--;
					*(tbuff+size) = '\0';
					strcpy(xact_curr->data, tbuff);
				} else
					size = strlen(xact_curr->data);
				rcvbuf = xact_curr->data;
				cptr = NULL;
				j = 0;
				break;
/*
 * right now, we're going to say that the user can't delete a record that
 * was inserted as part of -this- transaction.
 *
 * ok, well maybe.....
 */
			case DELETE:
				sndbuf = NULL;
				cptr = strchr(cptr, '|') + 1;
				i = atoi(cptr);
				cptr = strchr(cptr, '|') + 1;
				fileno = atoi(cptr);
				cptr = strchr(cptr, '|') + 1;
				recno = strtoll(cptr, NULL, 0);
				if (recno < 0) {
					recno = inserts[-recno];
					size = sprintf(rec_buff, "%d|%d|%d|%"PRId64"|2|", DELETE, i, fileno, recno);
					strcpy(xact_curr->data, rec_buff);
				} else {
					cptr = strrchr(xact_curr->data,'|') - 1;
					*cptr = '2';
					size = strlen(xact_curr->data);
				}
				rcvbuf = xact_curr->data;
				j = 0;
				break;

			case REMOVE:
				sndbuf = NULL;
				cptr = strrchr(xact_curr->data,'|') - 1;
				*cptr = '0';
				rcvbuf = xact_curr->data;
				size = strlen(xact_curr->data) + PTR_LENGTH;
				j = 0;
				break;
		}
		if (!send_to_server(ctx, rcvbuf, cptr, size, j))
			return(FALSE);
		if (!recv_from_server(ctx, &msgbuf, &sndbuf, &len, &i))
			return(FALSE);
		if (len < 0)
			return(FALSE);
/*
 * if this is an insert extract the record number from the return and
 * save it
 */
		if (xact_curr->cmd == INSERT) {
			cptr = strchr(msgbuf.txt, '|') + 1;
			cptr = strchr(cptr, '|') + 1;
			recno = strtoll(cptr, NULL, 0);
			inserts[++inserted] = recno;
		}
		xact_curr = xact_curr->next;
	}
/*
 * loop back through now to do final clean up for any blobs
 */
	xact_curr = xact_list;
	while(xact_curr) {
		i = atoi(xact_curr->data);
		if (i == DELETE) {
			cptr = strrchr(xact_curr->data, '|') + 1;
			*(cptr-2) = CLEANUP+060;
			len = cptr - xact_curr->data;
			send_to_server(ctx, xact_curr->data, NULL, len, 0);
			recv_from_server(ctx, &msgbuf, &sndbuf, &len, &i);
		}
		xact_curr = xact_curr->next;
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
