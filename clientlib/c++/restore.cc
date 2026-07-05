/* ***************************************************************
 *
 * PROCEDURE:	restore.cc
 *
 * PROJECT:		dataman client side c++ library routine
 * 
 * DATE:		Tue Jun 15 16:16:06 MDT 2004
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
 * this routine restores an index to it's last SAVEd state
 * the calling sequence is:
 *      restore (idx_name);
 * where idx_name is the name of the index whose state you want to restore
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
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include <fileEdit.hh>
#include <db_comm.hh>

#include "../../server/dbfunc.h"
#include "../../server/misc.h"

extern void db_err(int, const char *, ...);

#define TRUE	1
#define FALSE	0

using namespace Dataman;

int index::restore()
{
	int i;							/* misc usage */
	int len;                            /* internal key len */

	char cmd[128];
	char *buff;
	char *ptr;

	if (cur_index && cur_index->get_wrmode() == UPDATE)
		master.out_rec();				/* write out cur record */

    if (this->_savptr == NULL)
		db_err(0, "%s: in restore, index %s has not been saved\n",
					_progname, this->_idxname);

    len = this->_keylen + KEY_HEADER_LENGTH;		/* internal key length */

	sprintf(cmd, "%d|%d|%"PRId64"|%d|%"PRId64"|", RESTORE, this->_idxno,
					this->_savptr->_savnode, this->_savptr->_savoffs,
					this->_savptr->_savrec);
	i = strlen(cmd);
	::memcpy(cmd+i, this->_savptr->_savkey.get_data(), this->_keylen+KEY_HEADER_LENGTH);
	i += this->_keylen+KEY_HEADER_LENGTH;

	db_comm comm;
	try {
		buff = comm.db_send(cmd, i);
	}
	catch (int i) {
		comm.db_err(0, "%s: Can't read socket in RESTORE", _progname);
	}

	i = atoi(buff);
	if (i < 0)
		comm.db_err(i, "%s: restore error", _progname);

	if (i == 0) {
//		delete[] this->_savptr->_savkey;
		free(this->_savptr);
		this->_savptr = NULL;
		delete[] buff;
		return(FALSE);
	}
/*
 * parse the return and update the globals
 */
	master.len = i;
	ptr = strchr(buff, '|') + 1;
	master.fmt = atoi(ptr);
	ptr = strchr(ptr, '|') + 1;
	this->_curnode = strtoll(ptr, NULL, 0);
	ptr = strchr(ptr, '|') + 1;
	this->_offs = atoi(ptr);
	ptr = strchr(ptr, '|') + 1;

	key tmp_key(ptr, this->_keylen);
	this->_curkey = tmp_key;
	ptr += this->_keylen + KEY_HEADER_LENGTH;

	master.chan = this->_curkey.get_fno() - 1;
	this->_rptr = this->_savptr->_savrec;
	master.cur = this->_rptr;
	master._filedesc = this->_files[master.chan].get_desc();
	master.head = this->_files[master.chan].get_hlen();

	free(this->_savptr);
	this->_savptr = NULL;

	cur_index = this;
	master.in_rec(ptr);
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
