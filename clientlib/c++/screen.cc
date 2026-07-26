/* ***************************************************************
 *
 * PROCEDURE:	screen.c
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
 *				modified to use the curses package.
 ************************************************************* */

/*
 * this saves and restores the contents of the screen in the specified area
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

void save_scr(int row1, int col1,			/* row and column to begin */
			  int row2, int col2,			/* ending row and col */
			  chtype *buff)					/* where to put it */

{
	int i, j;

	for (i = row1; i <= row2; i++) {
		for (j = col1; j <= col2; j++)
			*buff++ = mvinch(i, j);
	}
}

void rest_scr(int row1, int col1,			/* where to begin restore */
			  int row2, int col2,			/* where to end it */
			  chtype *buff)					/* buffer of chars */
{
	int i, j;

	for (i = row1; i <= row2; i++) {
		move(i, col1);
		for (j = col1; j <= col2; j++)
			addch(*buff++);
	}
	refresh();
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
