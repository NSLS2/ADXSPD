TOP = ../..
include $(TOP)/configure/CONFIG

#----------------------------------------

PROD_IOC = ADXSPDTests


ADXSPDTests_SRCS += ADXSPDTestMain.cpp
ADXSPDTests_SRCS += ADXSPDTestUtils.cpp

# Add additional test source files here
# ADXSPDTests_SRCS +=

ADXSPDTests_LIBS += ADXSPD ADBase asyn cpr gtest gmock $(EPICS_BASE_IOC_LIBS) xml2

ifdef ZMQ_LIB
  zmq_DIR        += $(ZMQ_LIB)
  ADXSPDTests_LIBS      += zmq
else
  ADXSPDTests_SYS_LIBS  += zmq
endif

ADXSPDTests_SYS_LIBS += curl z boost_system

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE