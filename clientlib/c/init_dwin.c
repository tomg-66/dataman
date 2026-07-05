/* ***************************************************************
 *
 * PROCEDURE:	init_dwin.c
 *
 * PROJECT:		dataman client side
 * 
 * DATE:		Apr 4 14:52:09 MDT 2002
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 ************************************************************* */

/*
 * this initializes the old-style text windowing stuff
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
#include <curses.h>
#include <stdio.h>
#include <ctype.h>
#include <libgen.h>

WINDOW *stdscr;

chtype HELP;					/* the 'help' key */
char TOP[] = "\377";			/* the show top string */
char EOL[] = "\376";			/* the clear to end of line string */

int dwin_inited;
extern int dbgsw;
extern char *_progname;

void dm_endwin(void) {
	endwin();
}

void init_dwin()
{
	char *ptr;

	if (dbgsw) {
		char str[132];
		FILE *tmp;
		sprintf(str, "/tmp/%s.log", basename(_progname));
		if ((tmp = freopen(str, "w+", stderr)) == NULL) {
			perror("Can't freopen stderr");
			exit(0);
		}
	}
	stdscr = initscr();				/* set up curses */
	cbreak();						/* cbreak mode */
	noecho();						/* don't echo unless i say so */
	start_color();
	intrflush(stdscr, FALSE);		/* don't flush on interrupt */
	keypad(stdscr, TRUE);			/* turn on the keypad */
	scrollok(stdscr, FALSE);
	leaveok(stdscr, FALSE);

	ptr = getenv("HELP");
	if (ptr != NULL) {
		if (*ptr == '^')
			HELP = toupper(*(ptr+1)) - 0100;	/* a control key */
		else if (isdigit(*ptr))
			HELP = atoi(ptr);					/* given a numeric value */
		else
			HELP = *ptr;						/* get char entered */
	} else
		HELP = KEY_HOME;						/* default to home key */

	init_pair(1, COLOR_WHITE, COLOR_BLACK);
	init_pair(2, COLOR_WHITE, COLOR_BLUE);
	init_pair(3, COLOR_WHITE, COLOR_GREEN);
	init_pair(4, COLOR_WHITE, COLOR_CYAN);
	init_pair(5, COLOR_WHITE, COLOR_RED);
	init_pair(6, COLOR_WHITE, COLOR_MAGENTA);
	init_pair(7, COLOR_WHITE, COLOR_YELLOW);

	atexit(dm_endwin);					/* end the windowing on exit */
	dwin_inited = 1;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
