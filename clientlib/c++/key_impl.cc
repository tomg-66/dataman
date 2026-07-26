/* ***************************************************************
 *
 * PROCEDURE:	key_impl.cc
 *
 * PROJECT:		dataman client side c++ library routine
 * 
 * DATE:		Sat Jul 24 15:27:54 MDT 2004
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
//
// this class implements the methods of the key class 
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


#include <string.h>
#include <key.hh>

#include "../../server/misc.h"

extern int64_t get_ll(const char *);

using namespace Dataman;

//
// the default constructor.  don't do anything!
//
key::key() {
	memset(data, '\0', sizeof(data));
	memset(key_str, '\0', sizeof(key_str));
	_len = 0;
	_fno = 0;
	_rec = 0;
}

//
// construct from a string and a length.  this is great for
// constructing from the internal routines with keys that
// come from the server in a string array
//
key::key(const char *s, int i) {
	if (i == 0)
		i = strlen(s);
	this->_len = i;
	memset(this->data, '\0', sizeof(data));
	memset(this->key_str, '\0', sizeof(key_str));
	::memcpy(this->key_str, s, i);
	if (*(s+i)) {
		this->_fno = *(s+i);
		this->_rec = get_ll((const char *)s+i+1);
		i += KEY_HEADER_LENGTH;
	} else {
		this->_fno = 0;
		this->_rec = 0;
	}
	::memcpy(this->data, s, i);
}
//
// construct from another key.
//
key::key(key &k) {
   	this->_len = k._len;
	::memcpy(this->key_str, k.key_str, sizeof(key_str));
	::memcpy(this->data, k.data, sizeof(data));
	this->_fno = k._fno;
	this->_rec = k._rec;
}
//
// destructor doesn't need to do anything since nothing is allocated.
//
key::~key() {}
//
// assignment from a string.  this is not to be used for 'internal'
// keys, like those that come directly from the server.
//
void key::operator=(const char *s) {
	memset(this->key_str, '\0', sizeof(key_str));
	memset(this->data, '\0', sizeof(data));
	strcpy(this->key_str, s);
	::memcpy(this->data, s, strlen(s));
	this->_len = 0;
	_rec = 0;
	_fno = 0;
}
//
// assignment from another key.  is actually pretty straight forward.
//
void key::operator=(const key& k) {
	this->_len = k._len;
	::memcpy(this->key_str, k.key_str, sizeof(key_str));
	::memcpy(this->data, k.data, sizeof(data));
	this->_fno = k._fno;
	this->_rec = k._rec;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker ft=c:
 */
