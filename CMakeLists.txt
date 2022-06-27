cmake_minimum_required(VERSION 3.12)
project(monica)

add_compile_definitions(CAPNPROTO_SERIALIZATION_SUPPORT)
add_compile_definitions(NO_MYSQL)
set(MT_RUNTIME_LIB 1)

#set absolute filenames (to resolve .. in paths)
macro(set_absolute_path var_name path)
	get_filename_component(toAbsPath ${path} ABSOLUTE)
	set(${var_name} ${toAbsPath})
endmacro(set_absolute_path)

set_absolute_path(MAS_INFRASTRUCTURE_DIR "../mas-infrastructure")
set_absolute_path(CAPNP_SCHEMAS_DIR "${MAS_INFRASTRUCTURE_DIR}/capnproto_schemas")

if(NOT TARGET capnp_schemas_lib)
message(STATUS "target: capnp_schemas_lib")
	add_subdirectory(../mas-infrastructure/capnproto_schemas mas-infrastructure/capnproto_schemas)
endif()

add_subdirectory(../util/tools/date util/date)
add_subdirectory(../util/tools/helpers util/helpers)
add_subdirectory(../util/tools/read-ini util/read-ini)
add_subdirectory(../util/tools/debug util/debug)
add_subdirectory(../util/climate/climate-file-io util/climate-file-io)
add_subdirectory(../util/soil util/soil)
#add_subdirectory(../util/db/db util/db)
add_subdirectory(../util/zeromq util/zeromq)

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
		if(MSVC_VERSION GREATER_EQUAL 1930)
		  set(MSVC_TOOLSET_VERSION "") # leave unknown for future versions
		elseif(MSVC_VERSION GREATER_EQUAL 1920)
		  set(MSVC_TOOLSET_VERSION 142) # VS 2019
		elseif(MSVC_VERSION GREATER_EQUAL 1910)
		  set(MSVC_TOOLSET_VERSION 141) # VS 2017
		elseif(MSVC_VERSION EQUAL 1900)
		  set(MSVC_TOOLSET_VERSION 14) # VS 2015
		else()
		 # we do not support older version
		endif()
	endif()
endif()

# https://cmake.org/cmake/help/latest/prop_tgt/CXX_STANDARD.html
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#set(CMAKE_CXX_STANDARD 14)

# setup the target group variable, used to group targets into folders in IDEs
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include_directories("src")

message(STATUS "cmake_generator = ${CMAKE_GENERATOR}")
message(STATUS "size_of_void =  ${CMAKE_SIZEOF_VOID_P}")
if(${CMAKE_SIZEOF_VOID_P} STREQUAL "4")
    set(ARCH "x86")
elseif(${CMAKE_SIZEOF_VOID_P} STREQUAL "8")
    set(ARCH "x64")
endif()

link_directories($ENV{HOME}/lib)



find_package(Threads REQUIRED)

#define folder structure in vs solution corresponding to real folder structure
GroupSources("src" ${PROJECT_SOURCE_DIR})
GroupSources("util" "${PROJECT_SOURCE_DIR}/..")

#------------------------------------------------------------------------------

# create monica run static lib to compile code just once
add_library(monica_lib 
	src/core/crop.h
	src/core/crop.cpp
	src/core/crop-module.h
	src/core/crop-module.cpp
	src/core/monica-model.h
	src/core/monica-model.cpp
	src/core/monica-parameters.h
	src/core/monica-parameters.cpp
	src/core/O3-impact.h
	src/core/O3-impact.cpp
	src/core/photosynthesis-FvCB.h
	src/core/photosynthesis-FvCB.cpp
	src/core/soilcolumn.h
	src/core/soilcolumn.cpp
	src/core/soilmoisture.h
	src/core/soilmoisture.cpp
	src/core/frost-component.h
	src/core/frost-component.cpp
	src/core/snow-component.h
	src/core/snow-component.cpp
	src/core/soilorganic.h
	src/core/soilorganic.cpp
	src/core/soiltemperature.h
	src/core/soiltemperature.cpp
	src/core/soiltransport.h
	src/core/soiltransport.cpp
	src/core/voc-common.h
	src/core/voc-guenther.h
	src/core/voc-guenther.cpp
	src/core/voc-jjv.h
	src/core/voc-jjv.cpp
	src/core/stics-nit-denit-n2o.h
	src/core/stics-nit-denit-n2o.cpp

	src/io/output.h
	src/io/output.cpp
	src/io/build-output.h
	src/io/build-output.cpp

	src/run/cultivation-method.h
	src/run/cultivation-method.cpp
	src/run/run-monica.h
	src/run/run-monica.cpp

	src/resource/version.h
	src/resource/version_resource.rc
)
target_include_directories(monica_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR}/mas-infrastructure/capnproto_schemas)
if (MSVC)
	target_compile_options(monica_lib PRIVATE "/MT$<$<CONFIG:Debug>:d>")
endif()
target_link_libraries(monica_lib
	${CMAKE_THREAD_LIBS_INIT}
	${CMAKE_DL_LIBS}
	read_ini_lib
	debug_lib
	helpers_lib
	date_lib
	#db_lib
	soil_lib
	json11_lib
	#sqlite3_lib
	climate_common_lib
	climate_file_io_lib
	capnp_schemas_lib
)

