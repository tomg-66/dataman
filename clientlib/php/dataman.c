/*
   +----------------------------------------------------------------------+
   | Copyright © The PHP Group and Contributors.                          |
   +----------------------------------------------------------------------+
   | This source file is subject to the Modified BSD License that is      |
   | bundled with this package in the file LICENSE, and is available      |
   | through the World Wide Web at <https://www.php.net/license/>.        |
   |                                                                      |
   | SPDX-License-Identifier: BSD-3-Clause                                |
   +----------------------------------------------------------------------+
   | Author: Tom Green                                                    |
   +----------------------------------------------------------------------+
*/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "php.h"
#include "ext/standard/info.h"
#include "php_dataman.h"
#include "dataman_arginfo.h"
#include "SAPI.h"
#include <zend_exceptions.h>

/* For compatibility with older PHP versions */
#ifndef ZEND_PARSE_PARAMETERS_NONE
#define ZEND_PARSE_PARAMETERS_NONE() \
	ZEND_PARSE_PARAMETERS_START(0, 0) \
	ZEND_PARSE_PARAMETERS_END()
#endif

typedef enum {
	DATAMAN_MASTER_RECORD,
	DATAMAN_WORK_RECORD
} dataman_record_type;

typedef struct _dataman_record_object {
	dataman_record_type type;
	zend_object zo;
} dataman_record_object;

static inline dataman_record_object *dataman_record_from_obj(zend_object *obj)
{
	return (dataman_record_object *)((char *)(obj) - XtOffsetOf(dataman_record_object, zo));
}

zend_class_entry *master_record_ce;
zend_class_entry *work_record_ce;

static zend_object_handlers dataman_record_handlers;
static zval *dataman_record_read_dimension(zend_object *object, zval *offset, int type, zval *retval);
static void dataman_record_write_dimension(zend_object *object, zval *offset, zval *value);
static int dataman_record_has_dimension(zend_object *object, zval *offset, int check_empty);
static void dataman_record_unset_dimension(zend_object *object, zval *offset);

/*
 * prevent users from constructing a data record
 */
PHP_METHOD(datamanRecord, __construct)
{
	zend_throw_error(NULL, "Dataman record objects cannot be constructed directly");
}

ZEND_BEGIN_ARG_INFO_EX(
		arginfo_dataman_record_construct, 0,0,0)
ZEND_END_ARG_INFO()

static const zend_function_entry dataman_record_methods[] = {
	PHP_ME(
		datamanRecord,
		__construct,
		arginfo_dataman_record_construct,
		ZEND_ACC_PRIVATE | ZEND_ACC_FINAL)
  	PHP_FE_END
};


// hooking: $data = $record[field_number]
static zval *dataman_record_read_dimension(zend_object *object, zval *offset, int type, zval *retval)
{
	int max_field_number;
	char **record;

	if (!offset || Z_TYPE_P(offset) != IS_LONG) {
		zend_type_error("Record array access must be an integer.");
		return NULL;
	}
	zend_long field_number = Z_LVAL_P(offset);
	dataman_record_object *intern = dataman_record_from_obj(object);

	if (intern->type == DATAMAN_MASTER_RECORD) {
		if (!m_fdesc) {
			zend_throw_exception(zend_ce_exception, "Error: No current master record\n", -1);
			return NULL;
		}
		max_field_number = m_fdesc->record_desc[m_fmt-1].n_fields;
		record = mfld;
	} else {
		if (!w_fdesc) {
			zend_throw_exception(zend_ce_exception, "Error: No current work record\n", -1);
			return NULL;
		}
	   	max_field_number = w_fdesc->record_desc[w_fmt-1].n_fields;
		record = wfld;
	}

	if (field_number < 1 || field_number > max_field_number) {
		zend_argument_value_error(1, "is not a valid record index.");
		return NULL;
	}

	ssize_t datafield_length;
	char *datafield;
	if (intern->type == DATAMAN_MASTER_RECORD) {
		datafield = mfld[field_number];
	} else {
		datafield = wfld[field_number];
	}
	datafield = intern->type == DATAMAN_MASTER_RECORD ?
			mfld[field_number] : wfld[field_number];
// remember that format numbers and field numbers are 1 based, not 0
	datafield_length = intern->type == DATAMAN_MASTER_RECORD ? 
			m_fdesc->record_desc[m_fmt-1].field_sizes[field_number-1] :
			w_fdesc->record_desc[w_fmt-1].field_sizes[field_number-1];

// blobs store the data field length as a negative number
	if (datafield_length < 0)
		datafield_length = -datafield_length;

	ZVAL_STRINGL(retval, datafield, datafield_length);
	return retval;
}

