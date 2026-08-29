lib_LTLIBRARIES = libdataman++.la
libdataman___la_SOURCES = accept.cc back.cc clear.cc cl_win.cc datafield_impl.cc \
		db_comm.cc datamanError.cc delete.cc flush.cc forward.cc \
		get.cc get_current.cc get_first.cc get_last.cc get_long.cc \
		get_next.cc get_prior.cc grow_win.cc include.cc index.cc \
		init_dataman.cc init_dwin.cc insert.cc mask.cc mkidx.cc \
		new_win.cc parse_get.cc pause.cc pop_win.cc protect.cc recio.cc \
		release.cc restore.cc save.cc screen.cc show.cc sort.cc substr.cc \
		window.cc key_impl.cc remove.cc get_ll.cc put_ll.cc start_xact.cc \
		commit.cc rollback.cc subscript.cc

includedir = $(prefix)/include/dataman

libdataman___la_LDFLAGS = -version-info 4:1:0 -lcurses

noinst_HEADERS = db_comm.hpp wind.h window.h

include_HEADERS = datafield.hpp dataman.hpp datarecord.hpp fileEdit.hpp endSort.hpp  \
		index.hpp key.hpp proto.hpp sort.hpp datamanError.hpp

AM_CPPFLAGS = -I$(top_srcdir)/server

# have to add this because according to inttypes.h you muse explicitly 
# declare that you want the defines in C++

DEFS += -D__STDC_FORMAT_MACROS
