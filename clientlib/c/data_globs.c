/* ***************************************************************
 *
 * PROCEDURE:	data_globs.c
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
 * 				modified to add globals for client side
 ************************************************************* */

/*
 * this routine initializes (declares) the global dataman variables that
 * are common to both sort and data edit probrams.
 * the calling sequence is:
 *      data_globs();
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

#include <stddef.h>
#include <malloc.h>
#ifdef DATAMAN_CMAKE_BUILD
#include "config.h"
#else
#include "../../config.h"
#endif
#include <stdint.h>

#include "index.h"
#include "visibility.h"


/*
 * the reason these are here for sort procedures as well as file edits is
 * because recio needs them at least declared.  they are never used in
 * sorts and never set up, but they at least need to be declared.
 */
DATAMAN_HIDDEN FILEDESC *m_fdesc;		/* parsed description of master file */
DATAMAN_HIDDEN short m_head;           /* length of the file header */
DATAMAN_HIDDEN int64_t m_cur;			/* pointer ot current master record */
DATAMAN_HIDDEN int64_t m_prev;			/* pointer to prev master record */
DATAMAN_HIDDEN int64_t m_next;			/* pointer to next master record */
DATAMAN_HIDDEN int m_chan;             /* channel number of master file */
DATAMAN_HIDDEN char m_fmt;             /* curr master record format number */
DATAMAN_HIDDEN uint32_t *m_blob_lengths;	/* runtime lengths for master blob fields */
DATAMAN_HIDDEN char *_m_rec_;

/*
 * these are the work file parameters that really are used in sorts.
 */
DATAMAN_HIDDEN FILEDESC *w_fdesc;		/* parsed description of work file */
DATAMAN_HIDDEN int64_t w_cur;			/* pointer ot current work record */
DATAMAN_HIDDEN int64_t w_prev;			/* pointer to prev work record */
DATAMAN_HIDDEN int64_t w_next;			/* pointer to next work record */
DATAMAN_HIDDEN int w_chan;				/* channel number of work file */
DATAMAN_HIDDEN char w_fmt;				/* curr work record format number */
DATAMAN_HIDDEN uint32_t *w_blob_lengths;	/* runtime lengths for work blob fields */
DATAMAN_HIDDEN char *_w_rec_;			/* pointer to the work field buffer */
DATAMAN_HIDDEN char _file;				/* next file accessed flag */
DATAMAN_HIDDEN char _root[512];		/* pointer to ROOT dir */
DATAMAN_HIDDEN INDEX cur_index;			/* the current operating index */
DATAMAN_HIDDEN int is_sort;
DATAMAN_HIDDEN int dm_sock = -1;				/* dataman socket */
DATAMAN_HIDDEN int dbgsw;
DATAMAN_HIDDEN int traditional = 1;		/* default to using traditional */
DATAMAN_HIDDEN extern int dwin_inited;

DATAMAN_API char **mfld;			/* pointer to the individual fields */
DATAMAN_API char **wfld;			/* pointer to the individual fields */

DATAMAN_HIDDEN void data_globs(void)
{                               /* use only for the global declarations */
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
