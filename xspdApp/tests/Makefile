TOP = ../..
include $(TOP)/configure/CONFIG

#----------------------------------------

USR_CPPFLAGS += -std=c++17

PROD_IOC = ADXSPDTests

ADXSPDTests_SRCS += ADXSPDTestMain.cpp
# ADXSPDTests_SRCS += ADXSPDTestUtils.cpp
ADXSPDTests_SRCS += XSPDAPITest.cpp
ADXSPDTests_SRCS += MockXSPDAPI.cpp

# Add additional test source files here
# ADXSPDTests_SRCS +=

ADXSPDTests_LIBS += ADXSPD ADBase asyn cpr gtest gmock $(EPICS_BASE_IOC_LIBS) xml2

ifdef ZMQ_LIB
  zmq_DIR        += $(ZMQ_LIB)
  ADXSPDTests_LIBS      += zmq
else
  ADXSPDTests_SYS_LIBS  += zmq
endif

ADXSPDTests_SYS_LIBS += curl z

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
