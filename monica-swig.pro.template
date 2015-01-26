include(monica.pro)

##################################################
# Project file for python swig wrapping of monica
#################################################
SWIG_DIR = .
# swig configuration ###################################
#global defaults
isEmpty(SWIG_DIR):SWIG_DIR = .
isEmpty(SWIG_CMD):SWIG_CMD = swig -python -c++ -w -shadow $$SWIG_CMD_MONICA_DEFS

#dependency to generate *_wrap.cxx from *.i
swig_cxx.name = SWIG_CXX ${QMAKE_FILE_IN}
swig_cxx.commands = $$SWIG_CMD -o $$SWIG_DIR/${QMAKE_FILE_BASE}_wrap.cxx ${QMAKE_FILE_NAME} 
swig_cxx.variable_out = SOURCES
swig_cxx.CONFIG += no_link -w -nodefaultctor
swig_cxx.output = $$SWIG_DIR/${QMAKE_FILE_BASE}_wrap.cxx
swig_cxx.input = SWIG_FILES
swig_cxx.clean = $$SWIG_DIR/${QMAKE_FILE_BASE}_wrap.cxx
QMAKE_EXTRA_COMPILERS += swig_cxx

#dependency to generate *.py from *.i
swig_py.name = SWIG_PY ${QMAKE_FILE_IN}
swig_py.commands = $$SWIG_CMD -o $$SWIG_DIR/${QMAKE_FILE_BASE}_wrap.cxx ${QMAKE_FILE_NAME}

swig_py.output = $$SWIG_DIR/${QMAKE_FILE_BASE}.py
swig_py.input = SWIG_FILES
swig_py.clean = $$SWIG_DIR/${QMAKE_FILE_BASE}.py
QMAKE_EXTRA_COMPILERS += swig

################################################
# monica configuration
################################################

TEMPLATE = lib
TARGET = _monica
OBJECTS_DIR = obj
DESTDIR = .
CONFIG  += console plugin no_plugin_name_prefix debug
CONFIG  += no_include_pwd -w
CONFIG -= qt
DEFINES  += COMPILE_DL=1

QMAKE_CFLAGS += -I/usr/include/python2.6 
QMAKE_CXXFLAGS += -I/usr/include/python2.6

# add flags to create profiling file
QMAKE_CFLAGS_DEBUG += -pg -w
QMAKE_CXXFLAGS += -pg -w
QMAKE_LFLAGS += -pg -w

SWIG_FILES = monica-swig/monica.i

LIBS += -lpython2.6
