/* ***************************************************************
 *
 * PROCEDURE:	flush.c
 *
 * PROJECT:		dataman client side
 * 
 * DATE:		legacy, originally written in 1988
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 * 				Modified for use as a client side function
 *
 * 				Thu Mar 21 15:49:21 MDT 2013
 * 				Tom Green
 * 				added name space
 *
 ************************************************************* */

/*
 * this routine flushes the current work and master record to the disk
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

#include <fileEdit.hh>
#include <db_comm.hh>

#include <datarecord.hh>

using namespace Dataman;

void flush()
{
/*
 * this check is for if you call dataman_disconnect before you exit.
 * since this is registered with an atexit call, this can happen.
 */
	if (db_comm::get_sock() < 0)
		return;

/*
 * workfile records are -not- part of a transaction
 */
	if (!in_xact && workfile.getdirty())
		workfile.out_rec();

	if (cur_index && cur_index->get_wrmode() && master.getdirty())
		master.out_rec();
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker ft=c:
 */
