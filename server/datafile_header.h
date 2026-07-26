/* ***************************************************************
 *
 * PROCEDURE:	datafile_header.h
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
 *
 ************************************************************* */
/*
 * @#datafile_header.h rev 3.2.0 dataman file edit procedure header
 * Copyright (c) SuperUser Software 2004.  All rights reserved.
 *
 * this is the description of the datafile header
 *
 * the header consists of the following elements
 *
 * short int	- length of file header info
 * short int	- number of record formats in the file
 * for each record format there is the following entries
 * 		short int	- number of fields for this record format
 * 		short int	- length of the data record
 * 		for each field in this record format -
 * 			short int	- length of this field
 *
 * has_blob in RFDESC indicates how many fields in the record
 * are blobs.  When a field size has a length of 0 that indicates
 * that field is a blob.
 *
 * a record has a maximum size of signed long (with any blob).
 * a record with no blobs has a maximum size of signed short.
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

#if !defined _DATAFILE_HEADER_H_INCLUDED_
#define _DATAFILE_HEADER_H_INCLUDED_

#include <sys/types.h>

typedef struct _rfdesc_ {
	int16_t		n_fields;			/* number of fields in this record */
	int16_t		rf_len;				/* length of the data record this describes */
	int16_t		has_blob;			/* does this record format contain a blob? */
	int16_t		*field_sizes;		/* size of each field in the record */
}	RFDESC;

typedef struct _filedesc_ {
	int16_t		header_len;			/* length of the header in the file */
	int16_t		n_rformats;			/* number of record formats */
	int16_t		longest;			/* longest record format in the file */
	RFDESC		*record_desc;		/* array of record format descriptions */
} FILEDESC;

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