// hooking: $record[field_number] = "new data"
static void dataman_record_write_dimension(zend_object  *object, zval *offset, zval *value)
{
	if (!offset || Z_TYPE_P(offset) != IS_LONG) {
		zend_type_error("Record array access index must be an integer.");
		return;
	}

	zend_long field_number = Z_LVAL_P(offset);
	dataman_record_object *intern = dataman_record_from_obj(object);
	int max_field_number;

	if (intern->type == DATAMAN_MASTER_RECORD) {
		if (!m_fdesc) {
			zend_throw_exception(zend_ce_exception, "Error: No current master record\n", -1);
			return;
		}
		max_field_number = m_fdesc->record_desc[m_fmt-1].n_fields;
	} else {
		if (!w_fdesc) {
			zend_throw_exception(zend_ce_exception, "Error: No current work record\n", -1);
			return;
		}
		max_field_number = w_fdesc->record_desc[w_fmt-1].n_fields;
	}

	if (field_number < 1 || field_number > max_field_number) {
		zend_argument_value_error(1, "is not a valid record index.");
		return;
	}

	ssize_t datafield_length;
	char *datafield;
	char **fields;
	if (intern->type == DATAMAN_MASTER_RECORD) {
		datafield = mfld[field_number];
		datafield_length = m_fdesc->record_desc[m_fmt-1].field_sizes[field_number-1];
		fields = mfld;
	} else {
		datafield = wfld[field_number];
		datafield_length = w_fdesc->record_desc[w_fmt-1].field_sizes[field_number-1];
		fields = wfld;
	}

	switch (Z_TYPE_P(value)) {
		case IS_STRING:		// this handles strings and blobs (maybe we should think of not allowing work records to contain blobs)
			zend_string *src_string = Z_STR_P(value);
			size_t src_string_len = ZSTR_LEN(src_string);
			if (datafield_length < 0) {
				char *tmp_datafield;
				tmp_datafield = malloc(src_string_len);
				memcpy(tmp_datafield, ZSTR_VAL(src_string), src_string_len);
				if (intern->type == DATAMAN_MASTER_RECORD) {
					mfld[field_number] = tmp_datafield;
					m_fdesc->record_desc[m_fmt-1].field_sizes[field_number-1] = -src_string_len;
				} else {
					wfld[field_number] = tmp_datafield;
					w_fdesc->record_desc[w_fmt-1].field_sizes[field_number-1] = -src_string_len;
				}
				free(datafield);
			} else {
				memset(datafield, ' ', datafield_length);
				memcpy(datafield, ZSTR_VAL(src_string), MIN(datafield_length, src_string_len));
			}
			break;
		case IS_LONG:		// handle integers
			sprintf(datafield, "%-*d", datafield_length, Z_LVAL_P(value));
			break;
		case IS_DOUBLE:		// and floats
			sprintf(datafield, "%-*f", datafield_length, Z_DVAL_P(value));
			break;
		default:
			zend_type_error("Invalid record type data");
			return;
	}
	(fields[0] = (char *)((uintptr_t)fields[0] | 1));	/* set record dirty bit */
	return;
}

// hooking: isset($record[field_number])
static int dataman_record_has_dimension(zend_object *object, zval *offset, int check_empty)
{
	if (!offset || Z_TYPE_P(offset) != IS_LONG) {
		return 0;
	}
	zend_long record_number = Z_LVAL_P(offset);
	dataman_record_object *intern = dataman_record_from_obj(object);
	int max_field_number;

	if (intern->type == DATAMAN_MASTER_RECORD) {
		if (!m_fdesc) {
			zend_throw_exception(zend_ce_exception, "Error: No current master record\n", -1);
			return 0;
		}
		max_field_number = m_fdesc->record_desc[m_fmt-1].n_fields;
	} else {
		if (!m_fdesc) {
			zend_throw_exception(zend_ce_exception, "Error: No current work record\n", -1);
			return 0;
		}
		max_field_number = w_fdesc->record_desc[w_fmt-1].n_fields;
	}
	if (record_number < 1 || record_number > max_field_number)
		return 0;
	return 1;
}

