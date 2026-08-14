/* ***************************************************************
 *
 * PROCEDURE:	subscript.cc
 *
 * PROJECT:		dataman client side c++ header
 * 
 * DATE:		Sun Oct 21 20:30:20 MDT 2006
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
 * implements the bounded array operator for the datarecord type
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

#include <endSort.hpp>
#include "datamanError.hpp"

#include "../../server/datafile_header.h"
#include "../../server/errors.h"

using namespace Dataman;

datafield& datarecord::operator[](int i)
{
	if (i < 1 || i > _filedesc->record_desc[fmt-1].n_fields) {
		db_err(ESUBSCR, "%s: subscript %i is out of bounds "
				"for file %s (min: 1 max %d)", _progname, i,
				cur_index->get_files()[cur_index->get_fno()].get_fname(),
				_filedesc->record_desc[fmt-1].n_fields);
	}
	return(*(_fields+i));
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
