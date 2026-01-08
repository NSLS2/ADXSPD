TOP = ../..
include $(TOP)/configure/CONFIG

#----------------------------------------

USR_CPPFLAGS += -std=c++17

PROD_IOC = TestADXSPD

TestADXSPD_SRCS += TestADXSPDMain.cpp
# TestADXSPD_SRCS += ADXSPDTestUtils.cpp
TestADXSPD_SRCS += TestXSPDAPI.cpp
TestADXSPD_SRCS += MockXSPDAPI.cpp

# Add additional test source files here
# TestADXSPD_SRCS +=

TestADXSPD_LIBS += ADXSPD ADBase asyn cpr gtest gmock $(EPICS_BASE_IOC_LIBS) xml2

ifdef ZMQ_LIB
  zmq_DIR        += $(ZMQ_LIB)
  TestADXSPD_LIBS      += zmq
else
  TestADXSPD_SYS_LIBS  += zmq
endif

TestADXSPD_SYS_LIBS += curl z

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
