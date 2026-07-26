/* ***************************************************************
 *
 * PROCEDURE:	forward.cc
 *
 * PROJECT:		dataman client side c++ routines
 * 
 * DATE:		Thu May  1 21:33:36 MDT 2003
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
 * to the next logical record in the database.
 * the calling sequence (using the #define) is:
 *      forward(idx_name)
 * where idx_name is either the name of an IOPENed index or a NULL (meaning
 * to back the work file).  the internal sequence is:
 *      if (fwd(idx_name)) ;
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
#include <stdio.h>
#include <inttypes.h>

#include <fileEdit.hh>
#include <db_comm.hh>

#include "../../server/dbfunc.h"
#include "../../server/errors.h"


#define TRUE    1
#define FALSE   0

using namespace Dataman;

int index::forward()

{
	int i;
	int fmt;

	int64_t curr; 

	char cmd[128];
	char *buff;
	char *cptr;

	db_comm comm;

	if (in_xact && this->_rptr < 0)
		return(FALSE);
	if (cur_index && cur_index->get_wrmode())
		master.out_rec();
	if (this->_curkey.get_len() == 0)
		comm.db_err(ENOGET, "%s: error in forward", _progname);

	if (this->_idxno < 0 || this->_idxno > MAX_INDEX || this->_fno < 0 || this->_fno > this->_nfiles || this->_rptr < 0)
		comm.db_err(0, "%s: memory corruption detected in forward", _progname);

	sprintf(cmd, "%d|%d|%d|%" PRId64 "|", FORWARD, this->_idxno, this->_fno, this->_rptr);

	try {
		buff = comm.db_send(cmd, strlen(cmd));
	}
	catch(int comm_err) {
		comm.db_err(0, "%s: socket read error in forward", _progname);
	}

	i = atoi(buff);
	if (i < 0)
		comm.db_err(i, "%s: error in forward", _progname);
	else if (i == 0) {
		delete[] buff;
		return(FALSE);					/* no next record in this file */
	}

	cptr = strchr(buff, '|') + 1;
	curr = strtoll(cptr, NULL, 0);
	cptr = strchr(cptr, '|') + 1;
	fmt = atoi(cptr);
	cptr = strchr(cptr, '|') + 1;

	this->_rptr = curr;
	master.cur = curr;
	cur_index = this;
	master._filedesc = this->_files[this->_fno].get_desc();
	master.head = this->_files[this->_fno].get_hlen();
	master.len = i;
	master.fmt = fmt;
	master.in_rec(cptr);

	delete[] buff;
	return(TRUE);						/* give the ok signal */
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker ft=c:
 */
