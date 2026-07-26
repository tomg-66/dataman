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

#include <stddef.h>
#include <stdint.h>
#include "proto.h"				/* declare all the function prototypes */
#include "index.h"				/* open index description */
#include "wind.h"				/* windowing definitions */
#include "m_params.h"			/* master file parameters */
#include "w_params.h"			/* work file parameters */

#define BEFORE		0			/* insert before switch */
#define AFTER		1			/* insert after switch */
#define	ANY			0			/* accept any input */
#define	LOWER		1			/* translate input to lower case */
#define	UPPER		2			/* translate input to upper case */
#define NUMERIC		3			/* accept numeric only input */
#define NOECHO		04			/* bit mask to suppress echo on accept */
#define ENDLIST		-1			/* end of argument list flag */
#define RDONLY		0			/* open index (and files) for reading only */
#define UPDATE		1			/* open index for read/write */

#define accept(row,col,buf,mode)        if (acept(row,col,buf,mode)) ;
#define get(idx,key)                    if (db_g_key(idx,key)) ;
#define get_next(idx)                   if (db_g_next(idx)) ;
#define get_prior(idx)                  if (db_g_pror(idx)) ;
#define get_current(idx)                if (db_g_curr(idx)) ;
#define get_first(idx)                  if (db_g_frst(idx)) ;
#define get_last(idx)                   if (db_g_last(idx)) ;
#define forward(idx)                    if (db_fwd(idx)) ;
#define back(idx)                       if (db_bck(idx)) ;
#define remove(key,idx)					if (db_rm_key(key,idx)) ;
#define protect(idx)					if (db_prtct(idx)) ;
#define restore(idx)					if (db_restore(idx)) ;
#define commit							if (db_commit()) ;

#define itoa(val,buf)					sprintf(buf, "%d", val)

#define KEY				cur_index._curkey		/* last accesed key */
#define KEY_LEN			(cur_index._keylen + sizeof(char) + sizeof(int64_t))
#define MFMT			m_fmt					/* master file format number */
#define WFMT			w_fmt					/* work file format number */
#define FILE			cur_index._files[cur_index._fno]->_fname

#define W_FILE			NULL					/* work file "flag" */

#define when_mfmt(x)	if (mfld&&m_fmt == x)	/* master file format test */
#define when_wfmt(x)	if (wfld&&w_fmt == x)	/* work file format test */
#define when_file		if (wfld&&_file)		/* new file test */

#define dirty_m			(mfld[0] = (char *)((uintptr_t)mfld[0] | 1))	/* set master dirty bit */
#define dirty_w			(wfld[0] = (char *)((uintptr_t)wfld[0] | 1))	/* set work file dirty bit */


#define mstrcpy(pt1,pt2)        do { dirty_m;strcpy(pt1,pt2); } while (0)
#define mstrncpy(pt1,pt2,i)     do { dirty_m;strncpy(pt1,pt2,i); } while (0)
#define wstrcpy(pt1,pt2)        do { dirty_w;strcpy(pt1,pt2); } while (0)
#define wstrncpy(pt1,pt2,i)     do { dirty_w;strncpy(pt1,pt2,i); } while (0)

extern char _file;				/* new file flag */

extern INDEX cur_index;			/* last accessed index */

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker ft=c:
 */
