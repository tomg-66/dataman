/* ***************************************************************
 *
 * PROCEDURE:	datafield.hh
 *
 * PROJECT:		dataman client side c++ header
 * 
 * DATE:		Wed Jul  7 16:37:52 MDT 2004
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 * 				Sat Mar 26 15:31:52 MST 2005
 * 				added support for blobs.  added the put_blob
 * 				function, and made checks for blobs in the other
 * 				operators to make sure you can't do stuff you
 * 				ought not.
 * 				tomg
 *
 * 				Thu Mar 21 16:02:09 MDT 2013
 * 				tom
 * 				added namespace support
 *
 ************************************************************* */
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

//
//these are the data 'types' that we are currently supporting
//     (think of adding date, time, money in the future)
//

#if !defined _DATAMAN_DATAFIELD_INCLUDED_
#define  _DATAMAN_DATAFIELD_INCLUDED_


namespace Dataman {

enum fieldTypes {type_non = -1, type_chr, type_int, type_flt, type_blob = 4};
#define TYPE_MASK	007

#if !defined min
#define min(a,b)	((a) <= (b) ? (a) : (b))
#endif

class datafield {
	private:
		int length;				// length of the datarec buffer
		fieldTypes type;		// current type stored in the datarec
		char *data;				// the actual data
		int which;				/* is this a member of the master or work record */

	public:
//
//default constructor
//
		datafield(void);
//
//construct from string, offset, length
//
		datafield(const char *, int , int);
//
//standard copy constructor
//
		datafield(const datafield&);
//
//destructor
//
		~datafield();
//
// make a new field in in_rec.  not for use anywhere else
//
		void make_field(const char *, int, int);
//
//some access routines.
//
		inline const char *getptr() { return(data); }
		inline int datalen() { return(length); }
		inline int get_type() { return(type); }
		inline int get_which(void) { return(which); }
		int put_blob(const void *, int);

		friend const char *strncpy(datafield&, const char *, int);
		friend const void *memcpy (datafield&, const char *, int);
//
//the data pointer
//
		inline operator const char *() const {
			return(data);
		};

//
//the different assignments to a datafield
//
		void operator=(const datafield&);
		void operator=(const char *);
		void operator=(const int);
		void operator=(const float);
//
//define the different ways that you can 'add' to a datafield
//
		datafield operator+(const datafield&);
		datafield operator+(const char *);
		datafield operator+(int);
		datafield operator+(float);
//
//multiplication operators
//
		datafield operator*(const datafield&);
		datafield operator*(const char *);
		datafield operator*(int);
		datafield operator*(float);
//
//division operators
//
		datafield operator/(const datafield&);
		datafield operator/(const char *);
		datafield operator/(int);
		datafield operator/(float);

//
//substraction from a datafield only makes sense if you are
//using numbers, not strings.  but if the string represents a
//number then it's ok.
//		datafield operator-(datafield, datafield);
//		datafield operator-(datafield, char *);
//		datafield operator-(char *, datafield);
//		datafield operator-(datafield, int);
//		datafield operator-(int, datafield);
//		datafield operator-(datafield, float);
//		datafield operator-(float, datafield);

//
// equality operators
//
		bool operator==(const datafield&);
		bool operator==(const char *);
		bool operator==(int);
		bool operator==(float);

		bool operator!=(const datafield&);
		bool operator!=(const char *);
		bool operator!=(int);
		bool operator!=(float);

};			// end of class
	const void *memcpy (Dataman::datafield& d, const char *s, int i);
	const char *strncpy (Dataman::datafield& d, const char *s, int i);
};          // end of namespace

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
