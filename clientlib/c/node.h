/*
 * @#node.h rev 3.20 node description include file
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

#ifndef NODE_DEFINED
#define NODE_DEFINED

#define MIN_KEY_SIZE	1		/* smallest allowable key */
#define MAX_KEY_SIZE	32		/* biggest allowable key */
#define MIDDLE		6			/* "middle" key in node */
#define N_KEYS		12			/* number of keys in a node */
#define N_KIDS		(N_KEYS+1)	/* # of kid pointers in a branch */
#define MISC_LEN	(sizeof(char)+sizeof(int64_t))			/* sizeof(unsigned long) + 1 */

typedef struct node {                   /* node buffer description */
		unsigned char	_isleaf;
		char			_keys[(N_KEYS + 1) * (MAX_KEY_SIZE+MISC_LEN)];
		int64_t			_kids[N_KIDS+1];
		int64_t			_parent;
} NODE;

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker ft=c:
 */
