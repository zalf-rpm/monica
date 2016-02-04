cmake_minimum_required(VERSION 3.3)
project(monica)

add_definitions(-DNO_ZMQ
                -DNO_MYSQL)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(UTIL_DIR "../util")
set(SYS_LIBS_DIR "../sys-libs")
set(CARBIOCIAL_DIR "../carbiocial-project")

include_directories("src")
include_directories(${UTIL_DIR})
include_directories("${SYS_LIBS_DIR}/includes/zeromq-3.2.5")
include_directories(${CARBIOCIAL_DIR/monica/src})

set(MONICA_SOURCE

    src/core/crop-growth.cpp
    src/core/crop-growth.h

    src/core/crop.cpp
    src/core/crop.h

    src/core/monica-eom.cpp
    src/core/monica-eom.h

    src/core/monica-parameters.cpp
    src/core/monica-parameters.h

    src/core/monica-typedefs.h

    src/core/monica.cpp
    src/core/monica.h

    src/core/simulation.cpp
    src/core/simulation.h

    src/core/soilcolumn.cpp
    src/core/soilcolumn.h

    src/core/soilmoisture.cpp
    src/core/soilmoisture.h

    src/core/soilorganic.cpp
    src/core/soilorganic.h

    src/core/soiltemperature.cpp
    src/core/soiltemperature.h

    src/core/soiltransport.cpp
    src/core/soiltransport.h

    src/io/database-io.cpp
    src/io/database-io.h

    src/io/file-io.cpp
    src/io/file-io.h

    src/io/hermes-file-io.cpp
    src/io/hermes-file-io.h

    src/run/cultivation-method.cpp
    src/run/cultivation-method.h

    #src/run/monica-hermes-main.cpp
    #src/run/monica-landcare-dss-main.cpp
    #src/run/monica-main.cpp
    #src/run/monica-zmq-main.cpp

    src/run/run-monica-zmq.cpp
    src/run/run-monica-zmq.h

    src/run/run-monica.cpp
    src/run/run-monica.h

    src/run/env-from-json.h
    src/run/env-from-json.cpp

    # db library code
    #-------------------------------------------
    ${UTIL_DIR}/db/db.h
    ${UTIL_DIR}/db/db.cpp

    ${UTIL_DIR}/db/abstract-db-connections.h
    ${UTIL_DIR}/db/abstract-db-connections.cpp

    ${UTIL_DIR}/db/sqlite3.h
    ${UTIL_DIR}/db/sqlite3.c

    #${UTIL_DIR}/db/orm--.h
    #${UTIL_DIR}/db/orm--.cpp

    # climate library code
    #-------------------------------------------
    ${UTIL_DIR}/climate/climate-common.h
    ${UTIL_DIR}/climate/climate-common.cpp
    ${UTIL_DIR}/climate/climate-file-io.h
    ${UTIL_DIR}/climate/climate-file-io.cpp

    # json11 library code
    #-------------------------------------------
    ${UTIL_DIR}/json11/json11.hpp
    ${UTIL_DIR}/json11/json11.cpp

    # tools library code
    #-------------------------------------------
    ${UTIL_DIR}/tools/algorithms.h
    ${UTIL_DIR}/tools/algorithms.cpp

    ${UTIL_DIR}/tools/date.h
    ${UTIL_DIR}/tools/date.cpp

    ${UTIL_DIR}/tools/read-ini.h
    ${UTIL_DIR}/tools/read-ini.cpp

    ${UTIL_DIR}/tools/datastructures.h

    ${UTIL_DIR}/tools/helper.h
    ${UTIL_DIR}/tools/helper.cpp

    ${UTIL_DIR}/tools/json11-helper.h
    ${UTIL_DIR}/tools/json11-helper.cpp

    ${UTIL_DIR}/tools/zmq-helper.h
    ${UTIL_DIR}/tools/zmq-helper.cpp

    ${UTIL_DIR}/tools/debug.h
    ${UTIL_DIR}/tools/debug.cpp

    #${UTIL_DIR}/tools/coord-trans.h
    #${UTIL_DIR}/tools/coord-trans.cpp

    # soil library code
    #-------------------------------------------
    ${UTIL_DIR}/soil/constants.h
    ${UTIL_DIR}/soil/constants.cpp

    ${UTIL_DIR}/soil/conversion.h
    ${UTIL_DIR}/soil/conversion.cpp

    ${UTIL_DIR}/soil/soil.h
    ${UTIL_DIR}/soil/soil.cpp

    ${UTIL_DIR}/soil/soil-from-db.h
    ${UTIL_DIR}/soil/soil-from-db.cpp


    ${CARBIOCIAL_DIR}/monica/src/carbiocial.h
    ${CARBIOCIAL_DIR}/monica/src/carbiocial.cpp
    )


find_package(Threads REQUIRED)

#add_executable(monica ${SOURCE_FILES})
#set_target_properties(monica PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY})

add_library(monica SHARED ${MONICA_SOURCE})

target_link_libraries(monica
                      ${CMAKE_THREAD_LIBS_INIT}
                      dl)

set_target_properties(monica PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY})


#boost.python wrapper

set(CARBIOCIAL_PYTHON_WRAPPER_SOURCE
    #src/python/monica_py.cpp
    ../carbiocial-project/monica/src/carbiocial_py.cpp)

find_package(PythonLibs)
include_directories(${PYTHON_INCLUDE_DIRS})

find_package(Boost COMPONENTS python REQUIRED)
include_directories(${Boost_INCLUDE_DIR})


# Define the wrapper library that wraps our library
add_library(carbiocial_py SHARED ${CARBIOCIAL_PYTHON_WRAPPER_SOURCE})
target_link_libraries(carbiocial_py ${Boost_LIBRARIES} monica)

# don't prepend wrapper library name with lib
set_target_properties(carbiocial_py PROPERTIES PREFIX "")

set_target_properties(carbiocial_py PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY})


set(MONICA_PYTHON_WRAPPER_SOURCE
    src/python/monica_py.cpp)

# Define the wrapper library that wraps our library
add_library(monica_py SHARED ${MONICA_PYTHON_WRAPPER_SOURCE})
target_link_libraries(monica_py ${Boost_LIBRARIES} monica)

# don't prepend wrapper library name with lib
set_target_properties(monica_py PROPERTIES PREFIX "")

set_target_properties(monica_py PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY})


#-------------------------------------------------------------------------------

#swig wrapper !!!! doesn't work

#find_package(SWIG REQUIRED)
#include(${SWIG_USE_FILE})

#include_directories(${CMAKE_CURRENT_SOURCE_DIR})

#set(CMAKE_SWIG_FLAGS "")

#set_source_files_properties(monica.i PROPERTIES CPLUSPLUS ON)
#set_source_files_properties(monica.i PROPERTIES SWIG_FLAGS "-includeall")
#swig_add_module(monica_py python ${CARBIOCIAL_DIR}/monica/project-files/monica.i)
#swig_link_libraries(monica_py ${PYTHON_LIBRARIES})