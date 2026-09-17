/* Exercise connection-side transaction cleanup and reply handling without IPC. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "misc.h"
#include "msg.h"

bool recv_from_server(context_t *, MSG *, char **, int *, size_t *);
bool send_to_server(context_t *, char *, char *, int, int);
#include "../xact.c"
#include "../commit.c"
#include "../rollback.c"

static int reply_error;
static int replies;

bool send_to_server(context_t *ctx, char *fixed, char *payload, int flen, int plen)
{
	(void)ctx;
	(void)fixed;
	(void)payload;
	assert(flen > 0);
	assert(plen == 0);
	return(true);
}

bool recv_from_server(context_t *ctx, MSG *msg, char **data, int *len, size_t *mlen)
{
	(void)ctx;
	assert(*data == NULL);
	strcpy(msg->txt, "0|1|");
	*mlen = strlen(msg->txt);
	*len = reply_error;
	replies++;
	return(true);
}

static void queue_include(void)
{
	xact_list = calloc(1, sizeof(*xact_list));
	assert(xact_list);
	xact_curr = xact_list;
	xact_list->cmd = INCLUDE;
	xact_list->data = malloc(MAXSIZ);
	assert(xact_list->data);
	sprintf(xact_list->data, "%d|1|1|2|1|42|key|", INCLUDE);
}

int main(void)
{
	context_t ctx = {0};

	queue_include();
	inserts = calloc(2, sizeof(*inserts));
	assert(inserts);
	n_inserts = 1;
	xact_del_list();
	assert(!xact_list && !xact_curr && !inserts && !n_inserts);
	xact_del_list();
	/* Cleanup must also release auxiliary state when the queue is empty. */
	inserts = calloc(1, sizeof(*inserts));
	assert(inserts);
	n_inserts = 1;
	xact_del_list();
	assert(!inserts && !n_inserts);

	queue_include();
	assert(commit(&ctx));
	assert(replies == 1);
	xact_del_list();

	queue_include();
	reply_error = -1;
	assert(!rollback(&ctx));
	assert(xact_curr == xact_list);
	reply_error = 0;
	assert(rollback(&ctx));
	assert(!xact_curr);
	xact_del_list();
	return(0);
}
