< envPaths

dbLoadDatabase("../../dbd/testBusyAsyn.dbd")
testBusyAsyn_registerRecordDeviceDriver(pdbbase)

epicsEnvSet("PREFIX", "testBusyAsyn:")
epicsEnvSet("PORT",   "TBA")

# Arguments: Portname, canBlock
testBusyAsynConfigure("$(PORT)", 0)
# Enable ASYN_TRACE_WARNING
asynSetTraceMask("$(PORT)",0,0x21)
asynSetTraceIOMask("$(PORT)",0,0x2)

dbLoadRecords("$(BUSY)/db/testBusyAsyn.db","P=$(PREFIX),PORT=$(PORT),ADDR=0,TIMEOUT=1,PINI=YES")

dbLoadRecords("$(ASYN)/db/asynRecord.db","P=$(PREFIX),R=asyn1,PORT=$(PORT),ADDR=0,OMAX=80,IMAX=80")

< ../saveRestore.cmd

iocInit()

# save settings every thirty seconds
create_monitor_set("auto_settings.req",30,"P=$(PREFIX)")



