noinst_LTLIBRARIES = libmini.la

libmini_la_SOURCES = show.c accept.c pause.c screen.c mask.c init_dwin.c \
					db_err.c

includedir = $(prefix)/include/dataman


lib_LTLIBRARIES = libdataman.la

libdataman_la_SOURCES = accept.c add_protect.c back.c clear.c cl_win.c \
		data_globs.c db_connect.c db_discon.c db_err.c db_send.c \
		delete.c findex.c flush.c forward.c get.c get_current.c get_first.c \
		get_last.c get_long.c get_next.c get_prior.c grow_win.c \
		iclose.c include.c init_dataman.c init_dwin.c insert.c \
		iopen.c mask.c match.c mkidx.c move.c new_win.c \
		pause.c pop_win.c protect.c recio.c release.c remove.c \
		restore.c save.c screen.c show.c sort.c substr.c window.c \
		put_long.c put_blob.c get_ll.c start_xact.c commit.c rollback.c \
		put_ll.c


libdataman_la_LDFLAGS = -version-info 3:8:0 -lcurses

noinst_HEADERS = node.h

include_HEADERS = dataman.h globs.h index.h m_params.h proto.h \
		sort.h wind.h window.h w_params.h

INCLUDES = -I../../server

##AM_CFLAGS = -DDWINDOW
