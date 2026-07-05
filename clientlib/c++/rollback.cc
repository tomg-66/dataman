/* ***************************************************************
 *
 * PROCEDURE:	rollback.cc
 *
 * PROJECT:		dataman client side c++ library
 * 
 * DATE:		Wed Jul  5 21:46:27 MDT 2006
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
/*
 * this routine rolls back a transaction.  it has to end a block
 * that began with a start_transaction.  it makes sure nothing
 * since the start_transaction call has been commited.
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

#include <stdio.h>
#include <stdlib.h>

#include <fileEdit.hh>
#include <db_comm.hh>

#include "../../server/dbfunc.h"
#include "../../server/misc.h"

#define TRUE    1
#define FALSE   0

using Dataman::in_xact;
using Dataman::_progname;

/*
 * this isn't a member of a class.  just a callable function
 */
void rollback(void)
{
    int i;								/* temporary */

	char cmd[128];
	char *buff;

	i = sprintf(cmd, "%d|", ROLLBACK);
/*
 * send the command and deal with the return.
 */
	Dataman::db_comm comm;
	try {
		buff = comm.db_send(cmd, i);
	}
	catch (int comm_err) {
		comm.db_err(0, "%s: Can't read socket response in ROLLBACK", _progname);
	}
/*
 * the first field of the return is an error code if necessary,
 * otherwise the command worked.
 */
	i = atoi(buff);
	if (i < 0)
		comm.db_err(i, "%s: Error during ROLLBACK", _progname);
	delete [] buff;
	in_xact = FALSE;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
