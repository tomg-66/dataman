/* ***************************************************************
 *
 * PROCEDURE:	protect.cc
 *
 * PROJECT:		dataman client side c++ routines
 * 
 * DATE:		Thu May  1 21:43:35 MDT 2003
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
 * this routine 'protects' or checks the current record to the caller
 * it's calling sequence is:
 * 	protect(ixname) else
 * it's internal sequence is:
 * 	if (!prtct(ixname)) ;
 * you can not protect a master record if you have not done an initial get
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
#include <stdio.h>
#include <inttypes.h>

#include <fileEdit.hh>
#include <db_comm.hh>

#include "../../server/dbfunc.h"
#include "../../server/errors.h"

#define TRUE	1
#define FALSE	0

using namespace Dataman;

int index::protect()
{
	
	int i, tmp;
	int fmt;

	char cmd[128];
	char *buff;
	char *ptr;

/*
 * in a transaction, a rptr < 0 means that this is a newly
 * inserted record.  there is nothing in the datbase to
 * protect
 */
	if (in_xact && this->_rptr < 0)
		return(TRUE);
	db_comm comm;
	if (this->_idxno < 0 || this->_idxno > MAX_INDEX || this->_fno < 0 || this->_fno > this->_nfiles || this->_rptr < 0)
		comm.db_err(0, "%s: memory corruption detected in protect", _progname);
/*
 * you don't flush the current record when you call protect
 * because part of the idea is to read in the -most current-
 * disk version.  you don't want to write it, then get it!
 */
	sprintf(cmd, "%d|%d|%d|%"PRId64"|", PROTECT, this->_idxno, this->_fno, this->_rptr);

	try {
		buff = comm.db_send(cmd, strlen(cmd));
	}
	catch(int comm_err) {
		comm.db_err(0, "%s: socket read error in protect", _progname);
	}

	tmp = atoi(buff);
	if (tmp < 0)
		comm.db_err(tmp, "%s: error in protect", _progname);
	else if (tmp == 0) {
		delete[] buff;
		return(FALSE);
	}

	ptr = buff;
	for(i = 0; i < 5; i++) {
		ptr = strchr(ptr, '|') + 1;
		if (i == 0)
			fmt = atoi(ptr);
	}
//	i = ptr - buff;
//	memcpy(buff, ptr, tmp);
//	memset(buff+tmp, '\0', i);

	master._filedesc = this->_files[this->_fno].get_desc();
	master.head = this->_files[this->_fno].get_hlen();
	master.cur = this->_rptr;
	master.chan = this->_fno;
	master.len = tmp;
	master.fmt = fmt;
	master.in_rec(ptr);
	//add_protect(this->_idxno, this->_fno, this->_rptr);
	delete[] buff;
	return(TRUE);
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
