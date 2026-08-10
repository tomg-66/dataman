/* This is a generated file, edit the .stub.php file instead.
 * Stub hash: 1d3f2a1b88f949adae8b8c0d87a33368c78dfec0 */

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_dataman_connect, 0, 2, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, argc, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, argv, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_dataman_iopen, 0, 2, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, indexName, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, mode, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_dataman_get, 0, 2, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, indexName, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, key, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_dataman_mkidx, 0, 0, _IS_BOOL, 0)
	ZEND_ARG_VARIADIC_TYPE_INFO(0, args, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_dataman_sort, 0, 1, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, key, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_dataman_release, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_dataman_iclose, 0, 1, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, indexName, IS_STRING, 0)
ZEND_END_ARG_INFO()

#define arginfo_dataman_get_next arginfo_dataman_iclose

#define arginfo_dataman_get_prior arginfo_dataman_iclose

#define arginfo_dataman_get_current arginfo_dataman_iclose

#define arginfo_dataman_get_first arginfo_dataman_iclose

#define arginfo_dataman_get_last arginfo_dataman_iclose

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_dataman_forward, 0, 0, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, indexName, IS_STRING, 1, "null")
ZEND_END_ARG_INFO()

#define arginfo_dataman_back arginfo_dataman_forward

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_dataman_insert, 0, 3, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, fmt, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, placement, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, indexname, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_dataman_include, 0, 3, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, sourceIndex, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, destIndex, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, key, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_dataman_remove, 0, 2, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, key, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, indexNmae, IS_STRING, 0)
ZEND_END_ARG_INFO()

#define arginfo_dataman_protect arginfo_dataman_iclose

#define arginfo_dataman_clear arginfo_dataman_iclose

#define arginfo_dataman_save arginfo_dataman_iclose

#define arginfo_dataman_restore arginfo_dataman_iclose

#define arginfo_dataman_delete arginfo_dataman_iclose

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_dataman_get_format, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_dataman_get_key, 0, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()

#define arginfo_dataman_key_str arginfo_dataman_get_key

#define arginfo_dataman_get_index arginfo_dataman_get_key

#define arginfo_dataman_get_file arginfo_dataman_get_key

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_dataman_put_data, 0, 1, IS_VOID, 0)
	ZEND_ARG_TYPE_INFO(0, data, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_dataman_mark, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

#define arginfo_dataman_start_transaction arginfo_dataman_release

#define arginfo_dataman_commit arginfo_dataman_release

#define arginfo_dataman_rollback arginfo_dataman_release

#define arginfo_dataman_when_file arginfo_dataman_release

ZEND_FUNCTION(dataman_connect);
ZEND_FUNCTION(dataman_iopen);
ZEND_FUNCTION(dataman_get);
ZEND_FUNCTION(dataman_mkidx);
ZEND_FUNCTION(dataman_sort);
ZEND_FUNCTION(dataman_release);
ZEND_FUNCTION(dataman_iclose);
ZEND_FUNCTION(dataman_get_next);
ZEND_FUNCTION(dataman_get_prior);
ZEND_FUNCTION(dataman_get_current);
ZEND_FUNCTION(dataman_get_first);
ZEND_FUNCTION(dataman_get_last);
ZEND_FUNCTION(dataman_forward);
ZEND_FUNCTION(dataman_back);
ZEND_FUNCTION(dataman_insert);
ZEND_FUNCTION(dataman_include);
ZEND_FUNCTION(dataman_remove);
ZEND_FUNCTION(dataman_protect);
ZEND_FUNCTION(dataman_clear);
ZEND_FUNCTION(dataman_save);
ZEND_FUNCTION(dataman_restore);
ZEND_FUNCTION(dataman_delete);
ZEND_FUNCTION(dataman_get_format);
ZEND_FUNCTION(dataman_get_key);
ZEND_FUNCTION(dataman_key_str);
ZEND_FUNCTION(dataman_get_index);
ZEND_FUNCTION(dataman_get_file);
ZEND_FUNCTION(dataman_put_data);
ZEND_FUNCTION(dataman_mark);
ZEND_FUNCTION(dataman_start_transaction);
ZEND_FUNCTION(dataman_commit);
ZEND_FUNCTION(dataman_rollback);
ZEND_FUNCTION(dataman_when_file);

static const zend_function_entry ext_functions[] = {
	ZEND_FE(dataman_connect, arginfo_dataman_connect)
	ZEND_FE(dataman_iopen, arginfo_dataman_iopen)
	ZEND_FE(dataman_get, arginfo_dataman_get)
	ZEND_FE(dataman_mkidx, arginfo_dataman_mkidx)
	ZEND_FE(dataman_sort, arginfo_dataman_sort)
	ZEND_FE(dataman_release, arginfo_dataman_release)
	ZEND_FE(dataman_iclose, arginfo_dataman_iclose)
	ZEND_FE(dataman_get_next, arginfo_dataman_get_next)
	ZEND_FE(dataman_get_prior, arginfo_dataman_get_prior)
	ZEND_FE(dataman_get_current, arginfo_dataman_get_current)
	ZEND_FE(dataman_get_first, arginfo_dataman_get_first)
	ZEND_FE(dataman_get_last, arginfo_dataman_get_last)
	ZEND_FE(dataman_forward, arginfo_dataman_forward)
	ZEND_FE(dataman_back, arginfo_dataman_back)
	ZEND_FE(dataman_insert, arginfo_dataman_insert)
	ZEND_FE(dataman_include, arginfo_dataman_include)
	ZEND_FE(dataman_remove, arginfo_dataman_remove)
	ZEND_FE(dataman_protect, arginfo_dataman_protect)
	ZEND_FE(dataman_clear, arginfo_dataman_clear)
	ZEND_FE(dataman_save, arginfo_dataman_save)
	ZEND_FE(dataman_restore, arginfo_dataman_restore)
	ZEND_FE(dataman_delete, arginfo_dataman_delete)
	ZEND_FE(dataman_get_format, arginfo_dataman_get_format)
	ZEND_FE(dataman_get_key, arginfo_dataman_get_key)
	ZEND_FE(dataman_key_str, arginfo_dataman_key_str)
	ZEND_FE(dataman_get_index, arginfo_dataman_get_index)
	ZEND_FE(dataman_get_file, arginfo_dataman_get_file)
	ZEND_FE(dataman_put_data, arginfo_dataman_put_data)
	ZEND_FE(dataman_mark, arginfo_dataman_mark)
	ZEND_FE(dataman_start_transaction, arginfo_dataman_start_transaction)
	ZEND_FE(dataman_commit, arginfo_dataman_commit)
	ZEND_FE(dataman_rollback, arginfo_dataman_rollback)
	ZEND_FE(dataman_when_file, arginfo_dataman_when_file)
	ZEND_FE_END
};
