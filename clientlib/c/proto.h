/*
 * @#proto.h DATAMAN ver 3.20 function prototype declarations.
 * Copyright (c) SuperUser Software 1989-2004.  All rights reserved.
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


#if !defined _PROTO_H_
#define _PROTO_H_

#include <stdint.h>

typedef char key[64];

extern int db_g_key(char *, key);
extern int db_g_next(char *);
extern int db_g_pror(char *);
extern int db_g_frst(char *);
extern int db_g_last(char *);
extern int db_g_curr(char *);
extern int db_rm_key(key, char *);
extern int db_include(char *, char *, char *);
extern int db_delete(char *);
extern int db_fwd(char *);
extern int db_bck(char *);
extern int db_rel(void);
extern int db_prtct(char *);
extern int db_restore(char *);
extern int match(char *, char *);

extern int acept(int, int, char *, int);
extern int pop_win(void);

extern void init_dataman(int, char **);
extern void insert(int, int, char *);
extern void iopen(char *, int);
extern void save(char *);
extern void clear(char *);
extern void flush(void);
extern void mk_key(key, char *, int);
extern void iclose(char *);
extern void mkidx(int, char **);
extern void sort(char *);
extern void show(int, ...);
extern void pause(int, int, char *);
extern void grow_win(int, int, int, int, int);

extern char *substr(char *, int, int);
extern char *mask(int64_t, char *);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
