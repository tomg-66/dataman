function(dataman_server_program target)
	add_executable(${target} ${ARGN})
	target_compile_definitions(${target} PRIVATE _LARGEFILE64_SOURCE)
	target_include_directories(${target} PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}"
		"${PROJECT_BINARY_DIR}/generated")
	install(TARGETS ${target} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
endfunction()

dataman_server_program(dataman
	dataman.c err_sys.c verify_pid.c version.c)
if(BUILD_TESTING)
	add_test(NAME server-version COMMAND dataman -v)
	set_tests_properties(server-version PROPERTIES
		PASS_REGULAR_EXPRESSION "version ${PROJECT_VERSION}")
endif()

dataman_server_program(dataman_con
	serial.c serial_service.c serial_setup.c serial_cleanup.c
	server_comm.c err_sys.c put_long.c verify_pid.c term_dbserve.c
	version.c xact.c commit.c rollback.c put_ll.c get_ll.c)

dataman_server_program(dataman_srv
	dbserve.c back.c clear.c delete.c dispatch.c err_sys.c flush.c
	forward.c get.c get_current.c get_first.c get_last.c get_long.c
	get_next.c get_prior.c get_short.c iclose.c include.c insert.c iopen.c
	lock.c pend.c protect.c put_long.c get_file.c put_short.c remove.c
	substr.c upd_idx.c verify_pid.c get_desc.c init_dataman.c release.c
	mkidx.c sort.c restore.c get_datafile_desc.c version.c get_blobs.c
	put_blobs.c get_ll.c put_ll.c get_rec.c blob_ctl.c undelete.c index_v2.c)
target_link_libraries(dataman_srv PRIVATE Threads::Threads)

dataman_server_program(mkdf
	mkdf.c substr.c put_short.c put_long.c put_ll.c get_ll.c)
dataman_server_program(dumpdf
	dumpdf.c get_datafile_desc.c substr.c get_short.c get_long.c get_ll.c)
dataman_server_program(dumpix
	dumpix.c substr.c get_short.c get_long.c get_ll.c index_v2.c)
dataman_server_program(dnodes dnodes.c index_v2.c)
dataman_server_program(dbclean
	clean.c check_it.c rebuild.c get_datafile_desc.c get_ll.c put_ll.c
	get_short.c put_short.c get_long.c)
dataman_server_program(dfedit
	dfedit.c get_short.c put_short.c get_long.c put_long.c
	get_datafile_desc.c lock.c get_ll.c put_ll.c)
target_link_libraries(dfedit PRIVATE dataman_mini Threads::Threads ${CURSES_LIBRARIES})
if(DATAMAN_BUILD_DFEDIT_STATIC)
	target_link_options(dfedit PRIVATE -static)
endif()

install(FILES datafile_header.h
	DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/dataman")
