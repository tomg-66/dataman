/* ***************************************************************
 *
 * PROCEDURE:	save.cc
 *
 * PROJECT:		dataman client side c++ routines
 * 
 * DATE:		Tue Apr 22 19:19:10 MDT 2003
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
 * this procedure saves the state of an index.
 * the calling sequence is:
 *      save(index_name);
 * where index_name is the name of the index whose state you want to save.
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

#include <malloc.h>
#include <stdio.h>

#include "fileEdit.hpp"
#include "db_comm.hpp"
#include "datamanError.hpp"

#include "../../server/errors.h"

using namespace Dataman;

void index::save()

{

	db_comm comm;

	if (this->_curkey.get_len() == 0)
		throw makeError(ENOGET, "%s: error in save", _progname);

	if (this->_idxno < 0 || this->_idxno > MAX_INDEX || this->_fno < 0 || this->_fno > this->_nfiles)
		throw makeError(0, "%s: memory corruption detected in save", _progname);


	if (!this->_savptr)
		this->_savptr = (struct save *)malloc(sizeof(struct save));	/* get new space */

	this->_savptr->_savnode = this->_curnode;
	this->_savptr->_savrec = this->_rptr;
	this->_savptr->_savkey = this->_curkey;
	this->_savptr->_savfile = this->_fno;
	this->_savptr->_savfmt = masterRecord.getfmt();
	this->_savptr->_savoffs = this->_offs;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
