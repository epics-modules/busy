#Makefile at top of application tree
TOP = .
include $(TOP)/configure/CONFIG
DIRS += configure busyApp
busyApp_DEPEND_DIRS = configure

ifeq ($(BUILD_IOCS), YES)
DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard iocs))
iocs_DEPEND_DIRS += busyApp
endif

include $(TOP)/configure/RULES_TOP
