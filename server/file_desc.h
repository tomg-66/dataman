/* ***************************************************************
 *
 * PROCEDURE:	file_desc.h
 *
 * PROJECT:		dataman client side
 * 
 * DATE:		legacy, originally writtin in 1988
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 * 				March 2002
 *				Tom Green
 *				modified to use call interface to server
 ************************************************************* */

/*
 * @#file_desc.h rev 3.2.0 dataman file edit procedure header
 * Copyright (c) SuperUser Software 2004-2005.  All rights reserved.
 *
 * this is the description of the files that an index keeps
 * track of.  it should probably someday be merged with
 * datafile_header.h, but we won't worry about that for now.
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

#if !defined _FILE_DESC_H_INCLUDED_
#define _FILE_DESC_H_INCLUDED_

#include <sys/types.h>

#include "lock.h"
#include "datafile_header.h"

#define LOCK_TABLE_SIZE	128

typedef struct _lock_info_ {
	int64_t				_recno;
	struct _lock_info_ *_next;
} LOCKS;

typedef struct _file_info_ {
	int					_chan;			/* file channel if opened */
	int					_longest;		/* longest record in this file */
	int					_hlen;			/* length of description */
	int16_t			   *_desc;			/* unparsed description of this file */
	char 			   *_fname;			/* file name */
	FILEDESC		   *_filedesc;		/* parsed out file description */
	P_LOCK				_lock;			/* cond var for sh/ex locking */
	pthread_mutex_t		_mutex;			/* mutex to go around seek and read/write */
	LOCKS			   *_locks[LOCK_TABLE_SIZE];	/* hash table of protected records */
} FILES;

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
