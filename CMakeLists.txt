cmake_minimum_required(VERSION 2.8) #3.2.2)
project(monica)

add_definitions(
	#-DNO_ZMQ
	-DNO_MYSQL
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(UTIL_DIR "../util")
set(SYS_LIBS_DIR "../sys-libs")

include_directories("src")
include_directories(${UTIL_DIR})
include_directories("${SYS_LIBS_DIR}/includes/zeromq-dev-master")
include_directories("${SYS_LIBS_DIR}/includes/czmq-master")
include_directories("${SYS_LIBS_DIR}/includes/libsodium-1.0.10")

link_directories($ENV{HOME}/lib)

find_package(Threads REQUIRED)

# create monica shared library
set(LIBMONICA_SOURCE

	src/core/crop.h
	src/core/crop.cpp
	
	src/core/crop-growth.h
	src/core/crop-growth.cpp
	
	src/core/monica-model.h
	src/core/monica-model.cpp
	
	src/core/monica-parameters.h
	src/core/monica-parameters.cpp
		
	src/core/soilcolumn.h
	src/core/soilcolumn.cpp
	
	src/core/soilmoisture.h
	src/core/soilmoisture.cpp
	
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

	src/run/cultivation-method.h
	src/run/cultivation-method.cpp
	
	src/run/run-monica.h
	src/run/run-monica.cpp

	src/io/output.h
	src/io/output.cpp
	
	src/io/build-output.h
	src/io/build-output.cpp
	

	# db library code
	#-------------------------------------------
	${UTIL_DIR}/db/abstract-db-connections.h
	${UTIL_DIR}/db/abstract-db-connections.cpp
	
	${UTIL_DIR}/db/db.h
	${UTIL_DIR}/db/db.cpp
	
	${UTIL_DIR}/db/sqlite3.h
	${UTIL_DIR}/db/sqlite3.c

	# climate library code
	#-------------------------------------------
	${UTIL_DIR}/climate/climate-common.h
	${UTIL_DIR}/climate/climate-common.cpp
	
	# json11 library code
	#-------------------------------------------
	${UTIL_DIR}/json11/json11.hpp
	${UTIL_DIR}/json11/json11.cpp

	# tools library code
	#-------------------------------------------
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
	${UTIL_DIR}/tools/read-ini.cpp

	# soil library code
	#-------------------------------------------
	${UTIL_DIR}/soil/constants.h
	${UTIL_DIR}/soil/constants.cpp

	${UTIL_DIR}/soil/conversion.h
	${UTIL_DIR}/soil/conversion.cpp

	${UTIL_DIR}/soil/soil.h
	${UTIL_DIR}/soil/soil.cpp
)
add_library(libmonica SHARED ${LIBMONICA_SOURCE})
target_link_libraries(libmonica
	${CMAKE_THREAD_LIBS_INIT}
	dl
)
#set_target_properties(libmonica PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY})
set_target_properties(libmonica PROPERTIES PREFIX "")
set_target_properties(libmonica PROPERTIES COMPILE_DEFINITIONS "USE_DLL;DLL_EXPORTS")

#------------------------------------------------------------------------------

# create monica cli starter 
set(MONICA_SOURCE
	src/run/monica-main.cpp)
add_executable(monica ${MONICA_SOURCE})
target_link_libraries(monica
	${CMAKE_THREAD_LIBS_INIT}
	dl
)
#set_target_properties(monica PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY})

#------------------------------------------------------------------------------

# create monica-run, the non-zeromq cli client, which runs monica locally
set(MONICA_RUN_SOURCE
	
	src/io/csv-format.h
	src/io/csv-format.cpp
	
	src/io/database-io.h
	src/io/database-io.cpp
		
	src/run/env-from-json-config.h
	src/run/env-from-json-config.cpp

	src/run/env-json-from-json-config.h
	src/run/env-json-from-json-config.cpp
	
	src/run/monica-run-main.cpp
	
	# db library code
	#-------------------------------------------
	${UTIL_DIR}/db/abstract-db-connections.h
	${UTIL_DIR}/db/abstract-db-connections.cpp

	${UTIL_DIR}/db/db.h
	${UTIL_DIR}/db/db.cpp
	
	${UTIL_DIR}/db/sqlite3.h
	${UTIL_DIR}/db/sqlite3.c

	# climate library code
	#-------------------------------------------
	${UTIL_DIR}/climate/climate-file-io.h
	${UTIL_DIR}/climate/climate-file-io.cpp

	${UTIL_DIR}/climate/climate-common.h
	${UTIL_DIR}/climate/climate-common.cpp
	
	# json11 library code
	#-------------------------------------------
	${UTIL_DIR}/json11/json11.hpp
	${UTIL_DIR}/json11/json11.cpp

	# tools library code
	#-------------------------------------------
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
	${UTIL_DIR}/tools/read-ini.cpp

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
)
add_executable(monica-run ${MONICA_RUN_SOURCE})
target_link_libraries(monica-run
	${CMAKE_THREAD_LIBS_INIT}
	dl
	libmonica
)
set_target_properties(monica-run PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY})
#set_target_properties(monica-run PROPERTIES COMPILE_DEFINITIONS "USE_DLL")

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
target_link_libraries(monica-zmq-control
	${CMAKE_THREAD_LIBS_INIT}
	dl
	zmq
)
#set_target_properties(monica-zmq-control PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY})

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
target_link_libraries(monica-zmq-control-send
	${CMAKE_THREAD_LIBS_INIT}
	dl
	zmq
)
#set_target_properties(monica-zmq-control-send PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY})

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
target_link_libraries(monica-zmq-proxy
	${CMAKE_THREAD_LIBS_INIT}
	dl
	zmq
)
#set_target_properties(monica-zmq-proxy PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY})

