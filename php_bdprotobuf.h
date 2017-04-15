/*
  +----------------------------------------------------------------------+
  | PHP Version 7                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2016 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:                                                              |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef PHP_BDPROTOBUF_H
#define PHP_BDPROTOBUF_H

extern zend_module_entry bdprotobuf_module_entry;
#define phpext_bdprotobuf_ptr &bdprotobuf_module_entry

#define PHP_BDPROTOBUF_VERSION "0.1.0" /* Replace with version number for your extension */

#ifdef PHP_WIN32
#	define PHP_BDPROTOBUF_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#	define PHP_BDPROTOBUF_API __attribute__ ((visibility("default")))
#else
#	define PHP_BDPROTOBUF_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif


PHP_MINIT_FUNCTION(bdprotobuf);
PHP_MSHUTDOWN_FUNCTION(bdprotobuf);
PHP_RINIT_FUNCTION(bdprotobuf);
PHP_RSHUTDOWN_FUNCTION(bdprotobuf);
PHP_MINFO_FUNCTION(bdprotobuf);

PHP_FUNCTION(proto2desc);
PHP_FUNCTION(array2protobuf);
PHP_FUNCTION(protobuf2array);
PHP_FUNCTION(array2protobufbydesc);
PHP_FUNCTION(protobuf2arraybydesc);

/*
  	Declare any global variables you may need between the BEGIN
	and END macros here:

ZEND_BEGIN_MODULE_GLOBALS(bdprotobuf)
	zend_long  global_value;
	char *global_string;
ZEND_END_MODULE_GLOBALS(bdprotobuf)
*/

/* Always refer to the globals in your function as BDPROTOBUF_G(variable).
   You are encouraged to rename these macros something shorter, see
   examples in any other php module directory.
*/
#define BDPROTOBUF_G(v) ZEND_MODULE_GLOBALS_ACCESSOR(bdprotobuf, v)

#define PB_FOREACH(iter, hash)\
	for (zend_hash_internal_pointer_reset_ex((hash), (iter)); zend_hash_has_more_elements_ex((hash), (iter)) == SUCCESS; zend_hash_move_forward_ex((hash), (iter)))

#define PHP_BDPB_WARNING(fmt, arg...)\
	php_error(E_WARNING, "[%s:%d]" fmt, __FILE__, __LINE__, ## arg)

#if defined(ZTS) && defined(COMPILE_DL_BDPROTOBUF)
ZEND_TSRMLS_CACHE_EXTERN()
#endif

#endif	/* PHP_BDPROTOBUF_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
