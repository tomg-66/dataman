/* ***************************************************************
 *
 * PROCEDURE:	get_ll.c
 *
 * PROJECT:		dataman
 * 
 * DATE:		Tue Aug  2 12:08:23 MDT 2005
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 *
 ************************************************************* */

/*
 * get a long long from a sequence of bytes that is in network
 * byte order into the host byte order.
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

#ifdef __gnu_linux__
#include <byteswap.h>
#include <endian.h>
#else
#include "misc.h"
#endif
#include <stdint.h>
#include <string.h>

int64_t get_ll(void *ptr)
{

	int64_t tmp;
	memcpy(&tmp, ptr, sizeof(tmp));

#ifdef __gnu_linux__
	if (__BYTE_ORDER == __LITTLE_ENDIAN)
		return(bswap_64(tmp));
#else
	if (d_endian == LITTLE_ENDIAN)
		return((((tmp) & 0xff00000000000000ull) >> 56)	\
      		| (((tmp) & 0x00ff000000000000ull) >> 40)		\
      		| (((tmp) & 0x0000ff0000000000ull) >> 24)		\
      		| (((tmp) & 0x000000ff00000000ull) >> 8)		\
      		| (((tmp) & 0x00000000ff000000ull) << 8)		\
      		| (((tmp) & 0x0000000000ff0000ull) << 24)		\
      		| (((tmp) & 0x000000000000ff00ull) << 40)		\
      		| (((tmp) & 0x00000000000000ffull) << 56));
#endif
	else
		return(*(int64_t *)ptr);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
