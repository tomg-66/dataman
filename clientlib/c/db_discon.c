/* ***************************************************************
 *
 * PROCEDURE:	db_discon.c
 *
 * PROJECT:		dataman client side
 * 
 * DATE:		Mar 8 14:38:48 MDT 2002
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 *				Wed Nov  2 12:24:24 MST 2005
 *				checking new flag dataman_has_php so it won't
 *				automatically try to close a work file (which
 *				isn't open anyway for php) and to set the
 *				global dm_sock to -1 so that when (again only
 *				in php) this is called a second time we don't
 *				try to send a second shutdown message.
 *				tomg
 ************************************************************* */

/*
 * disconnect from the server
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
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "globs.h"
#include "../../server/dbfunc.h"

extern int is_sort;
extern int dataman_has_php;
extern INDEX _indices[6];

extern char *db_send(char *, int, char *);
extern int iclose(char *);

void db_discon(void)
{
	int i;

	char msg[128];

	char *buff;

/*
 * add this check here because in php we need to explicityly call this
 * routine, then it gets called again because it is registered with
 * atexit.  we do -not- want to do this twice
 */
/*
 * we are, for now, going to assume every iclose just works, since we need
 * to continue on and close everything left open.
 */
	if (dm_sock == -1)
		return;

	if (is_sort) {
		sprintf(msg, "%d|%d|", ICLOSE, cur_index._idxno);
		buff = db_send(msg, strlen(msg), __FILE__);

		if (buff)
			free(buff);

		sprintf(msg, "%d|%d|", ICLOSE, -w_chan);
		buff = db_send(msg, strlen(msg), __FILE__);

		if (buff)
			free(buff);
	} else {
/*
 * TODO:
 * do something here to clean up any records that are protected
 */
		for (i = 0; i < 6; i++) {
			if (strlen(_indices[i]._idxname)) {
#if 0
				sprintf(msg, "%d|%d|", ICLOSE, _indices[i]._idxno);
				buff = db_send(msg, strlen(msg), __FILE__);
				if (buff)
					free(buff);
#else
				iclose(_indices[i]._idxname);
#endif
			}
		}
		if (!dataman_has_php) {
			sprintf(msg, "%d|%d|", ICLOSE, -w_chan);
			buff = db_send(msg, strlen(msg), __FILE__);
			if (buff)
				free(buff);
		}
	}
	sprintf(msg, "%d", DISCON);
	buff = db_send(msg, strlen(msg), __FILE__);
	if (dbgsw && buff)
		fprintf(stderr, "discon response was ->%s<-\n", buff);
	if (buff && memcmp(buff, "ok", 2))
		fprintf(stderr, "Didn't get proper shutdown reply\n");
	if (buff)
		free(buff);
	close(dm_sock);
	dm_sock = -1;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
