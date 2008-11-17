TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================
DBD += busySupport.dbd
DBDINC += busyRecord

LIBRARY_IOC = busy
LIB_SRCS += busyRecord.c
LIB_SRCS += devBusyAsyn.c
LIB_SRCS += devBusySoft.c
LIB_LIBS += asyn
LIB_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

