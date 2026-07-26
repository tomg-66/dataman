/* ***************************************************************
 *
 * PROCEDURE:	show.c
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
 * this does cursor positioned output to the screen
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

#include <stdarg.h>
#include <curses.h>

#define	END	-1
#define TOP	'\377'
#define EOL	'\376'

/*
extern int dbgsw;
*/
void show(int tmp1,...)

{
	va_list pt;

	int tmp2;

	char *mess;

	chtype ch;
	chtype map;

	va_start(pt,tmp1);

	while(1) {
/*
		if (dbgsw) {
			fprintf(stderr, "%d, %d, %s\n", tmp1, tmp2, mess);
			fflush(stderr);
		}
*/
		tmp2 = va_arg(pt,int);			/* get colum pos */
		mess = va_arg(pt,char *);		/* the string to print */
		move(tmp1,tmp2);				/* position the cursor */
		refresh();
		if (*mess == TOP)				/* is it TOP? */
			clrtobot();					/* yes, so clear screen */
		else if (*mess == EOL)
			clrtoeol();					/* clear to end of line */
		else {
			while (*mess >= ' ' && *mess <= '~') {
				ch = inch();
				map = ch & A_COLOR;
				addch((chtype)(*mess)|map);
				mess++;
			}
		}
		refresh();
		tmp1 = va_arg(pt, int);
		if (tmp1 == END)
			break;
	}
	va_end(pt);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
