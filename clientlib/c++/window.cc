/* ***************************************************************
 *
 * PROCEDURE:	window.c
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
 *				modified to use the curses package
 ************************************************************* */

/*
 * this paints up a new 'window' on the standard window.  basically
 * it's a box with blanks in it set to the proper attribute.
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

void window(int row1, int col1,				/* row and col to begin */
			int row2, int col2,				/* row and col to end */
			int attr)						/* which color scheme to use */
{
	int i;
	char *buff;

	i = col2 - col1;
	buff = (char *)malloc(i);
	memset(buff, ' ', i);
	*(buff+i-1) = '\0';

	color_set(1, NULL);
	mvaddch(row1, col1, ACS_ULCORNER);
	for (i = col1+1; i < col2; i++)
		addch(ACS_HLINE);
	addch(ACS_URCORNER);

	for (i = row1+1; i < row2; i++) {
		color_set(1, NULL);
		mvaddch(i, col1, ACS_VLINE);
		color_set(attr, NULL);
		addstr(buff);
		color_set(1, NULL);
		addch(ACS_VLINE);
	}
	mvaddch(row2, col1, ACS_LLCORNER);
	for (i = col1+1; i < col2; i++)
		addch(ACS_HLINE);
	addch(ACS_LRCORNER);
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
