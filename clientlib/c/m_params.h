/*
 * @#m_params.h ver 3.20 dataman master file description header
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
#if !defined _MASTER_INC_
#define _MASTER_INC_

#include <stdint.h>
#include "datafile_header.h"
#include "visibility.h"

DATAMAN_HIDDEN extern short *m_desc;			/* description of master file */
DATAMAN_HIDDEN extern short m_head;			/* length of description */
DATAMAN_HIDDEN extern short m_longest;			/* longest recrod in file */
DATAMAN_HIDDEN extern int64_t m_cur;			/* pointer to current record */
DATAMAN_HIDDEN extern int64_t m_prev;			/* pointer to previous record */
DATAMAN_HIDDEN extern int64_t m_next;			/* pointer to next record */
DATAMAN_HIDDEN extern int m_chan;				/* master file channel */
DATAMAN_HIDDEN extern char m_fmt;				/* format number of current record */
DATAMAN_HIDDEN extern uint32_t *m_blob_lengths;	/* runtime lengths for blob fields */
DATAMAN_HIDDEN extern char *_m_rec_;			/* pointer to the record buffer */
DATAMAN_HIDDEN extern FILEDESC *m_fdesc;		/* parsed description of master file */

DATAMAN_API extern char **mfld;				/* pointers to each field */

#define MASTER	0				/* operate on master file */

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
