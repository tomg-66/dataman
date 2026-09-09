#pragma once

#include "endSort.hpp"

namespace Dataman {
	extern DATAMAN_HIDDEN bool in_xact;
	extern DATAMAN_HIDDEN char _file;
	extern DATAMAN_HIDDEN index *cur_index;
	extern DATAMAN_HIDDEN bool dbgsw;
	extern DATAMAN_HIDDEN bool is_sort;
	extern DATAMAN_HIDDEN bool dataman_has_php;
	extern DATAMAN_HIDDEN char *_progname;
	extern DATAMAN_HIDDEN char *_root;
	extern DATAMAN_HIDDEN char **_fnames;
	extern DATAMAN_HIDDEN char _fileno;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
