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

ifdef ZMQ_LIB
  zmq_DIR        += $(ZMQ_LIB)
  PROD_LIBS      += zmq
else
  PROD_SYS_LIBS  += zmq
endif

PROD_SYS_LIBS += curl

include $(ADCORE)/ADApp/commonDriverMakefile

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
