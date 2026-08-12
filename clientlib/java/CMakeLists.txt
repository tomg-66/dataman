find_package(Java REQUIRED COMPONENTS Development)
include(UseJava)

set(DATAMAN_JAVA_SOURCES
	Dataman.java DatamanComms.java DatamanErrVal.java DatamanField.java
	DatamanFileDesc.java DatamanFunc.java DatamanIndexFile.java
	DatamanIndex.java DatamanIndexSave.java DatamanKey.java
	DatamanRecDesc.java DatamanRecord.java DatamanRuntimeException.java)

add_jar(dataman_java
	SOURCES ${DATAMAN_JAVA_SOURCES}
	OUTPUT_NAME dataman)

install_jar(dataman_java "${CMAKE_INSTALL_DATADIR}/java")
