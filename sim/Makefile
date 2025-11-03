TOP=..
include $(TOP)/configure/CONFIG

LIBRARY_IOC += xspdSim

LIB_SRCS += xspdSimulator.cpp

LIB_SYS_LIBS += pthread zmq

PROD_IOC = xspdSimApp

PROD_SRCS += xspdSimMain.cpp

PROD_LIBS += xspdSim cpr

PROD_SYS_LIBS += pthread zmq


include $(TOP)/configure/RULES