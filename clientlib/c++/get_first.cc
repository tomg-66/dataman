/* ***************************************************************
 *
 * PROCEDURE:	get_first.cc
 *
 * PROJECT:		dataman client side c++ routines
 * 
 * DATE:		Fri Apr 18 21:06:00 MDT 2003
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
 * get the first key and its record from an index
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

#include <stdio.h>
#include <malloc.h>
#include <string.h>

#include <fileEdit.hh>
#include <db_comm.hh>
#include "../../server/dbfunc.h"
#include "../../server/errors.h"

#define TRUE    1
#define FALSE   0

using namespace Dataman;

int index::get_first()
{

    int i;			/* misc usage */

	char msg[128];
	char *ret;

	if (cur_index && cur_index->get_wrmode())
		master.out_rec();

	db_comm comm;
	if (this->_idxno < 0 || this->_idxno > MAX_INDEX || this->_fno < 0 || this->_fno > this->_nfiles)
		comm.db_err(0, "%s: memory corruption detected in first", _progname);

	sprintf(msg, "%d|%d|", GET_FIRST, this->_idxno);
	i = strlen(msg);
/*
 * send the command and deal with the return
 */
	try {
		ret = comm.db_send(msg, i);
	}
	catch (int comm_err) {
		comm.db_err(0, "%s: error reading server socket in get_first", _progname);
	}

	i = atoi(ret);
	if (i < 0)
		comm.db_err(i, "%s: error during get_first", _progname);
	else if (i == 0) {
		delete[] ret;
		return(FALSE);
	}
/*
 * parse the return and update the globals
 */
	this->parse_get(i, ret);
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
