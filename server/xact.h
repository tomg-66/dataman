/* ***************************************************************
 *
 * PROCEDURE:	xact.h
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		Thu Apr 28 17:15:28 MDT 2005
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 *
 ************************************************************* */

#if !defined _DATAMAN_XACT_H
#define _DATAMAN_XACT_H

#include <sys/types.h>
/*
 * describe the information needed to store things for transactions
 */
typedef struct _xact_ {
	int			cmd;
	int32_t		len;
	char		*data;
	struct _xact_ *next;
	struct _xact_ *prev;
} xact_t;

#ifndef START_XACT_C
extern xact_t *xact_list;				/* head pointer */
extern xact_t *xact_curr;				/* current pointer */
extern int _in_xact_;				/* are we in a transaction? */
#endif
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
