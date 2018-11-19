TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

USR_CPPFLAGS += -DUSE_TYPED_RSET

DBD_INSTALLS += busySupport.dbd
DBDINC += busyRecord

LIBRARY_IOC = busy
LIB_SRCS += busyRecord.c
LIB_SRCS += devBusySoft.c
LIB_SRCS += devBusySoftRaw.c

ifdef ASYN
	LIB_SRCS += devBusyAsyn.c
	LIB_LIBS += asyn
endif
LIB_LIBS += $(EPICS_BASE_IOC_LIBS)

ifdef ASYN
  ifdef AUTOSAVE
    LIB_SRCS += testBusyAsyn.cpp
    PROD_IOC += testBusyAsyn
    DBD += testBusyAsyn.dbd
    # <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
    testBusyAsyn_SRCS_DEFAULT += testBusyAsyn_registerRecordDeviceDriver.cpp testBusyAsynMain.cpp
    testBusyAsynVx_SRCS_vxWorks  += testBusyAsyn_registerRecordDeviceDriver.cpp
    testBusyAsyn_LIBS += busy asyn autosave
    testBusyAsyn_LIBS += $(EPICS_BASE_IOC_LIBS)
    testBusyAsyn_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary
  endif
endif

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

ifdef ASYN
busySupport.dbd: ../busySupport_LOCAL.dbd ../busySupport_withASYN.dbd
	$(PERL) -MExtUtils::Command -e cat $^ > $@
else
busySupport.dbd: ../busySupport_LOCAL.dbd
	$(PERL) -MExtUtils::Command -e cat $^ > $@
endif

# Parallel build sometimes fails.
# Make dependences on busyRecord.h explicit.
busyRecord$(OBJ):     $(COMMON_DIR)/busyRecord.h
devBusySoft$(OBJ):    $(COMMON_DIR)/busyRecord.h
devBusySoftRaw$(OBJ): $(COMMON_DIR)/busyRecord.h
devBusyAsyn$(OBJ):    $(COMMON_DIR)/busyRecord.h
$(COMMON_DIR)/testBusyAsyn.dbd: busySupport.dbd

