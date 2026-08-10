/* ***************************************************************
 *
 * PROCEDURE:	include.cc
 *
 * PROJECT:		dataman client side c++ library routine
 * 
 * DATE:		Fri Jun 18 12:03:38 MDT 2004
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
 * this routine inserts a key pointing to the current master record
 * of one index file into another index (potentially the same) index
 * file.
 * the calling sequence is:
 *
 *      idx2.include(idx1,key);
 *
 *      where idx1 is the source of the record to insert, idx2 is the
 *      destination of the key.
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
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "fileEdit.hpp"
#include "db_comm.hpp"
#include "datamanError.hpp"

#include "../../server/dbfunc.h"

#include <memory>

extern void put_ll(void *, int64_t);

#define FALSE 0
#define TRUE  1

using namespace Dataman;

int index::include(index *idx_1, const char *key)
{

    int tmp;						/* temporary, misc. usage */
    char buff[128];					/* output buffer */
	char *cptr;

	if (!idx_1 || !key)
		return FALSE;

	if (!this->get_wrmode()) {
		db_err(0, "%s: index %s not opened for update\n", _progname, this->get_ixname());
		return FALSE;
	}
/*
 * make sure the file currently referred to in 'this' is also
 * found in idx_1
 */
	for (tmp = 0; tmp < this->get_nfiles(); tmp++) {
		if (!strcmp(this->_files[tmp].get_fname(), (idx_1->get_file(idx_1->get_fno()))->get_fname()))
			break;
	}
	if (tmp >= this->get_nfiles()) {
		db_err(0, "%s: Include error: file %s not a member of index %s\n",
				_progname, idx_1->_files[idx_1->get_fno()].get_fname(), this->get_ixname());
		return FALSE;
	}
	this->_fno = tmp;

	db_comm comm;
	if (!in_xact && idx_1->get_rptr() < 0) {
		db_err(0, "%s: memory corruption detected in include", _progname);
		return FALSE;
	}
	if (this->_idxno < 0 || this->_idxno >= MAX_INDEX || this->_fno < 0 || this->_fno >= this->_nfiles) {
		db_err(0, "%s: memory corruption detected in include", _progname);
		return FALSE;
	}
	if (idx_1->get_idxno() < 0 || idx_1->get_idxno() >= MAX_INDEX ||
			idx_1->get_fno() < 0 || idx_1->get_fno() >= idx_1->get_nfiles()) {
		db_err(0, "%s: memory corruption detected in include", _progname);
		return FALSE;
	}

	sprintf(buff, "%d|%d|%d|%d|%d|%" PRId64 "|%s|", INCLUDE, idx_1->get_idxno(),
				idx_1->get_fno(), this->_idxno, this->_fno, idx_1->get_rptr(), key);
	std::unique_ptr<char[]> ret(comm.db_send(buff, strlen(buff)));

	tmp = atoi(ret.get());
	if (tmp < 0)
		throw makeError(tmp, "%s: error in include",_progname);
	if (tmp == 0)
		return FALSE;

	cptr = strchr(ret.get(), '|') + 1;
	this->_generation = strtoull(cptr, NULL, 10);
	cptr = strchr(cptr, '|') + 1;
	this->_curnode = strtoll(cptr, NULL, 0);
	cptr = strchr(cptr, '|') + 1;
	this->_offs = atoi(cptr);
	if (in_xact) {
		memset(buff, '\0', sizeof(buff));
		::strncpy(buff, key, this->_keylen);
		*(buff+this->_keylen) = this->_fno+1;
		put_ll(buff+this->_keylen+1, idx_1->_rptr);
		cptr = buff;
	} else
		cptr = strchr(cptr, '|') + 1;
	class key tmp_key(cptr, this->get_keylen());
	this->_curkey = tmp_key;
	this->_fno = this->_curkey.get_fno() - 1;
	return TRUE;
}

int index::include(index& idx_1, datafield& d)
{
	return this->include(&idx_1, d.getptr());
}

int index::include(index& idx_1, const char *s)
{
	return this->include(&idx_1, s);
}

int index::include(index *idx_1, datafield& d)
{
	return this->include(idx_1, d.getptr());
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
