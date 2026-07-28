/* ***************************************************************
 *
 * PROCEDURE:	srv_index.h
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
 *
 *				Mon Jul 27 08:37:53 PM MDT 2026
 *				tomg
 *				added an index V2 generation flag and moved the
 *				split information into the index_v2.h file
 *
 ************************************************************* */

/*
 * @#srv_index.h rev 3.1.0 dataman file edit procedure header
 * Copyright (c) SuperUser Software 1988-2005.  All rights reserved.
 *
 * this is the server side index information for the database server.
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

#ifndef _INDEX_INCLUDED_
#define _INDEX_INCLUDED_

#include <sys/types.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include "file_desc.h"
#include "lock.h"

/*
 * there is a mutex for each index because during the course of
 * any runtime the root position  and reference count may change.
 */
typedef struct _idxbuf_   {			/* main index information */
	pthread_mutex_t	 _mutex;		/* mutex for -this- index */
	int16_t			 _keylen;		/* length of key */
	int				 _idxchan;		/* channel number of index file */
	int				 _f_cnt;		/* count of files in this index */
	int				 _refcnt;		/* reference count */
	int64_t			 _rootpos;		/* position of root node */
	uint64_t		 _generation;	/* published v2 root generation */
	char			*_idxname;		/* name of index */
	char			*_rootdir;		/* root of database */
	FILES			**_files;		/* list of files */
	P_LOCK			 _lock;			/* cond var for sh/ex locking */
} INDEX;

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
