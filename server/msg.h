/* ***************************************************************
 *
 * PROCEDURE:	msg.h
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 * 				Fri May  5 15:04:16 MDT 2006
 * 				started specifying bit sizes in variable decls
 * 				that need it.
 * 				tomg
 *
 ************************************************************* */

/*
 * @#msg.h rev 3.1.0 dataman file edit procedure header
 * Copyright (c) SuperUser Software 1988-2005.  All rights reserved.
 *
 * message queue definitions for communications between the connection
 * server and the actual database server.
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
#ifndef _DATAMAN_MSG_INCLUDED_
#define _DATAMAN_MSG_INCLUDED_

typedef struct _msg_ {
	long type;
	char txt[256];
} MSG;

#define MSGKEY	93066			/* our message queue key */
#define MAXSIZ	256				/* max size of message */
#define SHMSIZ	1024			/* shared memory segment size */
#define PERMS	0666

#define MSG_ANY	0L				/* get any/all message from queue */
#define MSG_SRV	1L				/* get the db server message */

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
