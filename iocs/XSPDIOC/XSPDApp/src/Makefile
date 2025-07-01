TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

PROD_NAME = XSPDApp
PROD_IOC += $(PROD_NAME)

DBD += $(PROD_NAME).dbd
$(PROD_NAME)_DBD += XSPDSupport.dbd

$(PROD_NAME)_SRCS += $(PROD_NAME)_registerRecordDeviceDriver.cpp $(PROD_NAME)Main.cpp

# Add any libraries built by the EPICS build system here
$(PROD_NAME)_LIBS += ADXSPD

# Add any system libraries here
$(PROD_NAME)_SYS_LIBS +=

include $(ADCORE)/ADApp/commonDriverMakefile

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
