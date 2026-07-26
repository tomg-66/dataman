/* ***************************************************************
 *
 * PROCEDURE:	lock.h
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		Tue Jan 18 11:52:57 MST 2005
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 ************************************************************* */
/*
 * @#lock.h rev 3.2.5 dataman file edit procedure header
 * Copyright (c) SuperUser Software 2005.
 *
 * since the threading model in linux changed, (was fixed, really)
 * we can't use file locking now.  So, for every open file, we
 * now carry around one of the _plock_ structs.  This will let us
 * implement shared and exclusive locks on files that are thread-
 * safe.
 *
 * Dec 2005 -
 * I've finally added lock ownership and lock queueing for when
 * someone tries to exert a write lock and there are currently
 * locks asserted.
 *
 * this is now a good generalized thread safe, locking mechanism
 * that provides for shared and exclusive locks, protection from
 * other threads, queueing, and resource starvation prevention.
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

#ifndef _LOCK_INCLUDED_
#define _LOCK_INCLUDED_

#include <pthread.h>

#ifdef LOCK_UN
#undef LOCK_UN				/* fcntl.h defines this */
#endif

#define LOCK_UN		0		/* remove a lock */
#define LOCK_SH		1		/* assert a shared lock */
#define LOCK_EX		2		/* assert an exclusive lock */

/*
 * we want to have a list of those threads that have a low-level
 * lock on the file so that another thread can't remove a lock
 * that it hasn't asserted.  also use this struct as a queue for
 * locks that get backed up behind an exclusive lock so that we
 * can prevent resouce starvation.
 */

typedef struct _lock_owners_ {
	pthread_t			_owner;
	int					_type;
	struct	_lock_owners_ *_next;
} OWNER;

/*
 * this is what each file will carry around with it so that a
 * thread can perform lock operations.
 */
typedef struct _plock_ {
	pthread_mutex_t	mutex;				/* the mutex for this condition */
	pthread_cond_t	cond;				/* the actual condition */
	int				value;				/* the value of the lock */
	OWNER			*owners;			/* who currently owns locks */
	OWNER			*queue;				/* who wants to own a lock */
} P_LOCK;

#ifndef LOCK_C
extern int fl_lock(P_LOCK *lock, int type);
#endif

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
