CONFIG += HERMES #RUN_HERMES

isEmpty(ARCH){
  ARCH = x64
}

message("building code for $$ARCH architecture")

TEMPLATE = app
TARGET = monica
DESTDIR = ../bin
OBJECTS_DIR = temp/obj
DEFINES -= MONICA_GUI

QMAKE_CXXFLAGS += -std=c++0x
QMAKE_CXXFLAGS_RELEASE += -w
QMAKE_CFLAGS_RELEASE += -w

win32 {
  # add specific paths (e.g. boost) in pri file
  include(monica.win.pri)
}

UTIL_DIR = ../util

CONFIG += console 
CONFIG -= qt 

# add flags to create profiling file
unix {
#QMAKE_CFLAGS_DEBUG += -pg
#QMAKE_LFLAGS += -pg
}

# defining stand alone version of MONICA
DEFINES += STANDALONE

HERMES:DEFINES += NO_MYSQL

# monica code
HEADERS += src/soilcolumn.h
HEADERS += src/soiltransport.h
HEADERS += src/soiltemperature.h
HEADERS += src/soilmoisture.h
HEADERS += src/soilorganic.h
HEADERS += src/monica.h
HEADERS += src/monica-parameters.h
HEADERS += src/crop.h
HEADERS += src/debug.h
HEADERS += src/conversion.h
HEADERS += src/configuration.h

SOURCES += src/main.cpp
SOURCES += src/soilcolumn.cpp
SOURCES += src/soiltransport.cpp
SOURCES += src/soiltemperature.cpp
SOURCES += src/soilmoisture.cpp
SOURCES += src/soilorganic.cpp
SOURCES += src/monica.cpp
SOURCES += src/monica-parameters.cpp
SOURCES += src/crop.cpp
SOURCES += src/debug.cpp
SOURCES += src/conversion.cpp
SOURCES += src/configuration.cpp

# db library code
HEADERS += $${UTIL_DIR}/db/db.h
HEADERS += $${UTIL_DIR}/db/abstract-db-connections.h
HEADERS += $${UTIL_DIR}/db/sqlite3.h
HEADERS += $${UTIL_DIR}/db/sqlite3.h

SOURCES += $${UTIL_DIR}/db/db.cpp
SOURCES += $${UTIL_DIR}/db/abstract-db-connections.cpp
SOURCES += $${UTIL_DIR}/db/sqlite3.c

# climate library code
HEADERS += $${UTIL_DIR}/climate/climate-common.h

SOURCES += $${UTIL_DIR}/climate/climate-common.cpp

# json library code
HEADERS += $${UTIL_DIR}/cson/cson_amalgamation_core.h

SOURCES += $${UTIL_DIR}/cson/cson_amalgamation_core.c

# tools library code
HEADERS += $${UTIL_DIR}/tools/algorithms.h
HEADERS += $${UTIL_DIR}/tools/date.h
HEADERS += $${UTIL_DIR}/tools/read-ini.h
HEADERS += $${UTIL_DIR}/tools/datastructures.h
HEADERS += $${UTIL_DIR}/tools/helper.h
HEADERS += $${UTIL_DIR}/tools/use-stl-algo-boost-lambda.h
HEADERS += $${UTIL_DIR}/tools/stl-algo-boost-lambda.h

SOURCES += $${UTIL_DIR}/tools/algorithms.cpp
SOURCES += $${UTIL_DIR}/tools/date.cpp
SOURCES += $${UTIL_DIR}/tools/read-ini.cpp


#includes
#-------------------------------------------------------------

INCLUDEPATH += \
#../../boost_1_53_0 \
../util \
../loki-lib/include

#libs
#------------------------------------------------------------

#win32:LIBS += -L"C:/Program Files (x86)/Microsoft Visual Studio 11.0/VC/lib"

#CONFIG(debug, debug|release):LIBS += \
#-llibmysqld
#CONFIG(release, debug|release):LIBS += \
#-llibmysql

#CONFIG(debug, debug|release):QMAKE_LFLAGS += \
#/NODEFAULTLIB:msvcrt.lib #\
#/VERBOSE:lib

#CONFIG(release, debug|release):QMAKE_LFLAGS += \
#/NODEFAULTLIB:msvcrtd.lib


unix {
LIBS += \
-lm -ldl \
-lpthread \
-lboost_filesystem
}

#rc files
#------------------------------------------------------

win32:RC_FILE = monica.rc

# build configuration specific stuff
#--------------------------------------------------------

HERMES {
DEFINES += RUN_HERMES
message("Configuration: RUN_HERMES")
}


