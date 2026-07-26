/* ***************************************************************
 *
 * PROCEDURE:	lock.c
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		Fri Feb 22 08:23:36 MST 2002
 * 
 * AUTHOR:		Tom Green
 * 
 * MODIFICATION HISTORY:
 *				Tue Jan 18 18:36:41 MST 2005
 *				modified how fl_lock is used for the new file
 *				locking model.
 *				tomg
 *
 *				Wed Dec 28 18:04:18 MST 2005
 *				extended the lock concept further.  there is now
 *				the idea of a lock owner.  only a thread that
 *				holds a lock can remove it.  a thread can not
 *				hold more than one lock on a particular file.
 *				there is no such thing as lock promotion.  if
 *				you have a lock, you must release it before a
 *				new type of lock can be obtained.  failure to
 *				do so will cause a deadlock!  also implemented a
 *				queue for locks.  someone wating for a write
 *				lock could be shut out forever if read locks
 *				are	continually asserted and the predicate never
 *				reaches 0.
 *
 *				Fri Feb 10 19:57:24 MST 2006
 *				made the function detect where a deadlock based
 *				on the user trying to promote a lock would
 *				occur and return false in that case.
 *				tomg
 *
 ************************************************************* */
/*
 * perform file locking procedure.  There is a P_LOCK struct
 * associated with each open file.  This allows a lock to
 * only occur on the files, so multiple threads can continue
 * if they don't reference the same file at the same time.
 * this implements shared and exclusive locks with the use
 * of a condition.  If the value of the predicate is 0,
 * anyone can obtain a lock.  if the predicate is -1 this is
 * an exclusive lock, and there is only one available.  a
 * shared lock is when the predicate is > 0.  the predicate
 * is how many threads have the shared lock.  The biggest
 * problem here (and will have to be addressed) is that if a
 * thread terminates with the condition locked, then it
 * remains locked.
 *
 * the real question is, that since this is (so far) used just
 * in this project do I really need to fix this?  if a thread
 * terminates with a file locked, that would indicate a server
 * crash.
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

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#define LOCK_C

#include "lock.h"


/*
 * remove an entry from the owners or queue chain.  if it's not there
 * return false.
 */
static int remove_from_chain(OWNER **chain, pthread_t self)
{
	OWNER *optr;
	OWNER *last;

	optr = *chain;
	last = NULL;
	while(optr) {
		if (pthread_equal(optr->_owner, self))
			break;
		last = optr;
		optr = optr->_next;
	}
	if (optr) {
		if (last)
			last->_next = optr->_next;
		else
			*chain = optr->_next;
		free(optr);
		return(1);
	}
	return(0);
}

/*
 * add an entry to the owners or queue chain.  if it is already there
 * return false
 */
static int add_to_chain(OWNER **chain, pthread_t self, int type)
{
	OWNER *optr;

	optr = *chain;
	if (optr) {
		while (1) {
			if (pthread_equal(optr->_owner, self)) {
				if (optr->_type != type)
					return(-1);
				else
					return(0);
			}
			if (optr->_next == NULL)
				break;
			optr = optr->_next;
		}
		optr->_next= calloc(1, sizeof(OWNER));
		optr->_next->_owner = self;
		optr->_next->_type = type;
	} else {
		optr = calloc(1, sizeof(OWNER));
		optr->_owner = self;
		optr->_type = type;
		*chain = optr;
	}
	return(1);
}

/*
 * when trying to exert a shared lock and there is a queue we can
 * do it if our entry in the queue comes before any exclusive
 * lock.  So, if we come upon ourself, or the end of the chain
 * before we find an exclusive lock, return that it's ok to go
 * ahead and exert the lock.
 */
static int before_ex(OWNER *chain, pthread_t self)
{
	while(chain) {
		if (pthread_equal(chain->_owner, self))
			return(1);
		if (chain->_type == LOCK_EX)
			return(0);
		chain = chain->_next;
	}
	return(1);
}

/*
 * clean up from a thread termination while a file might
 * be locked
void lock_cleanup(void *ptr)
{
	P_LOCK *lptr;

	lptr = (P_LOCK *)ptr;
	pthread_t self;

	self = pthread_self();
	remove_from_chain(&lptr->owners,self);
	remove_from_chain(&lptr->queue, self);
}
 */


/*
 * routine to assert a lock on the file
 */
int fl_lock(P_LOCK *lock, int type)
{
	int retval;

	pthread_t self;

	pthread_mutex_lock(&lock->mutex);
	self = pthread_self();
/*
 * perform the different lock operations.  There is no such thing
 * as lock promotion.  you have to remove an obtained lock before
 * asserting a new type.
 */
	switch(type) {
		case LOCK_UN:
/*
 * remove a lock -
 * remove_from_chain returns false if this thread didn't
 * own a lock.  so, if it returns true, we can modify the
 * lock value and predicate.  if there are no locks being
 * held any longer broadcast to any process that might be
 * waiting to obtain one.
 */
			if (remove_from_chain(&lock->owners, self)) {
				if (lock->value < 0)
					lock->value++;
				else if (lock->value > 0)
					lock->value--;
/*
 * at this point the predicate (lock->value) can only be > -1)
 */
				if (!lock->value || (lock->value && lock->queue && lock->queue->_type == LOCK_SH))
					pthread_cond_broadcast(&lock->cond);
			}
			retval = 1;
			break;

		case LOCK_SH:
/*
 * obtain a shared lock -
 * we will only terminate this loop when we have obtained
 * our lock.  if there is an exclusive lock, or if there is
 * a queue and we are not before an exclusive lock, go to
 * sleep.  otherwise, we assert our lock (if we don't
 * already own one), and if there is a queue we remove
 * ourselves from it because we know we are at the head.
 * if there is another shared lock at the head of the queue
 * then broadcast so it can obtain a lock to.
 */
			while(1) {
				if (lock->value < 0 || (lock->queue && !before_ex(lock->queue, self))) {
					if (add_to_chain(&lock->queue, self, type) < 0) {
						retval = 0;
						break;
					}
					pthread_cond_wait(&lock->cond, &lock->mutex);
					continue;
				} else {
					if (add_to_chain(&lock->owners, self, type))
						lock->value++;
					if (lock->queue)
						remove_from_chain(&lock->queue, self);
					retval = 1;
					break;
				}
			}
			break;

		case LOCK_EX:
/*
 * obtain an exclusive lock-
 * this loop terminates when the lock has been obtained.
 * when the value of the predicate is 0, that indicates that
 * there are no locks currently on the file, so we can place
 * an exclusive lock.  but if we are queued behind any other
 * lock, we need to let the other go first.  add_to_chain
 * will not re-add us to the queue if we are already there.
 */
			while(1) {
				if (lock->value == 0 && (!lock->queue || pthread_equal(lock->queue->_owner,  self))) {
					add_to_chain(&lock->owners, self, type);
					lock->value--;
					if (lock->queue)
						remove_from_chain(&lock->queue, self);
					retval = 1;
					break;
				} else {
					if (add_to_chain(&lock->queue, self, type) < 0) {
						retval = 0;
						break;
					}
					pthread_cond_wait(&lock->cond, &lock->mutex);
					continue;
				}
			}
			break;
	}
	pthread_mutex_unlock(&lock->mutex);
	return(retval);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
