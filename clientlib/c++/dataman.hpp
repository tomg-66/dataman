/* ***************************************************************
 *
 * PROCEDURE:	dataman.hpp
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
 *				modified to implement the C++ version
 *
 * 				Thu Mar 21 16:02:09 MDT 2013
 * 				tom
 * 				added namespace support
 ************************************************************* */
/*
 * @#dataman.h rev 3.20 dataman file edit procedure header
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

#pragma once

#include "wind.h"				// windowing definitions

#include "fileEdit.hpp"
#include "proto.hpp"
#include "datamanError.hpp"

using Dataman::cur_index;
using Dataman::workRecord;
using Dataman::masterRecord;

#define BEFORE		0			// insert before switch
#define AFTER		1			// insert after switch
#define	ANY			0			// accept any input
#define	LOWER		1			// translate input to lower case
#define	UPPER		2			// translate input to upper case
#define NUMERIC		3			// accept numeric only input
#define NOECHO		04			// bit mask to suppress echo on accept
#define ENDLIST		-1			// end of argument list flag
#define RDONLY		0			// open index (and files) for reading only
#define UPDATE		1			// open index for read/write

#define accept(row,col,buf,mode)        if (acept(row,col,buf,mode)) ;

#define itoa(val,buf)					sprintf(buf, "%d", val)

#define KEY				cur_index->get_key()		// last accessed key object
#define KEY_STR			cur_index->get_key().get_kstr()	// visible key text
#define MFMT			masterRecord.getfmt()			// master file format number
#define WFMT			workRecord.getfmt()		// work file format number
#define INDEX			cur_index->get_ixname()		// name of last accessed index
#define CURRENT_FILE	Dataman::current_file()
#if defined DATAMAN_ENABLE_LEGACY_FILE_MACRO
#define FILE			CURRENT_FILE		// last accessed file name
#endif

#define when_masterFormat(x)	if (masterRecord.getfmt() == x)	// master file format test
#define when_workFormat(x)		if (workRecord.getfmt() == x)	// work file format test
#define when_workFile			if (workRecord.getfile())		// new file test

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
