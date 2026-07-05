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

#include <fileEdit.hh>
#include <db_comm.hh>

#include "../../server/dbfunc.h"
#include "../../server/errors.h"

using namespace Dataman;

void index::insert(int fmt, int mode)
{

	int tmp;

	char cmd[128];
	char *buff;
	char *ptr;

	db_comm comm;

	if (cur_index && cur_index->_wrmode)
    	master.out_rec();			/* write out the current record */
	if (!this->_wrmode)
		comm.db_err(0, "%s: index %s not opened for update\n",
						_progname, this->_idxname);

	if (fmt < 1 || fmt > this->_files[this->_fno].get_desc()->n_rformats)
		comm.db_err(EBADFMT, "%s: insert error: fmt=%d, file=%s\n",
						_progname,fmt, this->_files[this->_fno].get_fname());

	if (!this->_curkey.get_len())					/* can't insert around nothing */
		comm.db_err(ENOGET, "%s: insert error", _progname);

	if (!in_xact && this->_rptr < 0)
		comm.db_err(0, "%s: memory corruption detected in insert", _progname);
	if (this->_idxno < 0 || this->_idxno > MAX_INDEX || this->_fno < 0 || this->_fno > this->_nfiles)
		comm.db_err(0, "%s: memory corruption detected in insert", _progname);

	sprintf(cmd, "%d|%d|%d|%d|%d|%"PRId64"|", INSERT, fmt, mode,
					this->_idxno, this->_fno, this->_rptr);
	try {
		buff = comm.db_send(cmd, strlen(cmd));
	}
	catch (int comm_err) {
		comm.db_err(0, "%s: socket read error in insert", _progname);
	}

	tmp = atoi(buff);
	if (tmp < 0)
		comm.db_err(tmp, "%s: insert error", _progname);

	master.fmt = fmt;
 
	master.chan = this->_files[this->_fno].get_fno();
	master._filedesc = this->_files[this->_fno].get_desc();
	tmp = this->_files[this->_fno].get_desc()->record_desc[fmt-1].rf_len;

	master.len = tmp;
	ptr = strchr(buff, '|') + 1;
	this->_rptr = strtoll(ptr, NULL, 0);
	master.cur = this->_rptr;

	ptr = new char[master.len+1];
	memset(ptr, ' ', master.len);
	*(ptr+master.len) = '\0';
    cur_index = this;				/* save the new current index */
    master.in_rec(ptr);				/* read in the empty record */
	delete[] ptr;
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
