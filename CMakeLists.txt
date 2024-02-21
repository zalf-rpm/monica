cmake_minimum_required(VERSION 3.12)
project(Monica)

message(STATUS "-> Monica")

#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")

add_compile_definitions(CAPNPROTO_SERIALIZATION_SUPPORT)
add_compile_definitions(AMEI)
set(CAPNPROTO_SERIALIZATION_SUPPORT 1)
#add_compile_definitions(INCLUDE_SR_SUPPORT)
#set(INCLUDE_SR_SUPPORT 1)
add_compile_definitions(NO_MYSQL)
set(MT_RUNTIME_LIB 1)

#set absolute filenames (to resolve .. in paths)
macro(set_absolute_path var_name path)
    get_filename_component(toAbsPath ${path} ABSOLUTE)
    set(${var_name} ${toAbsPath})
endmacro(set_absolute_path)

set_absolute_path(MAS_INFRASTRUCTURE_DIR ../mas-infrastructure)
set_absolute_path(CAPNP_SCHEMAS_DIR ${MAS_INFRASTRUCTURE_DIR}/capnproto_schemas)
set_absolute_path(AMEI_COMPONENTS_DIR ../SoilTemperatureModels/Models)

if (NOT TARGET Monica_SoilTemp_lib)
    message(STATUS "target: Monica_SoilTemp_lib")
    add_subdirectory(${AMEI_COMPONENTS_DIR}/Monica_SoilTemp/src/cpp/Monica_SoilTemp amei-components/Monica_SoilTemp)
endif ()

if (NOT TARGET DSSAT_ST_standalone_lib)
    message(STATUS "target: DSSAT_ST_standalone_lib")
    add_subdirectory(${AMEI_COMPONENTS_DIR}/DSSAT_ST_standalone/src/cpp/DSSAT_ST_standalone amei-components/DSSAT_ST_standalone)
endif ()

if (NOT TARGET DSSAT_EPICST_standalone_lib)
    message(STATUS "target: DSSAT_EPICST_standalone_lib")
    add_subdirectory(${AMEI_COMPONENTS_DIR}/DSSAT_EPICST_standalone/src/cpp/DSSAT_EPICST_standalone amei-components/DSSAT_EPICST_standalone)
endif ()

if (NOT TARGET Simplace_Soil_Temperature_lib)
    message(STATUS "target: Simplace_Soil_Temperature_lib")
    add_subdirectory(${AMEI_COMPONENTS_DIR}/Simplace_Soil_Temperature/src/cpp/Simplace_Soil_Temperature amei-components/Simplace_Soil_Temperature)
endif ()

if (NOT TARGET capnp_schemas_lib)
    message(STATUS "target: capnp_schemas_lib")
    add_subdirectory(${CAPNP_SCHEMAS_DIR} mas-infrastructure/capnproto_schemas)
endif ()

if (NOT TARGET common_lib)
    message(STATUS "target: common_lib")
    add_subdirectory(${MAS_INFRASTRUCTURE_DIR}/src/cpp/common mas-infrastructure/common)
endif ()

if (NOT TARGET date_lib)
    message(STATUS "target: date_lib")
    add_subdirectory(${MAS_INFRASTRUCTURE_DIR}/src/cpp/tools/date mas-infrastructure/tools/date)
endif ()

if (NOT TARGET helpers_lib)
    message(STATUS "target: helpers_lib")
    add_subdirectory(${MAS_INFRASTRUCTURE_DIR}/src/cpp/tools/helpers mas-infrastructure/tools/helpers)
endif ()

if (NOT TARGET json11_lib)
    message(STATUS "target: json11_lib")
    add_subdirectory(${MAS_INFRASTRUCTURE_DIR}/src/cpp/json11 mas-infrastructure/json11)
endif ()

#if(NOT TARGET read_ini_lib)
#	message(STATUS "target: read_ini_lib")
#	add_subdirectory(${MAS_INFRASTRUCTURE_DIR}/src/cpp/tools/read-ini mas-infrastructure/tools/read-ini)
#endif()
#get_target_property(read_ini_interface_includes read_ini_lib INTERFACE_INCLUDE_DIRECTORIES)

