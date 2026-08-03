/* ***************************************************************
 *
 * PROCEDURE:	errors.h
 *
 * PROJECT:		dataman
 * 
 * DATE:		
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 *
 ************************************************************* */

/*
 * @#errors.h rev 3.1.0 dataman file edit procedure header
 * Copyright (c) SuperUser Software 2004-2005.  All rights reserved.
 *
 * dataman error codes and associated messages.
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

#if !defined _DBERRORS_INCLUDED_
#define _DBERRORS_INCLUDED_

#define ENOTHR			-1		/* can't start new thread */
#define ENOMSGQ			-2		/* couldn't attach to message queue */
#define ENOSHM			-3		/* couldn't creat or attach shared mem */
#define ENOSEM			-4		/* couldn't creat the semaphore */
#define ENOINDEX		-5		/* index file not found */
#define EINITINDEX		-6		/* error during initialization of index */
#define ENOIXSP			-7		/* no space to allocate more index */
#define ENOLOCK			-8		/* can't obtain a necessary file lock */
#define ENODERD			-9		/* can't read node from index */
#define ENOFILE			-10		/* file not found in index */
#define ENOALLOC		-11		/* misc can not allocate error */
#define EFHDRD			-12		/* error reading file header */
#define ERHREAD			-13		/* error reading record header */
#define ERECREAD		-14		/* error reading the rec from the file */
#define ERMKEY			-15		/* system key has been removed */
#define ENOREC			-16		/* system record does not exist */
#define ENOTOPEN		-17		/* requested file isn't open */
#define EBADFMT			-18		/* invalid format # for file */
#define ERECWRT			-19		/* can't write new record to file */
#define EBEGWRT			-20		/* can't write new begining offset */
#define ENODEL			-21		/* you can't del the only rec in the file */
#define EHDRWRT			-22		/* can't write header (record, file, index */
#define ENODWRT			-23		/* can't write node to index */
#define ENOPARENT		-24		/* can't find parent offset */
#define EPRTRD			-25		/* error reading during protect */
#define EPRCTDEL		-26		/* trying to protect a nonexistant record */
#define ECLRRD			-27		/* error reading during clear */
#define EIDXNOO			-28		/* index not open */
#define ENOWSP			-29		/* no work file space */
#define ENOWFILE		-30		/* could not open work file */
#define EIDXOPN			-31		/* index is already open */
#define EIDXCREAT		-32		/* can't create new index */
#define EFILWRT			-33		/* can't write file name to index */
#define ENOHOST			-34		/* invalid host name to connect to */
#define ENOSOCK			-35		/* can't open socket */
#define ENOCONN			-36		/* can't connect to the server */
#define ENORESP			-37		/* server didn't respond */
#define ENOGET			-38		/* initial GET not attempted */
#define EINVMSG			-39		/* invalid message received on sock */
#define ESOCKOPT		-40		/* can't set socket opt */
#define ENOTWORK		-41		/* not a work file */
#define ESUBSCR			-42		/* invalid data field subscript */
#define ESHUT			-43		/* dataman is shutting down */
#define ENOBLOB			-44		/* can't get blob file */
#define EBLOBTYP		-45		/* Operation not permitted on Blob type */
#define EINXACT			-46		/* already in a transaction */
#define ENOXACT			-47		/* not in a transaction */
#define EROLLBACK		-48		/* a commit failed, and the rollback failed too */
#define EJAVACON		-49		/* in the java classlib a connection error */
#define EJAVAREAD		-50		/* in the java classlib a read error */
#define EMULTIPLE		-51		/* multiple errors, your database may be corrupt */
#define EBLOBWRT		-52		/* can't write blob to file */
#define EOUTREC			-53		/* error in outrec */
#define EINREC			-54		/* error in inrec */


#ifdef DBERROR

#if defined __cplusplus
const
#endif

char *db_err_strings[] = {
	"no error",
	"can't start new thread",
	"couldn't attach to message queue",
	"couldn't create or attach shared mem",
	"couldn't create the semaphore",
	"index file not found",
	"error during initialization of index",
	"no space to allocate more index",
	"can't obtain a necessary file lock",
	"can't read node from index",
	"file not found in index",
	"misc can not allocate error",
	"error reading file header",
	"error reading record header",
	"error reading the rec from the file",
	"system key has been removed",
	"system record does not exist",
	"requested file isn't open",
	"invalid format # for file",
	"can't write new record to file",
	"can't write new begining offset",
	"you can't delete the only rec in the file",
	"can't write header (record, file, index)",
	"can't write node to index",
	"can't find parent offset",
	"error reading during protect",
	"trying to protect a nonexistant record",
	"error reading during clear",
	"index not open",
	"no work file space",
	"could not open work file",
	"index is already open",
	"can't create new index",
	"can't write file name to index",
	"invalid host name",
	"can't open socket",
	"can't connect to server",
	"received no response from server",
	"initial GET not attempted",
	"invalid message received on socket",
	"can't set socket option",
	"not a work file",
 	"invalid data field subscript",
	"dataman is shutting down",
	"Can't get BLOB file",
	"Operation not permitted on Blob type",
	"Already in a transaction",
	"Not in a transaction",
	"Rolling back a failed transaction failed",
	"java socket connection is not made",
	"java error reading socket",
	"Multiple errors, your database may be corrupt",
	"Can't write blob to file",
	"Error writing record to database",
	"Error reading record from database",
	NULL
};
#endif					/* ifdef DBERROR */

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
