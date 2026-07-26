/* ***************************************************************
 *
 * PROCEDURE:	flush.c
 *
 * PROJECT:		dataman client side
 * 
 * DATE:		legacy, originally written in 1988
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 * 				Modified for use as a client side function
 *
 *				Fri Aug 28 22:39:48 MDT 2009
 *				added the dm_sock check at the first because on
 *				library re-load (for example the database
 *				functionality is in a plug-in), the connection
 *				might be already closed and this function was
 *				pushed with atexit(), so it will get called there.
 ************************************************************* */

/*
 * this routine flushes the current work and master record to the disk
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

#include "w_params.h"
#include "m_params.h"
#include "index.h"
#include "globs.h"

extern void out_rec(int);

extern int in_xact;

void flush()
{
	if (dm_sock == -1)
		return;
	if (!in_xact && wfld && *wfld)
		out_rec(WORK);				/* write out work record */

	if (cur_index._wrmode)			/* do only if in update mode */
    	if (mfld && *mfld)			/* has the array been allocated yet? */
			out_rec(MASTER);		/* write it */
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker ft=c:
 */
