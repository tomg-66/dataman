/* ***************************************************************
 *
 * PROCEDURE:	new_win.c
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
 * this procedure saves the contents under the passed window, draws the
 * window on the screen, and exits.
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


#include <malloc.h>
#include <stddef.h>
#include <curses.h>
#include "window.h"

DB_WIN *chain;				/* pointer to head of window chain */
DB_WIN *cur_win;			/* pointer to current window */

#define POP_UP	0			/* pop up the window */
#define GROW	1			/* grow the window at you */
#define TRUE	1
#define FALSE	0

extern void save_scr(int,int,int,int,chtype *);
extern void grow_win(int,int,int,int,int);
extern void window(int,int,int,int,int);

int new_win(int row1, int col1,				/* row and col to begin */
			int row2, int col2,				/* row and col to end */
			int attr, int type)				/* background, and how to draw */
{
	int n_rows;				/* number of rows in window */
	int n_cols;				/* number of cols in window */
	int size;				/* actual size of window */

	chtype *buff;			/* pointer to saved screen */

	n_rows = row2 - row1 + 1;		/* get number of rows including boundry */
	n_cols = col2 - col1 + 1;		/* same for columns */
	size = n_rows * n_cols;
	if (!cur_win) {
		if ((chain = (DB_WIN *)malloc(sizeof(DB_WIN))) == NULL)
			return(FALSE);
		cur_win = chain;
		cur_win->prev_win = NULL;	/* make sure it points to nothing */
	} else {
		if ((cur_win->next_win = (DB_WIN *)malloc(sizeof(DB_WIN))) == NULL)
			return(FALSE);
		cur_win->next_win->prev_win = cur_win;
		cur_win = cur_win->next_win;
	}
	buff = calloc(size, sizeof(chtype));	/* get save buffer */
	save_scr(row1,col1,row2,col2,buff);		/* save the screen */
	cur_win->lh_row = row1;
	cur_win->lh_col = col1;
	cur_win->rh_row = row2;
	cur_win->rh_col = col2;
	cur_win->buffer = buff;			/* save pointer to buffer */
	if (type)
		grow_win(row1,col1,row2,col2,attr);	/* grow a window */
	else
		window(row1,col1,row2,col2,attr);	/* pop it up */
	return(TRUE);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker ft=c:
 */
