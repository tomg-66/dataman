
java_JAVA = Dataman.java DatamanComms.java DatamanErrVal.java \
	DatamanField.java DatamanFileDesc.java DatamanFunc.java \
	DatamanIndexFile.java DatamanIndex.java DatamanIndexSave.java \
	DatamanKey.java DatamanRecDesc.java DatamanRecord.java \
	DatamanRuntimeException.java

EXTRA_DIST = $(java_JAVA)

JAVAC = javac

JAVAROOT = ../

# Automake's CLASSPATH_ENV includes JAVAROOT, where package classes are built.
# An explicit -classpath ./ hides those classes during incremental compilation.

CLEANFILES = $(JAVAROOT)Dataman/*.class

javadir = /usr/local/java/Dataman
