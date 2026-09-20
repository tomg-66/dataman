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
	add_executable(index_find_test tests/index_find_test.c index_v2.c storage_io.c)
	target_include_directories(index_find_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
	add_test(NAME index-find COMMAND index_find_test)

	add_test(NAME server-version COMMAND dataman -v)
	set_tests_properties(server-version PROPERTIES
		PASS_REGULAR_EXPRESSION "version ${PROJECT_VERSION}")
endif()

dataman_server_program(dataman_con
	serial.c serial_service.c serial_setup.c serial_cleanup.c
	server_comm.c err_sys.c put_long.c verify_pid.c term_dbserve.c
	version.c put_ll.c get_ll.c)

dataman_server_program(dataman_srv
	dbserve.c back.c clear.c delete.c dispatch.c err_sys.c flush.c
	forward.c get.c get_current.c get_first.c get_last.c get_long.c
	get_next.c get_prior.c get_short.c iclose.c include.c insert.c iopen.c
	lock.c pend.c protect.c put_long.c get_file.c put_short.c remove.c
	substr.c upd_idx.c verify_pid.c get_desc.c init_dataman.c release.c
	mkidx.c sort.c restore.c get_datafile_desc.c version.c get_blobs.c
	put_blobs.c get_ll.c put_ll.c blob_ctl.c index_v2.c
	storage_io.c session_root.c undo_journal.c journal_startup.c transaction_session.c transaction_dispatch.c transaction_wire.c)
target_link_libraries(dataman_srv PRIVATE Threads::Threads)

if(BUILD_TESTING AND DATAMAN_ENABLE_RECOVERY_TESTS)
	if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
		message(FATAL_ERROR "The isolated recovery test requires Linux")
	endif()
	find_package(Python3 REQUIRED COMPONENTS Interpreter)
	foreach(kind srv con)
		get_target_property(sources dataman_${kind} SOURCES)
		add_executable(recovery_${kind} ${sources} tests/recovery_isolation.c)
		target_compile_definitions(recovery_${kind} PRIVATE _LARGEFILE64_SOURCE)
		target_include_directories(recovery_${kind} PRIVATE
			"${CMAKE_CURRENT_SOURCE_DIR}" "${PROJECT_BINARY_DIR}/generated")
		target_link_libraries(recovery_${kind} PRIVATE Threads::Threads)
		target_link_options(recovery_${kind} PRIVATE
			-Wl,--wrap=msgget -Wl,--wrap=verify_pid -Wl,--wrap=bind
			-Wl,--wrap=term_dbserve)
	endforeach()
	add_test(NAME transaction-recovery COMMAND "${Python3_EXECUTABLE}"
		"${PROJECT_SOURCE_DIR}/tests/transaction_recovery_test.py"
		$<TARGET_FILE:recovery_srv> $<TARGET_FILE:recovery_con> $<TARGET_FILE:mkdf>)
	set_tests_properties(transaction-recovery PROPERTIES TIMEOUT 120 LABELS integration)
endif()

if(BUILD_TESTING)
	add_executable(serial_prefix_test tests/serial_prefix_test.c)
	add_executable(protocol_handshake_test tests/protocol_handshake_test.c)
	target_include_directories(protocol_handshake_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
	add_test(NAME protocol-handshake COMMAND protocol_handshake_test)
	find_package(Python3 QUIET COMPONENTS Interpreter)
	if(Python3_Interpreter_FOUND)
		add_test(NAME protocol-mapping COMMAND "${Python3_EXECUTABLE}"
			"${PROJECT_SOURCE_DIR}/tests/protocol_mapping_test.py")
	endif()
	target_include_directories(serial_prefix_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
	target_compile_options(serial_prefix_test PRIVATE -ffunction-sections -fdata-sections)
	target_link_options(serial_prefix_test PRIVATE -Wl,--gc-sections)
	add_test(NAME serial-prefix COMMAND serial_prefix_test)

	add_executable(storage_io_test tests/storage_io_test.c)
	target_include_directories(storage_io_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
	add_test(NAME storage-io COMMAND storage_io_test)
	add_executable(flush_io_test tests/flush_io_test.c flush.c)
	target_include_directories(flush_io_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
	add_test(NAME flush-io COMMAND flush_io_test)
	add_executable(undo_journal_test tests/undo_journal_test.c storage_io.c)
	target_include_directories(undo_journal_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
	add_test(NAME undo-journal COMMAND undo_journal_test)
	add_executable(session_root_test tests/session_root_test.c)
	target_include_directories(session_root_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
	target_link_libraries(session_root_test PRIVATE Threads::Threads)
	add_test(NAME session-root COMMAND session_root_test)
	add_executable(journal_startup_test tests/journal_startup_test.c
		journal_startup.c storage_io.c)
	target_include_directories(journal_startup_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
	target_link_libraries(journal_startup_test PRIVATE Threads::Threads)
	add_test(NAME journal-startup COMMAND journal_startup_test)
	add_executable(verify_pid_test tests/verify_pid_test.c verify_pid.c)
	add_test(NAME pid-ownership COMMAND verify_pid_test)
	add_executable(transaction_session_test tests/transaction_session_test.c
		transaction_session.c transaction_dispatch.c transaction_wire.c undo_journal.c storage_io.c flush.c index_v2.c put_blobs.c blob_ctl.c get_long.c remove.c
		insert.c delete.c get_ll.c put_ll.c)
	target_include_directories(transaction_session_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
	target_link_libraries(transaction_session_test PRIVATE Threads::Threads)
	add_test(NAME transaction-session COMMAND transaction_session_test)
	add_executable(record_mutation_test tests/record_mutation_test.c
		insert.c delete.c storage_io.c get_ll.c put_ll.c)
	target_include_directories(record_mutation_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
	add_test(NAME record-mutation COMMAND record_mutation_test)
	add_executable(blob_io_test tests/blob_io_test.c storage_io.c get_long.c)
	target_include_directories(blob_io_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
	add_test(NAME blob-io COMMAND blob_io_test)
endif()

dataman_server_program(mkdf
	mkdf.c substr.c put_short.c put_long.c put_ll.c get_ll.c)
dataman_server_program(dumpdf
	dumpdf.c get_datafile_desc.c substr.c get_short.c get_long.c get_ll.c)
dataman_server_program(dumpix
	dumpix.c substr.c get_short.c get_long.c get_ll.c index_v2.c storage_io.c)
dataman_server_program(dnodes dnodes.c index_v2.c storage_io.c)
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
