/* ***************************************************************
 *
 * PROCEDURE:	datamanError.hpp
 *
 * PROJECT:		dataman client side c++ header
 * 
 * DATE:		Mon Aug 10 02:25:42 PM MDT 2026
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
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

#pragma once

#include <stdexcept>
#include <string>
#include <cstdarg>
#include <cstring>

namespace Dataman {

class datamanError : public std::runtime_error
{
private:
	int _code;
public:
	datamanError(int code, const std::string& message) : std::runtime_error(message), _code(code)
	{
	}
	int code() const { return _code; }
};

extern datamanError makeError(int code, const char *format, ...);
extern void db_err(int code, const char *format, ...);

};

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
