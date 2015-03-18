#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <dlfcn.h>
#include <ffi.h>

#include "php.h"
#include "zend_exceptions.h"
#include "php_mffi.h"
#include "mffi_internal.h"

zend_class_entry *mffi_ce_function;

static zend_object_handlers mffi_function_object_handlers;

/* {{{ */
PHP_METHOD(MFFI_Function, __construct)
{
	zend_throw_exception(mffi_ce_exception, "MFFI\\Function cannot be constructed directly", 1);
}
/* }}} */

static void php_mffi_set_argument(zval *arg, php_mffi_value *dest, long type) {
	zval tmp = *arg;
	zval_copy_ctor(&tmp);

	switch(type) {
		case FFI_TYPE_INT:
			convert_to_long(&tmp);
			dest->i = Z_LVAL(tmp);
			break;

		case FFI_TYPE_FLOAT:
			convert_to_double(&tmp);
			dest->f = Z_DVAL(tmp);
			break;

		case FFI_TYPE_DOUBLE:
			convert_to_double(&tmp);
			dest->d = Z_DVAL(tmp);
			break;

		case FFI_TYPE_LONGDOUBLE:
			convert_to_double(&tmp);
			dest->D = Z_DVAL(tmp);
			break;

		case FFI_TYPE_UINT8:
		case FFI_TYPE_SINT8:
			convert_to_long(&tmp);
			dest->c = Z_LVAL(tmp);
			break;

		case FFI_TYPE_UINT16:
		case FFI_TYPE_SINT16:
			convert_to_long(&tmp);
			dest->i = Z_LVAL(tmp);
			break;

		case FFI_TYPE_UINT32:
			convert_to_long(&tmp);
			dest->l = Z_LVAL(tmp);
			break;

		case FFI_TYPE_SINT32:
			convert_to_long(&tmp);
			dest->l = Z_LVAL(tmp);
			break;

		case FFI_TYPE_UINT64:
		case FFI_TYPE_SINT64:
			convert_to_long(&tmp);
			dest->l = Z_LVAL(tmp);
			break;

		case FFI_TYPE_STRUCT:
		case FFI_TYPE_POINTER:
		default:
			zend_throw_exception(mffi_ce_exception, "Unimplemented type", 1);
			return;
	}	
}

static inline void php_mffi_set_return_value(zval *return_value, php_mffi_value *result, long type) {
	switch (type) {
		case FFI_TYPE_INT:
			ZVAL_LONG(return_value, result->i);
			break;

		case FFI_TYPE_FLOAT:
			ZVAL_DOUBLE(return_value, result->f);
			break;

		case FFI_TYPE_DOUBLE:
			ZVAL_DOUBLE(return_value, result->d);
			break;

		case FFI_TYPE_LONGDOUBLE:
			ZVAL_DOUBLE(return_value, result->D);
			break;

		case FFI_TYPE_UINT8:
		case FFI_TYPE_SINT8:
		case FFI_TYPE_UINT16:
		case FFI_TYPE_SINT16:
			ZVAL_LONG(return_value, result->i);
			break;

		case FFI_TYPE_UINT32:
		case FFI_TYPE_SINT32:
		case FFI_TYPE_UINT64:
		case FFI_TYPE_SINT64:
			ZVAL_LONG(return_value, result->l);
			break;

		default:
			ZVAL_NULL(return_value);
			break;
	}
}

/* {{{ */
PHP_METHOD(MFFI_Function, __invoke)
{
	zval *args = NULL, *current_arg = NULL;
	long arg_count = 0, i = 0;
	php_mffi_function_object *intern;
	php_mffi_value ret_value, *values;
	void **value_pointers;

	PHP_MFFI_ERROR_HANDLING();
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "*", &args, &arg_count) == FAILURE) {
		PHP_MFFI_RESTORE_ERRORS();
		return;
	}
	PHP_MFFI_RESTORE_ERRORS();

	PHP_MFFI_FUNCTION_FROM_OBJECT(intern, getThis());

	if (arg_count != intern->arg_count) {
		zend_throw_exception(mffi_ce_exception, "Incorrect argument count", 1);
		return;
	}

	values = (php_mffi_value *) ecalloc(arg_count, sizeof(php_mffi_value));
	value_pointers = (void **) ecalloc(arg_count, sizeof(void *));
	
	for (i = 0; i < arg_count; i++) {
		php_mffi_set_argument(&args[i], &values[i], intern->php_arg_types[i]);
		value_pointers[i] = &values[i];
	}

	ffi_call(&intern->cif, intern->function, &ret_value, value_pointers);

	php_mffi_set_return_value(return_value, &ret_value, intern->php_return_type);

	efree(values);
	efree(value_pointers);
}
/* }}} */

/* {{{ */
const zend_function_entry mffi_function_methods[] = {
	PHP_ME(MFFI_Function, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR|ZEND_ACC_FINAL)
	PHP_ME(MFFI_Function, __invoke, NULL, ZEND_ACC_PUBLIC)
	PHP_FE_END
};
/* }}} */

/* {{{ */
php_mffi_function_object *php_mffi_function_fetch_object(zend_object *obj) {
	return (php_mffi_function_object *)((char*)(obj) - XtOffsetOf(php_mffi_function_object, std));
}
/* }}} */

/* {{{ */
static zend_object *mffi_function_object_new(zend_class_entry *ce)
{
	php_mffi_function_object *object = ecalloc(1, sizeof(php_mffi_function_object) + zend_object_properties_size(ce));
	zend_object_std_init(&object->std, ce);
	object_properties_init(&object->std, ce);
	object->std.handlers = &mffi_function_object_handlers;
	return &object->std;
}
/* }}} */

/* {{{ */
static void mffi_function_object_free_storage(zend_object *object)
{
	php_mffi_function_object *intern = php_mffi_function_fetch_object(object);

	if (!intern) {
		return;
	}

	if (intern->ffi_arg_types) {
		efree(intern->ffi_arg_types);
	}
	
	if (intern->php_arg_types) {
		efree(intern->php_arg_types);
	}

	zend_object_std_dtor(&intern->std);
}
/* }}} */

/* {{{ */
PHP_MINIT_FUNCTION(mffi_function)
{
	zend_class_entry function_ce;

	memcpy(&mffi_function_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	mffi_function_object_handlers.offset = XtOffsetOf(php_mffi_function_object, std);
	mffi_function_object_handlers.free_obj = mffi_function_object_free_storage;
	mffi_function_object_handlers.clone_obj = NULL;

	INIT_NS_CLASS_ENTRY(function_ce, "MFFI", "Function", mffi_function_methods);
	function_ce.create_object = mffi_function_object_new;
	mffi_ce_function = zend_register_internal_class(&function_ce);

	return SUCCESS;
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
