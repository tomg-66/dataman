cmake_minimum_required(VERSION 3.16)

project(Dataman VERSION 4.1.0 LANGUAGES C CXX)

include(GNUInstallDirs)
include(CTest)

option(DATAMAN_BUILD_JAVA "Build the Java client library" OFF)
option(DATAMAN_BUILD_DFEDIT_STATIC "Link dfedit statically" OFF)
option(DATAMAN_ENABLE_INTEGRATION_TESTS
	"Register tests that require a running Dataman server" OFF)
option(DATAMAN_ENABLE_RECOVERY_TESTS
	"Build isolated Linux servers for the crash/restart protocol test" OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

find_package(Curses REQUIRED)
find_package(Threads REQUIRED)

configure_file(cmake/config.h.in generated/config.h)

add_subdirectory(clientlib/c)
add_subdirectory(clientlib/c++)
add_subdirectory(server)

if(DATAMAN_BUILD_JAVA)
	add_subdirectory(clientlib/java)
endif()

if(DATAMAN_ENABLE_INTEGRATION_TESTS)
	add_test(NAME java-integration
		COMMAND "${CMAKE_SOURCE_DIR}/tests/run_java_tests.sh")
	set_tests_properties(java-integration PROPERTIES
		WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()

install(FILES Cleanfile sample_rc_script
	DESTINATION "${CMAKE_INSTALL_DOCDIR}/examples")

install(PROGRAMS scripts/dataman-system-setup DESTINATION "${CMAKE_INSTALL_SBINDIR}")
install(CODE "
    execute_process(COMMAND sh \"${CMAKE_CURRENT_SOURCE_DIR}/scripts/dataman-system-setup\"
        RESULT_VARIABLE setup_result)
    if(NOT setup_result EQUAL 0)
        message(FATAL_ERROR \"Dataman host provisioning failed\")
    endif()
")

set(DATAMAN_SYSTEMD_UNIT_DIR "lib/systemd/system" CACHE STRING
    "systemd unit installation directory (relative to prefix or absolute)")
set(DATAMAN_BINDIR "${CMAKE_INSTALL_FULL_BINDIR}")
configure_file(systemd/dataman.service.in dataman.service @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/dataman.service"
    DESTINATION "${DATAMAN_SYSTEMD_UNIT_DIR}")