// hooking: unset($record[field_number])
static void dataman_record_unset_dimension(zend_object *object, zval *offset)
{
	zend_throw_error(NULL, "Cannot delete individual integer records from a structured buffer template.");
}

static zend_object *dataman_record_create_object(zend_class_entry *ce)
{
    dataman_record_object *intern;

	intern 	= zend_object_alloc(sizeof(*intern), ce);
    zend_object_std_init(&intern->zo, ce);
    object_properties_init(&intern->zo, ce);

	intern->type = ce == master_record_ce ? DATAMAN_MASTER_RECORD : DATAMAN_WORK_RECORD;

    intern->zo.handlers = &dataman_record_handlers;

    return &intern->zo;
}

static void dataman_record_free_object(zend_object *object)
{
    dataman_record_object *intern = dataman_record_from_obj(object);

    // Clean up the internal array safely when the object is destroyed
	zend_object_std_dtor(&intern->zo);
}

/*
 * The executor symbol table does not exist yet when RINIT runs.  Register
 * these as JIT auto-globals so PHP creates each object when the script first
 * refers to its reserved variable name.
 */
static bool dataman_create_record_global(zend_string *name)
{
	zval record;
	zend_class_entry *ce;

	if (zend_string_equals_literal(name, "masterRecord"))
		ce = master_record_ce;
	else if (zend_string_equals_literal(name, "workRecord"))
		ce = work_record_ce;
	else
		return false;

	object_init_ex(&record, ce);
	zend_hash_update(&EG(symbol_table), name, &record);
	return false;
}

