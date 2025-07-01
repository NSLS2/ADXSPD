TOP=..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

SRC_DIRS = ..

INC +=

# Add libraries from vendor, based on OS and architecture

ifeq (linux-x86_64, $(findstring linux-x86_64, $(T_A)))
LIB_INSTALLS_Linux +=
endif

ifeq (windows-x64, $(findstring windows-x64, $(T_A)))
LIB_INSTALLS_Windows +=
endif

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE


