/* ***************************************************************
 *
 * PROCEDURE:	db_err.cc
 *
 * PROJECT:		dataman client side, c++ routines
 * 
 * DATE:		Fri May 16 21:02:29 MDT 2003
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 * 				Thu Mar 21 15:49:21 MDT 2013
 * 				Tom Green
 * 				added name space
 *
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

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <stdlib.h>

#include <db_comm.hh>
#include "../../config.h"

#include <curses.h>

#ifdef DWINDOW
extern int dwin_inited;
#endif

#define DBERROR
#include "../server/errors.h"

void Dataman::db_comm::db_err(int val, const char *fmt, ...)
{
	va_list pt;
#ifdef DWINDOW
	if (dwin_inited)
		endwin();
#endif
	va_start(pt, fmt);
	
	fprintf(stderr, "\n");
	vfprintf(stderr, fmt, pt);
	if (val)
		fprintf(stderr, ": %s\n", db_err_strings[-val]);
	else {
		if (errno != 0)
			perror("");
	}
	exit(val?val:errno);
}

//
// this is for use before the communications has been inited.
// so there is no db_comm object yet, and we don't want to
// establish comms just to print the error.
//
void db_err(int val, const char *fmt, ...)
{
	va_list pt;

#ifdef DWINDOW
	if (dwin_inited)
		endwin();
#endif
	va_start(pt, fmt);
	
	fprintf(stderr, "\n");
	vfprintf(stderr, fmt, pt);
	if (val)
		fprintf(stderr, ": %s\n", db_err_strings[-val]);
	else {
		if (errno != 0)
			perror("");
	}
	exit(val?val:errno);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
