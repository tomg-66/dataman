/* ***************************************************************
 *
 * PROCEDURE:	insert.cc
 *
 * PROJECT:		dataman client side c++ routines
 * 
 * DATE:		Mon Apr 21 22:32:59 MDT 2003
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
 * insert a new data record into a data file
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
#include <malloc.h>
#include <stdio.h>
#include <inttypes.h>

#include "fileEdit.hpp"
#include "db_comm.hpp"
#include "datamanError.hpp"

#include "../../server/dbfunc.h"
#include "../../server/errors.h"

#include <memory>

#define FALSE 0
#define TRUE  1

using namespace Dataman;

int index::insert(int fmt, int mode)
{

	int tmp;

	char cmd[128];
	char *ptr;

	db_comm comm;

	if (cur_index && cur_index->_wrmode)
    	masterRecord.out_rec();			/* write out the current record */
	if (!this->_wrmode) {
		db_err(0, "%s: index %s not opened for update\n",
						_progname, this->_idxname);
		return FALSE;
	}

	if (fmt < 1 || fmt > this->_files[this->_fno].get_desc()->n_rformats) {
		db_err(EBADFMT, "%s: insert error: fmt=%d, file=%s\n",
						_progname,fmt, this->_files[this->_fno].get_fname());
		return FALSE;
	}

	if (!this->_curkey.get_len()) {					/* can't insert around nothing */
		db_err(ENOGET, "%s: insert error", _progname);
		return FALSE;
	}

	if (!in_xact && this->_rptr < 0) {
		db_err(0, "%s: memory corruption detected in insert", _progname);
		return FALSE;
	}
	if (this->_idxno < 0 || this->_idxno >= MAX_INDEX || this->_fno < 0 || this->_fno >= this->_nfiles) {
		db_err(0, "%s: memory corruption detected in insert", _progname);
		return FALSE;
	}

	sprintf(cmd, "%d|%d|%d|%d|%d|%" PRId64 "|", INSERT, fmt, mode,
					this->_idxno, this->_fno, this->_rptr);
	std::unique_ptr<char[]> buff(comm.db_send(cmd, strlen(cmd)));

	tmp = atoi(buff.get());
	if (tmp < 0)
		throw makeError(tmp, "%s: insert error", _progname);
	if (tmp == 0)
		return FALSE;

	masterRecord.fmt = fmt;
 
	masterRecord.chan = this->_files[this->_fno].get_fno();
	masterRecord._filedesc = this->_files[this->_fno].get_desc();
	tmp = this->_files[this->_fno].get_desc()->record_desc[fmt-1].rf_len;

	masterRecord.len = tmp;
	ptr = strchr(buff.get(), '|') + 1;
	this->_rptr = strtoll(ptr, NULL, 0);
	masterRecord.cur = this->_rptr;

	ptr = new char[masterRecord.len+1];
	memset(ptr, ' ', masterRecord.len);
	*(ptr+masterRecord.len) = '\0';

    if (!masterRecord.in_rec(ptr, this)) {				/* read in the empty record */
		return FALSE;
	}

    cur_index = this;				/* save the new current index */
	delete[] ptr;
	return TRUE;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
