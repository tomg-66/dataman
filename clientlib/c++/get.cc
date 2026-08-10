/* ***************************************************************
 *
 * PROCEDURE:	get.cc
 *
 * PROJECT:		dataman client side c++ routines
 * 
 * DATE:		Thu Apr 17 20:09:28 MDT 2003
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
 * get a record from the database server.  the user passes in
 * the key that is required from the index.
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

#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#include "fileEdit.hpp"
#include "db_comm.hpp"
#include "datamanError.hpp"

#include "../../server/dbfunc.h"
#include "../../server/misc.h"

#include <memory>

#define TRUE    1
#define FALSE   0

using namespace Dataman;

int index::get(const key &info)
{
    int i;								/* temporary */

	char cmd[128];
	key *ptr = (key *)&info;

	const char *kpt = ptr->get_data();

	if (cur_index && cur_index->get_wrmode()) {
		masterRecord.out_rec();				/* flush the current record */
	}

	db_comm comm;
	if (this->_idxno < 0 || this->_idxno > MAX_INDEX || this->_fno < 0 || this->_fno > this->_nfiles) {
		db_err(0, "%s: memory corruption detected in get", _progname);
		return FALSE;
	}

/*
 * if this byte isn't a null (we should be getting a proper key)
 * this indicates that they are using the system KEY, and the
 * get_current function is better suited to that.
 */
    if (*(kpt+this->_keylen) != 0) {
		i = sprintf(cmd, "%d|%d|%" PRId64 "|%" PRId64 "|%d|", GET_CURRENT, this->_idxno,
						this->_generation, this->_curnode, this->_offs);
		::memcpy(cmd+i, kpt, this->_keylen+KEY_HEADER_LENGTH);
		i += this->_keylen+KEY_HEADER_LENGTH;
	} else {
		i = sprintf(cmd, "%d|%d|%s|", GET, this->_idxno, kpt);
    }
/*
 * send the command and deal with the return. Let callers deal
 * with exceptions.
 */
	std::unique_ptr<char[]> buff(comm.db_send(cmd, i));

/*
 * the first field of the return is an error code if necessary,
 * zero if the key wasn't found, or the length of the data record
 */
	i = atoi(buff.get());

	if (i < 0)
		throw makeError(i, "%s: Error during GET", _progname);
	if (i == 0)
		return(FALSE);
/*
 * parse the return and update the globals
 */
	i = this->parse_get(i, buff.get());
	if (!i)
		return FALSE;
	return(TRUE);
}

int index::get(const char *stuff)
{
	key gbuf;
	gbuf = (char *)stuff;
	return(this->get(gbuf));
}

int index::get(datafield& d)
{
	key gbuf;
	gbuf = d.getptr();
	return(this->get(gbuf));
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
