TOP=../..
include $(TOP)/configure/CONFIG

PROD_NAME = testBusyAsynApp
PROD_IOC = $(PROD_NAME)

DBD += $(PROD_NAME).dbd

$(PROD_NAME)_DBD += base.dbd
$(PROD_NAME)_DBD += $(BUSY_IOC_DBDS)
$(PROD_NAME)_DBD += asyn.dbd
$(PROD_NAME)_DBD += asSupport.dbd
$(PROD_NAME)_DBD += testBusyAsyn.dbd

$(PROD_NAME)_SRCS += testBusyAsyn.cpp
$(PROD_NAME)_SRCS += $(PROD_NAME)_registerRecordDeviceDriver.cpp
$(PROD_NAME)_SRCS_DEFAULT += $(PROD_NAME)Main.cpp
$(PROD_NAME)_SRCS_vxWorks += -nil-

$(PROD_NAME)_LIBS += $(BUSY_IOC_LIBS)
$(PROD_NAME)_LIBS += asyn autosave
$(PROD_NAME)_LIBS += $(EPICS_BASE_IOC_LIBS)

$(PROD_NAME)_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

include $(TOP)/configure/RULES
