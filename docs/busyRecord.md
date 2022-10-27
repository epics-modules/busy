---
layout: default
title: Busy Record
nav_order: 2
---


The Busy Record
===============

The purpose of the BUSY record is to give EPICS application developers a way to signal the completion of an operation via EPICS' putNotify mechanism (the code that underlies Channel Access' ca\_put\_callback() function.)

putNotify is described in the EPICS Application Developer's Guide (currently, section 5.11); for purposes here, it is essentially an execution trace. When ca\_put\_callback() causes a record to process (i.e., to execute) EPICS takes note of that processing, and any other record processing that results directly from it. "Directly" means that the processing is caused by either a forward link, or a database link with the attribute 'PP'. Only this kind of processing is traced by EPICS. When all traced processing completes, EPICS sends a callback to the source of the original ca\_put\_callback() command.

If all of the processing that results from a ca\_put\_callback() is traced by EPICS, the callback from EPICS is sufficient to indicate completion, and use of the BUSY record is not warranted. But if you want to include some processing that _isn't_ traced by EPICS, the BUSY record is a convenient way to fold it in. If you arrange for a BUSY record to be processed as a traceable result of the ca\_put\_callback(), and cause its VAL field to have the value "Busy" (1), the BUSY record will pretend to be doing something (i.e., it will not execute its forward link) as long as its VAL field remains "Busy". When VAL is set to "Done", and the record is processed, it will execute its forward link, notifying EPICS that it is finished, and EPICS will send the callback.

The write that sets a BUSY record to 1 must be done by either a ca\_put\_callback(), or a PP link. (Note that several records in the synApps package of EPICS-application software have output links that are capable of issuing a ca\_put\_callback(), and that some of them do so via a CA link. Don't be fooled by this. The trick is performed by device support, which calls dbCaPutLinkCallback(), rather than dbPutLink(). dbCaPutLinkCallback() queues a request that eventually results in a ca\_put\_callback().

The write that resets the record to 0 can be done by a channel-access client, or by device support. If it's done by a CA client, it must be done by either a ca\_put() or a CA link.

The BUSY record is essentially a copy of the BO record from EPICS 3.14.8.2, with two modifications: (1)the process routine declines to call recGblFwdLink() when the VAL field has the value 1 ("Busy"); (2) the ZNAM and ONAM strings have the default values "Done", and "Busy", respectively.

For version 1.2 of the BUSY module, Ben Franksen added "Raw Soft Channel" device support, which allows the BUSY record to write a user configurable integer value (the value of the MASK field) via its OUT link, when the record's VAL field has the value 1. Currently, MASK can only be specified at DCT time. For example, you can configure MASK to a bit-pattern and have the BUSY record write this value to a selRecord's SELN field.

The busy module also optionally contains asyn device support. This device support is identical to that for the bo record in standard asyn device support, with one exception. For the bo record the info tag asyn:READBACK causes the device support to register for driver callbacks, and proceses the record with the new value when a callback occurs. For the busy record this behaviour is always enabled, and the asyn:READBACK info tag is not required. The busy record supports the same ring buffer as the bo record, and as with the bo record the default size is 10 and it can be changed with the asyn:FIFO info tag.
