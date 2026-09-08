#include "internal_state.hpp"

namespace Dataman {

datarecord workRecord(WORK);

DATAMAN_HIDDEN char _file;
DATAMAN_HIDDEN index *cur_index;
DATAMAN_HIDDEN bool dbgsw;
DATAMAN_HIDDEN bool is_sort;
DATAMAN_HIDDEN bool dataman_has_php;
DATAMAN_HIDDEN char *_progname;
DATAMAN_HIDDEN char *_root;
DATAMAN_HIDDEN char **_fnames;
DATAMAN_HIDDEN char _fileno;

const char *current_file()
{
	return _fnames[_fileno];
}

const key& current_key()
{
	return cur_index->get_key();
}

const char *current_index_name()
{
	return cur_index->get_ixname();
}

}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
