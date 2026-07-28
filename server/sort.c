/* ***************************************************************
 *
 * PROCEDURE:	sort.c
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		legacy, originally written in 1988
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 * 				March 2002
 * 				tomg
 * 				made the appropriate changes for making this a
 * 				server side routine.  no locking since no one else
 * 				should be opening this index when building.
 *
 *				Mon Jul 27 08:03:35 PM MDT 2026
 *				tomg
 *				modified to use the index V2 routines.  again,
 *				since this is building a new index, locking isn't
 *				a concern. this is now version 4.0.0
 *
 ************************************************************* */
/* Insert a work-file key into a v2 index while rebuilding/sorting. */
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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "srv_index.h"
#include "errors.h"
#include "index_v2.h"
#include "misc.h"

extern int idx_cnt;
extern INDEX *_indices;

int sort(char *cmd, int c_off, char **ret)
{
	char v2_key[MAX_KEY_SIZE];
	char *cptr, *key;
	int ixno, fileno;
	int64_t record_offset;
	uint64_t root;

	*ret = NULL;
	if ((cptr = strrchr(cmd + c_off, '|')) == NULL)
		return(EINVMSG);
	*cptr = '\0';
	ixno = atoi(cmd + c_off);
	if (ixno < 0 || ixno >= idx_cnt)
		return(EINVMSG);
	if ((cptr = strchr(cmd + c_off, '|')) == NULL)
		return(EINVMSG);
	fileno = atoi(++cptr);
	if ((cptr = strchr(cptr, '|')) == NULL)
		return(EINVMSG);
	record_offset = strtoll(++cptr, NULL, 0);
	if ((key = strchr(cptr, '|')) == NULL)
		return(EINVMSG);
	key++;
	memset(v2_key, 0, sizeof(v2_key));
	memcpy(v2_key, key, strnlen(key, _indices[ixno]._keylen));
	if (!index_v2_build_insert(_indices[ixno]._idxchan, v2_key, fileno,
			(uint64_t)record_offset, &root))
		return(ENODWRT);
	_indices[ixno]._rootpos = (int64_t)root;
	strcpy(cmd, "0|1|");
	return(4);
}

/* vim: set noet sw=4 sts=4 ts=4 fdm=marker: */
