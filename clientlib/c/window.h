/*
 * @#window.h dataman rev 3.20. window struct description header
 * Copyright (c) SuperUser Software 1989-2004.  All rights reserved.
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

#ifndef WINDOW_INCLUDED
#define WINDOW_INCLUDED

#include <curses.h>

typedef struct win_buff {
	char lh_row;			/* left hand corner row */
	char lh_col;			/* left hand corner col */
	char rh_row;			/* right hand corner row */
	char rh_col;			/* right hand column */
	chtype *buffer;			/* pointer to data under screen */
	struct win_buff *prev_win;	/* pointer to prior window in stack */
	struct win_buff *next_win;	/* pointer to next window on stack */
} DB_WIN;

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
