/* ***************************************************************
 *
 * PROCEDURE:	key.hh
 *
 * PROJECT:		dataman client side c++ header file
 * 
 * DATE:		Wed Jul  7 16:53:31 MDT 2004
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 * 				Thu Mar 21 16:02:09 MDT 2013
 * 				tom
 * 				added namespace support
 *
 ************************************************************* */
//
// this class defines the internal structure of the key
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

#if !defined _DATAMAN_KEY_INCLUDED_
#define _DATAMAN_KEY_INCLUDED_

#include <string.h>
#include <stdint.h>

#define MIN_KEY_SIZE	1
#define MAX_KEY_SIZE	32

extern unsigned long get_long(char *);

namespace Dataman {

class key {
	private:
		char data[64];						// the combined key string
		int _len;							// length of the key (minus pointers)
		char _fno;							// the file number for the key
		int64_t _rec;						// the record number of the key
		char key_str[MAX_KEY_SIZE+1];		// maximum key length == 32
	public:
//define the constructors and destructor
		key();
		key(const char *s, int i = 0);
		key(key &k);
		~key();
//now a couple of operators that we want to use
		void operator=(const char *s);
		void operator=(const key& k);
		inline operator const char *() const { return(key_str); }
//and finally, a few access methods
		int get_len() { return(this->_len); }
		int get_fno() { return((int)this->_fno); }
		int64_t get_rec() { return(this->_rec); }
		const char *get_data() { return(this->data); }
		const char *get_kstr() { return(this->key_str); }

};

};	// end of namespace

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
