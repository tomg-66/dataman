/* ***************************************************************
 *
 * PROCEDURE:	datafile.hpp
 *
 * PROJECT:		dataman client side c++ header file
 * 
 * DATE:		Wed Jul  7 16:51:59 MDT 2004
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY: Sun Sep  6 09:24:55 AM MDT 2026
 *
 ************************************************************* */
//
// this describes the datafiles that an index will refer to
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
#if !defined _DATAMAN_DATAFILE_INCLUDED_
#define _DATAMAN_DATAFILE_INCLUDED_

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "visibility.h"
#include "datafile_header.h"

namespace Dataman {

DATAMAN_API char *substr(const char *, int, int);

class DATAMAN_HIDDEN files {
	private:
		int				_fno;				// file number in server
		int				_longest;			// longest record in this file
		int				_hlen;				// length of header desc
		FILEDESC		*_desc;				// parsed description of this file
		char *			_fname;				// file name
	public:
		files() {
			_fname = NULL;
			_longest = 0;
			_desc = NULL;
			_fno = -1;
		}
		files(const files&) = delete;
		files& operator=(const files&) = delete;
		files(files&&) = delete;
		files& operator=(files&&) = delete;
		~files() {
			if (_fname)
				delete[] _fname;
			if (_desc) {
				for (int i = 0; i < _desc->n_rformats; i++) {
					free(_desc->record_desc[i].field_sizes);
				}
				free(_desc->record_desc);
				free(_desc);
			}
		}
		void set_fno(int n) { _fno = n; }
		int get_fno() { return(_fno); }
		void set_name(char *s) {
			if (_fname) delete[] _fname;
				_fname = substr(s, 0, strlen(s));
		}
		char *get_fname() { return (_fname); }
		FILEDESC *get_desc() { return (_desc); }
		void set_desc(FILEDESC *d) { _desc = d; }
		int get_longest() { return(_longest); }
		void set_longest(int i) { _longest = i; }
		int get_hlen() { return(_hlen); }
		void set_hlen(int i) { _hlen = i; }
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
