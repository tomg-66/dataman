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

#ifndef PHP_DATAMAN_H
# define PHP_DATAMAN_H

#include <dataman/dataman.h>

#if !defined MIN
#define MIN(x,y)	((x) < (y) ? (x) : (y))
#endif

extern zend_module_entry dataman_module_entry;
# define phpext_dataman_ptr &dataman_module_entry

# define PHP_DATAMAN_VERSION "0.3.0"

# if defined(ZTS) && defined(COMPILE_DL_DATAMAN)
ZEND_TSRMLS_CACHE_EXTERN()
# endif

#endif	/* PHP_DATAMAN_H */
