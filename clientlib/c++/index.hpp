/* ***************************************************************
 *
 * PROCEDURE:	index.hpp
 *
 * PROJECT:		dataman client side c++ header file
 * 
 * DATE:		Wed Jul  7 16:51:59 MDT 2004
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 * 				Sat Mar 26 18:53:59 MST 2005
 * 				for supporting blobs we changed the name of the
 * 				include file from file_desc.h to datafile_header.h
 * 				tomg
 *
 * 				Thu Mar 21 16:02:09 MDT 2013
 * 				tom
 * 				added namespace support
 *
 ************************************************************* */
//
// this is the implementation header for an index.
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

#if !defined _DATAMAN_INDEX_INCLUDED_
#define _DATAMAN_INDEX_INCLUDED_

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "datafield.hpp"
#include "key.hpp"

#include "datafile_header.h"

extern char *substr(const char *, int, int);

namespace Dataman {

class files {
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
		~files() {
			if (_fname)
				delete[] _fname;
			if (_desc)
				free(_desc);
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

struct save {
		int64_t			_savnode;
		int64_t			_savrec;
		int				_savfile;
		key				_savkey;
		char			_savfmt;
		unsigned char	_savoffs;
};


#define	RDONLY	0						// open index in read only mode
#define UPDATE	1						// open index in read/write mode

class index {
	public:
		index();						// this is used in mkidx...
		index(char *name, int mode);	// open an index
		~index();						// close and index

//
//interface routines
//
		int get(const key&);			// get key from index
		int get(const char *);			// get key using string
		int get(datafield&);
		int get_next();					// get next key from index
		int get_prior();				// get prior key from index
		int get_first();				// get first key from index
		int get_last();					// get last key from index
		int get_current();
		int forward();					// get next record in data file
		int back();						// get prior record from data file
		int protect();					// protect the current data record
		int clear();						// clear the protect from the rec
		int delrec();					// delete record from database
		int remove(const key&);			// remove key from database
		int remove(const char *);		// remove key using string
		void save();					// save index state
		int restore();					// restore index state
		int insert(const int fmt, const int where);
		int include(index& , const char *);
		int include(index&, datafield&);
		int include(index *, const char *);
		int include(index *, datafield&);
		void iclose();

		const key& get_key(void) { return(_curkey); }
//
//umm... these need to be public, but the user should -NEVER- use them
//
		int get_wrmode() { return(this->_wrmode); }
		int get_idxno() { return(this->_idxno); }
		int get_nfiles() { return(this->_nfiles); }
		int get_fno() { return(this->_fno); }
		int get_keylen(void) { return(this->_keylen); }

		char *get_ixname() { return(this->_idxname); }
		int64_t get_rptr() { return(this->_rptr); }

		int _mkidx(int, char **);		// internal mkidx routine

		files *get_file(int i) { return(this->_files+i); };
		files *get_files() { return(this->_files); }

	private:
#define MAX_INDEX	6
		static char		*_onames[MAX_INDEX];	// name of open indices
		char			*_idxname;		// name of index file name
		int				_idxno;			// index number (order of open)
		int				_wrmode;		// read/write mode
		int				_fno;			// offset in files to current file
		int				_nfiles;		// nuber of files referred to
		int				_keylen;		// length of key
		int 			_longest;		// longest master record
		int64_t			_curnode;		// pointer to current node
		int64_t			_rptr;			// pointer to current record
		int64_t			_generation;	// V2 index generation flag
		unsigned char	_offs;			// offset into node
		key				_curkey;
		files			*_files;		// each of the files in the index
		struct save		*_savptr;		// pointer to save structure
//
// ummm- non interface routines
//
		void _iopen(const char *name, int mode);	// constructor will call this
		void _iclose();						// destructor will call this
		void _unwind();                 // clean up if _iclose throws
		int parse_get(int, char *);		// parse ret from get* routines
};

};	// end of namespace

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
