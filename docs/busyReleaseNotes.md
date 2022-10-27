<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN"
        "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xml:lang="en" xmlns="http://www.w3.org/1999/xhtml">
<head>
  <title>busyReleaseNotes</title>
  <meta content="text/html; charset=ISO-8859-1" http-equiv="Content-Type" />
  <style type="text/css">
h1 {text-align:center}
h2 {text-align:center}
</style>
</head>
<body>
  <h1>
    busy Release Notes</h1>

  <h2>
	Release 1-7-3</h2>
	<p>
	  Added bob files, updated edl and ui files.
	
  <h2>
	Release 1-7-2</h2>
	<p>
	  Req files installed to top level db folder
	
  <h2>
	Release 1-7-1</h2>
	<p>
	  Fixes to build with EPICS v7
	<p>
	  Cleaned up Documentation
	
  <h2>
    Release 1-7</h2>
  <p>
    Fix to asyn device support. There were 2 problems:</p>
  <ul>
    <li>Problem 1
      <ul>
        <li>Busy record has a value of 1 autosave file. (It is not sufficient to have 1 in
          the .db file, because device support reads the value from the driver, and that will
          be 0.</li>
        <li>Driver does an initial callback with value 0 as soon as iocInit completes, i.e.
          when interruptAccept=1.</li>
        <li>Driver gets sent the value 1, but the record has the value 0.</li>
        <li>This happens because the callback with value 0 occurs after the driver has queued
          the request to send 1 but before the value is written to the driver. The 0 to 1
          callback which happens later is ignored by device support.</li>
        <li>This occurs in devBusyAsyn.c R1-6-1 and earlier.</li>
      </ul>
    </li>
    <li>Problem 2
      <ul>
        <li>Driver is configured for asynchronous device support (ASYN_CANBLOCK=1)</li>
        <li>Busy record sends a value of 1 to driver.</li>
        <li>Driver immediately does a callback with a value of 0.</li>
        <li>Busy record is stuck in the 1 state, it ignores the callback value of 0 because
          PACT=1 when callback occurs.</li>
        <li>This problem was introduced in devBusyAsyn.c commit a04e121da0acd71226fbe722fbf1b9551b21fe00
          which was an attempt to fix problem 1 above. There was no released version of the
          busy module with this bug.</li>
      </ul>
    </li>
  </ul>
  <p>
    A test application to demonstrate these problems was added to the busy module. It
    allows testing all combinations of the following 6 settings:</p>
  <ol>
    <li>Synchronous driver, i.e. ASYN_CANBLOCK not set.</li>
    <li>Asynchronous driver, i.e. ASYN_CANBLOCK is set.</li>
    <li>Driver callback is done in the write() operation.</li>
    <li>Driver callback is done in a separate thread. The callbacks are triggered with
      the TriggerCallbacks record.</li>
    <li>Single callback is done in each operation.</li>
    <li>Multiple callbacks are done in each operation. The NumCallbacks record selects
      the number of callbacks.</li>
  </ol>
  <p>
    The callback value is the logical inverse of the current record value. This means
    that if 1 is written to the record the callback value will be 0, and the record
    value will immediately change to 0. For the triggered callbacks the values will
    toggle between 0 and 1. The following is a screen shot of the test application:</p>
  <p style="text-align: center">
    <img alt="testBusyAsyn.png" src="testBusyAsyn.png" /></p>
  <p>
    The test application requires autosave, so busy is now optionally dependent on autosave.</p>
  <p>
    This release changes the interrupt callback logic. Previously it was directly calling
    monitor() and recGblFwdLink and not actually processing the record. It was hard
    to get the logic right, and the 2 previous versions of this support had bugs discussed
    above. The proper solution is to process the record on each callback.</p>
  <p>
    This version is based directly on the code for the bo record in devAsynInt32.c in
    asyn R4-33. The bo record was just changed to the busy record, and the logic is
    the same as that for bo records with the asyn:READBACK info tag. It processes the
    record on each callback from the driver, and distinguishes between record processing
    due to driver callbacks (in which case the driver must not be called) and normal
    record processing (in which case the driver must be called).</p>
  <p>
    This fix also makes the record have correct timestamps when callbacks occur, which
    was not the case with the previous versions.</p>
  <p>
    Mark Rivers is responsible for these changes.</p>
  <h2>
    Release 1-6-1</h2>
  <p>
    Build failed on Windows because comment character not at beginning of line.</p>
  <h2>
    Release 1-6</h2>
  <p>
    Fixed a bug when the using asynchronous device support, for example devBusyAsyn
    with an asyn port driver with ASYN_CANBLOCK. The problem occurred when 0 was written
    to the record, followed immediately by writing 1. If the record was still processing
    with the 0 value when the 1 was written, device support would not process a second
    time with the 1 value. It should have processed a second time with the 1 value (via
    the EPICS .RPRO field), but this was not happening.
  </p>
  <h2>
    Release 1-5</h2>
  <p>
    configure/CONFIG no longer defines STATIC_BUILD=YES on any platform.</p>
  <p>
    Added caQtDM display.</p>
  <h2>
    Release 1-4</h2>
  <p>
    Added autosave-request file busyRecord_settings.req.</p>
  <p>
    RELEASE* changes</p>
  <p>
    Added .opi display file for CSS-BOY</p>
  <h2>
    Release 1-3</h2>
  <p>
    Added busyRecord.db database, xxBusyRecord.adl medm-display file.</p>
  <p>
    busyRecord.c rewritten to minimize differences from 3.14.11 bo record.</p>
  <h2>
    Release 1-2</h2>
  <p>
    Ben Franksen's "Raw Soft Channel" device support allows the busy record to write
    a user configurable integer value (the value of the MASK field) via its OUT link,
    when the record's VAL field has the value 1. Currently, MASK can only be specified
    at DCT time. For example, you can configure MASK to a bit-pattern and have the BUSY
    record write this value to a selRecord's SELN field.</p>
  <h2>
    Release 1-1</h2>
  <p>
    The busy record was reimplemented to permit device support to reset the VAL field
    to zero.</p>
  <p>
    This version is intended to build with EPICS base 3.14.10.</p>
  <h2>
    Release 1-0</h2>
  <p>
    This is the first release of the synApps busy module.</p>
  <p>
    This version is intended to build with EPICS base 3.14.10.</p>
  <address>
    Suggestions and Comments to:
    <br />
    <a href="mailto:mooney@aps.anl.gov">Tim Mooney </a>: (mooney@aps.anl.gov)
    <br />
    Last modified: Nov 14, 2017
  </address>
</body>
</html>
