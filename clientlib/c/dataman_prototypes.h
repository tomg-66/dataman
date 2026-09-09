/*
 * @#dataman_prototypes.h DATAMAN ver 4.0.0 function prototype declarations.
 * Copyright (c) SuperUser Software 1989-2026.  All rights reserved.
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


#if !defined _DATAMAN_PROTOTYPES_H_
#define _DATAMAN_PROTOTYPES_H_

#include <stdint.h>
#include "visibility.h"
#include "wind.h"

typedef char key[64];
typedef enum {
	DATAMAN_MASTER_RECORD,
	DATAMAN_WORK_RECORD
} dataman_record_type;

DATAMAN_API extern int db_g_key(char *, key);
DATAMAN_API extern int db_g_next(char *);
DATAMAN_API extern int db_g_pror(char *);
DATAMAN_API extern int db_g_frst(char *);
DATAMAN_API extern int db_g_last(char *);
DATAMAN_API extern int db_g_curr(char *);
DATAMAN_API extern int db_rm_key(key, char *);
DATAMAN_API extern int db_include(char *, char *, char *);
DATAMAN_API extern int db_delete(char *);
DATAMAN_API extern int db_fwd(char *);
DATAMAN_API extern int db_bck(char *);
DATAMAN_API extern int db_rel(void);
DATAMAN_API extern int db_prtct(char *);
DATAMAN_API extern int db_restore(char *);
DATAMAN_API extern int match(const char *, const char *);
DATAMAN_API extern int db_sort(const char *);
DATAMAN_API extern int db_iopen(char *, int);
DATAMAN_API extern int db_insert(int, int, char *);
DATAMAN_API extern int rollback(void);
DATAMAN_API extern int start_transaction(void);
DATAMAN_API extern int db_commit(void);
DATAMAN_API extern int iclose(char *);
DATAMAN_API extern int init_dataman(int, char **);
DATAMAN_API extern int mkidx(int, const char **);
DATAMAN_API extern int db_clear(char *);
DATAMAN_API extern int save(char *);
DATAMAN_API extern int put_blob(char *, void *, int);

DATAMAN_API extern int acept(int, int, char *, int);
DATAMAN_API extern int pop_win(void);

DATAMAN_API extern void mk_key(key, const char *, int);
DATAMAN_API extern void dtm_show(int, ...);
DATAMAN_API extern void dtm_pause(int, int, const char *);

DATAMAN_API extern char *substr(const char *, const int, const int);
DATAMAN_API extern char *mask(int64_t, char *);


DATAMAN_API const char *_get_indexname(void);
DATAMAN_API const char *_get_curkey(void);
DATAMAN_API const char *_get_filename(void);
DATAMAN_API const char *_get_workfilename();

DATAMAN_API bool _is_master_format(int f);
DATAMAN_API bool _is_work_format(int f);
DATAMAN_API bool _is_new_file(void);
DATAMAN_API bool _is_blob(dataman_record_type, int);
//DATAMAN_API const void *_get_description(dataman_record_type);
DATAMAN_API bool _has_record(dataman_record_type);
DATAMAN_API int _get_keylength(void);
DATAMAN_API int _get_master_format(void);
DATAMAN_API int _get_work_format(void);
DATAMAN_API int _get_maxfields(dataman_record_type);
DATAMAN_API void _set_blob_length(dataman_record_type, int, ssize_t);
DATAMAN_API ssize_t _get_datafield_length(dataman_record_type, int);

DATAMAN_API extern char **mfld;
DATAMAN_API extern char **wfld;

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
