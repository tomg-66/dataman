/* ***************************************************************
 *
 * PROCEDURE:	grow_win.c
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
 * this is a function to grow a window.
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

#include <stdlib.h>
#include <sys/time.h>

extern void window(int,int,int,int,int);

void grow_win(r1,c1,r2,c2,attr)
int r1,r2,c1,c2,attr;			/* row and column of start and end */

{
	int start_row,start_col;		/* beginning row and column */
	int end_row,end_col;
	int n_cols,n_rows;			/* number of rows and columns */
	int i;
	int idx;

	struct timeval tv;

	n_cols = c2-c1+1;			/* calc number of columns */
	n_rows = r2-r1+1;			/* calc number of rows */
	i = n_cols > n_rows? n_rows : n_cols;
	i /= 2;

	start_row = r1 + i - 1;
	start_col = c1 + i - 1;
	end_row = r2 - i + 1;
	end_col = c2 - i + 1;

	for(idx = start_row;idx >= r1;idx--,start_row--,start_col--,end_row++,end_col++) {
		window(start_row,start_col,end_row,end_col,attr);
		tv.tv_sec = 0;
		tv.tv_usec = 23000;
		select(0, NULL, NULL, NULL, &tv);
    }
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
