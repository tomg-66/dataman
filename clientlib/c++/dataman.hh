/* ***************************************************************
 *
 * PROCEDURE:	dataman.hh
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

#ifndef _DATAMAN_INC_
#define _DATAMAN_INC_

#include "wind.h"				// windowing definitions

#include "fileEdit.hh"
#include "proto.hh"

using Dataman::cur_index;
using Dataman::workfile;
using Dataman::master;

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

#define KEY				cur_index->get_key()		// last accesed key
#define MFMT			master.getfmt()			// master file format number
#define WFMT			workfile.getfmt()		// work file format number
#define FILE			cur_index->_fname		// last accessed file name
#define INDEX			cur_index->get_ixname()		// name of last accessed index

#define when_mfmt(x)	if (master.getfmt() == x)	// master file format test
#define when_wfmt(x)	if (workfile.getfmt() == x)	// work file format test
#define when_file		if (workfile.getfile())		// new file test

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
