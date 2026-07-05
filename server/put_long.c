/* ***************************************************************
 *
 * PROCEDURE:	put_long.c
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 ************************************************************* */

/*
 * this procedure puts a long int into a string.  The reason this is used
 * instead of memcpy() is that on dos the bytes of a long are stored in
 * reverse order.  This copies the long in in reverse byte order.  this is
 * very dos dependant.
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

#include <netinet/in.h>

void put_long(char *ptr, uint32_t lval)
{
/*
	char *lptr;

	if (endian == LITTLE_ENDIAN) {
		lptr = (char *)&lval;
		*ptr = *(lptr+3);
		*(ptr+1) = *(lptr+2);
		*(ptr+2) = *(lptr+1);
		*(ptr+3) = *lptr;
	} else
		*((unsigned long *)ptr) = lval;
*/
/*
 * this is implemented really well, a no-op for BIG-ENDIAN and
 * -really- efficient inline assembly for LITTLE-ENDIAN
 */
	*(uint32_t *)ptr = ntohl(lval);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
