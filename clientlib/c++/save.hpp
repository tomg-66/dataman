/* ***************************************************************
 *
 * PROCEDURE:	save.hpp
 *
 * PROJECT:		dataman client side c++ header file
 * 
 * DATE:		Sun Sep  6 08:24:42 PM MDT 2026
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 *
 ************************************************************* */
//
// describe the structure used to save index state
//
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

#if !defined DATAMAN_SAVE_INCLUDED
#define DATAMAN_SAVE_INCLUDED

namespace Dataman {

class key;

struct save {
		int64_t			_savnode;
		int64_t			_savrec;
		int				_savfile;
		key				_savkey;
		char			_savfmt;
		unsigned char	_savoffs;
};

};			// end of namespace
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
