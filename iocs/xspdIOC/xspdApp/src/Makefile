TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

PROD_NAME = xspdApp
PROD_IOC += $(PROD_NAME)

DBD += $(PROD_NAME).dbd
$(PROD_NAME)_DBD += xspdSupport.dbd

$(PROD_NAME)_SRCS += $(PROD_NAME)_registerRecordDeviceDriver.cpp $(PROD_NAME)Main.cpp

# Add any libraries built by the EPICS build system here
$(PROD_NAME)_LIBS += ADXSPD cpr

ifeq ($(GTEST_EXTERNAL), NO)
  $(PROD_NAME)_LIBS += gtest gmock
else
  ifdef GTEST_LIB
    gtest_DIR        += $(GTEST_LIB)
    $(PROD_NAME)_LIBS      += gtest gmock
  else
    $(PROD_NAME)_SYS_LIBS += gtest gmock
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

include $(ADCORE)/ADApp/commonDriverMakefile

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
