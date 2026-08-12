/* ***************************************************************
 *
 * PROCEDURE:	db_err.c
 *
 * PROJECT:		dataman client side
 * 
 * DATE:		Mar 10 14:39:28 MDT 2002
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 ************************************************************* */

/*
 * this prints out the database errors encountered to the stderr
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
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#ifdef DATAMAN_CMAKE_BUILD
#include "config.h"
#else
#include "../../config.h"
#endif

#include <curses.h>
extern int dwin_inited;

#define DBERROR
#include "../server/errors.h"

void db_err(int val, char *fmt, ...)
{
	va_list pt;

	if (dwin_inited)
		endwin();

	va_start(pt, fmt);
	
	vfprintf(stderr, fmt, pt);
	if (val)
		fprintf(stderr, ": %s\n", db_err_strings[-val]);
	else {
		if (errno != 0)
			perror("");
	}
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