if (NOT TARGET debug_lib)
    message(STATUS "target: debug_lib")
    add_subdirectory(${MAS_INFRASTRUCTURE_DIR}/src/cpp/tools/debug mas-infrastructure/tools/debug)
endif ()

if (NOT TARGET climate_file_io_lib)
    message(STATUS "target: climate_file_io_lib")
    add_subdirectory(${MAS_INFRASTRUCTURE_DIR}/src/cpp/climate/climate-file-io mas-infrastructure/climate/climate-file-io)
endif ()

if (NOT TARGET soil_lib)
    message(STATUS "target: soil_lib")
    add_subdirectory(${MAS_INFRASTRUCTURE_DIR}/src/cpp/soil mas-infrastructure/soil)
endif ()

if (NOT TARGET zeromq_lib)
    message(STATUS "target: zeromq_lib")
    add_subdirectory(${MAS_INFRASTRUCTURE_DIR}/src/cpp/zeromq mas-infrastructure/zeromq)
endif ()

#define delete file from path
macro(remove_file list_name file_name)
    get_filename_component(toAbsPath ${file_name} ABSOLUTE)
    list(REMOVE_ITEM ${list_name} "${toAbsPath}")
endmacro(remove_file)

#define source_groups 
macro(GroupSources curdir baseDir)
    file(GLOB children RELATIVE ${baseDir}/${curdir} ${baseDir}/${curdir}/*)
    foreach (child ${children})
        if (IS_DIRECTORY ${baseDir}/${curdir}/${child})
            GroupSources(${curdir}/${child} ${baseDir})
        else ()
            string(REPLACE "/" "\\" groupname ${curdir})
            source_group(${groupname} FILES ${baseDir}/${curdir}/${child})
        endif ()
    endforeach ()
endmacro()

if (MSVC)
    # disable security warnings in windows
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)

    # define MS Visual Studio toolset version
    if (NOT DEFINED MSVC_TOOLSET_VERSION)
        if (MSVC_VERSION GREATER_EQUAL 1930)
            set(MSVC_TOOLSET_VERSION "") # leave unknown for future versions
        elseif (MSVC_VERSION GREATER_EQUAL 1920)
            set(MSVC_TOOLSET_VERSION 142) # VS 2019
        elseif (MSVC_VERSION GREATER_EQUAL 1910)
            set(MSVC_TOOLSET_VERSION 141) # VS 2017
        elseif (MSVC_VERSION EQUAL 1900)
            set(MSVC_TOOLSET_VERSION 14) # VS 2015
        else ()
            # we do not support older version
        endif ()
    endif ()
endif ()

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
if (${CMAKE_SIZEOF_VOID_P} STREQUAL "4")
    set(ARCH "x86")
elseif (${CMAKE_SIZEOF_VOID_P} STREQUAL "8")
    set(ARCH "x64")
endif ()

link_directories($ENV{HOME}/lib)

find_package(Threads REQUIRED)
#find_package(unofficial-sodium CONFIG REQUIRED)

#define folder structure in vs solution corresponding to real folder structure
GroupSources("src" ${PROJECT_SOURCE_DIR})
GroupSources("util" ${PROJECT_SOURCE_DIR}/..)

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
        src/run/capnp-helper.h src/run/capnp-helper.cpp)
target_link_libraries(monica_lib
        PUBLIC
        ${CMAKE_THREAD_LIBS_INIT}
        ${CMAKE_DL_LIBS}
        capnp_schemas_lib
        debug_lib
        helpers_lib
        date_lib
        soil_lib
        json11_lib
        common_lib
        climate_file_io_lib
        Monica_SoilTemp_lib
        DSSAT_ST_standalone_lib
        DSSAT_EPICST_standalone_lib
        Simplace_Soil_Temperature_lib
        )

#message(STATUS "json11_lib_interface_includes:")
#foreach(X ${json11_lib_interface_includes})
#    message(STATUS ${X})
#endforeach()

target_include_directories(monica_lib
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        )
#message(STATUS "monica_lib_interface_includes:")
#get_target_property(monica_lib_interface_includes monica_lib INTERFACE_INCLUDE_DIRECTORIES)
#foreach(X ${monica_lib_interface_includes})
#    message(STATUS ${X})
#endforeach()

#message(STATUS "monica_lib_includes:")
#get_target_property(monica_lib_includes monica_lib INCLUDE_DIRECTORIES)
#foreach(X ${monica_lib_includes})
#    message(STATUS ${X})
#endforeach()

if (MSVC)
    target_compile_options(monica_lib PRIVATE "/MT$<$<CONFIG:Debug>:d>")
endif ()

#------------------------------------------------------------------------------

# create monica run static lib to compile code just once

add_library(monica_run_lib
        src/io/csv-format.h
        src/io/csv-format.cpp

        src/run/create-env-from-json-config.h
        src/run/create-env-from-json-config.cpp
        )

target_link_libraries(monica_run_lib monica_lib)

#target_include_directories(monica_run_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
if (MSVC)
    target_compile_options(monica_run_lib PRIVATE "/MT$<$<CONFIG:Debug>:d>")
endif ()

#------------------------------------------------------------------------------

# create monica-run, the non-zeromq cli client, which runs monica locally
add_executable(monica-run src/run/monica-run-main.cpp)

target_link_libraries(monica-run monica_run_lib)

if (MSVC)
    target_compile_options(monica-run PRIVATE "/MT$<$<CONFIG:Debug>:d>")
endif ()

#------------------------------------------------------------------------------

# create monica zmq proxy executable for forwarding jobs to monica-zmq-server 
add_executable(monica-zmq-proxy src/run/monica-zmq-proxy-main.cpp)

target_link_libraries(monica-zmq-proxy
        debug_lib
        zeromq_lib
        )

if (MSVC)
    target_compile_options(monica-zmq-proxy PRIVATE "/MT$<$<CONFIG:Debug>:d>")
endif ()

#------------------------------------------------------------------------------

# create monica-zmq-server executable, the server running monica via zeromq requests
add_executable(monica-zmq-server
        src/run/serve-monica-zmq.h
        src/run/serve-monica-zmq.cpp
        src/run/monica-zmq-server-main.cpp
        )

target_link_libraries(monica-zmq-server
        monica_lib
        zeromq_lib
        )

if (MSVC)
    target_compile_options(monica-zmq-server PRIVATE "/MT$<$<CONFIG:Debug>:d>")
endif ()

#------------------------------------------------------------------------------

# create monica-capnp-server executable, the server running monica via CapNProto RPC
add_executable(monica-capnp-server
        src/run/monica-capnp-server-main.cpp
        src/run/run-monica-capnp.h
        src/run/run-monica-capnp.cpp
        )

target_link_libraries(monica-capnp-server monica_lib)

if (MSVC)
    target_compile_options(monica-capnp-server PRIVATE "/MT$<$<CONFIG:Debug>:d>")
endif ()

#------------------------------------------------------------------------------

# create monica-capnp-proxy executable, a server forwarding monica CapNProto RPC requests to registered monica instances
add_executable(monica-capnp-proxy
        src/run/monica-capnp-proxy-main.cpp
        src/run/run-monica-capnp.h
        src/run/run-monica-capnp.cpp
        )

target_link_libraries(monica-capnp-proxy monica_lib)

if (MSVC)
    target_compile_options(monica-capnp-proxy PRIVATE "/MT$<$<CONFIG:Debug>:d>")
endif ()

#------------------------------------------------------------------------------

# create monica-capnp-server executable, the server running monica via CapNProto RPC
add_executable(monica-capnp-fbp-component
        src/run/monica-capnp-fbp-component-main.cpp
        src/run/run-monica-capnp.h
        src/run/run-monica-capnp.cpp
        )

target_link_libraries(monica-capnp-fbp-component monica_lib)

if (MSVC)
    target_compile_options(monica-capnp-fbp-component PRIVATE "/MT$<$<CONFIG:Debug>:d>")
endif ()

#------------------------------------------------------------------------------

message(STATUS "<- Monica")