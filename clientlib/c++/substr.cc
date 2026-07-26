/* ***************************************************************
 *
 * PROCEDURE:	substr.cc
 *
 * PROJECT:		dataman client side
 * 
 * DATE:		legacy, originally writtin in 1988
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 * 				March 2002
 *				Tom Green
 *				modified to use call interface to server
 *
 *				Wed Jul 28 18:29:21 MDT 2004
 *				modified to take a key variable
 *				tomg
 *
 * 				Thu Mar 21 15:49:21 MDT 2013
 * 				Tom Green
 * 				added name space
 ************************************************************* */
/*
 * allocate a new substring from an existing one.  return NULL
 * if something goes wrong.  the arguments are the original
 * string, and the begining and ending offsets to make the
 * substring out of.
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

#include <stddef.h>
#include <string.h>
#include <key.hh>

char *substr(const char *src, int beg, int end)
{

    char *buff;

    int offs = end - beg +2;

	if (offs < 2)
		return((char *)0);
	buff = new char[offs];
	if (!buff)
		return((char *)0);
	memcpy(buff, src+beg, offs-1);
	*(buff+offs-1) = '\0';
	return(buff);
}

char *substr(const Dataman::key &k, int b, int e)
{
	return(substr((const char *)k, b, e));
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
