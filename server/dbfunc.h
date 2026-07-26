/* ***************************************************************
 *
 * PROCEDURE:	dbfunc.h
 *
 * PROJECT:		dataman
 * 
 * DATE:		
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 *				 Fri May 12 18:35:09 MDT 2006
 *				 changed to add one argument to all service
 *				 functions so that we can get rid of a
 *				 memcpy.
 *				 tomg
 *
 ************************************************************* */

/*
 * @#dbfunc.h rev 3.1.0 dataman file edit procedure header
 * Copyright (c) SuperUser Software 1988-2005.  All rights reserved.
 *
 * these are the database function prototype declarations
 * and the initialization of the array of functions as well.
 *
 * This is the server side of the datman database.
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

#ifndef _DBFUNC_INCLUDED_
#define _DBFUNC_INCLUDED_

#ifdef DISPATCH_C

extern int iopen(char *, int, char **);
extern int iclose(char *, int, char **);
extern int get(char *, int, char **);
extern int get_first(char *, int, char **);
extern int get_last(char *, int, char **);
extern int get_next(char *, int, char **);
extern int get_prior(char *, int, char **);
extern int get_current(char *, int, char **);
extern int insert(char *, int, char **);
extern int undelete(char *, int, char **);
extern int delete(char *, int, char **);
extern int include(char *, int, char **);
extern int dbremove(char *, int, char **);
extern int flush(char *, int, char **);
extern int protect(char *, int, char **);
extern int clear(char *, int, char **);
extern int forward(char *, int, char **);
extern int back(char *, int, char **);
extern int get_desc(char *, int, char **);
extern int init_dataman(char *, int, char **);
extern int release(char *, int, char **);
extern int sort(char *, int, char **);
extern int mkidx(char *, int, char **);
extern int restore(char *, int, char **);
extern int get_rec(char *, int, char **);

/*
 * these functions -must- go in the -same- order as
 * the defines.  the defines are used as an index into
 * this array, so if you don't do it right, it won't work!
 */
static int (*(dbfunc)[])(char *, int, char **)  = {
	get,
	get_first,
	get_last,
	get_next,
	get_prior,
	get_current,
	forward,
	back,
	protect,
	get_desc,
	init_dataman,
	release,
	mkidx,
	restore,
	get_rec,
	undelete,
	delete,
	insert,
	include,
	dbremove,
	clear,
	iopen,
	iclose,
	sort,
	flush
};
#endif

/*
 * transaction processing functions.  they are here because
 * they don't have a server function attached to them
 */
#define ROLLBACK	-3
#define COMMIT		-2
#define START_XACT	-1
/*
 * short commands with long responses
 * DELETE -MUST- BE THE LAST ONE IN THIS SECTION
 */
#define GET			0
#define GET_FIRST	1
#define GET_LAST	2
#define GET_NEXT	3
#define GET_PRIOR	4
#define GET_CURRENT	5
#define FORWARD		6
#define BACK		7
#define PROTECT		8
#define GET_DESC	9
#define INIT_DAT	10
#define RELEASE		11
#define MKIDX		12
#define RESTORE		13
#define GET_REC		14
#define UNDELETE	15
#define DELETE		16
/*
 * short commands with short responses
 */
#define INSERT		17
#define INCLUDE		18
#define REMOVE		19
#define CLEAR		20
#define IOPEN		21
#define ICLOSE		22
#define SORT		23
/*
 * long commands with short responses
 * FLUSH -MUST- BE THE FIRST ONE
 */
#define FLUSH		24

/*
 * and finally, the last of all of them...
 */
#define DISCON		25

/*
 * limits
 */
#define FUNC_MIN	ROLLBACK
#define FUNC_MAX	DISCON

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