#------------------------------------------------------------------------------

# create monica run static lib to compile code just once
set(MONICA_RUN_SOURCE_FILES
	src/io/csv-format.h
	src/io/csv-format.cpp
		
	src/run/env-from-json-config.h
	src/run/env-from-json-config.cpp

	src/run/env-json-from-json-config.h
	src/run/env-json-from-json-config.cpp
)
add_library(monica_run_lib ${MONICA_RUN_SOURCE_FILES})
#target_include_directories(monica_run_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
if (MSVC)
	target_compile_options(monica_run_lib PRIVATE "/MT$<$<CONFIG:Debug>:d>")
endif()
target_link_libraries(monica_run_lib
	${CMAKE_THREAD_LIBS_INIT}
	${CMAKE_DL_LIBS}
	monica_lib
)

#------------------------------------------------------------------------------

# create monica-run, the non-zeromq cli client, which runs monica locally
add_executable(monica-run src/run/monica-run-main.cpp)
if (MSVC)
	target_compile_options(monica-run PRIVATE "/MT$<$<CONFIG:Debug>:d>")
endif()
target_link_libraries(monica-run
	${CMAKE_THREAD_LIBS_INIT}
	${CMAKE_DL_LIBS}
	monica_run_lib
)

#------------------------------------------------------------------------------

# create monica zmq proxy executable for forwarding jobs to monica-zmq-server 
add_executable(monica-zmq-proxy src/run/monica-zmq-proxy-main.cpp)
if (MSVC)
	target_compile_options(monica-zmq-proxy PRIVATE "/MT$<$<CONFIG:Debug>:d>")
endif()
target_link_libraries(monica-zmq-proxy
	${CMAKE_THREAD_LIBS_INIT}
	debug_lib
	zmq_lib
)

#------------------------------------------------------------------------------

# create monica-zmq-server executable, the server running monica via zeromq requests
set(MONICA_ZMQ_SERVER_SOURCE
	src/run/serve-monica-zmq.h
	src/run/serve-monica-zmq.cpp
	src/run/monica-zmq-server-main.cpp
)
add_executable(monica-zmq-server ${MONICA_ZMQ_SERVER_SOURCE})
if (MSVC)
	target_compile_options(monica-zmq-server PRIVATE "/MT$<$<CONFIG:Debug>:d>")
endif()
target_link_libraries(monica-zmq-server
	${CMAKE_THREAD_LIBS_INIT}
	${CMAKE_DL_LIBS}
	monica_lib
	zmq_lib
)

#------------------------------------------------------------------------------

# create monica-capnp-server executable, the server running monica via CapNProto RPC
set(MONICA_CAPNP_SERVER_SOURCE_FILES
	src/run/monica-capnp-server-main.cpp
	src/run/run-monica-capnp.h
	src/run/run-monica-capnp.cpp
	${MAS_INFRASTRUCTURE_DIR}/src/cpp/common/rpc-connections.h
	${MAS_INFRASTRUCTURE_DIR}/src/cpp/common/rpc-connections.cpp
	${MAS_INFRASTRUCTURE_DIR}/src/cpp/common/common.h
	${MAS_INFRASTRUCTURE_DIR}/src/cpp/common/common.cpp
)
add_executable(monica-capnp-server ${MONICA_CAPNP_SERVER_SOURCE_FILES})
target_link_libraries(monica-capnp-server
	${CMAKE_THREAD_LIBS_INIT}
	${CMAKE_DL_LIBS}
	monica_lib
)
target_include_directories(monica-capnp-server PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/mas-infrastructure/capnproto_schemas)
if (MSVC)
	target_compile_options(monica-capnp-server PRIVATE "/MT$<$<CONFIG:Debug>:d>")
endif()

#------------------------------------------------------------------------------

# create monica-capnp-proxy executable, a server forwarding monica CapNProto RPC requests to registered monica instances
set(MONICA_CAPNP_PROXY_SOURCE_FILES
	src/run/monica-capnp-proxy-main.cpp
	src/run/run-monica-capnp.h
	src/run/run-monica-capnp.cpp
	${MAS_INFRASTRUCTURE_DIR}/src/cpp/common/rpc-connections.h
	${MAS_INFRASTRUCTURE_DIR}/src/cpp/common/rpc-connections.cpp
	${MAS_INFRASTRUCTURE_DIR}/src/cpp/common/common.h
	${MAS_INFRASTRUCTURE_DIR}/src/cpp/common/common.cpp
)
add_executable(monica-capnp-proxy ${MONICA_CAPNP_PROXY_SOURCE_FILES})
target_link_libraries(monica-capnp-proxy
	${CMAKE_THREAD_LIBS_INIT}
	${CMAKE_DL_LIBS}
	monica_lib
)
target_include_directories(monica-capnp-proxy PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/mas-infrastructure/capnproto_schemas)
if (MSVC)
	target_compile_options(monica-capnp-proxy PRIVATE "/MT$<$<CONFIG:Debug>:d>")
endif()
