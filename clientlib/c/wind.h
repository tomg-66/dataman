/* ***************************************************************
 *
 * PROCEDURE:	wind.h
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
 *				modified to use curses package
 ************************************************************* */
/*
 * @#wind.h rev 3.20 dataman text mode window definitions.
 * Copyright (c) SuperUser Software 1988-2004.  All rights reserved.
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

#ifndef _DATAMAN_WINS_DEFINED
#define _DATAMAN_WINS_DEFINED

#define POP_UP		0		/* pop up a window */
#define GROW		1		/* draw an "exploding" window */

#define BLACK	1			/* define window colors */
#define	BLUE	2
#define GREEN	3
#define	CYAN	4
#define	RED		5
#define	MAGEN	6
#define	YELLOW	7

extern void window(int, int, int, int, int);
extern int pop_win(void);

extern void grow_win(int, int, int, int, int);
extern void new_win(int, int, int, int, int, int);
extern void cl_win(int, int, int, int, int);

extern char HELP;
extern unsigned char EOL[];
extern unsigned char TOP[];

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
