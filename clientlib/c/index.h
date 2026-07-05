/*
 * @#index.h rev 3.20 index description header file
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

#if !defined _INDEX_INCLUDED_
#define _INDEX_INCLUDED_

#include "datafile_header.h"

typedef struct _files_ {
	int			_fno;					/* file number in server */
	int			_longest;				/* longest record in this file */
	int			_hlen;					/* length of description */
	int16_t		*_desc;					/* description of this file */
	char		*_fname;				/* file name */
	FILEDESC	*_filedesc;				/* parsed out file description */
} FILES;

typedef struct savbuff  {				/* buffer for SAVE info */
	int64_t			 _savnode;			/* node pos to save */
	int64_t 		 _savrec;			/* saved record pointer */
	char			*_savkey;			/* saved key */
	char			 _savfile;			/* saved file number */
	char			 _savfmt;			/* saved format number */
	unsigned char	 _savoffs;			/* offset in node */
} SAVE;


typedef struct idxbuf   {               /* main index information */
	int             _wrmode;			/* read/write mode */
	int				_idxno;				/* index number (order of open) */
	int             _fno;				/* offset in files to current file */
	int				_nfiles;			/* number of files referred to */
	int16_t         _keylen;			/* length of key */
	int16_t			_longest;			/* longest master record */
	char			_idxname[64];		/* name of index */
	int64_t			_curnode;			/* pointer to current node */
	int64_t			_rptr;				/* pointer to current record */
	unsigned char	_offs;				/* offset into node */
	char           *_curkey;			/* pointer to current key */
	FILES          *_files;				/* each of the files in the index */
	SAVE           *_savptr;			/* pointer to save structure */
} INDEX;

#define MIN_KEY_SIZE	1		/* smallest allowable key */
#define MAX_KEY_SIZE	32		/* biggest allowable key */

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
