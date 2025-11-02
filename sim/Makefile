TOP=..
include $(TOP)/configure/CONFIG

LIBRARY_IOC += xspdSim

LIBRARY_SRCS += xspdSimulator.cpp

LIBRARY_SYS_LIBS += pthread zmq

PROD_IOC = xspdSimApp

PROD_SRCS += xspdSimMain.cpp

PROD_LIBS += xspdSim

PROD_SYS_LIBS += pthread zmq


include $(TOP)/configure/RULES