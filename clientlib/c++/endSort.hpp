/* ***************************************************************
 *
 * PROCEDURE:	endSort.hpp
 *
 * PROJECT:		dataman client side C++
 * 
 * DATE:		Thu Mar 21 15:47:08 MDT 2013
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 *
 ************************************************************* */
/*
 * @#dataman.h rev 3.3.7 dataman end sort procedure header
 * Copyright (c) SuperUser Software 2013.  All rights reserved.
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

#if !defined _DATAMAN_ENDSORT_INCLUDED_
#define _DATAMAN_ENDSORT_INCLUDED_

#include "index.hpp"
#include "datarecord.hpp"

namespace Dataman {
	extern Dataman::datarecord workRecord;

	extern char _file;				// the when_file flag

	extern class Dataman::index *cur_index;

	extern bool dbgsw;				// debug switch
	extern bool is_sort;
	extern bool dataman_has_php;

	extern char *_progname;			// name of currently running program
	extern char *_root;				// pointer to ROOT dir

	extern char **_fnames;			// the names of the files in index
	extern char _fileno;			// the offset to the current file
};

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
