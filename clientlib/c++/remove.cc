/* ***************************************************************
 *
 * PROCEDURE:	remove.cc
 *
 * PROJECT:		dataman client side c++ library routine
 * 
 * DATE:		Sat Dec 11 11:05:02 MST 2004
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 * 				Thu Mar 21 15:49:21 MDT 2013
 * 				Tom Green
 * 				added name space
 *
 ************************************************************* */
//
// this function removes a key from the named index.  it does
// not remove the associated data record.  since you don't
// have to have a key on every record.
//
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

#include <string.h>
#include <malloc.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <fileEdit.hh>
#include <db_comm.hh>

#include "../../server/dbfunc.h"
#include "../../server/misc.h"

#define TRUE	1
#define FALSE	0

using namespace Dataman;

int index::remove(const key& k)
{

	int i;
	int tmp;

    char cmd[128];			/* command */
	char *buff;

	key *ptr = (key *)&k;

	db_comm comm;
	if (!this->_wrmode)
		comm.db_err(0, "%s: in remove - index %s not opened for update\n",
						_progname, this->_idxname);
	if (strchr(ptr->get_kstr(), '*'))
		comm.db_err(0, "%s: can't use wildcard in remove\n", _progname);

	if (ptr->get_fno())
		tmp = this->_keylen + KEY_HEADER_LENGTH;
	else
		tmp = strlen(ptr->get_kstr());

	if (this->_idxno < 0 || this->_idxno > MAX_INDEX || this->_fno < 0 || this->_fno > this->_nfiles)
		comm.db_err(0, "%s: memory corruption detected in remove", _progname);

	i = sprintf(cmd, "%d|%d|%d|", REMOVE, this->_idxno, in_xact);
	::memcpy(cmd+i, ptr->get_data(), tmp);
	i += tmp;

	try {
		buff = comm.db_send(cmd, i);
	}
	catch (int comm_err) {
		comm.db_err(0, "%s: Can't read socket response in REMOVE", _progname);
	}

	i = atoi(buff);
	delete [] buff;
	if (i < 0)
		comm.db_err(i, "%s: remove error", _progname);
	else if (i == 0)
		return(FALSE);
	return(TRUE);
}

int index::remove(const char *s)
{
	key k(s);
	return(remove(k));
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
