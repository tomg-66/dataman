
includedir = $(prefix)/include/dataman

AM_CFLAGS = -D_LARGEFILE64_SOURCE

bin_PROGRAMS=dataman dataman_con dataman_srv mkdf dumpdf dumpix dnodes \
			 dbclean dfedit

dataman_con_SOURCES = serial.c serial_service.c serial_setup.c \
	serial_cleanup.c server_comm.c err_sys.c put_long.c verify_pid.c \
	term_dbserve.c version.c xact.c commit.c rollback.c put_ll.c get_ll.c

dataman_srv_SOURCES = dbserve.c back.c clear.c delete.c \
	dispatch.c err_sys.c flush.c forward.c get.c get_current.c \
	get_first.c get_last.c get_long.c get_next.c \
	get_prior.c get_short.c iclose.c include.c insert.c iopen.c \
	lock.c pend.c protect.c put_long.c get_file.c \
	put_short.c remove.c substr.c \
	upd_idx.c verify_pid.c get_desc.c init_dataman.c \
	release.c mkidx.c sort.c restore.c get_datafile_desc.c \
	version.c get_blobs.c put_blobs.c get_ll.c put_ll.c get_rec.c \
	blob_ctl.c undelete.c index_v2.c

dataman_srv_LDADD = -lpthread

dfedit_LDADD = -lmini -lcurses

dfedit_LDFLAGS = -static -L$(top_srcdir)/clientlib/c

dataman_SOURCES = dataman.c err_sys.c verify_pid.c version.c

mkdf_SOURCES = mkdf.c substr.c put_short.c put_long.c put_ll.c get_ll.c

dumpdf_SOURCES =  dumpdf.c get_datafile_desc.c substr.c get_short.c \
				  get_long.c get_ll.c

dbclean_SOURCES = clean.c check_it.c rebuild.c get_datafile_desc.c get_ll.c \
				  put_ll.c get_short.c put_short.c get_long.c

dumpix_SOURCES = dumpix.c substr.c get_short.c get_long.c get_ll.c index_v2.c

dnodes_SOURCES = dnodes.c index_v2.c

dfedit_SOURCES = dfedit.c get_short.c put_short.c get_long.c \
				 put_long.c get_datafile_desc.c lock.c get_ll.c \
				 put_ll.c

noinst_HEADERS = dbfunc.h errors.h lock.h msg.h srv_index.h \
				 file_desc.h index_v2.h misc.h xact.h

include_HEADERS = datafile_header.h
