/*
 * @#sort.h rev 3.20 dataman sort procedure header
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


#ifndef _SORT_INC_
#define _SORT_INC_

#include "proto.h"			/* the function prototypes */
#include "wind.h"			/* the window definitions */
#include "index.h"			/* current index description */
#include "w_params.h"		/* work file parameters */

#define ENDLIST		-1					/* end of arg list */
#define FILE 		_fnames[_fileno]	/* current file name */
#define WFMT		w_fmt				/* format number */

#define	ANY			0		/* accept any input */
#define	LOWER		1		/* translate input to lower case */
#define	UPPER		2		/* translate input to upper case */
#define NUMERIC		3		/* accept numeric only input */
#define NOECHO		04		/* bit mask to suppress echo on accept */
#define ENDLIST		-1		/* end of argument list flag */
#define release			if (db_rel()) ;		/* def for release */
#define when_wfmt(x)	if (w_fmt == x)
#define when_file		if (_file)			/* test _file switch */
#define dirty_w			wfld[0] = (char *)((uintptr_t)wfld[0] | 1)	/* set dirty bit */
#define wstrcpy(pt1,pt2)        do { dirty_w;strcpy(pt1,pt2) } while (0)
#define wstrncpy(pt1,pt2,i)     do { dirty_w;strncpy(pt1,pt2,i) } while (0)

extern char **_fnames;				/* names of files for index */
extern char _file;					/* new file switch */
extern int _fileno;					/* offset into _fnames */

//extern char TOP[];				/* show clear screen */
//extern char EOL[];				/* clear to end of line */

extern INDEX cur_index;				/* index being built */
//extern NODE  cur_node;				/* last accessed node */


#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
