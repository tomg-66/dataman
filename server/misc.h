/* ***************************************************************
 *
 * PROCEDURE:	misc.h
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
 * @#misc.h rev 3.2.4  dataman header file
 *
 * Copyright (c) SuperUser Software 1988-2005.  All rights reserved.
 *
 * check for endin-ness and various constants for offsets, headers,
 * and sizes
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

#ifndef _DATAMAN_MISC_H_INCLUDED_
#define  _DATAMAN_MISC_H_INCLUDED_

#if !defined __gnu_linux__
#define LITTLE_ENDIAN	1
#define BIG_ENDIAN		0

#ifndef _check_endian_c_
extern int d_endian;				/* flag for endian ness */
#endif
#else
#include <endian.h>
#define LITTLE_ENDIAN	__LITTLE_ENDIAN
#define BIG_ENDIAN		__BIG_ENDIAN
#endif

#define DATARECORD_HEADER_LENGTH	(sizeof(int64_t)*2+sizeof(char))
#define DATARECORD_FLAG_OFFSET		(sizeof(char))
#define DATARECORD_FLAG_LENGTH		(sizeof(char))
#define OFFSET_TO_PREV				(sizeof(char))
#define OFFSET_TO_NEXT				(sizeof(int64_t) + sizeof(char))
#define KEY_HEADER_LENGTH			(sizeof(int64_t) + sizeof(char))
#define KEY_BUFFER_SIZE				(MAX_KEY_SIZE+sizeof(int64_t)+sizeof(char))
#define NODE_FLAG_LENGTH			(sizeof(char))
#define INDEX_HEADER_LENGTH			(sizeof(int16_t)*2)
#define INDEX_FILE_OFFSET			(INDEX_HEADER_LENGTH + sizeof(int64_t))
#define PTR_LENGTH					(sizeof(int64_t))

#define FALSE						0
#define TRUE						1

#define UNLINK						0
#define UNHIDE						1
#define HIDE						2

#define NOXACT						0
#define XACT						1
#define INCOMMIT					2
#define CLEANUP						3

#define MAX_CONNS					256

#include <sys/types.h>

extern off_t lseek(int, off_t, int);

static inline loff_t dataman_llseek(int fd, loff_t offset, int whence)
{
	return((loff_t)lseek(fd, (off_t)offset, whence));
}

#define llseek dataman_llseek

typedef struct serial_context {
		char *shptr;				/* pointer to shared mem segment */
		key_t msgid;				/* our message queue ident */
		key_t semid;				/* semaphore id returned by semget() */
		key_t shmid;				/* shared memory id returned by shmget() */
		pid_t mypid;				/* hmmm... what might this be? */
} context_t;

#endif		/* _DATAMAN_MISC_H_INCLUDED */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
