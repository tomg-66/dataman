/*
 * @#w_params.h rev 3.20 work file description file header
 * Copyright (c) Superuser Software 1988-2004.  All rights reserved.
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

#ifndef _WPARAMS_INC_
#define _WPARAMS_INC_

#include "datafile_header.h"

extern short *w_desc;			/* work file description */
extern short w_longest;			/* longest record in work file */
extern int64_t w_cur;			/* pointer to current work record */
extern int64_t w_prev;			/* pointer to next work record */
extern int64_t w_next;			/* pointer to previous work record */
extern int w_chan;				/* channel of work file */
extern char w_fmt;				/* format number of work record */
extern char **wfld;				/* pointers to data fields */
extern char *_w_rec_;			/* pointer to buffer */
extern char _file;				/* new file switch */
extern FILEDESC *w_fdesc;

#define WORK	1			/* operate on work file */

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker ft=c:
 */
