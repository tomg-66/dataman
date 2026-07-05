/* ***************************************************************
 *
 * PROCEDURE:	parse_get.cc
 *
 * PROJECT:		dataman client side routines in c++
 * 
 * DATE:		Fri Apr 18 20:41:44 MDT 2003
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
 * one common routine that parses the return string from all of
 * the get...() functions
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
#include <stdlib.h>

#include <fileEdit.hh>

#include "../../server/misc.h"

using namespace Dataman;

void index::parse_get(int i, char *buff)
{
	char *cptr;

//	master.setlen(i);
	master.len = i;
	cptr = strchr(buff, '|') + 1;
//	master.setfmt(atoi(cptr));
	master.fmt = atoi(cptr);
	cptr = strchr(cptr, '|') + 1;
	this->_curnode = strtoll(cptr, NULL, 0);
	cptr = strchr(cptr, '|') + 1;
	this->_offs = atoi(cptr);
	cptr = strchr(cptr, '|') + 1;
//	if (this->_curkey)
//		delete[] this->_curkey;
//	this->_curkey = new char[this->_keylen+5];
//	memcpy(this->_curkey, cptr, this->_keylen+5);
	key tmp_key(cptr, this->_keylen);
	this->_curkey = tmp_key;

	this->_fno = this->_curkey.get_fno() - 1;
	cptr += this->_keylen + KEY_HEADER_LENGTH;

	master.chan = this->_curkey.get_fno() - 1;
	this->_rptr = this->_curkey.get_rec();
	master.cur = this->_rptr;
	master._filedesc = this->_files[master.chan].get_desc();
	master.head = this->_files[master.chan].get_hlen();
	master.longest = this->_files[master.chan].get_longest();

	cur_index = this;
	master.in_rec(cptr);
	delete[] buff ;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
