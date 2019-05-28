cmake_minimum_required(VERSION 3.2.2)
project(monica)

add_definitions(
	#-DNO_ZMQ
	-DNO_MYSQL
)

#set absolute filenames (to resolve .. in paths)
macro(set_absolute_path var_name path)
	get_filename_component(toAbsPath ${path} ABSOLUTE)
	set(${var_name} ${toAbsPath})
endmacro(set_absolute_path)

#define delete file from path
macro(remove_file list_name file_name)
	get_filename_component(toAbsPath ${file_name} ABSOLUTE)
	list(REMOVE_ITEM ${list_name} "${toAbsPath}")
endmacro(remove_file)

#define source_groups 
macro(GroupSources curdir baseDir)
   file(GLOB children RELATIVE ${baseDir}/${curdir} ${baseDir}/${curdir}/*)
   foreach(child ${children})
		  if(IS_DIRECTORY ${baseDir}/${curdir}/${child})
				  GroupSources(${curdir}/${child} ${baseDir})
		  else()
				string(REPLACE "/" "\\" groupname ${curdir})
			source_group(${groupname} FILES ${baseDir}/${curdir}/${child})
		  endif()
   endforeach()
endmacro()

if(MSVC)
	# disable security warnings in windows
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
	
	# define MS Visual Studio toolset version
	if(NOT DEFINED MSVC_TOOLSET_VERSION)
		if(MSVC_VERSION GREATER_EQUAL 1920)
		  set(MSVC_TOOLSET_VERSION "") # leave unknown for future versions
		elseif(MSVC_VERSION GREATER_EQUAL 1910)
		  # VS 2017
		  set(MSVC_TOOLSET_VERSION 141)
		elseif(MSVC_VERSION EQUAL 1900)
		  # VS 2015
		  set(MSVC_TOOLSET_VERSION 14)
		else()
		 # we do not support older version
		endif()
	endif()
endif()

set(CMAKE_CXX_STANDARD 11)

set_absolute_path(UTIL_DIR "../util")
set_absolute_path(SYS_LIBS_DIR "../sys-libs")
set_absolute_path(BOOST_DIR "../boost")
set_absolute_path(CAPNP_SCHEMAS_DIR "../../GitHub/capnproto_schemas/cpp-out")
set_absolute_path(CAPNP_DIR "../../GitHub/capnproto")

# setup the target group variable, used to group targets into folders in IDEs
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
	
include_directories("src")
include_directories("${UTIL_DIR}")
include_directories("${SYS_LIBS_DIR}/includes/zmq-helper")
include_directories("${CAPNP_SCHEMAS_DIR}")
include_directories("${CAPNP_DIR}/c++/src")
#include_directories("${SYS_LIBS_DIR}/includes/zeromq-dev-master")
#include_directories("${SYS_LIBS_DIR}/includes/czmq-master")
#include_directories("${SYS_LIBS_DIR}/includes/libsodium-1.0.10")


message(STATUS "cmake_generator = ${CMAKE_GENERATOR}")
message(STATUS "size_of_void =  ${CMAKE_SIZEOF_VOID_P}")
if(${CMAKE_SIZEOF_VOID_P} STREQUAL "4")
    set(ARCH "x86")
elseif(${CMAKE_SIZEOF_VOID_P} STREQUAL "8")
    set(ARCH "x64")
endif()

if(MSVC)
	find_package(ZeroMQ CONFIG REQUIRED)
	set(ZMQ_LIB_NAME libzmq-static)
elseif(UNIX)
    find_package(ZeroMQ CONFIG REQUIRED)
	set(ZMQ_LIB_NAME libzmq-static)
   
	set(DL_LIB_NAME dl)
endif()

link_directories($ENV{HOME}/lib)

find_package(Threads REQUIRED)

#define folder structure in vs solution corresponding to real folder structure
GroupSources("src" ${PROJECT_SOURCE_DIR})
GroupSources("util" "${PROJECT_SOURCE_DIR}/..")

# resource
file(GLOB LIBMONICA_RESOURCE_SOURCE
	   src/resource/*)

# core
file(GLOB LIBMONICA_CORE_SOURCE
     src/core/*.cpp
	 src/core/*.h)
remove_file(LIBMONICA_CORE_SOURCE "src/core/simulation.cpp")
remove_file(LIBMONICA_CORE_SOURCE "src/core/simulation.h")
remove_file(LIBMONICA_CORE_SOURCE "src/core/monica-eom.cpp")
remove_file(LIBMONICA_CORE_SOURCE "src/core/monica-eom.h")	   

# util db
file(GLOB_RECURSE LIBMONICA_UTIL_DB_SOURCE
     ${UTIL_DIR}/db/*.cpp
	 ${UTIL_DIR}/db/*.h 
	 ${UTIL_DIR}/db/*.c )   
remove_file(LIBMONICA_UTIL_DB_SOURCE "${UTIL_DIR}/db/orm--typedefs.h")
remove_file(LIBMONICA_UTIL_DB_SOURCE "${UTIL_DIR}/db/orm--.cpp")
remove_file(LIBMONICA_UTIL_DB_SOURCE "${UTIL_DIR}/db/orm--.h")

# util climate
file(GLOB_RECURSE LIBMONICA_UTIL_CLIMATE_SOURCE
     ${UTIL_DIR}/climate/climate-common.cpp
	 ${UTIL_DIR}/climate/climate-common.h )   

file(GLOB_RECURSE LIBMONICA_UTIL_JSON11_SOURCE
	 ${UTIL_DIR}/json11/*.cpp
	 ${UTIL_DIR}/json11/*.hpp 
	 ${UTIL_DIR}/json11/*.h )   

file(GLOB_RECURSE LIBMONICA_UTIL_TOOL_SOURCE
	 ${UTIL_DIR}/tools/algorithms.h
	 ${UTIL_DIR}/tools/algorithms.cpp

	 ${UTIL_DIR}/tools/datastructures.h

	 ${UTIL_DIR}/tools/date.h
	 ${UTIL_DIR}/tools/date.cpp

	 ${UTIL_DIR}/tools/debug.h
	 ${UTIL_DIR}/tools/debug.cpp

	 ${UTIL_DIR}/tools/helper.h
	 ${UTIL_DIR}/tools/helper.cpp

	 ${UTIL_DIR}/tools/json11-helper.h
	 ${UTIL_DIR}/tools/json11-helper.cpp

	 ${UTIL_DIR}/tools/read-ini.h
	 ${UTIL_DIR}/tools/read-ini.cpp)

file(GLOB_RECURSE LIBMONICA_UTIL_COMMON_SOURCE
	 ${UTIL_DIR}/common/*.h)

file(GLOB_RECURSE LIBMONICA_UTIL_SOIL_SOURCE
	 ${UTIL_DIR}/soil/*.cpp
	 ${UTIL_DIR}/soil/*.h )   
remove_file(LIBMONICA_UTIL_SOIL_SOURCE "${UTIL_DIR}/soil/soil-from-db.h")
remove_file(LIBMONICA_UTIL_SOIL_SOURCE "${UTIL_DIR}/soil/soil-from-db.cpp")

file(GLOB_RECURSE LIBMONICA_RUN_SOURCE
	 src/run/cultivation-method.h
	 src/run/cultivation-method.cpp
	 src/run/run-monica.h
	 src/run/run-monica.cpp)   

file(GLOB_RECURSE LIBMONICA_IO_SOURCE
	 src/io/output.h
	 src/io/output.cpp
	 src/io/build-output.h
	 src/io/build-output.cpp)   

set(LIBMONICA_SOURCE 
    ${LIBMONICA_IO_SOURCE} 
	${LIBMONICA_RUN_SOURCE} 
	${LIBMONICA_CORE_SOURCE} 
	${LIBMONICA_UTIL_DB_SOURCE} 
	${LIBMONICA_UTIL_SOIL_SOURCE} 
	${LIBMONICA_UTIL_TOOL_SOURCE} 
	${LIBMONICA_UTIL_JSON11_SOURCE} 
	${LIBMONICA_UTIL_CLIMATE_SOURCE}
	${LIBMONICA_UTIL_COMMON_SOURCE}
	${LIBMONICA_UTIL_COMMON_SOURCE}
	${LIBMONICA_RESOURCE_SOURCE})
						
#------------------------------------------------------------------------------

# create monica cli starter 
set(MONICA_SOURCE
	src/run/monica-main.cpp)
add_executable(monica ${MONICA_SOURCE})
target_link_libraries(monica
	${CMAKE_THREAD_LIBS_INIT}
)


#------------------------------------------------------------------------------

# create monica-run, the non-zeromq cli client, which runs monica locally
set(MONICA_RUN_SOURCE_FILES
	
	src/io/csv-format.h
	src/io/csv-format.cpp
	
	src/io/database-io.h
	src/io/database-io.cpp
		
	src/run/env-from-json-config.h
	src/run/env-from-json-config.cpp

	src/run/env-json-from-json-config.h
	src/run/env-json-from-json-config.cpp
	
	src/run/monica-run-main.cpp

	# climate library code
	#-------------------------------------------
	${UTIL_DIR}/climate/climate-file-io.h
	${UTIL_DIR}/climate/climate-file-io.cpp

	# soil library code
	#-------------------------------------------
	${UTIL_DIR}/soil/soil-from-db.h
	${UTIL_DIR}/soil/soil-from-db.cpp
)

set(MONICA_RUN_SOURCE ${MONICA_RUN_SOURCE_FILES} ${LIBMONICA_SOURCE})
add_executable(monica-run ${MONICA_RUN_SOURCE})
target_link_libraries(monica-run
	${CMAKE_THREAD_LIBS_INIT}
	${CMAKE_DL_LIBS}
)

#------------------------------------------------------------------------------

# create monica-zmq-control executable for starting/stopping monica-zmq-server nodes
set(MONICA_ZMQ_CONTROL_SOURCE
	
	src/run/monica-zmq-control-main.cpp

	# json11 library code
	#-------------------------------------------
	${UTIL_DIR}/json11/json11.hpp
	${UTIL_DIR}/json11/json11.cpp

	# tools library code
	#-------------------------------------------
	${UTIL_DIR}/tools/date.h
	${UTIL_DIR}/tools/date.cpp

	${UTIL_DIR}/tools/debug.h
	${UTIL_DIR}/tools/debug.cpp
	
	${UTIL_DIR}/tools/helper.h
	${UTIL_DIR}/tools/helper.cpp

	${UTIL_DIR}/tools/json11-helper.h
	${UTIL_DIR}/tools/json11-helper.cpp

	${UTIL_DIR}/tools/zmq-helper.h
	${UTIL_DIR}/tools/zmq-helper.cpp
)
add_executable(monica-zmq-control ${MONICA_ZMQ_CONTROL_SOURCE})
if (MSVC)
target_compile_options(monica-zmq-control PRIVATE "/MT$<$<CONFIG:Debug>:d>")
endif()
target_link_libraries(monica-zmq-control
	${ZMQ_LIB_NAME}
	${CMAKE_THREAD_LIBS_INIT}
)

#------------------------------------------------------------------------------

# create monica-zmq-control-send executable for sending messages to monica-zmq-control
set(MONICA_ZMQ_CONTROL_SEND_SOURCE
	
	src/run/monica-zmq-control-send-main.cpp

	# json11 library code
	#-------------------------------------------
	${UTIL_DIR}/json11/json11.hpp
	${UTIL_DIR}/json11/json11.cpp

	# tools library code
	#-------------------------------------------
	${UTIL_DIR}/tools/date.h
	${UTIL_DIR}/tools/date.cpp

	${UTIL_DIR}/tools/debug.h
	${UTIL_DIR}/tools/debug.cpp
	
	${UTIL_DIR}/tools/helper.h
	${UTIL_DIR}/tools/helper.cpp

	${UTIL_DIR}/tools/json11-helper.h
	${UTIL_DIR}/tools/json11-helper.cpp

	${UTIL_DIR}/tools/zmq-helper.h
	${UTIL_DIR}/tools/zmq-helper.cpp
)
add_executable(monica-zmq-control-send ${MONICA_ZMQ_CONTROL_SEND_SOURCE})
if (MSVC)
	target_compile_options(monica-zmq-control-send PRIVATE "/MT$<$<CONFIG:Debug>:d>")
endif()
target_link_libraries(monica-zmq-control-send
	${ZMQ_LIB_NAME}
	${CMAKE_THREAD_LIBS_INIT}
)


#------------------------------------------------------------------------------

# create monica zmq proxy executable for forwarding jobs to monica-zmq-server 
set(MONICA_ZMQ_PROXY_SOURCE
	
	src/run/monica-zmq-proxy-main.cpp

	# json11 library code
	#-------------------------------------------
	${UTIL_DIR}/json11/json11.hpp
	${UTIL_DIR}/json11/json11.cpp

	# tools library code
	#-------------------------------------------
	${UTIL_DIR}/tools/date.h
	${UTIL_DIR}/tools/date.cpp

	${UTIL_DIR}/tools/debug.h
	${UTIL_DIR}/tools/debug.cpp
	
	${UTIL_DIR}/tools/helper.h
	${UTIL_DIR}/tools/helper.cpp

	${UTIL_DIR}/tools/json11-helper.h
	${UTIL_DIR}/tools/json11-helper.cpp

	${UTIL_DIR}/tools/zmq-helper.h
	${UTIL_DIR}/tools/zmq-helper.cpp
)
add_executable(monica-zmq-proxy ${MONICA_ZMQ_PROXY_SOURCE})
if (MSVC)
target_compile_options(monica-zmq-proxy PRIVATE "/MT$<$<CONFIG:Debug>:d>")
endif()
target_link_libraries(monica-zmq-proxy
	${ZMQ_LIB_NAME}
	${CMAKE_THREAD_LIBS_INIT}
)

#------------------------------------------------------------------------------

# create monica-zmq-run executable, the cli client to monica-zmq-server
set(MONICA_ZMQ_RUN_SOURCE_FILES
	
	src/io/csv-format.h
	src/io/csv-format.cpp
	
	src/io/database-io.h
	src/io/database-io.cpp

	src/run/env-from-json-config.h
	src/run/env-from-json-config.cpp

	src/run/env-json-from-json-config.h
	src/run/env-json-from-json-config.cpp

	src/run/run-monica-zmq.h
	src/run/run-monica-zmq.cpp

	src/run/monica-zmq-run-main.cpp
	
	# climate library code
	#-------------------------------------------
	${UTIL_DIR}/climate/climate-file-io.h
	${UTIL_DIR}/climate/climate-file-io.cpp

	# tools library code
	#-------------------------------------------
	${UTIL_DIR}/tools/zmq-helper.h
	${UTIL_DIR}/tools/zmq-helper.cpp

	# soil library code
	#-------------------------------------------
	${UTIL_DIR}/soil/soil-from-db.h
	${UTIL_DIR}/soil/soil-from-db.cpp
)
set(MONICA_ZMQ_RUN_SOURCE 
    ${MONICA_ZMQ_RUN_SOURCE_FILES} 
	${LIBMONICA_SOURCE})

add_executable(monica-zmq-run ${MONICA_ZMQ_RUN_SOURCE})
if (MSVC)
target_compile_options(monica-zmq-run PRIVATE "/MT$<$<CONFIG:Debug>:d>")
endif()
target_link_libraries(monica-zmq-run
	${ZMQ_LIB_NAME}
	${CMAKE_THREAD_LIBS_INIT}
	${CMAKE_DL_LIBS}
)

#------------------------------------------------------------------------------

# create monica-zmq-server executable, the server running monica via zeromq requests
set(MONICA_ZMQ_SERVER_SOURCE_FILES
	
	src/run/serve-monica-zmq.h
	src/run/serve-monica-zmq.cpp
	src/run/monica-zmq-server-main.cpp
	
	# climate library code
	#-------------------------------------------

	${UTIL_DIR}/climate/climate-file-io.h
	${UTIL_DIR}/climate/climate-file-io.cpp

	# tools library code
	#-------------------------------------------
	${UTIL_DIR}/tools/zmq-helper.h
	${UTIL_DIR}/tools/zmq-helper.cpp
	
	# soil library code
	#-------------------------------------------
	${UTIL_DIR}/soil/constants.h
	${UTIL_DIR}/soil/conversion.h
	${UTIL_DIR}/soil/soil.h
)

set(MONICA_ZMQ_SERVER_SOURCE 
    ${LIBMONICA_SOURCE} 
	${MONICA_ZMQ_SERVER_SOURCE_FILES})
add_executable(monica-zmq-server ${MONICA_ZMQ_SERVER_SOURCE})
target_link_libraries(monica-zmq-server
	${ZMQ_LIB_NAME}
	${CMAKE_THREAD_LIBS_INIT}
	${CMAKE_DL_LIBS}
)
if (MSVC)
target_compile_options(monica-zmq-server PRIVATE "/MT$<$<CONFIG:Debug>:d>")
endif()

#------------------------------------------------------------------------------

#set path and lib manually because for some reasons on the cluster1/2 on debian 9 python3.4m is found instead of 2.7
message(STATUS "PYTHON_EXECUTABLE = ${PYTHON_EXECUTABLE}")

find_package(PythonLibs 2.7 EXACT REQUIRED)
find_package(PythonInterp 2.7 EXACT REQUIRED)
message(STATUS "PYTHON_LIBRARIES = ${PYTHON_LIBRARIES}")
message(STATUS "PYTHON_EXECUTABLE = ${PYTHON_EXECUTABLE}")
include_directories(${PYTHON_INCLUDE_DIR})

if(WIN32)
    link_directories(${SYS_LIBS_DIR}/binaries/windows/vc${MSVC_TOOLSET_VERSION}/${ARCH}/boost-python)
	message("${SYS_LIBS_DIR}/binaries/windows/vc${MSVC_TOOLSET_VERSION}/${ARCH}/boost-python")
	include_directories(${BOOST_DIR})
elseif(UNIX)
	find_package(Boost COMPONENTS python REQUIRED)
	include_directories(${Boost_INCLUDE_DIR})
endif()

# create monica python wrapper lib
set(MONICA_PYTHON_SOURCE_FILES

	src/io/csv-format.h
	src/io/csv-format.cpp
	
	src/io/database-io.h
	src/io/database-io.cpp
		
	src/run/env-from-json-config.h
	src/run/env-from-json-config.cpp

	src/run/env-json-from-json-config.h
	src/run/env-json-from-json-config.cpp
	
	src/python/monica_py.cpp
	
	# climate library code
	#-------------------------------------------
	${UTIL_DIR}/climate/climate-file-io.h
	${UTIL_DIR}/climate/climate-file-io.cpp
)
set(MONICA_PYTHON_SOURCE 
    ${MONICA_PYTHON_SOURCE_FILES} 
	${LIBMONICA_SOURCE})

add_library(monica_python SHARED ${MONICA_PYTHON_SOURCE})
target_link_libraries(monica_python 
	${Boost_LIBRARIES} 
	${PYTHON_LIBRARIES}
)
if (MSVC)
	set_target_properties(monica_python PROPERTIES SUFFIX .pyd)
endif()
set_target_properties(monica_python PROPERTIES PREFIX "")

#------------------------------------------------------------------------------

# create monica-zmq-run executable, the cli client to monica-zmq-server
set(CLIMATE_IO_LIB_FILES
	
	# climate library code
	#-------------------------------------------
	${UTIL_DIR}/climate/climate-file-io.h
	${UTIL_DIR}/climate/climate-file-io.cpp

	${UTIL_DIR}/tools/algorithms.h
	${UTIL_DIR}/tools/algorithms.cpp

	${UTIL_DIR}/tools/date.h
	${UTIL_DIR}/tools/date.cpp

	${UTIL_DIR}/tools/debug.h
	${UTIL_DIR}/tools/debug.cpp

	${UTIL_DIR}/tools/helper.h
	${UTIL_DIR}/tools/helper.cpp

	${UTIL_DIR}/tools/json11-helper.h
	${UTIL_DIR}/tools/json11-helper.cpp
)
set(CLIMATE_IO_LIB_SOURCE 	
    ${CLIMATE_IO_LIB_FILES} 
	${LIBMONICA_UTIL_CLIMATE_SOURCE}
	${LIBMONICA_UTIL_JSON11_SOURCE}
	${LIBMONICA_UTIL_COMMON_SOURCE})

add_library(libclimateio SHARED ${CLIMATE_IO_LIB_SOURCE})
if (MSVC)
target_compile_options(libclimateio PRIVATE "/MT$<$<CONFIG:Debug>:d>")
endif()
target_link_libraries(libclimateio
	                  ${ZMQ_LIB_NAME}
	                  ${CMAKE_THREAD_LIBS_INIT}
	                  ${CMAKE_DL_LIBS}
)
set_target_properties(libclimateio PROPERTIES PREFIX "")
set_target_properties(libclimateio PROPERTIES COMPILE_DEFINITIONS "USE_DLL;DLL_EXPORTS")

#------------------------------------------------------------------------------

# create monica-capnp-server executable, the server running monica via CapNProto RPC
set(MONICA_CAPNP_SERVER_SOURCE_FILES
	src/run/monica-capnp-server-main.cpp
	
	# capnproto schema files
    #--------------------
    ${CAPNP_SCHEMAS_DIR}/climate_data.capnp.h
    ${CAPNP_SCHEMAS_DIR}/climate_data.capnp.c++
    ${CAPNP_SCHEMAS_DIR}/model.capnp.h
    ${CAPNP_SCHEMAS_DIR}/model.capnp.c++
    ${CAPNP_SCHEMAS_DIR}/geo_coord.capnp.h
    ${CAPNP_SCHEMAS_DIR}/geo_coord.capnp.c++
    ${CAPNP_SCHEMAS_DIR}/common.capnp.h
    ${CAPNP_SCHEMAS_DIR}/common.capnp.c++
    ${CAPNP_SCHEMAS_DIR}/service.capnp.h
    ${CAPNP_SCHEMAS_DIR}/service.capnp.c++

	# climate library code
	#-------------------------------------------

	${UTIL_DIR}/climate/climate-file-io.h
	${UTIL_DIR}/climate/climate-file-io.cpp

	# climate library code
	#-------------------------------------------
	#${UTIL_DIR}/climate/capnp-data-accessor.h
	#${UTIL_DIR}/climate/capnp-data-accessor.cpp
)

set(MONICA_CAPNP_SERVER_SOURCE 
    ${LIBMONICA_SOURCE} 
    ${MONICA_CAPNP_SERVER_SOURCE_FILES})

# create monica-capnp-server
add_executable(monica-capnp-server ${MONICA_CAPNP_SERVER_SOURCE})
target_link_libraries(monica-capnp-server
${CMAKE_THREAD_LIBS_INIT}
${CMAKE_DL_LIBS}
"Ws2_32.lib"
"${SYS_LIBS_DIR}/binaries/windows/vc141/x64/capnproto/static$<$<CONFIG:Debug>:-debug>/kj.lib"
"${SYS_LIBS_DIR}/binaries/windows/vc141/x64/capnproto/static$<$<CONFIG:Debug>:-debug>/kj-async.lib"
"${SYS_LIBS_DIR}/binaries/windows/vc141/x64/capnproto/static$<$<CONFIG:Debug>:-debug>/kj-http.lib"
"${SYS_LIBS_DIR}/binaries/windows/vc141/x64/capnproto/static$<$<CONFIG:Debug>:-debug>/kj-gzip.lib"
"${SYS_LIBS_DIR}/binaries/windows/vc141/x64/capnproto/static$<$<CONFIG:Debug>:-debug>/capnp.lib"
"${SYS_LIBS_DIR}/binaries/windows/vc141/x64/capnproto/static$<$<CONFIG:Debug>:-debug>/capnpc.lib"
"${SYS_LIBS_DIR}/binaries/windows/vc141/x64/capnproto/static$<$<CONFIG:Debug>:-debug>/capnp-json.lib"
"${SYS_LIBS_DIR}/binaries/windows/vc141/x64/capnproto/static$<$<CONFIG:Debug>:-debug>/capnp-rpc.lib")

if (MSVC)
#target_compile_options(monica-capnp-server PRIVATE "/MT$<$<CONFIG:Debug>:d>")
endif()