PHP_MINIT_FUNCTION(dataman)
{
	REGISTER_LONG_CONSTANT("BEFORE", 0, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("AFTER", 1, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("RDONLY", 0, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("UPDATE", 1, CONST_CS | CONST_PERSISTENT);

	// define the master record
	zend_class_entry master_entry;
	INIT_CLASS_ENTRY(master_entry, "masterRecord", dataman_record_methods);
	master_record_ce = zend_register_internal_class(&master_entry);
	master_record_ce->create_object = dataman_record_create_object;
	master_record_ce->ce_flags |= ZEND_ACC_FINAL | ZEND_ACC_NOT_SERIALIZABLE;

	// define the worfile record
	zend_class_entry work_entry;
	INIT_CLASS_ENTRY(work_entry, "workRecord", dataman_record_methods);
	work_record_ce = zend_register_internal_class(&work_entry);
	work_record_ce->create_object = dataman_record_create_object;
	work_record_ce->ce_flags |= ZEND_ACC_FINAL | ZEND_ACC_NOT_SERIALIZABLE;

    // Set up standard handlers, then override the ones we want to restrict
	memcpy(&dataman_record_handlers, &std_object_handlers, sizeof(zend_object_handlers));
	dataman_record_handlers.offset = XtOffsetOf(dataman_record_object, zo);
	dataman_record_handlers.free_obj = dataman_record_free_object;
	dataman_record_handlers.read_dimension = dataman_record_read_dimension;
	dataman_record_handlers.write_dimension = dataman_record_write_dimension;
	dataman_record_handlers.has_dimension = dataman_record_has_dimension;
	dataman_record_handlers.unset_dimension = dataman_record_unset_dimension;
	dataman_record_handlers.clone_obj = NULL;

	if (zend_register_auto_global(
			zend_string_init("masterRecord", sizeof("masterRecord") - 1, true),
			true, dataman_create_record_global) == FAILURE)
		return FAILURE;
	if (zend_register_auto_global(
			zend_string_init("workRecord", sizeof("workRecord") - 1, true),
			true, dataman_create_record_global) == FAILURE)
		return FAILURE;

	return SUCCESS;
}

/*
 * this function initializes a connection to the dataman server
 * if there is an error in the argument list it just calls exit
 * instead of returning a true/false value
 */
PHP_FUNCTION(dataman_connect)
{
	zval *user_args = NULL;
	bool has_php_switch = false;

// check if the user passed an optional array in PHP 
	ZEND_PARSE_PARAMETERS_START(0, 1)
		Z_PARAM_OPTIONAL
		Z_PARAM_ARRAY(user_args)
	ZEND_PARSE_PARAMETERS_END();

// if the user passed explicit args in the php script
	if (user_args != NULL) {
		uint32_t num_elements = zend_hash_num_elements(Z_ARRVAL_P(user_args));
		int argc = (int)num_elements+1;
		char **argv = safe_emalloc(argc+1, sizeof(char *), 0);
		argv[0] = estrdup("php_user_script"); 	// fake description

		int idx = 1;
		zval *val;
		ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(user_args), val) {
			zend_string *str = zval_get_string(val);
			argv[idx] = estrdup(ZSTR_VAL(str));
			if (!strcmp(argv[idx], "-p"))
				has_php_switch = true;
			zend_string_release(str);
			idx++;
		} ZEND_HASH_FOREACH_END();

		if (!has_php_switch) {
			argv[argc] = estrdup("-p");
			argc++;
		}

		bool ret = init_dataman(argc, argv);
		efree(argv);
		if (ret)
			RETURN_TRUE;
		RETURN_FALSE;
	}
// no array was passed.  let's look for CLI inputs
	if (strcmp(sapi_module.name, "cli") == 0 && SG(request_info).argc > 0) {
		int argc = SG(request_info).argc;
		char **argv = SG(request_info).argv;
		if (init_dataman(argc, argv))
			RETURN_TRUE;
		RETURN_FALSE;
	}
// from a web page but no parameters provided (ROOT and DSRVHOST must be reachable env variables)
	int argc = 2;
	char *argv[] = { "php", "-p" };
	if (init_dataman(argc, argv))
		RETURN_TRUE;
	RETURN_FALSE;
}

//
// some functions we don't want the user to access from a web page.
//
#define CHECK_CLI_ONLY() \
	if (strcmp(sapi_module.name, "cli") != 0) { \
		zend_throw_exception(zend_ce_exception, "This function is avilable to the CLI only\n", -1); \
		RETURN_THROWS(); \
	}

/* we don't want sort routines running from a web page.
 * make these three un-available.
 * first is mkidx.  this is the initializer of sort
 * routines.  Next is sort... guess what this one does.
 * release moves to the next work record, when_file is
 * true if this is the first record of a newly opened work file
 */
PHP_FUNCTION(dataman_mkidx)
{
	CHECK_CLI_ONLY();

	int argc = SG(request_info).argc;
	char **argv = SG(request_info).argv;
	if (mkidx(argc, argv))
		RETURN_TRUE;
	RETURN_FALSE;
}

PHP_FUNCTION(dataman_sort)
{
	CHECK_CLI_ONLY();
	char *key;
	size_t key_len;

	ZEND_PARSE_PARAMETERS_START(1,1)		// set this up correctly
		Z_PARAM_STRING(key, key_len);
	ZEND_PARSE_PARAMETERS_END();
	if (!sort(key))
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(dataman_release) 
{
	CHECK_CLI_ONLY();
	ZEND_PARSE_PARAMETERS_NONE();
	if (!db_rel())
		RETURN_FALSE;
	RETURN_TRUE;
}
PHP_FUNCTION(dataman_when_file)
{
	CHECK_CLI_ONLY();
	ZEND_PARSE_PARAMETERS_NONE();
	if (_file)
		RETURN_TRUE;
	RETURN_FALSE;
}

/*
 * open a new index
 */
PHP_FUNCTION(dataman_iopen)
{
	char *idx_name;
	size_t name_len;

	zend_long mode;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_STRING(idx_name, name_len);
		Z_PARAM_LONG(mode);
	ZEND_PARSE_PARAMETERS_END();

	if (!iopen(idx_name, (int)mode))
		RETURN_FALSE;
	RETURN_TRUE;
}

/* close an index */
PHP_FUNCTION(dataman_iclose)
{
	char *idx_name;
	size_t name_len;
	ZEND_PARSE_PARAMETERS_START(1,1)
		Z_PARAM_STRING(idx_name, name_len);
	ZEND_PARSE_PARAMETERS_END();

	if (!iclose(idx_name))
		RETURN_FALSE;
	RETURN_TRUE;
}

/*
 * get a key and associated master record from the named index
 */
PHP_FUNCTION(dataman_get)
{
	char *idx_name;
	size_t name_len;

	char *input_key;
	size_t input_key_len;

	key lookup_key = {0};

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_STRING(idx_name, name_len);
		Z_PARAM_STRING(input_key, input_key_len);
	ZEND_PARSE_PARAMETERS_END();

	if (input_key_len > sizeof(lookup_key)) {
		zend_argument_value_error(2, "Key must not exceed %zu bytes", sizeof(lookup_key));
		RETURN_THROWS();
	}

	memcpy(lookup_key, input_key, input_key_len);

	RETVAL_BOOL(db_g_key(idx_name, lookup_key));
}

/* get the next key from an index */
PHP_FUNCTION(dataman_get_next)
{
	char *idx_name;
	size_t name_len;
	ZEND_PARSE_PARAMETERS_START(1,1)
		Z_PARAM_STRING(idx_name, name_len);
	ZEND_PARSE_PARAMETERS_END();

	if (!db_g_next(idx_name))
		RETURN_FALSE;
	RETURN_TRUE;
}

/* get the prior key from an index */
PHP_FUNCTION(dataman_get_prior)
{
	char *idx_name;
	size_t name_len;
	ZEND_PARSE_PARAMETERS_START(1,1)
		Z_PARAM_STRING(idx_name, name_len);
	ZEND_PARSE_PARAMETERS_END();

	if (!db_g_pror(idx_name))
		RETURN_FALSE;
	RETURN_TRUE;
}

/* get the current key from an index */
PHP_FUNCTION(dataman_get_current)
{
	char *idx_name;
	size_t name_len;
	ZEND_PARSE_PARAMETERS_START(1,1)
		Z_PARAM_STRING(idx_name, name_len);
	ZEND_PARSE_PARAMETERS_END();

	if (!db_g_curr(idx_name))
		RETURN_FALSE;
	RETURN_TRUE;
}

/* get the first key from an index */
PHP_FUNCTION(dataman_get_first)
{
	char *idx_name;
	size_t name_len;
	ZEND_PARSE_PARAMETERS_START(1,1)
		Z_PARAM_STRING(idx_name, name_len);
	ZEND_PARSE_PARAMETERS_END();

	if (!db_g_frst(idx_name))
		RETURN_FALSE;
	RETURN_TRUE;
}

/* get the last key from the named index */
PHP_FUNCTION(dataman_get_last)
{
	char *idx_name;
	size_t name_len;
	ZEND_PARSE_PARAMETERS_START(1,1)
		Z_PARAM_STRING(idx_name, name_len);
	ZEND_PARSE_PARAMETERS_END();

	if (!db_g_last(idx_name))
		RETURN_FALSE;
	RETURN_TRUE;
}

/* get the next record from a datafile */
PHP_FUNCTION(dataman_forward)
{
	char *idx_name = NULL;
	size_t name_len = 0;
	ZEND_PARSE_PARAMETERS_START(0,1)
		Z_PARAM_OPTIONAL
		Z_PARAM_STRING(idx_name, name_len);
	ZEND_PARSE_PARAMETERS_END();

	if (name_len > 0) {
		// set up for master record
	} else {
		// set up for the work record
	}

	if (!db_fwd(idx_name))
		RETURN_FALSE;
	RETURN_TRUE;
}

/* get the prior record from the datafile */
PHP_FUNCTION(dataman_back)
{
	char *idx_name = NULL;
	size_t name_len = 0;
	ZEND_PARSE_PARAMETERS_START(0,1)
		Z_PARAM_OPTIONAL
		Z_PARAM_STRING(idx_name, name_len);
	ZEND_PARSE_PARAMETERS_END();

	if (name_len > 0) {
		// set up for master record
	} else {
		// set up for the work record
	}

	if (!db_bck(idx_name))
		RETURN_FALSE;
	RETURN_TRUE;
}

/* insert a new record */
PHP_FUNCTION(dataman_insert)
{
	char *idx_name;
	size_t name_len;

	zend_long format_number = 0;
	zend_long position = -1;

	ZEND_PARSE_PARAMETERS_START(3,3)
		Z_PARAM_LONG(format_number);
		Z_PARAM_LONG(position);
		Z_PARAM_STRING(idx_name, name_len);
	ZEND_PARSE_PARAMETERS_END();

	if (format_number < 1 || format_number > /*master_file_max_record_format */ 5) {
		zend_argument_value_error(1, "Invalid record format number");
        	RETURN_THROWS();
	}
	if (position != BEFORE && position != AFTER) {
		zend_argument_value_error(1, "Insert position must be BEFORE or AFTER");
        	RETURN_THROWS();
	}
	if (!insert(format_number, position, idx_name))
		RETURN_FALSE;
	RETURN_TRUE;
}

/* add a key to an index */
PHP_FUNCTION(dataman_include)
{
	char *source_index;
	size_t source_len;
	char *dest_index;
	size_t dest_len;
	char *key;
	size_t key_len;

	ZEND_PARSE_PARAMETERS_START(3,3)
		Z_PARAM_STRING(source_index, source_len);
		Z_PARAM_STRING(dest_index, dest_len);
		Z_PARAM_STRING(key, key_len);
	ZEND_PARSE_PARAMETERS_END();

	if (!db_include(source_index, dest_index, key))
		RETURN_FALSE;
	RETURN_TRUE;
}

/* remove a key from the named index */
PHP_FUNCTION(dataman_remove)
{
	char *key;
	size_t key_len;
	char *index_name;
	size_t index_len;

	ZEND_PARSE_PARAMETERS_START(2,2)
		Z_PARAM_STRING(key, key_len);
		Z_PARAM_STRING(index_name, index_len);
	ZEND_PARSE_PARAMETERS_END();

	if (!db_rm_key(key, index_name))
		RETURN_FALSE;
	RETURN_TRUE;
}

/* protect the current record in the named index
 * that record becomes the current master record
 * if you don't name an index (idx_name is a nullptr
 * it's the work record that gets protected
 */
PHP_FUNCTION(dataman_protect)
{
	char *idx_name = NULL;
	size_t name_len = 0;
	ZEND_PARSE_PARAMETERS_START(0,1)
		Z_PARAM_OPTIONAL
		Z_PARAM_STRING(idx_name, name_len);
	ZEND_PARSE_PARAMETERS_END();

	if (name_len > 0) {
		// set up for master record
	} else {
		// set up for the work record
	}

	if (!db_prtct(idx_name))
		RETURN_FALSE;
	RETURN_TRUE;
}

/* remove the protect bit from the record 
 * same rules apply for the index name
 */
PHP_FUNCTION(dataman_clear)
{
	char *idx_name = NULL;
	size_t name_len = 0;
	ZEND_PARSE_PARAMETERS_START(0,1)
		Z_PARAM_OPTIONAL
		Z_PARAM_STRING(idx_name, name_len);
	ZEND_PARSE_PARAMETERS_END();

	if (name_len > 0) {
		// set up for master record
	} else {
		// set up for the work record
	}

	if (!clear(idx_name))
		RETURN_FALSE;
	RETURN_TRUE;
}

/* save the current state of the named index
 * this doesn't change anything, it merely saves
 * state
 */
PHP_FUNCTION(dataman_save)
{
	char *idx_name;
	size_t name_len;
	ZEND_PARSE_PARAMETERS_START(1,1)
		Z_PARAM_STRING(idx_name, name_len);
	ZEND_PARSE_PARAMETERS_END();

	if (!save(idx_name))
		RETURN_FALSE;
	RETURN_TRUE;
}

/* restore the state of a saved index
 * this will change the key pointer to the saved key
 * and read the record that was in memory at the save
 * (because of forward/back it might be different than
 * the record pointed to by the key)
 */
PHP_FUNCTION(dataman_restore)
{
	char *idx_name;
	size_t name_len;
	ZEND_PARSE_PARAMETERS_START(1,1)
		Z_PARAM_STRING(idx_name, name_len);
	ZEND_PARSE_PARAMETERS_END();

	if (!db_restore(idx_name))
		RETURN_FALSE;
	RETURN_TRUE;
}

/* delete the current record from the named index
 * if this is not the last record in the data file
 * the next record is read into memory.  if it is
 * the last record in the file the prior record is
 * read.  if it's the only record in the the file
 * it's an error
 */
PHP_FUNCTION(dataman_delete)
{
	char *idx_name;
	size_t name_len;
	ZEND_PARSE_PARAMETERS_START(1,1)
		Z_PARAM_STRING(idx_name, name_len);
	ZEND_PARSE_PARAMETERS_END();

	if (!db_delete(idx_name))
		RETURN_FALSE;
	RETURN_TRUE;
}

/* get the format number of the current record
 * if it's a sort routine return the work file
 * format number, otherwise return the record
 * format number of the current master record
 */
PHP_FUNCTION(dataman_get_format)
{
	ZEND_PARSE_PARAMETERS_START(0,0)
	ZEND_PARSE_PARAMETERS_END();

	if (/* is a sort routine */ false)
		RETURN_LONG(/* work file format */ 1);
	else
		RETURN_LONG(/* _mfmt */ 2);
}

/* get the key of the current index */
PHP_FUNCTION(dataman_get_key)
{
	char *system_key;
	int length = /* current_index._key_len+header_len */ 8;
	system_key = emalloc(length+1);
	memcpy(system_key, /*current_index._current_key*/ "hi there", length);
	RETURN_STRINGL(system_key, length);
}

/* get the string rep of the key of the current index */
PHP_FUNCTION(dataman_key_str)
{
	char *display_key;
	int length = /*current_index._key_len*/ 5;
	display_key = emalloc(length+1);
	memcpy(display_key, /*current_index._current_key*/ "MYkEY", length);
	RETURN_STRINGL(display_key, length);
}

/* get the name of the currend index */
PHP_FUNCTION(dataman_get_index)
{
	char *index_name;
	int length = strlen(/*current_index._idxname*/ "rxidx01");
	index_name = emalloc(length+1);
	memcpy(index_name, /*current_index._idxname*/ "rxidx01", length);
	RETURN_STRINGL(index_name, length);
}

/* get the name of the current master file */
PHP_FUNCTION(dataman_get_file)
{
	char *file_name;
	int length = strlen(/*current_index._files[_fileno]._name*/ "rxxfam");
	file_name = emalloc(length+1);
	memcpy(file_name, /*current_index._files[_fileno]._name*/ "rxxfam", length);
	RETURN_STRINGL(file_name, length);
}

/* put data into a field of the mater record */
PHP_FUNCTION(dataman_put_data)
{
}

/* mark the master record */
PHP_FUNCTION(dataman_mark)
{
}

/* start a new transaction */
PHP_FUNCTION(dataman_start_transaction)
{
	if (!start_transaction())
		RETURN_FALSE;
	RETURN_TRUE;
}

/* commit the transaction */
PHP_FUNCTION(dataman_commit)
{
	if (!db_commit())
		RETURN_FALSE;
	RETURN_TRUE;
}

/* cancel (rollback) the transaction */
PHP_FUNCTION(dataman_rollback)
{
	if (!rollback())
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_RINIT_FUNCTION(dataman)
{
#if defined(ZTS) && defined(COMPILE_DL_DATAMAN)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif

	return SUCCESS;
}

PHP_MINFO_FUNCTION(dataman)
{
	php_info_print_table_start();
	php_info_print_table_row(2, "dataman support", "enabled");
	php_info_print_table_end();
}

zend_module_entry dataman_module_entry = {
	STANDARD_MODULE_HEADER,
	"dataman",				/* Extension name */
	ext_functions,				/* zend_function_entry */
	PHP_MINIT(dataman),			/* PHP_MINIT - Module initialization */
	NULL,					/* PHP_MSHUTDOWN - Module shutdown */
	PHP_RINIT(dataman),			/* PHP_RINIT - Request initialization */
	NULL,					/* PHP_RSHUTDOWN - Request shutdown */
	PHP_MINFO(dataman),			/* PHP_MINFO - Module info */
	PHP_DATAMAN_VERSION,			/* Version */
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_DATAMAN
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(dataman)
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
