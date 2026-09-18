/* ***************************************************************
 *
 * PROCEDURE:	transaction_wire.h
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		Thu Sep 17 08:41:32 PM MDT 2026
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 *
 ************************************************************* */
/* GPL-2.0-or-later. */

#ifndef DATAMAN_TRANSACTION_WIRE_H
#define DATAMAN_TRANSACTION_WIRE_H

#include <stddef.h>

int dm_tx_wire(int shmid, int command, char *message, int offset, char **data,
		size_t payload, int (*handler)(char *, int, char **));
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
