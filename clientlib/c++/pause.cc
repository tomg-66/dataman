/* ***************************************************************
 *
 * PROCEDURE:	pause.c
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
 *
 ************************************************************* */

/*
 * this is the 'pause' for dos programs.  the purpose is to
 * do a cursor positioned write with a bell and wait for the 
 * defined 'help' key to be hit before return
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

#include <string.h>
#include <curses.h>
#include <malloc.h>

extern chtype HELP;		/* the help key character */

extern void save_scr(int, int, int, int, chtype *);
extern void rest_scr(int, int, int, int, chtype *);

void pause(int row, int col, const char *mess)

{
	int offs;
	int tst=0;					/* the returned char from dosgch */
	int map;

	short len;					/* length of output string */

	chtype ch;
	chtype *buff;

	len = strlen(mess);
	offs = col + len;
	buff = (chtype *)calloc(len, sizeof(chtype));
	save_scr(row, col, row, offs-1, buff);
	move(row, col);
	while(*mess) {
		ch = inch();
		map = ch & A_COLOR;
		addch(*mess|map);
		mess++;
	}
	refresh();
	beep();						/* send a bell to screen */
	move(row, offs);			/* position for read */
	while(tst != HELP)
		tst = getch();			/* go until correct char entered */
	rest_scr(row, col, row, offs-1, buff);
	free(buff);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
