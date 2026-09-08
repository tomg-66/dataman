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

#include "key.hpp"
#include "visibility.h"

namespace Dataman {

DATAMAN_API char *substr(const char *, int, int);

class files;
class datafield;

#define	RDONLY	0						// open index in read only mode
#define UPDATE	1						// open index in read/write mode

class index {
	public:
		DATAMAN_API index();						// this is used in mkidx...
		DATAMAN_API index(char *name, int mode);	// open an index
		index(const index&) = delete;
		index& operator=(const index&) = delete;
		index(index&&) = delete;
		index& operator=(index&&) = delete;
		DATAMAN_API ~index();						// close and index

//
//interface routines
//
		DATAMAN_API int get(const key&);			// get key from index
		DATAMAN_API int get(const char *);			// get key using string
		DATAMAN_API int get(datafield&);
		DATAMAN_API int get_next();					// get next key from index
		DATAMAN_API int get_prior();				// get prior key from index
		DATAMAN_API int get_first();				// get first key from index
		DATAMAN_API int get_last();					// get last key from index
		DATAMAN_API int get_current();
		DATAMAN_API int forward();					// get next record in data file
		DATAMAN_API int back();						// get prior record from data file
		DATAMAN_API int protect();					// protect the current data record
		DATAMAN_API int clear();						// clear the protect from the rec
		DATAMAN_API int delrec();					// delete record from database
		DATAMAN_API int remove(const key&);			// remove key from database
		DATAMAN_API int remove(const char *);		// remove key using string
		DATAMAN_API void save();					// save index state
		DATAMAN_API int restore();					// restore index state
		DATAMAN_API int insert(const int fmt, const int where);
		DATAMAN_API int include(index& , const char *);
		DATAMAN_API int include(index&, datafield&);
		DATAMAN_API int include(index *, const char *);
		DATAMAN_API int include(index *, datafield&);
		DATAMAN_API void iclose();

		DATAMAN_API const key& get_key(void) { return(this->_curkey); }
		DATAMAN_API char *get_ixname() { return(this->_idxname); }
//
//umm... these need to be public, but the user should -NEVER- use them
//
		DATAMAN_HIDDEN int get_wrmode();
		DATAMAN_HIDDEN int get_idxno();
		DATAMAN_HIDDEN int get_nfiles();
		DATAMAN_HIDDEN int get_fno();
		DATAMAN_HIDDEN int get_keylen();
		DATAMAN_HIDDEN int64_t get_rptr();
		DATAMAN_HIDDEN int _mkidx(int, char **);	// internal mkidx routine
		DATAMAN_HIDDEN files *get_file(int);
		DATAMAN_HIDDEN files *get_files();

	private:
#define MAX_INDEX	6
		static DATAMAN_HIDDEN char		*_onames[MAX_INDEX]; // names of open indices
		char			*_idxname;		// name of index file name
		int				_idxno;			// index number (order of open)
		int				_wrmode;		// read/write mode
		int				_fno;			// offset in files to current file
		int				_nfiles;		// nuber of files referred to
		int				_keylen;		// length of key
		int				_longest;		// longest master record
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
		DATAMAN_HIDDEN void _iopen(const char *name, int mode); // constructor helper
		DATAMAN_HIDDEN void _iclose();		// destructor helper
		DATAMAN_HIDDEN void _unwind();		// clean up if _iclose throws
		DATAMAN_HIDDEN int  _parse_get(int, char *); // parse get responses
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
