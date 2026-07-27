#include <dbAccess.h>
#include <recGbl.h>
#include <recSup.h>
#include <devSup.h>
#include <busyRecord.h>
#include <epicsExport.h>

static long init_record(struct dbCommon *pcommon);
static long write_busy(busyRecord *pbusy);

busydset devBusySoftRaw = {
    {5, NULL, NULL, init_record, NULL},
    write_busy
};
epicsExportAddress(dset, devBusySoftRaw);

static long init_record(struct dbCommon *pcommon)
{
    (void)pcommon;
    return 2; /* dont convert */
}

static long write_busy(busyRecord *pbusy)
{
    return dbPutLink(&pbusy->out, DBR_LONG, &pbusy->rval, 1);
}
