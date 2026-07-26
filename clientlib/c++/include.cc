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

#include <fileEdit.hh>
#include <db_comm.hh>

#include "../../server/dbfunc.h"

extern void db_err(int, const char *, ...);
extern void put_ll(void *, int64_t);

using namespace Dataman;

void index::include(index *idx_1, const char *key)
{

    int tmp;						/* temporary, misc. usage */
    char buff[128];					/* output buffer */
	char *cptr;
	char *ret;

	if (!this->get_wrmode())
		db_err(0, "%s: index %s not opened for update\n",
						_progname, this->get_ixname());
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
	}
	this->_fno = tmp;

	db_comm comm;
	if (!in_xact && (this->_rptr < 0 || idx_1->get_rptr() < 0))
		comm.db_err(0, "%s: memory corruption detected in include", _progname);
	if (this->_idxno < 0 || this->_idxno > MAX_INDEX || this->_fno < 0 || this->_fno > this->_nfiles)
		comm.db_err(0, "%s: memory corruption detected in include", _progname);
	if (idx_1->get_idxno() < 0 || idx_1->get_idxno() > MAX_INDEX || idx_1->get_fno() < 0 || idx_1->get_fno() > idx_1->get_nfiles() < 0)
		comm.db_err(0, "%s: memory corruption detected in include", _progname);

	sprintf(buff, "%d|%d|%d|%d|%d|%" PRId64 "|%s|", INCLUDE, idx_1->get_idxno(),
				idx_1->get_fno(), this->_idxno, this->_fno, idx_1->get_rptr(), key);
	try {
		ret = comm.db_send(buff, strlen(buff));
	}
	catch (int tmp) {
		comm.db_err(0, "%s: Can't read socket response in INCLUDE", _progname);
	}

	tmp = atoi(ret);
	if (tmp < 0)
		comm.db_err(tmp, "%s: error in include",_progname);

	this->_offs = tmp;
	cptr = strchr(ret, '|') + 1;
	this->_curnode = strtoll(cptr, NULL, 0);
	if (in_xact) {
		memset(buff, '\0', sizeof(buff));
		strcpy(buff, key);
		*(buff+this->_keylen) = idx_1->_fno+1;
		put_ll(buff+this->_keylen+1, idx_1->_rptr);
		cptr = buff;
	} else
		cptr = strchr(cptr, '|') + 1;
	class key *tmp_key = new class key(cptr, this->get_keylen());
	this->_curkey = *tmp_key;
	this->_fno = this->_curkey.get_fno() - 1;
	delete[] ret;
}

void index::include(index& idx_1, datafield& d)
{
	this->include(&idx_1, d.getptr());
}

void index::include(index& idx_1, const char *s)
{
	this->include(&idx_1, s);
}

void index::include(index *idx_1, datafield& d)
{
	this->include(idx_1, d.getptr());
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
