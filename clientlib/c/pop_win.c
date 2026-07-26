/* ***************************************************************
 *
 * PROCEDURE:	pop_win.c
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
 * this pops the top "window" off the "stack" and restores
 * what was under it.
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

#include "window.h"
#include <malloc.h>

#define TRUE	1
#define FALSE	0

extern DB_WIN *cur_win;
extern DB_WIN *chain;

extern void rest_scr(int,int,int,int,chtype *);

int pop_win()

{
	int row1,row2,col1,col2;		/* rows, and columns */
	chtype *ptr;				/* pointer to buffer */

	if (!cur_win)
		return(FALSE);			/* there is no window level */

	row1 = cur_win->lh_row;		/* make the call to rest win */
	row2 = cur_win->rh_row;		/* look neater */
	col1 = cur_win->lh_col;
	col2 = cur_win->rh_col;
	ptr = cur_win->buffer;

	rest_scr(row1,col1,row2,col2,ptr);	/* restore what underlies the window */
	free(cur_win->buffer);		/* free the character buffer */
	if (cur_win->prev_win == 0) {
		free(cur_win);
		chain = cur_win = 0;
	} else {
		cur_win = cur_win->prev_win;		/* point to prior window */
		free(cur_win->next_win);		/* free the window struct */
	}
	return(TRUE);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker ft=c:
 */
