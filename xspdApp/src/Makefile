TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

ifeq (windows-x64, $(findstring windows-x64, $(T_A)))
USR_CPPFLAGS += -D__X64
endif

USR_CPPFLAGS += -std=c++11

INC += ADXSPD.h ADXSPDParamDefs.h ADXSPDModule.h ADXSPDModuleParamDefs.h

LIBRARY_IOC = ADXSPD
LIB_SRCS += ADXSPDParamDefs.cpp ADXSPD.cpp ADXSPDModuleParamDefs.cpp ADXSPDModule.cpp


DBD += XSPDSupport.dbd

include $(ADCORE)/ADApp/commonLibraryMakefile

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
