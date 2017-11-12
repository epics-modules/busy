/* devBusyAsyn.c */
/***********************************************************************
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory, and the Regents of the University of
* California, as Operator of Los Alamos National Laboratory, and
* Berliner Elektronenspeicherring-Gesellschaft m.b.H. (BESSY).
* asynDriver is distributed subject to a Software License Agreement
* found in file LICENSE that is included with this distribution.
***********************************************************************/
/*
    Authors:  Mark Rivers
    05-Sept-2004
*/

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <alarm.h>
#include <recGbl.h>
#include <epicsMath.h>
#include <dbAccess.h>
#include <dbDefs.h>
#include <dbEvent.h>
#include <dbStaticLib.h>
#include <link.h>
#include <cantProceed.h>
#include <epicsPrint.h>
#include <epicsMutex.h>
#include <epicsThread.h>
#include <cantProceed.h>
#include <dbCommon.h>
#include <dbScan.h>
#include <callback.h>
#include <recSup.h>
#include <devSup.h>

#include "asynDriver.h"
#include "asynDrvUser.h"
#include "asynInt32.h"
#include "asynInt32SyncIO.h"
#include "asynEnum.h"
#include "asynEnumSyncIO.h"
#include "asynEpicsUtils.h"

#include <epicsExport.h>
#include <busyRecord.h>

#define INIT_OK 0
#define INIT_DO_NOT_CONVERT 2
#define INIT_ERROR -1

#define DEFAULT_RING_BUFFER_SIZE 10
/* We should be getting these from db_access.h, but get errors including that file? */
#define MAX_ENUM_STATES 16
#define MAX_ENUM_STRING_SIZE 26

static const char *driverName = "devBusyAsyn";

typedef struct ringBufferElement {
    epicsInt32          value;
    epicsTimeStamp      time;
    asynStatus          status;
    epicsAlarmCondition alarmStatus;
    epicsAlarmSeverity  alarmSeverity;
} ringBufferElement;

typedef struct devPvt{
    dbCommon          *pr;
    asynUser          *pasynUser;
    asynUser          *pasynUserSync;
    asynUser          *pasynUserEnumSync;
    asynInt32         *pint32;
    void              *int32Pvt;
    void              *registrarPvt;
    int               canBlock;
    epicsInt32        deviceLow;
    epicsInt32        deviceHigh;
    epicsMutexId      ringBufferLock;
    ringBufferElement *ringBuffer;
    int               ringHead;
    int               ringTail;
    int               ringSize;
    int               ringBufferOverflows;
    ringBufferElement result;
    interruptCallbackInt32 interruptCallback;
    double            sum;
    int               numAverage;
    int               bipolar;
    epicsInt32        mask;
    epicsInt32        signBit;
    CALLBACK          processCallback;
    CALLBACK          outputCallback;
    int               newOutputCallbackValue;
    int               numDeferredOutputCallbacks;
    IOSCANPVT         ioScanPvt;
    char              *portName;
    char              *userParam;
    int               addr;
    char              *enumStrings[MAX_ENUM_STATES];
    int               enumValues[MAX_ENUM_STATES];
    int               enumSeverities[MAX_ENUM_STATES];
    asynStatus        previousQueueRequestStatus;
}devPvt;

static void setEnums(char *outStrings, int *outVals, epicsEnum16 *outSeverities, 
                     char *inStrings[], int *inVals, int *inSeverities, 
                     size_t numIn, size_t numOut);
static long createRingBuffer(dbCommon *pr);
static void processCallbackOutput(asynUser *pasynUser);
static void outputCallbackCallback(CALLBACK *pcb);
static int  getCallbackValue(devPvt *pPvt);
static void interruptCallbackOutput(void *drvPvt, asynUser *pasynUser,
                epicsInt32 value);

static long initBusy(busyRecord *pr);
static long processBusy(busyRecord *pr);

typedef struct analogDset { /* analog  dset */
    long          number;
    DEVSUPFUN     dev_report;
    DEVSUPFUN     init;
    DEVSUPFUN     init_record;
    DEVSUPFUN     get_ioint_info;
    DEVSUPFUN     processCommon;/*(0)=>(success ) */
    DEVSUPFUN     special_linconv;
} analogDset;

analogDset asynBusyInt32 = {
    5, 0, 0, initBusy, 0, processBusy };

epicsExportAddress(dset, asynBusyInt32);

static long initCommon(dbCommon *pr, DBLINK *plink,
    userCallback processCallback,interruptCallbackInt32 interruptCallback, interruptCallbackEnum callbackEnum,
    int maxEnums, char *pFirstString, int *pFirstValue, epicsEnum16 *pFirstSeverity)
{
    devPvt *pPvt;
    asynStatus status;
    asynUser *pasynUser;
    asynInterface *pasynInterface;
    epicsUInt32 mask=0;
    static const char *functionName="initCommon";

    pPvt = callocMustSucceed(1, sizeof(*pPvt), "devBusyAsyn::initCommon");
    pr->dpvt = pPvt;
    pPvt->pr = pr;
    /* Create asynUser */
    pasynUser = pasynManager->createAsynUser(processCallback, 0);
    pasynUser->userPvt = pPvt;
    pPvt->pasynUser = pasynUser;
    pPvt->ringBufferLock = epicsMutexCreate();
 
    /* Parse the link to get addr and port */
    /* We accept 2 different link syntax (@asyn(...) and @asynMask(...)
     * If parseLink returns an error then try parseLinkMask. */
    status = pasynEpicsUtils->parseLink(pasynUser, plink, 
                &pPvt->portName, &pPvt->addr, &pPvt->userParam);
    if (status != asynSuccess) {
        status = pasynEpicsUtils->parseLinkMask(pasynUser, plink, 
                &pPvt->portName, &pPvt->addr, &mask, &pPvt->userParam);
    }
    if (status != asynSuccess) {
        printf("%s %s::%s  %s\n",
                     pr->name, driverName, functionName, pasynUser->errorMessage);
        goto bad;
    }
    
    /* Connect to device */
    status = pasynManager->connectDevice(pasynUser, pPvt->portName, pPvt->addr);
    if (status != asynSuccess) {
        printf("%s %s::%s connectDevice failed %s\n",
                     pr->name, driverName, functionName, pasynUser->errorMessage);
        goto bad;
    }
    status = pasynManager->canBlock(pPvt->pasynUser, &pPvt->canBlock);
    if (status != asynSuccess) {
        printf("%s %s::%s canBlock failed %s\n",
                     pr->name, driverName, functionName, pasynUser->errorMessage);
        goto bad;
    }
    /*call drvUserCreate*/
    pasynInterface = pasynManager->findInterface(pasynUser,asynDrvUserType,1);
    if(pasynInterface && pPvt->userParam) {
        asynDrvUser *pasynDrvUser;
        void       *drvPvt;

        pasynDrvUser = (asynDrvUser *)pasynInterface->pinterface;
        drvPvt = pasynInterface->drvPvt;
        status = pasynDrvUser->create(drvPvt,pasynUser,pPvt->userParam,0,0);
        if(status!=asynSuccess) {
            printf("%s %s::%s drvUserCreate %s\n",
                     pr->name, driverName, functionName, pasynUser->errorMessage);
            goto bad;
        }
    }
    /* Get interface asynInt32 */
    pasynInterface = pasynManager->findInterface(pasynUser, asynInt32Type, 1);
    if (!pasynInterface) {
        printf("%s %s::%s findInterface asynInt32Type %s\n",
                     pr->name, driverName, functionName,pasynUser->errorMessage);
        goto bad;
    }
    pPvt->pint32 = pasynInterface->pinterface;
    pPvt->int32Pvt = pasynInterface->drvPvt;
    scanIoInit(&pPvt->ioScanPvt);
    pPvt->interruptCallback = interruptCallback;
    /* Initialize synchronous interface */
    status = pasynInt32SyncIO->connect(pPvt->portName, pPvt->addr, 
                 &pPvt->pasynUserSync, pPvt->userParam);
    if (status != asynSuccess) {
        printf("%s %s::%s Int32SyncIO->connect failed %s\n",
               pr->name, driverName, functionName, pPvt->pasynUserSync->errorMessage);
        goto bad;
    }
    /* Initialize asynEnum interfaces */
    pasynInterface = pasynManager->findInterface(pPvt->pasynUser,asynEnumType,1);
    if (pasynInterface && (maxEnums > 0)) {
        size_t numRead;
        asynEnum *pasynEnum = pasynInterface->pinterface;
        void *registrarPvt;
        status = pasynEnumSyncIO->connect(pPvt->portName, pPvt->addr, 
                 &pPvt->pasynUserEnumSync, pPvt->userParam);
        if (status != asynSuccess) {
            printf("%s %s::%s EnumSyncIO->connect failed %s\n",
                   pr->name, driverName, functionName, pPvt->pasynUserEnumSync->errorMessage);
            goto bad;
        }
        status = pasynEnumSyncIO->read(pPvt->pasynUserEnumSync,
                    pPvt->enumStrings, pPvt->enumValues, pPvt->enumSeverities, maxEnums, 
                    &numRead, pPvt->pasynUser->timeout);
        if (status == asynSuccess) {
            setEnums(pFirstString, pFirstValue, pFirstSeverity, 
                     pPvt->enumStrings, pPvt->enumValues,  pPvt->enumSeverities, numRead, maxEnums);
        }
        status = pasynEnum->registerInterruptUser(
           pasynInterface->drvPvt, pPvt->pasynUser,
           callbackEnum, pPvt, &registrarPvt);
        if(status!=asynSuccess) {
            printf("%s %s::%s enum registerInterruptUser %s\n",
                   pr->name, driverName, functionName,pPvt->pasynUser->errorMessage);
        }
    }
    /* If interruptCallback is not NULL then register for callbacks */
    if (interruptCallback) {
        status = createRingBuffer(pr);
        if (status!=asynSuccess) goto bad;
        status = pPvt->pint32->registerInterruptUser(
           pPvt->int32Pvt,pPvt->pasynUser,
           pPvt->interruptCallback,pPvt,&pPvt->registrarPvt);
        if (status!=asynSuccess) {
            printf("%s %s::%s error calling registerInterruptUser %s\n",
                   pr->name, driverName, functionName,pPvt->pasynUser->errorMessage);
        }
        /* Initialize the interrupt callback */
        callbackSetCallback(outputCallbackCallback, &pPvt->outputCallback);
        callbackSetPriority(pr->prio, &pPvt->outputCallback);
        callbackSetUser(pPvt, &pPvt->outputCallback);
    }
    return INIT_OK;
bad:
    recGblSetSevr(pr,LINK_ALARM,INVALID_ALARM);
    pr->pact=1;
    return INIT_ERROR;
}

