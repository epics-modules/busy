/*
 * testBusyAsyn.cpp
 * 
 * Asyn driver that inherits from the asynPortDriver class to test busy records using callbacks
 *
 * Author: Mark Rivers
 *
 * It tests the following:
 *   If the value in the autosave file is correctly written to the driver
 *   Callbacks done in immediately in the writeXXX function
 *   Callbacks done asynchronously in another driver thread
 * Configuration parameter in the driver constructor control whether the driver is synchronous or asynchronous
 *
 * Author: Mark Rivers
 *
 * Created November 13, 2017
 */

#include <string.h>
#include <epicsThread.h>
#include <epicsEvent.h>
#include <epicsTypes.h>
#include <iocsh.h>

#include <asynPortDriver.h>
#include <epicsExport.h>

//static const char *driverName="testBusyAsyn";

/* These are the drvInfo strings that are used to identify the parameters.
 * They are used by asyn clients, including standard asyn device support */
#define P_BusyValueString                   "BUSY_VALUE"              /* asynInt32,         r/w */
#define P_NumCallbacksString                "NUM_CALLBACKS"           /* asynInt32,         r/w */
#define P_SleepTimeString                   "SLEEP_TIME"              /* asynFloat64,       r/w */
#define P_TriggerCallbacksString            "TRIGGER_CALLBACKS"       /* asynInt32,         r/w */

/** Class that tests error handing of the asynPortDriver base class using both normally scanned records and I/O Intr
  * scanned records. */
class testBusyAsyn : public asynPortDriver {
public:
    testBusyAsyn(const char *portName, int canBlock);
    asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);
    void callbackThread();

private:
    int P_BusyValue;
    int P_TriggerCallbacks;
    int P_NumCallbacks;
    int P_SleepTime;

    epicsEventId callbackEvent_;
    int numCallbacks_;
    epicsFloat64 sleepTime_;
    void doBusyCallbacks();
};

static void callbackThreadC(void *pPvt)
{
    testBusyAsyn *p = (testBusyAsyn*)pPvt;
    p->callbackThread();
}

/** Constructor for the testBusyAsyn class.
  * Calls constructor for the asynPortDriver base class.
  * \param[in] portName The name of the asyn port driver to be created. */
testBusyAsyn::testBusyAsyn(const char *portName, int canBlock) 
   : asynPortDriver(portName, 
                    1, /* maxAddr */ 
                     /* Interface mask */
                    asynInt32Mask | asynFloat64Mask | asynDrvUserMask,
                    /* Interrupt mask */
                    asynInt32Mask | asynOctetMask,
                    canBlock ? ASYN_CANBLOCK : 0, /* asynFlags.  canblock is passed to constructor and multi-device is 0 */
                    1, /* Autoconnect */
                    0, /* Default priority */
                    0) /* Default stack size*/,
     numCallbacks_(1), sleepTime_(0.)  
{
    createParam(P_BusyValueString,                  asynParamInt32,         &P_BusyValue);
    createParam(P_NumCallbacksString,               asynParamInt32,         &P_NumCallbacks);
    createParam(P_SleepTimeString,                  asynParamFloat64,       &P_SleepTime);
    createParam(P_TriggerCallbacksString,           asynParamInt32,         &P_TriggerCallbacks);
    
    setIntegerParam(P_BusyValue, 0);
    setIntegerParam(P_NumCallbacks, numCallbacks_);
    setDoubleParam(P_SleepTime, sleepTime_);
    
    callbackEvent_ = epicsEventCreate(epicsEventEmpty);
    epicsThreadCreate("callbackThread",
        epicsThreadPriorityMedium,
        epicsThreadGetStackSize(epicsThreadStackSmall),
        (EPICSTHREADFUNC)callbackThreadC, this);
}

asynStatus testBusyAsyn::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    int function=pasynUser->reason;

    setIntegerParam(function, value);

    if (function == P_BusyValue) {
        doBusyCallbacks();
    }
    else if (function == P_NumCallbacks) {
        numCallbacks_ = value;
    }
    else if (function == P_TriggerCallbacks) {
        epicsEventSignal(callbackEvent_);
    }
    return asynSuccess;
}

asynStatus testBusyAsyn::writeFloat64(asynUser *pasynUser, epicsFloat64 value)
{
    int function=pasynUser->reason;

    setDoubleParam(function, value);

    if (function == P_SleepTime) {
        sleepTime_ = value;
    }
    return asynSuccess;
}

void testBusyAsyn::doBusyCallbacks()
{
    epicsInt32 value;
    for (int i=0; i<numCallbacks_; i++) {
        getIntegerParam(P_BusyValue, &value);
        value = value ? 0:1;
        setIntegerParam(P_BusyValue, value);
        callParamCallbacks();
        if (sleepTime_ > 0) epicsThreadSleep(sleepTime_);
    }
}

void testBusyAsyn::callbackThread()
{
    lock();
    while (1) {
        unlock();
        (void)epicsEventWait(callbackEvent_);
        lock();
        doBusyCallbacks();
    }
}


/* Configuration routine.  Called directly, or from the iocsh function below */
extern "C" {

/** EPICS iocsh callable function to call constructor for the testBusyAsyn class.
  * \param[in] portName The name of the asyn port driver to be created.
  * \param[in] canBlock Whether the driver is synchronous (0) or asynchronous (1)
  */
int testBusyAsynConfigure(const char *portName, int canBlock)
{
    new testBusyAsyn(portName, canBlock);
    return asynSuccess ;
}


/* EPICS iocsh shell commands */

static const iocshArg initArg0 = { "portName", iocshArgString};
static const iocshArg initArg1 = { "canBlock", iocshArgInt};
static const iocshArg * const initArgs[] = {&initArg0, &initArg1};
static const iocshFuncDef initFuncDef = {"testBusyAsynConfigure",2,initArgs};
static void initCallFunc(const iocshArgBuf *args)
{
    testBusyAsynConfigure(args[0].sval, args[1].ival);
}

void testBusyAsynRegister(void)
{
    iocshRegister(&initFuncDef,initCallFunc);
}

epicsExportRegistrar(testBusyAsynRegister);

}

