/*
 * @#globs.h dataman rev 3.20 global include for init routines.
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

#ifndef _GLOBS_INC_
#define _GLOBS_INC_ 

#include "index.h"
#include "w_params.h"

//extern char HELP;				/* the 'help' key */
//extern char TOP[];			/* the show top string */
//extern char EOL[];			/* the clear to end of line string */
extern char _file;				/* the when_file flag */

extern INDEX cur_index;			/* the current operating index */

extern int dm_sock;				/* socket to server */
extern int dbgsw;				/* debug switch */
extern int is_sort;

extern int traditional;

extern char *_progname;			/* name of currently running program */
extern char _root[];			/* pointer to various ROOT dirs */

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
