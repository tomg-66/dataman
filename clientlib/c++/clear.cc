/* ***************************************************************
 *
 * PROCEDURE:	clear.c
 *
 * PROJECT:		dataman client side, c++ library
 * 
 * DATE:		Fri May 16 21:25:01 MDT 2003
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
 * this routine clears the protection on a record that was 'checked out'
 * with a call to protect.  it's calling sequence is:
 *      clear(idx_name)
 * where idx_name is either the name of an IOPENed index or a NULL (meaning
 * to back the work file).
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

#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#include <fileEdit.hh>
#include <db_comm.hh>

#include "../../server/dbfunc.h"

using namespace Dataman;

using Dataman::master;

void index::clear()

{
	int i;

	char cmd[128];
	char *buff;

	if (in_xact && this->_rptr < 0)
		return;
	if (cur_index && cur_index->get_wrmode())
		master.out_rec();

	db_comm comm;
	if (this->_idxno < 0 || this->_idxno > MAX_INDEX || this->_fno < 0 || this->_fno > this->_nfiles || this->_rptr < 0)
		comm.db_err(0, "%s: memory corruption detected in clear", _progname);
	sprintf(cmd, "%d|%d|%d|%" PRId64 "|", CLEAR, this->_idxno, this->_fno, this->_rptr);

	try {
		buff = comm.db_send(cmd, strlen(cmd));
	}
	catch (int comm_err) {
		comm.db_err(0, "%s: socket read error in clear", _progname);
	}

	i = atoi(cmd);
	if (i < 0)
		comm.db_err(i, "%s: error in clear", _progname);
	delete[] buff;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