static long createRingBuffer(dbCommon *pr)
{
    devPvt *pPvt = (devPvt *)pr->dpvt;
    asynStatus status;
    const char *sizeString;
    static const char *functionName="createRingBuffer";
    
    if (!pPvt->ringBuffer) {
        DBENTRY *pdbentry = dbAllocEntry(pdbbase);
        pPvt->ringSize = DEFAULT_RING_BUFFER_SIZE;
        status = dbFindRecord(pdbentry, pr->name);
        if (status) {
            asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR,
                "%s %s::%s error finding record\n",
                pr->name, driverName, functionName);
            return -1;
        }
        sizeString = dbGetInfo(pdbentry, "asyn:FIFO");
        if (sizeString) pPvt->ringSize = atoi(sizeString);
        pPvt->ringBuffer = callocMustSucceed(pPvt->ringSize+1, sizeof *pPvt->ringBuffer, "devBusyAsyn::createRingBuffer");
    }
    return asynSuccess;
}

static void setEnums(char *outStrings, int *outVals, epicsEnum16 *outSeverities, char *inStrings[], int *inVals, int *inSeverities, 
                     size_t numIn, size_t numOut)
{
    size_t i;
    
    for (i=0; i<numOut; i++) {
        if (outStrings) outStrings[i*MAX_ENUM_STRING_SIZE] = '\0';
        if (outVals) outVals[i] = 0;
        if (outSeverities) outSeverities[i] = 0;
    }
    for (i=0; (i<numIn && i<numOut); i++) {
        if (outStrings) strncpy(&outStrings[i*MAX_ENUM_STRING_SIZE], inStrings[i], MAX_ENUM_STRING_SIZE);
        if (outVals) outVals[i] = inVals[i];
        if (outSeverities) outSeverities[i] = inSeverities[i];
    }
}

