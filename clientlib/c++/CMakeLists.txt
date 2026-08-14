set(DATAMAN_CXX_SOURCES
	accept.cc back.cc clear.cc cl_win.cc datafield_impl.cc
	db_comm.cc datamanError.cc delete.cc flush.cc forward.cc
	get.cc get_current.cc get_first.cc get_last.cc get_long.cc
	get_next.cc get_prior.cc grow_win.cc include.cc index.cc
	init_dataman.cc init_dwin.cc insert.cc mask.cc mkidx.cc
	new_win.cc parse_get.cc pause.cc pop_win.cc protect.cc recio.cc
	release.cc restore.cc save.cc screen.cc show.cc sort.cc substr.cc
	window.cc key_impl.cc remove.cc get_ll.cc put_ll.cc start_xact.cc
	commit.cc rollback.cc subscript.cc)

add_library(dataman_cpp SHARED ${DATAMAN_CXX_SOURCES})
add_library(Dataman::CXX ALIAS dataman_cpp)
target_compile_definitions(dataman_cpp PRIVATE
	DATAMAN_CMAKE_BUILD __STDC_FORMAT_MACROS)
target_include_directories(dataman_cpp
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/dataman>
	PRIVATE
		"${PROJECT_SOURCE_DIR}/server"
		"${PROJECT_BINARY_DIR}/generated"
		${CURSES_INCLUDE_DIRS})
target_link_libraries(dataman_cpp PRIVATE ${CURSES_LIBRARIES})
set_target_properties(dataman_cpp PROPERTIES
	OUTPUT_NAME "dataman++"
	VERSION ${PROJECT_VERSION}
	SOVERSION ${PROJECT_VERSION_MAJOR})

set(DATAMAN_CXX_HEADERS
	datafield.hpp dataman.hpp datarecord.hpp fileEdit.hpp endSort.hpp
	index.hpp key.hpp proto.hpp sort.hpp datamanError.hpp)

install(TARGETS dataman_cpp
	LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES ${DATAMAN_CXX_HEADERS}
	DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/dataman")

if(BUILD_TESTING)
	add_executable(datafield_test
		"${PROJECT_SOURCE_DIR}/tests/cpp/datafield_test.cc")
	target_include_directories(datafield_test PRIVATE
		"${PROJECT_SOURCE_DIR}/server")
	target_link_libraries(datafield_test PRIVATE Dataman::CXX)
	add_test(NAME cpp-datafield COMMAND datafield_test)
endif()
