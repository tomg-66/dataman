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

#include "datafile_header.h"

extern short *m_desc;			/* description of master file */
extern short m_head;			/* length of description */
extern short m_longest;			/* longest recrod in file */
extern int64_t m_cur;			/* pointer to current record */
extern int64_t m_prev;			/* pointer to previous record */
extern int64_t m_next;			/* pointer to next record */
extern int m_chan;				/* master file channel */
extern char m_fmt;				/* format number of current record */
extern char **mfld;				/* pointers to each field */
extern char *_m_rec_;			/* pointer to the record buffer */
extern FILEDESC *m_fdesc;		/* parsed description of master file */

#define MASTER	0				/* operate on master file */

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