static void processCallbackOutput(asynUser *pasynUser)
{
    devPvt *pPvt = (devPvt *)pasynUser->userPvt;
    dbCommon *pr = pPvt->pr;
    static const char *functionName="processCallbackOutput";

    pPvt->result.status = pPvt->pint32->write(pPvt->int32Pvt, pPvt->pasynUser,pPvt->result.value);
    pPvt->result.time = pPvt->pasynUser->timestamp;
    pPvt->result.alarmStatus = pPvt->pasynUser->alarmStatus;
    pPvt->result.alarmSeverity = pPvt->pasynUser->alarmSeverity;
    if(pPvt->result.status == asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
            "%s %s::%s process value %d\n",pr->name, driverName, functionName,pPvt->result.value);
    } else {
       asynPrint(pasynUser, ASYN_TRACE_ERROR,
           "%s %s::%s process error %s\n",
           pr->name, driverName, functionName, pasynUser->errorMessage);
    }
    if(pr->pact) callbackRequestProcessCallback(&pPvt->processCallback,pr->prio,pr);
}


static void interruptCallbackOutput(void *drvPvt, asynUser *pasynUser,
                epicsInt32 value)
{
    devPvt *pPvt = (devPvt *)drvPvt;
    dbCommon *pr = pPvt->pr;
    ringBufferElement *rp;
    static const char *functionName="interruptCallbackOutput";

    if (pPvt->mask) {
        value &= pPvt->mask;
        if (pPvt->bipolar && (value & pPvt->signBit)) value |= ~pPvt->mask;
    }
    asynPrint(pPvt->pasynUser, ASYN_TRACEIO_DEVICE,
        "%s %s::%s new value=%d\n",
        pr->name, driverName, functionName, value);
    if (!interruptAccept) return;
    /* We need the scan lock because we look at PACT and pPvt->numDeferredOutputCallbacks
     * Must take scan lock before ring buffer lock */
    dbScanLock(pr);
    epicsMutexLock(pPvt->ringBufferLock);
    rp = &pPvt->ringBuffer[pPvt->ringHead];
    rp->value = value;
    rp->time = pasynUser->timestamp;
    rp->status = pasynUser->auxStatus;
    rp->alarmStatus = pasynUser->alarmStatus;
    rp->alarmSeverity = pasynUser->alarmSeverity;
    pPvt->ringHead = (pPvt->ringHead==pPvt->ringSize) ? 0 : pPvt->ringHead+1;
    if (pPvt->ringHead == pPvt->ringTail) {
        pPvt->ringTail = (pPvt->ringTail==pPvt->ringSize) ? 0 : pPvt->ringTail+1;
        pPvt->ringBufferOverflows++;
    } else {
        /* If PACT is true then this callback was received during asynchronous record processing
         * Must defer calling callbackRequest until end of record processing */
        if (pr->pact) {
            pPvt->numDeferredOutputCallbacks++;
        } else { 
            callbackRequest(&pPvt->outputCallback);
        }
    }
    epicsMutexUnlock(pPvt->ringBufferLock);
    dbScanUnlock(pr);
}

static void outputCallbackCallback(CALLBACK *pcb)
{
    static const char *functionName="outputCallbackCallback";

    devPvt *pPvt; 
    callbackGetUser(pPvt, pcb);
    {
        dbCommon *pr = pPvt->pr;
        dbScanLock(pr);
        pPvt->newOutputCallbackValue = 1;
        dbProcess(pr);
        if (pPvt->newOutputCallbackValue != 0) {
            /* We called dbProcess but the record did not process, perhaps because PACT was 1 
             * Need to remove ring buffer element */
            asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR, 
                "%s %s::%s warning dbProcess did not process record, PACT=%d\n", 
                pr->name, driverName, functionName,pr->pact);
            getCallbackValue(pPvt);
            pPvt->newOutputCallbackValue = 0;
        }
        dbScanUnlock(pr);
    }
}

static void interruptCallbackEnumBusy(void *drvPvt, asynUser *pasynUser,
                char *strings[], int values[], int severities[], size_t nElements)
{
    devPvt *pPvt = (devPvt *)drvPvt;
    busyRecord *pr = (busyRecord *)pPvt->pr;

    if (!interruptAccept) return;
    dbScanLock((dbCommon*)pr);
    setEnums((char*)&pr->znam, NULL, &pr->zsv, 
             strings, NULL, severities, nElements, 2);
    db_post_events(pr, &pr->val, DBE_PROPERTY);
    dbScanUnlock((dbCommon*)pr);
}

