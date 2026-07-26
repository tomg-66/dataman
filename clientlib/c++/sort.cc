/* ***************************************************************
 *
 * PROCEDURE:	sort.cc
 *
 * PROJECT:		dataman client side c++ library
 * 
 * DATE:		Mon Jun 14 08:57:09 MDT 2004
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
 * this is the sort routine,  it takes as its argument the key to be sorted
 * into the index created by mkidx,  it's only function is to insert a key
 * pointing to the current work file record into the current index.  as a rule
 * it should only be used when creating new indexes.  at the very first of
 * the process the root node is a leaf.
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

#include <endSort.hh>
#include <db_comm.hh>

#include "../../server/dbfunc.h"

using namespace Dataman;

void sort(const char *pkey)
{

	int i;

	char cmd[128];
	char *buf;

	sprintf(cmd, "%d|%d|%d|%" PRId64 "|%s|", SORT, cur_index->get_idxno(),
					_fileno, workfile.getcur(), pkey);
	db_comm comm;

	try {
		buf = comm.db_send(cmd, strlen(cmd));
	}
	catch (int comm_err) {
		comm.db_err(0, "%s: communication error during sort", _progname);
	}
	i = atoi(buf);
	if (i < 0)
		comm.db_err(i, "%s: error during sort", _progname);
	delete[] buf;
}

void sort(datafield& k)
{
	sort(k.getptr());
}

void sort(const int i)
{
	char k[32];
	sprintf(k, "%d", i);
	sort(k);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
