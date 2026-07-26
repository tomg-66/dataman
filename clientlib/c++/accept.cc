/* ***************************************************************
 *
 * PROCEDURE:	accept.c
 *
 * PROJECT:		dataman client side
 * 
 * DATE:		legacy, originally written in 1988
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 * 				March 2002
 * 				Tom Green
 * 				modified to use curses windowing package.
 *
 ************************************************************* */

/*
 * this does a cursor positioned, keystroke polled read.  it gets each
 * character one by one and checks the value and returns when the
 * passed string is full, the enter key is hit, the 'help' key is hit,
 * or a 'function' key is hit.  the function key is returned in the
 * first character of the string with the high bit and the scan code
 * or'd together.
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
#include <ctype.h>
#include <curses.h>

#define TRUE	1
#define FALSE	0
#define NL	'\n'		/* the newline char? */
#define BS	'\177'		/* the backspace character */
#define NOECHO	04		/* the bit to determine if echo */

extern chtype HELP;		/* the 'help' key value */

static void __show__(int row, int col, char *str)
{
	chtype ch;
	chtype map;

	move(row, col);
	refresh();
	while(*str) {
		ch = inch();
		map = ch & A_COLOR;
		addch((chtype)(*str)|map);
		str++;
	}
	refresh();
}

static void __myecho__(int row, int col, char c)
{
	chtype ch;
	chtype map;
	ch = inch();
	map = ch & A_COLOR;
	mvaddch(row, col, (chtype)c|map);
}

int acept(int row, int col, char *acc, int mode)
{

	short idx;				/* loop counter */

	int len;				/* input string length */
	int noecho;				/* the no echo flag */
	int buf;				/* the input character */

	unsigned char c;

	void pause(int,int,const char *);

	noecho = mode & NOECHO;		/* find if the echo is to be made */
	mode &= ~NOECHO;			/* strip off the noecho bit */
	len = strlen(acc);			/* get the input length */
	memset(acc,'_',len);		/* fill with underscores */
	if (noecho) {
		__myecho__(row, col, '_');
	} else
		__show__(row,col,acc);	/* write the input template */

	memset(acc,' ',len);		/* now blank it out */

	for(idx = 0;idx < len;idx++) {
		move(row, col);			/* position for read */
		refresh();
		buf = getch();			/* get the character */
		switch(buf) {

			case NL:
				if (!noecho)
					__show__(row,col,acc+idx);	/* clear to end of input */
				*(acc+idx) = '\0';				/* terminate the string */
				return(TRUE);					/* good read */

			case BS:
			case KEY_BACKSPACE:
				if (idx > 0) {
					col--;						/* back up one column */
					__myecho__(row, col, '_');
					idx--;						/* back up the string offset */
					*(acc+idx) = ' ';			/* blank the character */
				}
				idx--;					/* the loop re-increments the idx */
				continue;				/* continue the loop */

//			case FTN:
//				buf = dosgch();		/* the next char of sequence */
//				buf |= 0200;		/* set the high bit */
//				memset(acc,' ',len);	/* blank the buffer */
//				cpos(row,col-idx);	/* position for write */
//				if (!echo)
//					__show__(row,col-idx,acc);	/* clear the input string */
//				if (buf == HELP) {
//					*acc = '\0';	/* terminate the string */
//					return(FALSE);	/* bad entry */
//				} else {
//					*acc = buf;		/* the return value */
//					*(acc+1) = '\0';	/* terminate the string */
//					return(TRUE);	/* good read */
//				}

			default:
				if (buf == HELP) {
					memset(acc,' ',len);			/* blank out the buffer */
					if (!noecho)
						__show__(row,col-idx,acc);	/* blank the input line */
					*acc = '\0';					/* return zero length string */
					return(FALSE);					/* bad read */
				}

				if (buf > KEY_F0 && buf <= KEY_F(12)) {
					c = 0200 | (buf - 0316);
					memset(acc, ' ', len);
					if (!noecho)
						__show__(row, col-idx, acc);
					*acc = c;
					return(TRUE);
				}

				if (buf < 040 || buf > 0176) {
					idx--;							/* re read the char */
					continue;						/* continue the loop */
				}
				if (mode == 3 && (buf < 060 || buf > 071)) {
					pause(1,1,"Numeric only!");		/* show error */
					idx--;
					continue;
				}
				if (mode)
					buf = (mode == 2? toupper(buf) : tolower(buf));
				if (!noecho) {
					__myecho__(row, col, buf);		/* echo the character */
					col++;				/* increment the input col */
				}
				*(acc+idx) = buf;		/* save the input char */
		}								/* end of switch */
	}									/* end of loop */
	return(TRUE);						/* good read */
}

int acept(int row, int col, unsigned char *acc, int mode)
{
	return(acept(row, col, (char *)acc, mode));
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