static int getCallbackValue(devPvt *pPvt)
{
    int ret = 0;
    static const char *functionName="getCallbackValue";

    epicsMutexLock(pPvt->ringBufferLock);
    if (pPvt->ringTail != pPvt->ringHead) {
        if (pPvt->ringBufferOverflows > 0) {
            asynPrint(pPvt->pasynUser, ASYN_TRACE_WARNING,
                "%s %s::%s warning, %d ring buffer overflows\n",
                                    pPvt->pr->name, driverName, functionName, pPvt->ringBufferOverflows);
            pPvt->ringBufferOverflows = 0;
        }
        pPvt->result = pPvt->ringBuffer[pPvt->ringTail];
        pPvt->ringTail = (pPvt->ringTail==pPvt->ringSize) ? 0 : pPvt->ringTail+1;
        asynPrint(pPvt->pasynUser, ASYN_TRACEIO_DEVICE,
            "%s %s::%s from ringBuffer value=%d\n",
                                            pPvt->pr->name, driverName, functionName,pPvt->result.value);
        ret = 1;
    }
    epicsMutexUnlock(pPvt->ringBufferLock);
    return ret;
}

static void reportQueueRequestStatus(devPvt *pPvt, asynStatus status)
{
    static const char *functionName="reportQueueRequestStatus";

    if (status != asynSuccess) pPvt->result.status = status;
    if (pPvt->previousQueueRequestStatus != status) {
        pPvt->previousQueueRequestStatus = status;
        if (status == asynSuccess) {
            asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR,
                "%s %s::%s queueRequest status returned to normal\n", 
                pPvt->pr->name, driverName, functionName);
        } else {
            asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR,
                "%s %s::%s queueRequest error %s\n", 
                pPvt->pr->name, driverName, functionName,pPvt->pasynUser->errorMessage);
        }
    }
}


static long initBusy(busyRecord *pr)
{
    devPvt *pPvt;
    int status;
    epicsInt32 value;

    status = initCommon((dbCommon *)pr,&pr->out,
        processCallbackOutput,interruptCallbackOutput, interruptCallbackEnumBusy,
        2, (char*)&pr->znam, NULL, &pr->zsv);
    if (status != INIT_OK) return status;
    pPvt = pr->dpvt;
    /* Read the current value from the device */
    status = pasynInt32SyncIO->read(pPvt->pasynUserSync,
                      &value, pPvt->pasynUser->timeout);
    if (status == asynSuccess) {
        pr->rval = value;
        return INIT_OK;
    }
    return INIT_DO_NOT_CONVERT;
}

static long processBusy(busyRecord *pr)
{
    devPvt *pPvt = (devPvt *)pr->dpvt;
    int status;

    if(pPvt->newOutputCallbackValue && getCallbackValue(pPvt)) {
        /* We got a callback from the driver */
        if (pPvt->result.status == asynSuccess) {
            pr->rval = pPvt->result.value;
            pr->val = (pr->rval) ? 1 : 0;
            pr->udf = 0;
        }
    } else if(pr->pact == 0) {
        pPvt->result.value = pr->rval;
        if(pPvt->canBlock) pr->pact = 1;
        status = pasynManager->queueRequest(pPvt->pasynUser, 0, 0);
        if((status==asynSuccess) && pPvt->canBlock) return 0;
        if(pPvt->canBlock) pr->pact = 0;
        reportQueueRequestStatus(pPvt, status);
    }
    pasynEpicsUtils->asynStatusToEpicsAlarm(pPvt->result.status, 
                                            WRITE_ALARM, &pPvt->result.alarmStatus,
                                            INVALID_ALARM, &pPvt->result.alarmSeverity);
    (void)recGblSetSevr(pr, pPvt->result.alarmStatus, pPvt->result.alarmSeverity);
    if (pPvt->numDeferredOutputCallbacks > 0) {
        callbackRequest(&pPvt->outputCallback);
        pPvt->numDeferredOutputCallbacks--;
    }
    pPvt->newOutputCallbackValue = 0;
    if(pPvt->result.status == asynSuccess) {
        return 0;
    } else {
        pPvt->result.status = asynSuccess;
        return -1;
    }
}
