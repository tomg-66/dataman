/* ***************************************************************
 *
 * PROCEDURE:	proto.hh
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
 * 				March 2004
 *				Tom Green
 *				modified to implement C++ library
 *
 *				Wed Jul 28 18:36:14 MDT 2004
 *				updated to add some new functions with type KEY
 *				tomg
 *
 * 				Thu Mar 21 16:02:09 MDT 2013
 * 				tom
 * 				added namespace support
 ************************************************************* */
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

#include "index.hh"
#include "datarecord.hh"
#include "datafield.hh"
#include "key.hh"

using Dataman::index;
using Dataman::datarecord;
using Dataman::datafield;
using Dataman::key;

extern int acept(int, int, unsigned char *, int);
extern int acept(int, int, char *, int);
extern int pop_win(void);
extern int match(char *, char *);
extern int match(const key&, const char *);
extern char *mask(int64_t, char *);

extern void show(int, ...);
extern void pause(int, int, const char *);
extern void grow_win(int, int, int, int, int);

extern void sort(const char *);
extern void sort(datafield&);
extern void sort(int);

extern void init_dataman(int, char **);
extern void dataman_disconnect(void);
extern void mkidx(int, char **);
extern void flush(void);

extern char *substr(const char *, int, int);
extern char *substr(const key&, int, int);

extern const char *strcpy(datafield&, const char *);
extern const char *Dataman::strncpy(datafield&, const char *, int);
extern const void *Dataman::memcpy(datafield&, const char *, int);

extern char *strcpy(char *, datafield&);
extern char *strncpy(char *, datafield&, int);

extern char *strcat(char *, datafield&);
extern char *strncat(char *, datafield&, int);

extern int atoi(datafield&);

extern void start_transaction(void);
extern void rollback(void);
extern int commit(void);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker ft=c:
 */
