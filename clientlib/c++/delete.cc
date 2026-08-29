/* ***************************************************************
 *
 * PROCEDURE:	delete.cc
 *
 * PROJECT:		dataman client side c++ library routine
 * 
 * DATE:		legacy, originally written in 1988
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
 * this routine deletes the current record pointed to by the named index.
 * the calling sequence is:
 *      delete(idx_name);
 * where idx_name is any index that is open and has a record.  the current
 * record usually then becomes the one logically prior to the deleted one.
 * if the deleted record is the first of the file, the current record
 * becomes the record that logically followed the deleted record.
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
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#include "fileEdit.hpp"
#include "db_comm.hpp"
#include "datamanError.hpp"

#include "../../server/dbfunc.h"
#include "../../server/errors.h"

#include <memory>

using namespace Dataman;

#define FALSE 0
#define TRUE  1

int index::delrec()
{

	int recordLen;

	char cmd[128];				/* command to send */
	char *ptr;

	db_comm comm;

	if (cur_index && cur_index->get_wrmode())
		masterRecord.out_rec();			/* write out the current record */

    if (this->_rptr == 0) {			/* can't del if we don't have it */
		db_err(ENOGET, "%s: delete error", _progname);
		return FALSE;
	}

	if (!in_xact && this->_rptr < 0) {
		db_err(0, "%s: memory corruption detected in delete", _progname);
		return FALSE;
	}
	if (this->_idxno < 0 || this->_idxno > MAX_INDEX || this->_fno < 0 || this->_fno > this->_nfiles) {
		db_err(0, "%s: memory corruption detected in delete", _progname);
		return FALSE;
	}

	sprintf(cmd, "%d|%d|%d|%" PRId64 "|%d|", DELETE, this->_idxno,
					this->_fno, this->_rptr, in_xact);
	std::unique_ptr<char[]> buff(comm.db_send(cmd, strlen(cmd)));

	recordLen = atoi(buff.get());
	if (recordLen < 0)
		throw makeError(recordLen, "%s: communication error", _progname);
	if (recordLen == 0)
		return FALSE;

	ptr = strchr(buff.get(), '|') + 1;
	int64_t recordPointer = strtoll(ptr, NULL, 0);
	ptr = strchr(ptr, '|') + 1;
	int fmt = atoi(ptr);
	ptr = strchr(ptr, '|') + 1;

	datarecord tmpRec(MASTER);

	tmpRec._filedesc = this->_files[this->_fno].get_desc();
	tmpRec.chan = this->_fno;
	tmpRec.cur = recordPointer;
	tmpRec.head = this->_files[this->_fno].get_hlen();
	tmpRec.len = recordLen;
	tmpRec.fmt = fmt;

	if (!tmpRec.in_rec(ptr, this))
		return FALSE;

	{
		datafield *oldFields = masterRecord._fields;
		masterRecord.fmt = fmt;
		masterRecord.cur = recordPointer;
		masterRecord.chan = this->_fno;
		masterRecord._filedesc = this->_files[masterRecord.chan].get_desc();
		masterRecord.head = this->_files[masterRecord.chan].get_hlen();
		masterRecord.len = recordLen;
		masterRecord._fields = tmpRec._fields;
		tmpRec._fields = oldFields;
	}

	this->_rptr = recordPointer;
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
