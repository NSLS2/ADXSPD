TOP=..
include $(TOP)/configure/CONFIG

#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

DIRS := $(DIRS) json
DIRS := $(DIRS) magic_enum
DIRS := $(DIRS) cpr

ifeq ($(BUILD_XSPD_TESTS), YES)
ifeq ($(GTEST_EXTERNAL), NO)
DIRS := $(DIRS) gtest
endif
endif

#=============================

include $(TOP)/configure/RULES_DIRS
#----------------------------------------
#  ADD RULES AFTER THIS LINE
