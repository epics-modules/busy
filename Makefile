#Makefile at top of application tree
TOP = .
include $(TOP)/configure/CONFIG
DIRS += configure busyApp iocBoot
busyApp_DEPEND_DIRS   = configure

include $(TOP)/configure/RULES_TOP
