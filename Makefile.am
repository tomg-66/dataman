SUBDIRS=clientlib/c clientlib/c++ server
if BUILD_JAVA
SUBDIRS += clientlib/java
endif

remake: clean all

PHP_EXTENSION_DIST = \
	clientlib/php/CREDITS \
	clientlib/php/EXPERIMENTAL \
	clientlib/php/README \
	clientlib/php/config.m4 \
	clientlib/php/config.w32 \
	clientlib/php/dataman.c \
	clientlib/php/dataman.stub.php \
	clientlib/php/dataman_arginfo.h \
	clientlib/php/php_dataman.h \
	clientlib/php/tests/001.phpt \
	clientlib/php/tests/002.phpt \
	clientlib/php/tests/003.phpt \
	clientlib/php/tests/004.phpt \
	clientlib/php/tests/005.phpt \
	clientlib/php/tests/006.phpt \
	clientlib/php/tests/007.phpt \
	clientlib/php/tests/008.phpt \
	clientlib/php/tests/009.phpt \
	clientlib/php/tests/010.phpt \
	clientlib/php/tests/011.phpt \
	clientlib/php/tests/012.phpt \
	clientlib/php/tests/013.phpt \
	clientlib/php/tests/014.phpt \
	clientlib/php/tests/015.phpt

CMAKE_DIST = \
	CMakeLists.txt \
	cmake/config.h.in \
	clientlib/c/CMakeLists.txt \
	clientlib/c++/CMakeLists.txt \
	clientlib/java/CMakeLists.txt \
	server/CMakeLists.txt

DOCUMENTATION_DIST = \
	docs/README.md \
	docs/c-api.md \
	docs/cmake.md \
	docs/concepts.md \
	docs/cpp-api.md \
	docs/development.md \
	docs/getting-started.md \
	docs/indexes.md \
	docs/java-api.md \
	docs/operations.md \
	docs/php-extension.md \
	docs/server.md \
	docs/transactions.md \
	docs/utilities.md \
	old_docs/c \
	old_docs/c++ \
	old_docs/c++_reference.html \
	old_docs/c_reference.html \
	old_docs/example.html \
	old_docs/faq.html \
	old_docs/index.html \
	old_docs/installing.html \
	old_docs/java \
	old_docs/license.html \
	old_docs/long.html \
	old_docs/long.sxw \
	old_docs/long.txt \
	old_docs/manual.html \
	old_docs/php_reference.html \
	old_docs/running.html \
	old_docs/theory.html \
	old_docs/utilities.html

TEST_DIST = \
	tests/README.md \
	tests/fixtures/one_rec.i \
	tests/fixtures/one_rec.t \
	tests/fixtures/blob_rec.i \
	tests/fixtures/blob_rec.t \
	tests/cpp/datafield_test.cc \
	tests/java/BuildOneRecordIndex.java \
	tests/java/OneRecordIntegrationTest.java \
	tests/run_java_tests.sh \
	tests/setup_test.sh

EXTRA_DIST = Cleanfile sample_rc_script $(PHP_EXTENSION_DIST) \
	$(CMAKE_DIST) $(DOCUMENTATION_DIST) $(TEST_DIST)
