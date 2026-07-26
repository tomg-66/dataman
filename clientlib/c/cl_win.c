/* ***************************************************************
 *
 * PROCEDURE:	cl_win.c
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
 * 				April 2002
 * 				Tom Green
 * 				Modified to use the curses package.
 ************************************************************* */

/*
 * clear out a section of the screen
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

#include <curses.h>
#include <malloc.h>
#include <string.h>

void cl_win(int row1, int col1,			/* row and col to start */
			int row2, int col2,			/* where to end */
			int attr)
{
	int i, j;

	char *buff;

	j = col2 - col1 + 1;
	buff = malloc(j+1);
	memset(buff, ' ', j);
	*(buff+j) = '\0';
	color_set(attr, NULL);
	for (i = row1; i <= row2; i++)
		mvaddstr(i, col1, buff);
	refresh();
	free(buff);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker ft=c:
 */
