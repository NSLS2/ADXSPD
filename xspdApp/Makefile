TOP = ..
include $(TOP)/configure/CONFIG
#----------------------------------------

DIRS := $(DIRS) src
ifeq ($(BUILD_TESTS), YES)
DIRS := $(DIRS) tests
tests_DEPEND_DIRS += src
endif

DIRS := $(DIRS) Db

include $(TOP)/configure/RULES_DIRS
