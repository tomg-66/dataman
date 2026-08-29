set(DATAMAN_C_SOURCES
	accept.c add_protect.c back.c clear.c cl_win.c
	data_globs.c db_connect.c db_discon.c db_err.c db_send.c
	delete.c findex.c flush.c forward.c get.c get_current.c get_first.c
	get_last.c get_long.c get_next.c get_prior.c grow_win.c
	iclose.c include.c init_dataman.c init_dwin.c insert.c
	iopen.c mask.c match.c mkidx.c move.c new_win.c
	pause.c pop_win.c protect.c recio.c release.c remove.c response.c
	restore.c save.c screen.c show.c sort.c substr.c window.c
	put_long.c put_blob.c get_ll.c start_xact.c commit.c rollback.c
	put_ll.c)

set(DATAMAN_MINI_SOURCES
	show.c accept.c pause.c screen.c mask.c init_dwin.c db_err.c)

add_library(dataman_c SHARED ${DATAMAN_C_SOURCES})
add_library(Dataman::C ALIAS dataman_c)
target_compile_definitions(dataman_c PRIVATE DATAMAN_CMAKE_BUILD)
target_include_directories(dataman_c
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/dataman>
	PRIVATE
		"${PROJECT_SOURCE_DIR}/server"
		"${PROJECT_BINARY_DIR}/generated")
target_link_libraries(dataman_c PRIVATE ${CURSES_LIBRARIES})
set_target_properties(dataman_c PROPERTIES
	OUTPUT_NAME dataman
	VERSION ${PROJECT_VERSION}
	SOVERSION ${PROJECT_VERSION_MAJOR})

add_library(dataman_mini STATIC ${DATAMAN_MINI_SOURCES})
target_compile_definitions(dataman_mini PRIVATE DATAMAN_CMAKE_BUILD)
target_include_directories(dataman_mini PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}"
	"${PROJECT_SOURCE_DIR}/server"
	"${PROJECT_BINARY_DIR}/generated"
	${CURSES_INCLUDE_DIRS})
target_link_libraries(dataman_mini PUBLIC ${CURSES_LIBRARIES})

set(DATAMAN_C_HEADERS
	dataman.h globs.h index.h m_params.h proto.h
	sort.h wind.h window.h w_params.h)

install(TARGETS dataman_c
	LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES ${DATAMAN_C_HEADERS}
	DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/dataman")
