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

#include <fileEdit.hpp>

#include "../../server/misc.h"

using namespace Dataman;

/*
 * this buffer only comes from the server.  we assume that it's formatted correctly.
 */
int index::parse_get(int recordLength, char *buff)
{
	char *cptr;
	char *next;
	int formatNumber;
	int fileNumber;
	int offs;
	int64_t generation;
	int64_t node;
	int64_t recordPointer;

/*
 * parse into locala variables.  since this is coming from the server
 * (and the connection is verified) assume the data is in correct order
 */
	cptr = strchr(buff, '|') + 1;
	formatNumber = atoi(cptr);
	cptr = strchr(cptr, '|') + 1;
	generation = strtoll(cptr, NULL, 0);
	cptr = strchr(cptr, '|') + 1;
	node = strtoll(cptr, NULL, 0);
	cptr = strchr(cptr, '|') + 1;
	offs = atoi(cptr);
	cptr = strchr(cptr, '|') + 1;

	key tmpKey(cptr, this->_keylen);

	fileNumber = tmpKey.get_fno() - 1;
	if (fileNumber < 0 || fileNumber >= this->_nfiles)
		return FALSE;
	recordPointer = tmpKey.get_rec();
	cptr += this->_keylen + KEY_HEADER_LENGTH;
/*
 * now put together the replacement master record.  if in_rec()
 * fails tmpRec cleans up its fields and the current master record
 * remains unchanged
 */
	datarecord tmpRec(MASTER);
	tmpRec.len = recordLength;
	tmpRec.fmt = formatNumber;
	tmpRec.chan = fileNumber;
	tmpRec.cur = recordPointer;
	tmpRec._filedesc = this->_files[fileNumber].get_desc();
	tmpRec.head = this->_files[fileNumber].get_hlen();
	tmpRec.longest = this->_files[fileNumber].get_longest();

	if (!tmpRec.in_rec(cptr))
		return FALSE;
/*
 * parsing and allocations have succeded.  publish the new index state.
 * key assignment copies its fixed size internal storage
 */
	this->_generation = generation;
	this->_curnode = node;
	this->_offs = (unsigned char)offs;
	this->_curkey = tmpKey;
	this->_fno = fileNumber;
	this->_rptr = recordPointer;
/*
 * transfer tmpRec's newly allocate fields to master.  swapping the
 * pointers lets tmpRec's destructor reclaim the master records old fields
 */
	{
		datafield *oldFields = masterRecord._fields;
		masterRecord.len = tmpRec.len;
		masterRecord.fmt = tmpRec.fmt;
		masterRecord.chan = tmpRec.chan;
		masterRecord.cur = tmpRec.cur;
		masterRecord._filedesc = tmpRec._filedesc;
		masterRecord.head = tmpRec.head;
		masterRecord.longest = tmpRec.longest;
		masterRecord._dirty = tmpRec._dirty;
		masterRecord._fields = tmpRec._fields;
		tmpRec._fields = oldFields;
	}

	cur_index = this;
	return TRUE;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
