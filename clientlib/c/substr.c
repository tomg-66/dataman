/*
 * this routine returns a pointer to a substring.  The first arg is the
 * source string, the second and third arguments are the start and end
 * offset in the source.
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
#include <malloc.h>
#include <string.h>
#include "visibility.h"

DATAMAN_API char *substr(const char *arg1, const int arg2, const int arg3)
{

	char *buff;					/* returned buffer */

	unsigned idx;				/* index value */

	if (arg2 > arg3)			/* validate args */
		return(NULL);

	idx = arg3 - arg2 + 2;			/* length of buffer + \0 */
	buff = (char *)malloc(idx);		/* get return buffer */
	if (buff == NULL)
		return(NULL);

	buff = memcpy(buff,arg1+arg2,idx-1);	/* copy in substring */
	*(buff + idx - 1) = '\0';			/* null terminate */
	return(buff);				/* return */
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
