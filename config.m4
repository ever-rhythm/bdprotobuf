dnl $Id$
dnl config.m4 for extension bdprotobuf

dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary. This file will not work
dnl without editing.

dnl If your extension references something external, use with:

dnl PHP_ARG_WITH(bdprotobuf, for bdprotobuf support,
dnl Make sure that the comment is aligned:
dnl [  --with-bdprotobuf             Include bdprotobuf support])

dnl Otherwise use enable:

PHP_ARG_ENABLE(bdprotobuf, whether to enable bdprotobuf support,
[  --enable-bdprotobuf           Enable bdprotobuf support])

if test "$PHP_BDPROTOBUF" != "no"; then

PHP_REQUIRE_CXX()
PHP_ADD_INCLUDE(../libs/include)

PHP_ADD_LIBRARY_WITH_PATH(protobuf, ../libs/lib/, BDPROTOBUF_SHARED_LIBADD)
PHP_ADD_LIBRARY(stdc++,"",BDPROTOBUF_SHARED_LIBADD)
PHP_SUBST(BDPROTOBUF_SHARED_LIBADD)

  dnl Write more examples of tests here...

  dnl # --with-bdprotobuf -> check with-path
  dnl SEARCH_PATH="/usr/local /usr"     # you might want to change this
  dnl SEARCH_FOR="/include/bdprotobuf.h"  # you most likely want to change this
  dnl if test -r $PHP_BDPROTOBUF/$SEARCH_FOR; then # path given as parameter
  dnl   BDPROTOBUF_DIR=$PHP_BDPROTOBUF
  dnl else # search default path list
  dnl   AC_MSG_CHECKING([for bdprotobuf files in default path])
  dnl   for i in $SEARCH_PATH ; do
  dnl     if test -r $i/$SEARCH_FOR; then
  dnl       BDPROTOBUF_DIR=$i
  dnl       AC_MSG_RESULT(found in $i)
  dnl     fi
  dnl   done
  dnl fi
  dnl
  dnl if test -z "$BDPROTOBUF_DIR"; then
  dnl   AC_MSG_RESULT([not found])
  dnl   AC_MSG_ERROR([Please reinstall the bdprotobuf distribution])
  dnl fi

  dnl # --with-bdprotobuf -> add include path
  dnl PHP_ADD_INCLUDE($BDPROTOBUF_DIR/include)

  dnl # --with-bdprotobuf -> check for lib and symbol presence
  dnl LIBNAME=bdprotobuf # you may want to change this
  dnl LIBSYMBOL=bdprotobuf # you most likely want to change this 

  dnl PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  dnl [
  dnl   PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $BDPROTOBUF_DIR/$PHP_LIBDIR, BDPROTOBUF_SHARED_LIBADD)
  dnl   AC_DEFINE(HAVE_BDPROTOBUFLIB,1,[ ])
  dnl ],[
  dnl   AC_MSG_ERROR([wrong bdprotobuf lib version or lib not found])
  dnl ],[
  dnl   -L$BDPROTOBUF_DIR/$PHP_LIBDIR -lm
  dnl ])
  dnl
  dnl PHP_SUBST(BDPROTOBUF_SHARED_LIBADD)

  PHP_NEW_EXTENSION(bdprotobuf, bdprotobuf.cpp reader.cpp writer.cpp, $ext_shared,, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)
fi
PHP_C_BIGENDIAN()
