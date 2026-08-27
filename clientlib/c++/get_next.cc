/* ***************************************************************
 *
 * PROCEDURE:	get_next.cc
 *
 * PROJECT:		dataman client routines for c++ lib
 * 
 * DATE:		Thu Apr 17 21:21:41 MDT 2003
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
 * get the next key and its record from an index
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
#include <malloc.h>
#include <string.h>
#include <inttypes.h>

#include "fileEdit.hpp"
#include "db_comm.hpp"
#include "datamanError.hpp"

#include "../../server/dbfunc.h"
#include "../../server/errors.h"
#include "../../server/misc.h"

#include <memory>

#define TRUE    1
#define FALSE   0

using namespace Dataman;

int index::get_next()
{
    int i;			/* misc usage */

	char msg[128];

	db_comm comm;

	if (in_xact && this->_curkey.get_rec() < 0)
		return(FALSE);

	if (cur_index && cur_index->get_wrmode())
		masterRecord.out_rec();

    if (this->_curkey.get_len() == 0) {
        db_err(ENOGET, "%s: Can't get_next", _progname);
		return FALSE;
	}

	if (this->_idxno < 0 || this->_idxno >= MAX_INDEX || this->_fno < 0 || this->_fno >= this->_nfiles) {
		db_err(0, "%s: memory corruption detected in get_next", _progname);
		return FALSE;
	}

	sprintf(msg, "%d|%d|%" PRId64 "|%" PRId64 "|%d|", GET_NEXT,
					this->_idxno, this->_generation, this->_curnode, this->_offs);
	i = strlen(msg);
	::memcpy(msg+i, this->_curkey.get_data(), this->_keylen+KEY_HEADER_LENGTH);
	i += this->_keylen+KEY_HEADER_LENGTH;

	std::unique_ptr<char[]> ret(comm.db_send(msg, i));

	i = atoi(ret.get());
	if (i < 0)
		throw makeError(i, "%s: error during get_next", _progname);
	if (i == 0)
		return FALSE;
/*
 * parse the return and update the globals
 */
	i = this->parse_get(i, ret.get());
	if (!i)
		return FALSE;
	return TRUE;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
