TOP = ../..
include $(TOP)/configure/CONFIG

#----------------------------------------

USR_CPPFLAGS += -std=c++17

PROD_NAME = TestADXSPD
TESTPROD_IOC = $(PROD_NAME)

$(PROD_NAME)_SRCS += TestADXSPDMain.cpp
$(PROD_NAME)_SRCS += TestXSPDAPI.cpp
$(PROD_NAME)_SRCS += MockXSPDAPI.cpp

$(PROD_NAME)_LIBS += ADXSPD ADBase asyn cpr

ifeq ($(GTEST_EXTERNAL), NO)
  $(PROD_NAME)_LIBS += gmock gtest
else
  ifdef GTEST_LIB
    gtest_DIR        += $(GTEST_LIB)
    $(PROD_NAME)_LIBS      += gmock gtest
  else
    $(PROD_NAME)_SYS_LIBS += gmock gtest
  endif
endif

ifeq ($(ZMQ_EXTERNAL), NO)
  $(PROD_NAME)_LIBS += zmq
else
  ifdef ZMQ_LIB
    zmq_DIR        += $(ZMQ_LIB)
    $(PROD_NAME)_LIBS      += zmq
  else
    $(PROD_NAME)_SYS_LIBS  += zmq
  endif
endif

ifeq ($(CURL_EXTERNAL), NO)
  $(PROD_NAME)_LIBS += curl
else
  ifdef CURL_LIB
    curl_DIR        += $(CURL_LIB)
    $(PROD_NAME)_LIBS      += curl
  else
    $(PROD_NAME)_SYS_LIBS += curl
  endif
endif

ifeq ($(ZLIB_EXTERNAL), NO)
  $(PROD_NAME)_LIBS += zlib
else
  ifdef ZLIB_LIB
    zlib_DIR        += $(ZLIB_LIB)
    $(PROD_NAME)_LIBS      += z
  else
    $(PROD_NAME)_SYS_LIBS += z
  endif
endif

$(PROD_NAME)_LIBS += $(EPICS_BASE_IOC_LIBS)


include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