#------------------------------------------------------------------------------

# create monica-zmq-run executable, the cli client to monica-zmq-server
set(MONICA_ZMQ_RUN_SOURCE
	
	src/io/csv-format.h
	src/io/csv-format.cpp
	
	src/io/database-io.h
	src/io/database-io.cpp

	src/io/output.h
	src/io/output.cpp
		
	src/run/env-from-json-config.h
	src/run/env-from-json-config.cpp

	src/run/env-json-from-json-config.h
	src/run/env-json-from-json-config.cpp

	src/run/run-monica-zmq.h
	src/run/run-monica-zmq.cpp

	src/run/monica-zmq-run-main.cpp
	
	# db library code
	#-------------------------------------------
	${UTIL_DIR}/db/abstract-db-connections.h
	${UTIL_DIR}/db/abstract-db-connections.cpp

	${UTIL_DIR}/db/db.h
	${UTIL_DIR}/db/db.cpp
	
	${UTIL_DIR}/db/sqlite3.h
	${UTIL_DIR}/db/sqlite3.c

	# climate library code
	#-------------------------------------------
	${UTIL_DIR}/climate/climate-file-io.h
	${UTIL_DIR}/climate/climate-file-io.cpp

	${UTIL_DIR}/climate/climate-common.h
	${UTIL_DIR}/climate/climate-common.cpp
	
	# json11 library code
	#-------------------------------------------
	${UTIL_DIR}/json11/json11.hpp
	${UTIL_DIR}/json11/json11.cpp

	# tools library code
	#-------------------------------------------
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
	${UTIL_DIR}/tools/read-ini.cpp

	${UTIL_DIR}/tools/zmq-helper.h
	${UTIL_DIR}/tools/zmq-helper.cpp

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
)
add_executable(monica-zmq-run ${MONICA_ZMQ_RUN_SOURCE})
target_link_libraries(monica-zmq-run
	${CMAKE_THREAD_LIBS_INIT}
	dl
	zmq
	libmonica
)
set_target_properties(monica-zmq-run PROPERTIES COMPILE_DEFINITIONS "USE_DLL")
#set_target_properties(monica-zmq-run PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY})

