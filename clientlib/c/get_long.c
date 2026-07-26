/*
 * this procedure gets a long int from the pointer passed.  Since this
 * is dos, the long is stored in the string in reverse byte order.  this
 * routine is very dos dependant.
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

#include <arpa/inet.h>		/* for def of ntohl */
unsigned long get_long(char *ptr)
{
/*
    char ret[4];

    *ret = *(ptr+3);
    *(ret+1) = *(ptr+2);
    *(ret+2) = *(ptr+1);
    *(ret+3) = *ptr;
    return(*((unsigned long*)ret));
*/
	return(ntohl(*(unsigned long *)ptr));
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker ft=c:
 */
