/* ***************************************************************
 *
 * PROCEDURE:	back.cc
 *
 * PROJECT:		dataman client side c++ routine
 * 
 * DATE:		Thu May  1 21:18:26 MDT 2003
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
 * this routine releases the current record in the database and moves
 * to the previous logical record in the database.
 * the calling sequence (using the #define) is:
 *      back(idx_name)
 * where idx_name is either the name of an IOPENed index or a NULL (meaning
 * to back the work file).  the internal sequence is:
 *      if (bck(idx_name)) ;
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
#include <malloc.h>
#include <string.h>
#include <stdio.h>
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

int index::back()

{
	int i;
	int fmt;

	int64_t curr;

	char cmd[128];
	char *cptr;

	db_comm comm;

	if (in_xact && this->_rptr < 0)
		return(FALSE);

	if (cur_index && cur_index->get_wrmode())
		masterRecord.out_rec();

	if (this->_curkey.get_len() == 0) {
		db_err(ENOGET, "%s: error in back", _progname);
		return FALSE;
	}

	if (this->_idxno < 0 || this->_idxno > MAX_INDEX || this->_fno < 0 || this->_fno > this->_nfiles || this->_rptr < 0) {
		db_err(0, "%s: memory corruption detected in back", _progname);
		return FALSE;
	}

	sprintf(cmd, "%d|%d|%d|%" PRId64 "|", BACK, this->_idxno, this->_fno, this->_rptr);
	std::unique_ptr<char[]> buff(comm.db_send(cmd, strlen(cmd)));

	i = atoi(buff.get());
	if (i < 0)
		throw makeError(i, "%s: error in back", _progname);
	if (i == 0)
		return(FALSE);					/* no next record in this file */

	cptr = strchr(buff.get(), '|') + 1;
	curr = strtoll(cptr, NULL, 0);
	cptr = strchr(cptr, '|') + 1;
	fmt = atoi(cptr);
	cptr = strchr(cptr, '|') + 1;

	datarecord tmpRec(MASTER);

	tmpRec._filedesc = this->_files[this->_fno].get_desc();
	tmpRec.cur = curr;
	tmpRec.head = this->_files[this->_fno].get_hlen();
	tmpRec.len = i;
	tmpRec.fmt = fmt;

	if (!tmpRec.in_rec(cptr)) {
		return FALSE;
	}

	{
		datafield *oldFields = masterRecord._fields;
		masterRecord.cur = curr;
		masterRecord._filedesc = this->_files[this->_fno].get_desc();
		masterRecord.head = this->_files[this->_fno].get_hlen();
		masterRecord.len = i;
		masterRecord.fmt = fmt;
		masterRecord._fields = tmpRec._fields;
		tmpRec._fields = oldFields;
	}
	this->_rptr = curr;

	cur_index = this;
	return(TRUE);						/* give the ok signal */
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
