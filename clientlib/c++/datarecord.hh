/* ***************************************************************
 *
 * PROCEDURE:	datarecord.hh
 *
 * PROJECT:		dataman client side c++ header
 * 
 * DATE:		Wed Jul  7 16:43:48 MDT 2004
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
/*
 * defines the data record type and the functions that can
 * access it.
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

#if !defined _DATAMAN_DATARECORD_INCLUDED_
#define  _DATAMAN_DATARECORD_INCLUDED_

#include "datafield.hh"
#include "index.hh"

extern void init_dataman(int, char **);
extern int commit(void); 

namespace Dataman {

#define MASTER	0			// master file data record
#define WORK	1			// work file data record

class datarecord {
	private:
		short		head;		// length of description
		short		longest;	// longest record in file
		int64_t		cur;		// pointer to current record
		int64_t		prev;		// pointer to previous record
		int64_t		next;		// pointer to next record
		int			chan;		// file number returned from server
		int			len;		// length of current record
		char		fmt;		// format number of current record
		char		_file;		// when_file flag for work files
		bool		_dirty;		// is this record dirty?
		int			which;		// master or work rec
		FILEDESC 	*_filedesc;	// a parsed out file description
		datafield 	*field;		// an array of datafields

	public:
		datarecord(int t) {
			head = longest = 0;
			cur = prev = next = 0ll;
			chan = len = 0;
			fmt = _file = 0;
			_dirty = false;
			which = t;
			_filedesc = (FILEDESC *)NULL;
		}
		void init(void) {
			head = longest = 0;
			cur = prev = next = 0ll;
			chan = len = 0;
			fmt = _file = 0;
			_dirty = false;
			_filedesc = (FILEDESC *)NULL;
		}

		~datarecord() { if (this->field) delete[] field; }

		datafield& operator[](int i);

		friend class index;			// this will mess with my privates!
		friend void ::init_dataman(int, char **);
		friend int commit(void); 

		void out_rec();
		void in_rec(char *);

		int getwhich() { return(this->which); }
//		int gethead() { return(this->head); }
//		int getlongest() { return (this->longest); }
		int64_t	 getcur() { return(this->cur); }
		int64_t	 getnext() { return(this->next); }
//		int64_t	 getprev() { return(this->prev); }
		int getchan() { return(this->chan); }
//		int getlen() { return(this->len); }
		char getfmt() { return(this->fmt); }
		char getfile() { return(this->_file); }
		bool getdirty() { return(this->_dirty); }
		void setdirty(bool b) { this->_dirty = b; }

		FILEDESC *get_desc() { return(this->_filedesc); }

		int release(void);

} ;				// end of class
};              // end of namespace

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
