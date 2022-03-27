cmake_minimum_required(VERSION 3.7.0)

if(EXISTS ${CMAKE_BINARY_DIR}/version.cmake)
	include(${CMAKE_BINARY_DIR}/version.cmake)
else()

	message(WARNING "version.cmake file not found, will try to access git tree")

	if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/cmake/GetGitRevisionDescription.cmake)
		set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
		include(GetGitRevisionDescription)
		git_describe_working_tree(WORKING_TREE_DESCRIPTION)

		message(${WORKING_TREE_DESCRIPTION})

		string(REGEX MATCH "v([.0-9]).([.0-9]).([.0-9])[-]([-0-9a-z]*)$" regex_match ${WORKING_TREE_DESCRIPTION})
		set(VERSION_MAJOR ${CMAKE_MATCH_1})
		set(VERSION_MINOR ${CMAKE_MATCH_2})
		set(VERSION_PATCH ${CMAKE_MATCH_3})
		set(VERSION_TWEAK_ALL ${CMAKE_MATCH_4})

		# In case of not accessed Git Tree WORKING_TREE_DESCRIPTION
		# can be empty. Possible reasons are .git direcory is not exist
		# or the project is a submodule and moved to a different location
		if (${WORKING_TREE_DESCRIPTION} STREQUAL "-128-NOTFOUND")
			message(FATAL_ERROR "Not a Git Repository")
		endif ()

		if (NOT ${VERSION_TWEAK_ALL} STREQUAL "")
			# If working tree is move further from tagged commit or dirty

			string(REGEX MATCH "([0-9]*)-([0-9a-z-]*)$" regex_match ${VERSION_TWEAK_ALL})

			if (NOT ${CMAKE_MATCH_1} STREQUAL "")
				set(VERSION_TWEAK ${CMAKE_MATCH_1})
			else()
				set(VERSION_TWEAK 0)
			endif ()

			if (NOT ${CMAKE_MATCH_2} STREQUAL "")
				set(VERSION_TWEAK_DIRTY 1)
			else()
				set(VERSION_TWEAK_DIRTY 0)
			endif ()

		else()
			# Working tree is all clear
			set(VERSION_TWEAK 0)
			set(VERSION_TWEAK_DIRTY 0)
		endif ()
	else()
		message(FATAL_ERROR "CMake is failed to get version information")
	endif()
endif()

set(PROJECT_VERSION_5 ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}.${VERSION_TWEAK}.${VERSION_TWEAK_DIRTY})
message(${PROJECT_VERSION_5})
set(PROJECT_VERSION_4 ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}.${VERSION_TWEAK})
set(PROJECT_VERSION_3 ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})
set(PROJECT_VERSION_2 ${VERSION_MAJOR}.${VERSION_MINOR})
set(PROJECT_VERSION_1 ${VERSION_MAJOR})
set(PROJECT_NAME "cpp-can-parser")
set(PROJECT_NAME_V1 "${PROJECT_NAME}_${PROJECT_VERSION_1}")
set(PROJECT_NAME_V2 "${PROJECT_NAME}_${PROJECT_VERSION_2}")
set(PROJECT_NAME_V3 "${PROJECT_NAME}_${PROJECT_VERSION_3}")
set(PROJECT_NAME_V4 "${PROJECT_NAME}_${PROJECT_VERSION_4}")
set(PROJECT_NAME_V5 "${PROJECT_NAME}_${PROJECT_VERSION_5}")

set(EXECUTABLE_NAME ${PROJECT_NAME_V3})
set(LIBRARY_NAME ${PROJECT_NAME})
set(TEST_EXECUTABLE_NAME TEST_${PROJECT_NAME_V3})

project(${PROJECT_NAME} VERSION ${PROJECT_VERSION_4})
message(STATUS "Detected version ${PROJECT_VERSION_4}")

set(CMAKE_CXX_STANDARD 20)
# Get UNIX path prefixes
include(GNUInstallDirs)
# Get access to helper functions for creating config files
include(CMakePackageConfigHelpers)

# Prefer RPATH over RUNPATH
SET(CMAKE_EXE_LINKER_FLAGS "-Wl,--disable-new-dtags")

# Include conan created dependency variables
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

include(CTest)
include(GenerateExportHeader)

set(CPPPARSER_INCLUDE_DIRECTORY 
	${CMAKE_CURRENT_LIST_DIR}/include
)

set(CPPPARSER_INCLUDE_DIRECTORY_PRIVATE 
	${CMAKE_CURRENT_LIST_DIR}/src/parsing
)

set(CPPPARSER_SRC_FILES
	src/models/CANDatabase.cpp
	src/models/CANFrame.cpp
  	src/models/CANSignal.cpp
	src/parsing/DBCParser.cpp
	src/parsing/ParsingUtils.cpp
	src/parsing/Tokenizer.cpp
	src/analysis/CANFrameAnalysis.cpp
	)

set(CPP_CAN_PARSER_COMPILATION_TYPE SHARED)
if(CPP_CAN_PARSER_USE_STATIC)
	set(CPP_CAN_PARSER_COMPILATION_TYPE STATIC)
endif()

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS True)

add_library(${LIBRARY_NAME}
	${CPP_CAN_PARSER_COMPILATION_TYPE} 
	${CPPPARSER_SRC_FILES}
	)

set_target_properties(${LIBRARY_NAME} PROPERTIES
		VERSION ${PROJECT_VERSION}
		SOVERSION ${PROJECT_VERSION_MAJOR}
		)

target_include_directories(${LIBRARY_NAME}
	PUBLIC ${CPPPARSER_INCLUDE_DIRECTORY}
		   ${CMAKE_CURRENT_BINARY_DIR}/exports/
	PRIVATE ${CPPPARSER_INCLUDE_DIRECTORY_PRIVATE})
generate_export_header(${LIBRARY_NAME}
	BASE_NAME cpp_can_parser
	EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/exports/cpp_can_parser_export.h)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/exports/cpp_can_parser_export.h
		DESTINATION ${CMAKE_INSTALL_PREFIX}/include)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/cpp-can-parser
		DESTINATION ${CMAKE_INSTALL_PREFIX}/include)

install(TARGETS cpp-can-parser LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)

add_executable(can-parse 
	utils/can-parse/can-parse.cpp
	utils/can-parse/print-frame.cpp
	utils/can-parse/print-single-frame.cpp
	utils/can-parse/check-frame.cpp)
target_link_libraries(can-parse cpp-can-parser)

if(BUILD_TESTING)
	file(COPY tests/dbc-files/
		DESTINATION dbc-files/)
	
	add_executable(cpc-test-parsing
		tests/test-parsing.cpp)
	target_link_libraries(cpc-test-parsing PUBLIC cpp-can-parser)

	add_test(NAME cpc-test-parsing
			COMMAND cpc-test-parsing)

	add_test(NAME cpc-checkframe-1
			 COMMAND can-parse checkframe dbc-files/single-frame-1.dbc)

	add_test(NAME cpc-checkframe-2
			 COMMAND can-parse checkframe dbc-files/single-frame-2.dbc)

	add_test(NAME cpc-checkframe-big-endian-1
			 COMMAND can-parse checkframe 294 dbc-files/big-endian-1.dbc)

	add_test(NAME cpc-checkframe-big-endian-2
			 COMMAND can-parse checkframe 1807 dbc-files/big-endian-1.dbc)
	
	add_test(NAME cpc-checkframe-big-endian-3
			 COMMAND can-parse checkframe 1800 dbc-files/big-endian-1.dbc)
endif()