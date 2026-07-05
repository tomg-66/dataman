/* ***************************************************************
 *
 * PROCEDURE:	pend.c
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
 * this is the routine to pend some seconds or micro seconds
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
#include <unistd.h>
#include <time.h>
#include <sys/types.h>

void pend(num, units)
short num;
short units;

{
	struct timeval tmot;

    if (units) {			/* a units value means pause for seconds */
		tmot.tv_sec = num;
		tmot.tv_usec = 0;
	} else {
		tmot.tv_sec = 0;
		tmot.tv_usec = num;
	}
	select(1, NULL, NULL, NULL, &tmot); 
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
