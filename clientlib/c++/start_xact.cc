/* ***************************************************************
 *
 * PROCEDURE:	start_transaction.cc
 *
 * PROJECT:		dataman client side c++ library
 * 
 * DATE:		Wed Jul  5 21:24:04 MDT 2006
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
 * this routine starts a transaction.  it should be bounded
 * by either a rollback or commit call.
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

#include <stdlib.h>
#include <stdio.h>

#include "fileEdit.hpp"
#include "db_comm.hpp"
#include "datamanError.hpp"

#include "../../server/dbfunc.h"
#include "../../server/misc.h"

#include <memory>

#define TRUE    1
#define FALSE   0

using Dataman::in_xact;
using Dataman::_progname;

using namespace Dataman;

void start_transaction(void)
{
    int i;								/* temporary */

	char cmd[128];

	i = sprintf(cmd, "%d|", START_XACT);
/*
 * send the command and deal with the return.
 */
	Dataman::db_comm comm;
	std::unique_ptr<char[]> buff(comm.db_send(cmd, i));
/*
 * the first field of the return is an error code if necessary,
 * otherwise the command worked.
 */
	i = atoi(buff.get());

	if (i < 0) {
		throw makeError(i, "%s: Error during START_TRANSACTION", _progname);
	}
	in_xact = TRUE;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
