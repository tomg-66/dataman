/* ***************************************************************
 *
 * PROCEDURE:	proto.hpp
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

#include "index.hpp"
#include "datarecord.hpp"
#include "datafield.hpp"
#include "key.hpp"
#include "visibility.h"

namespace Dataman {

enum windowType { POP_UP = 0, GROW = 1 };
enum windowColor {
	BLACK = 1, BLUE, GREEN, CYAN, RED, MAGEN, YELLOW
};

extern DATAMAN_API unsigned int HELP;
extern DATAMAN_API unsigned char EOL[];
extern DATAMAN_API unsigned char TOP[];

DATAMAN_API int acept(int, int, unsigned char *, int);
DATAMAN_API int acept(int, int, char *, int);
DATAMAN_API int pop_win(void);
DATAMAN_API char *mask(int64_t, char *);

DATAMAN_API void show(int, ...);
DATAMAN_API void pause(int, int, const char *);
DATAMAN_API void grow_win(int, int, int, int, int);
DATAMAN_API int new_win(int, int, int, int, int, int);
DATAMAN_API void window(int, int, int, int, int);
DATAMAN_API void cl_win(int, int, int, int, int);
DATAMAN_API void init_dwin(void);

DATAMAN_API void sort(const char *);
DATAMAN_API void sort(datafield&);
DATAMAN_API void sort(int);

DATAMAN_API int init_dataman(int, char **);
DATAMAN_API int mkidx(int, char **);
DATAMAN_API void flush(void);

DATAMAN_API char *substr(const char *, int, int);
DATAMAN_API char *substr(const key&, int, int);

DATAMAN_API const char *strcpy(datafield&, const char *);
DATAMAN_API const char *strncpy(datafield&, const char *, int);
DATAMAN_API const void *memcpy(datafield&, const char *, int);
DATAMAN_API char *strcpy(char *, datafield&);
DATAMAN_API char *strncpy(char *, datafield&, int);
DATAMAN_API char *strcat(char *, datafield&);
DATAMAN_API char *strncat(char *, datafield&, int);
DATAMAN_API int atoi(const datafield&);

DATAMAN_API void start_transaction(void);
DATAMAN_API void rollback(void);
DATAMAN_API int commit(void);

} // namespace Dataman

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