#------------------------------------------------------------------------------

# create monica-zmq-server executable, the server running monica via zeromq requests
set(MONICA_ZMQ_SERVER_SOURCE
	
	src/run/serve-monica-zmq.h
	src/run/serve-monica-zmq.cpp

	src/run/monica-zmq-server-main.cpp
	
	# db library code
	#-------------------------------------------
	${UTIL_DIR}/db/abstract-db-connections.h
	${UTIL_DIR}/db/abstract-db-connections.cpp

	${UTIL_DIR}/db/db.h
	${UTIL_DIR}/db/db.cpp
	
	${UTIL_DIR}/db/sqlite3.h
	${UTIL_DIR}/db/sqlite3.c

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
	${UTIL_DIR}/tools/read-ini.cpp

	${UTIL_DIR}/tools/zmq-helper.h
	${UTIL_DIR}/tools/zmq-helper.cpp

	# soil library code
	#-------------------------------------------
	${UTIL_DIR}/soil/constants.h
	${UTIL_DIR}/soil/constants.cpp

	${UTIL_DIR}/soil/conversion.h
	${UTIL_DIR}/soil/conversion.cpp

	${UTIL_DIR}/soil/soil.h
	${UTIL_DIR}/soil/soil.cpp
)
add_executable(monica-zmq-server ${MONICA_ZMQ_SERVER_SOURCE})
target_link_libraries(monica-zmq-server
	${CMAKE_THREAD_LIBS_INIT}
	dl
	zmq
	libmonica
)
set_target_properties(monica-zmq-server PROPERTIES COMPILE_DEFINITIONS "USE_DLL")
#set_target_properties(monica-zmq-server PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY})

#------------------------------------------------------------------------------

find_package(PythonLibs)
include_directories(${PYTHON_INCLUDE_DIRS})

find_package(Boost COMPONENTS python REQUIRED)
include_directories(${Boost_INCLUDE_DIR})

# create monica python wrapper lib
set(MONICA_PYTHON_SOURCE

	src/io/csv-format.h
	src/io/csv-format.cpp
	
	src/io/database-io.h
	src/io/database-io.cpp
		
	src/run/env-from-json-config.h
	src/run/env-from-json-config.cpp

	src/run/env-json-from-json-config.h
	src/run/env-json-from-json-config.cpp
	
	src/python/monica_py.cpp
	
	# db library code
	#-------------------------------------------
	${UTIL_DIR}/db/abstract-db-connections.h
	${UTIL_DIR}/db/abstract-db-connections.cpp

	${UTIL_DIR}/db/db.h
	${UTIL_DIR}/db/db.cpp
	
	${UTIL_DIR}/db/sqlite3.h
	${UTIL_DIR}/db/sqlite3.c

	# climate library code
	#-------------------------------------------
	${UTIL_DIR}/climate/climate-file-io.h
	${UTIL_DIR}/climate/climate-file-io.cpp

	${UTIL_DIR}/climate/climate-common.h
	${UTIL_DIR}/climate/climate-common.cpp
	
	# json11 library code
	#-------------------------------------------
	${UTIL_DIR}/json11/json11.hpp
	${UTIL_DIR}/json11/json11.cpp

	# tools library code
	#-------------------------------------------
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
	${UTIL_DIR}/tools/read-ini.cpp

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
)
add_library(monica_python SHARED ${MONICA_PYTHON_SOURCE})
target_link_libraries(monica_python 
	${Boost_LIBRARIES} 
	libmonica
)
set_target_properties(monica_python PROPERTIES PREFIX "")
#set_target_properties(monica_python PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY})
set_target_properties(monica_python PROPERTIES COMPILE_DEFINITIONS "USE_DLL")

