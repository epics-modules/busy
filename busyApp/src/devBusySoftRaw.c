#include <dbAccess.h>
#include <recGbl.h>
#include <recSup.h>
#include <devSup.h>
#include <busyRecord.h>
#include <epicsExport.h>

static long init_record();
static long write();

struct {
   long 	   number;
   DEVSUPFUN   report;
   DEVSUPFUN   init;
   DEVSUPFUN   init_record;
   DEVSUPFUN   get_ioint_info;
   DEVSUPFUN   write;
}devBusySoftRaw={
   5,
   NULL,
   NULL,
   init_record,
   NULL,
   write
};
epicsExportAddress(dset,devBusySoftRaw);

static long init_record(busyRecord *pbusy)
{
	return 2; /* dont convert */
}

static long write(busyRecord *pbusy)
{
	return dbPutLink(&pbusy->out,DBR_LONG,&pbusy->rval,1);
}
