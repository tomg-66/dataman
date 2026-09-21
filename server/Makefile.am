
includedir = $(prefix)/include/dataman

AUTOMAKE_OPTIONS = subdir-objects
EXTRA_DIST = tests/recovery_isolation.c

AM_CFLAGS = -D_LARGEFILE64_SOURCE

bin_PROGRAMS=dataman dataman_con dataman_srv mkdf dumpdf dumpix dnodes \
			 dbclean dfedit

dataman_con_SOURCES = serial.c serial_service.c serial_setup.c \
	serial_cleanup.c server_comm.c err_sys.c put_long.c verify_pid.c \
	term_dbserve.c version.c put_ll.c get_ll.c

dataman_srv_SOURCES = dbserve.c back.c clear.c delete.c \
	dispatch.c err_sys.c flush.c forward.c get.c get_current.c \
	get_first.c get_last.c get_long.c get_next.c \
	get_prior.c get_short.c iclose.c include.c insert.c iopen.c \
	lock.c pend.c protect.c put_long.c get_file.c \
	put_short.c remove.c substr.c upd_idx.c verify_pid.c \
	get_desc.c init_dataman.c release.c mkidx.c sort.c restore.c \
	get_datafile_desc.c version.c get_blobs.c put_blobs.c get_ll.c \
	put_ll.c blob_ctl.c index_v2.c storage_io.c session_root.c \
	undo_journal.c journal_startup.c transaction_session.c \
	transaction_dispatch.c transaction_wire.c

dataman_srv_LDADD = -lpthread

dfedit_LDADD = $(top_builddir)/clientlib/c/libmini.la -lcurses

dfedit_LDFLAGS = -static

dataman_SOURCES = dataman.c err_sys.c verify_pid.c version.c

mkdf_SOURCES = mkdf.c substr.c put_short.c put_long.c put_ll.c get_ll.c

dumpdf_SOURCES =  dumpdf.c get_datafile_desc.c substr.c get_short.c \
		get_long.c get_ll.c

dbclean_SOURCES = clean.c check_it.c rebuild.c get_datafile_desc.c get_ll.c \
		put_ll.c get_short.c put_short.c get_long.c

dumpix_SOURCES = dumpix.c substr.c get_short.c get_long.c get_ll.c index_v2.c storage_io.c

dnodes_SOURCES = dnodes.c index_v2.c storage_io.c

dfedit_SOURCES = dfedit.c get_short.c put_short.c get_long.c \
		put_long.c get_datafile_desc.c lock.c get_ll.c \
		put_ll.c

noinst_HEADERS = dbfunc.h errors.h protocol.h lock.h msg.h srv_index.h file_desc.h index_v2.h misc.h \
		 storage_io.h undo_journal.h session_root.h journal_startup.h \
		 transaction_session.h transaction_dispatch.h transaction_wire.h

check_PROGRAMS = index_find_test serial_prefix_test storage_io_test flush_io_test \
		 undo_journal_test session_root_test journal_startup_test verify_pid_test \
		 transaction_session_test record_mutation_test \
		 blob_io_test protocol_handshake_test

TESTS = $(check_PROGRAMS)

protocol_handshake_test_SOURCES = tests/protocol_handshake_test.c

index_find_test_SOURCES = tests/index_find_test.c index_v2.c storage_io.c
storage_io_test_SOURCES = tests/storage_io_test.c
flush_io_test_SOURCES = tests/flush_io_test.c flush.c
undo_journal_test_SOURCES = tests/undo_journal_test.c storage_io.c
session_root_test_SOURCES = tests/session_root_test.c
session_root_test_LDADD = -lpthread
journal_startup_test_SOURCES = tests/journal_startup_test.c journal_startup.c storage_io.c
journal_startup_test_LDADD = -lpthread
verify_pid_test_SOURCES = tests/verify_pid_test.c verify_pid.c
transaction_session_test_SOURCES = tests/transaction_session_test.c transaction_session.c \
		transaction_dispatch.c transaction_wire.c undo_journal.c \
		storage_io.c flush.c index_v2.c put_blobs.c blob_ctl.c \
		get_long.c remove.c insert.c delete.c get_ll.c \
		put_ll.c

transaction_session_test_LDADD = -lpthread
record_mutation_test_SOURCES = tests/record_mutation_test.c insert.c delete.c storage_io.c get_ll.c put_ll.c
blob_io_test_SOURCES = tests/blob_io_test.c storage_io.c get_long.c
AM_CPPFLAGS = -I$(srcdir)

include_HEADERS = datafile_header.h

serial_prefix_test_SOURCES = tests/serial_prefix_test.c
serial_prefix_test_CFLAGS = $(AM_CFLAGS) -ffunction-sections -fdata-sections
serial_prefix_test_LDFLAGS = -Wl,--gc-sections
