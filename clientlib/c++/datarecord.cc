/* ***************************************************************
 *
 * PROCEDURE:	datarecord.cc
 *
 * PROJECT:		dataman client data record implementation
 * 
 * DATE:		Sun Sep  6 04:42:29 PM MDT 2026
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 *
 ************************************************************* */
/*
 * implements the datarecord accessors that the user doesn't need
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

#include "datarecord.hpp"

namespace Dataman {

datarecord::datarecord(int t) {
	head = longest = 0;
	cur = prev = next = 0ll;
	chan = len = 0;
	fmt = _file = 0;
	_dirty = false;
	which = t;
	_filedesc = (FILEDESC *)NULL;
	_fields = NULL;
}

void datarecord::init(void) {
	if (_fields) {
		delete[] _fields;
		_fields = NULL;
	}
	clear_desc();
	head = longest = 0;
	cur = prev = next = 0ll;
	chan = len = 0;
	fmt = _file = 0;
	_dirty = false;
}

int datarecord::getwhich()
{
	return(this->which);
}

int64_t datarecord::getcur()
{
	return(this->cur);
}

int64_t datarecord::getnext()
{
	return(this->next);
}

int datarecord::getchan()
{
	return(this->chan);
}

bool datarecord::getdirty()
{
	return(this->_dirty);
}

FILEDESC * datarecord::get_desc()
{
	return(this->_filedesc);
}

} // end of namespace

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
