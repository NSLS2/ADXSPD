#Makefile at top of application tree
TOP = .
include $(TOP)/configure/CONFIG
DIRS := $(DIRS) configure
DIRS := $(DIRS) xspdApp
DIRS := $(DIRS) xspdSupport

xspdApp_DEPEND_DIRS += xspdSupport
ifeq ($(BUILD_IOCS), YES)
DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard iocs))
iocs_DEPEND_DIRS += xspdApp
endif
include $(TOP)/configure/RULES_TOP

uninstall: uninstall_iocs
uninstall_iocs:
	$(MAKE) -C iocs uninstall
.PHONY: uninstall uninstall_iocs

realuninstall: realuninstall_iocs
realuninstall_iocs:
	$(MAKE) -C iocs realuninstall
.PHONY: realuninstall realuninstall_iocs


bobfiles:
	epicsdb2bob xspdApp/Db xspdApp/op/bob -m P=DEV:XSPD1: R=cam1: PORT=XSPD1 ADDR=0 TIMEOUT=1 -d -r _RBV -t none -b xspdApp/op/bob

paramdefs:
	scripts/generate_param_defs.py xspdApp/Db/ADXSPD.template xspdApp/src
	scripts/generate_param_defs.py xspdApp/Db/ADXSPDModule.template xspdApp/src
