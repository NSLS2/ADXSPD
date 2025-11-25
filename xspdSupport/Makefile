TOP=..
include $(TOP)/configure/CONFIG

#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

DIRS := $(DIRS) json
DIRS := $(DIRS) magic_enum
DIRS := $(DIRS) cpr

ifeq ($(BUILD_TESTS), YES)
DIRS := $(DIRS) gtest
endif

#=============================

include $(TOP)/configure/RULES_DIRS
#----------------------------------------
#  ADD RULES AFTER THIS LINE
