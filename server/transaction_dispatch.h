/* ***************************************************************
 *
 * PROCEDURE:	transaction_dispatch.h
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		Thu Sep 17 08:39:02 PM MDT 2026
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 *
 ************************************************************* */
/*
 * Internal owner dispatch; GPL-2.0-or-later.
 */
#ifndef DATAMAN_TRANSACTION_DISPATCH_H
#define DATAMAN_TRANSACTION_DISPATCH_H
#include "transaction_session.h"

/* files contains borrowed descriptors and absolute paths below the registered
 * root. Paths must have canonical components (no dot, dot-dot, empty or trailing
 * components). Build these bindings from server metadata, never client claims.
 * The callback executes with exclusive owner admission and relative bindings.
 * Negative callback results make the transaction abort-only. Wire dispatch
 * restricts handlers to covered operations and keeps metadata alive until
 * completion. Cached index state is restored before admission reopens.
 */
int dm_tx_dispatch(int shmid, const dm_tx_target *files, size_t count,
		int (*handler)(void *), void *context);
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
