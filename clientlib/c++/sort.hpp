/* ***************************************************************
 *
 * PROCEDURE:	sort.hpp
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
 * 				March 2004
 *				Tom Green
 *				modified to implement C++ library
 *
 * 				Thu Mar 21 16:02:09 MDT 2013
 * 				tom
 * 				added namespace support
 ************************************************************* */
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


#ifndef _DATAMAN_SORT_INC_
#define _DATAMAN_SORT_INC_

#include "wind.h"			/* the window definitions */

#include "endSort.hpp"
#include "proto.hpp"

using Dataman::_fnames;
using Dataman::_fileno;
using Dataman::workRecord;

#define CURRENT_FILE	Dataman::current_file()
#if defined DATAMAN_ENABLE_LEGACY_FILE_MACRO
#define FILE		CURRENT_FILE
#endif
#define WORK_FORMAT		workRecord.getfmt()	/* format number */

#define	ANY			0		/* accept any input */
#define	LOWER		1		/* translate input to lower case */
#define	UPPER		2		/* translate input to upper case */
#define NUMERIC		3		/* accept numeric only input */
#define NOECHO		04		/* bit mask to suppress echo on accept */
#define ENDLIST		-1		/* end of argument list flag */
#define release				if (workRecord.release()) ;			/* def for release */
#define when_workFormat(x)	if (workRecord.getfmt() == x)
#define when_workFile		if (workRecord.getfile())			/* test _file switch */

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
