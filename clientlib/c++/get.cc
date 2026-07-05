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

#include <fileEdit.hh>
#include <db_comm.hh>

#include "../../server/dbfunc.h"
#include "../../server/misc.h"

#define TRUE    1
#define FALSE   0

using namespace Dataman;

int index::get(const key &info)
{
    int i;								/* temporary */

	char cmd[128];
	char *buff;
	key *ptr = (key *)&info;

	const char *kpt = ptr->get_data();

	if (cur_index && cur_index->get_wrmode())
		master.out_rec();					/* flush the current record */

	db_comm comm;
	if (this->_idxno < 0 || this->_idxno > MAX_INDEX || this->_fno < 0 || this->_fno > this->_nfiles)
		comm.db_err(0, "%s: memory corruption detected in get", _progname);

/*
 * if this byte isn't a null (we should be getting a proper key)
 * this indicates that they are using the system KEY, and the
 * get_current function is better suited to that.
 */
    if (*(kpt+this->_keylen) != 0) {
		sprintf(cmd, "%d|%d|%" PRId64 "|%d|", GET_CURRENT, this->_idxno,
						this->_curnode, this->_offs);
		i = strlen(cmd);
		::memcpy(cmd+i, kpt, this->_keylen+KEY_HEADER_LENGTH);
		i += this->_keylen+KEY_HEADER_LENGTH;
	} else {
		sprintf(cmd, "%d|%d|%s|", GET, this->_idxno, kpt);
		i = strlen(cmd);
    }
/*
 * send the command and deal with the return.
 */
	try {
		buff = comm.db_send(cmd, i);
	}
	catch (int comm_err) {
		comm.db_err(0, "%s: Can't read socket response in GET", _progname);
	}
/*
 * the first field of the return is an error code if necessary,
 * zero of the key wasn't found, or the length of the data record
 */
	i = atoi(buff);
	if (i < 0)
		comm.db_err(i, "%s: Error during GET", _progname);
	else if (i == 0) {
		delete[] buff;
		return(FALSE);
	}
/*
 * parse the return and update the globals
 */
	this->parse_get(i, buff);
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
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
