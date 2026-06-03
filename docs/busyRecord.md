---
layout: default
title: Busy Record
nav_order: 2
---


# The Busy Record

## Overview

The busy record gives EPICS application developers a way to signal
the completion of an operation via EPICS' `putNotify` mechanism -- the code
that underlies Channel Access' `ca_put_callback()` function.

### The Problem

When `ca_put_callback()` causes a record to process, EPICS traces that
processing and any other record processing that results directly from it.
"Directly" means processing caused by either a forward link or a database link
with the `PP` attribute. When all traced processing completes, EPICS sends a
callback to the source of the original `ca_put_callback()`.

If all of the processing that results from a `ca_put_callback()` is traced by
EPICS, the callback is sufficient to indicate completion, and the busy record
is not needed. But if you need to include processing that is *not* traced by
EPICS -- for example, work done by a CA monitor callback, a background thread,
or an external program -- the busy record provides a way to fold it in.

### How It Works

1. A `ca_put_callback()` (or a `PP` database link) writes 1 ("Busy") to the
   busy record, causing it to process.
2. The busy record sees that its VAL is 1 and **declines to execute its forward
   link**. This tells EPICS that traced processing is not yet complete.
3. Some external agent (a CA client, device support, or another record) does
   the actual work.
4. When the work is done, the external agent writes 0 ("Done") to the busy
   record.
5. The busy record processes, sees that VAL is 0, executes its forward link,
   and EPICS sends the completion callback.

The write that sets a busy record to 1 must be done by a `ca_put_callback()`
or a `PP` link so that `putNotify` tracing is active. (Note that several
records in the synApps package have output links capable of issuing a
`ca_put_callback()` via device support calling `dbCaPutLinkCallback()` rather
than `dbPutLink()`.)

The write that resets the record to 0 can be done by a Channel Access client
(via `ca_put()` or a CA link) or by device support.


## Differences from the BO Record

The busy record is derived from the EPICS base bo (binary output) record. It
differs in two ways:

1. **Forward link suppression**: The `process()` routine declines to call
   `recGblFwdLink()` when VAL is 1 ("Busy"). This is the core mechanism that
   holds the `putNotify` completion. See [Record Processing](#record-processing)
   for the full logic.

2. **Default enum strings**: ZNAM defaults to "Done" and ONAM defaults to
   "Busy" (the bo record's defaults are empty strings).


## Record Fields

The following tables list fields specific to the busy record. Fields inherited
from `dbCommon` (NAME, DESC, SCAN, PINI, FLNK, etc.) are documented in the
[EPICS Record Reference Manual](https://docs.epics-controls.org/en/latest/guides/EPICS_Process_Database_Concepts.html).

### Value Fields

| Field | Type | Description | Default | Access |
|-------|------|-------------|---------|--------|
| VAL | DBF_ENUM | Current value (0 = "Done", 1 = "Busy"). Writing to this field with `PP` triggers record processing. | 0 | Read/Write |
| RVAL | DBF_ULONG | Raw value. If MASK is non-zero: RVAL = MASK when VAL=1, RVAL = 0 when VAL=0. If MASK is zero: RVAL = VAL. | 0 | Read/Write |
| OVAL | DBF_ULONG | Previous value of VAL, saved at the start of each processing cycle (before the write to device support). Used internally to determine whether to execute the forward link. | 0 | Read-only |
| ORAW | DBF_ULONG | Previous raw value. Used internally by `monitor()` to detect RVAL changes for posting events. | 0 | Read-only |
| RBV | DBF_ULONG | Readback value. Can be set by device support to reflect the actual hardware state. | 0 | Read-only |
| ORBV | DBF_ULONG | Previous readback value. Used internally by `monitor()` to detect RBV changes for posting events. | 0 | Read-only |
| MASK | DBF_ULONG | Hardware mask. When non-zero, RVAL is set to MASK (instead of 1) when VAL=1. Used by "Raw Soft Channel" device support to write a configurable integer through the OUT link. Can only be set at database configuration time (DCT). | 0 | Read-only (runtime) |
| MLST | DBF_USHORT | Last value monitored. Used internally to detect VAL changes for posting events. | 0 | Read-only |
| LALM | DBF_USHORT | Last value alarmed. Used internally for change-of-state alarm detection. | 0 | Read-only |

### Output Fields

| Field | Type | Description | Default | Access |
|-------|------|-------------|---------|--------|
| OUT | DBF_OUTLINK | Output specification. For "Soft Channel" and "Raw Soft Channel" device support, this is the link through which the record writes its value. For "asynInt32" device support, this specifies the asyn port connection (e.g., `@asyn(port, addr, timeout)drvinfo`). | | Read/Write |
| DOL | DBF_INLINK | Desired output location. When OMSL is `closed_loop`, the record reads its desired value from this link before each processing cycle. | | Read/Write |
| OMSL | DBF_MENU | Output mode select (`menuOmsl`). When set to `supervisory`, VAL is set by the caller. When set to `closed_loop`, VAL is read from DOL before processing. | supervisory | Read/Write |

### Timing Fields

| Field | Type | Description | Default | Access |
|-------|------|-------------|---------|--------|
| HIGH | DBF_DOUBLE | Seconds to hold high. When greater than zero and VAL is set to 1, a delayed callback automatically resets VAL to 0 and reprocesses the record after this many seconds. See [Auto-Reset Timer](#auto-reset-timer-high-field). | 0 | Read/Write |

### Display Fields

| Field | Type | Description | Default | Access |
|-------|------|-------------|---------|--------|
| ZNAM | DBF_STRING (26) | Zero name. Display string for the VAL=0 state. | "Done" | Read/Write |
| ONAM | DBF_STRING (26) | One name. Display string for the VAL=1 state. | "Busy" | Read/Write |

### Alarm Fields

| Field | Type | Description | Default | Access |
|-------|------|-------------|---------|--------|
| ZSV | DBF_MENU | Zero error severity (`menuAlarmSevr`). Alarm severity when VAL is 0. | NO_ALARM | Read/Write |
| OSV | DBF_MENU | One error severity (`menuAlarmSevr`). Alarm severity when VAL is 1. | NO_ALARM | Read/Write |
| COSV | DBF_MENU | Change of state severity (`menuAlarmSevr`). Alarm severity on any transition of VAL. | NO_ALARM | Read/Write |

The record checks alarms in this order:

1. **UDF alarm**: If the record has never been initialized (`UDF=TRUE`),
   a `UDF_ALARM` is raised at `INVALID_ALARM` severity (or the `UDFS` field
   severity on EPICS base 3.15.0.2+).
2. **State alarm**: If VAL=0, a `STATE_ALARM` is raised at `ZSV` severity. If
   VAL=1, a `STATE_ALARM` is raised at `OSV` severity. (If the severity is
   `NO_ALARM`, no alarm is raised.)
3. **Change of state alarm**: If VAL differs from the last alarmed value, a
   `COS_ALARM` is raised at `COSV` severity.

### Simulation Fields

| Field | Type | Description | Default | Access |
|-------|------|-------------|---------|--------|
| SIML | DBF_INLINK | Simulation mode location. Link to a record whose value determines whether simulation mode is active. | | Read/Write |
| SIMM | DBF_MENU | Simulation mode (`menuYesNo`). When `YES`, the record writes through SIOL instead of calling device support. | NO | Read/Write |
| SIOL | DBF_OUTLINK | Simulation output specification. The link used for output when simulation mode is active. | | Read/Write |
| SIMS | DBF_MENU | Simulation mode alarm severity (`menuAlarmSevr`). Alarm severity applied when in simulation mode. | NO_ALARM | Read/Write |

### Invalid Output Action Fields

| Field | Type | Description | Default | Access |
|-------|------|-------------|---------|--------|
| IVOA | DBF_MENU | Invalid output action (`menuIvoa`). Controls what happens when the record has an `INVALID` alarm severity and needs to write an output. | `Continue normally` | Read/Write |
| IVOV | DBF_USHORT | Invalid output value. The value to write when IVOA is `Set output to IVOV`. | 0 | Read/Write |

The three IVOA modes are:

- **Continue normally**: Write the output as usual despite the invalid alarm.
- **Don't drive outputs**: Skip the write entirely.
- **Set output to IVOV**: Override VAL with IVOV, convert to RVAL, and write
  that value instead.


## Record Processing

### Forward Link Behavior

The key behavior that distinguishes the busy record from a bo record is in the
forward link logic at the end of `process()`. After writing the value and
posting monitors, the record evaluates:

```c
if ((prec->val == 0) || (prec->oval == 0))
    recGblFwdLink(prec);
```

The OVAL field captures the value of VAL at the *start* of the processing
cycle, before the write to device support. This means:

- **VAL=1, OVAL=1**: Neither is zero. Forward link is **not** executed.
  The record is "holding" -- `putNotify` remains pending.
- **VAL=0, OVAL=0**: VAL is zero. Forward link **is** executed.
  Normal completion.
- **VAL=0, OVAL=1**: VAL is zero. Forward link **is** executed.
  The record was busy and has now completed.
- **VAL=1, OVAL=0**: OVAL is zero. Forward link **is** executed.
  This case arises when asynchronous device support sets `.RPRO` (reprocess
  request) while the record was processing a previous write. The forward link
  must execute to ensure `putNotify` tracing continues and the record is
  reprocessed with the new value.

### Auto-Reset Timer (HIGH Field)

When VAL is set to 1 and the HIGH field is greater than zero, the record
automatically schedules a delayed callback to reset itself after HIGH seconds:

1. At the end of `process()`, if `VAL=1` and `HIGH>0`, a delayed callback is
   requested at the record's priority level, with a delay of HIGH seconds.
2. When the callback fires:
   - If PACT is true (the record is still being processed asynchronously) and
     VAL is still 1 and HIGH is still > 0, the timer re-arms itself for another
     HIGH seconds.
   - If PACT is false, the callback sets VAL to 0 and calls `dbProcess()`,
     which causes the record to process and execute its forward link.

This feature is useful for cases where you want a busy record to automatically
clear itself after a timeout, rather than relying on an external agent to
reset it.


## Device Support

The busy record supports three device support types. The DTYP field selects
which one is used.

### Soft Channel

**DTYP**: `"Soft Channel"`

The default device support. On each write, it puts the value of VAL through
the OUT link as `DBR_USHORT`:

- VAL=0 writes 0
- VAL=1 writes 1

This is useful when the OUT link points to another record that should receive
the busy/done state directly, or when no output link is needed at all (OUT is
left empty and the record is used purely for its `putNotify`-holding behavior).

### Raw Soft Channel

**DTYP**: `"Raw Soft Channel"`

Similar to Soft Channel, but writes RVAL (the raw value) through the OUT link
as `DBR_LONG`:

- VAL=0 writes RVAL, which is 0
- VAL=1 writes RVAL, which is the value of MASK (or 1 if MASK is 0)

This is useful when you want the busy record to write a configurable integer
value rather than just 0 or 1. For example, you can set MASK to a bit pattern
and have the busy record write that value to a `sel` record's SELN field.

MASK can only be configured at database configuration time (it has the
`SPC_NOMOD` special attribute and cannot be changed at runtime via Channel
Access).

### asynInt32

**DTYP**: `"asynInt32"`

Asyn device support for use with asyn port drivers. This device support is
based on the bo record support in `devAsynInt32.c` from asyn R4-33. It uses
the `asynInt32` interface to write values to and receive callbacks from an asyn
driver.

**OUT link format**:

```
field(OUT, "@asyn(port, addr, timeout)drvinfo")
```

Where `port` is the asyn port name, `addr` is the address (often 0), `timeout`
is the I/O timeout in seconds, and `drvinfo` is the driver-specific parameter
string.

**Key differences from bo record asyn device support**:

- **Readback callbacks are always enabled**. The bo record requires the
  `asyn:READBACK` info tag to enable driver callback registration. The busy
  record always registers for callbacks, so the `asyn:READBACK` info tag is
  not needed and has no effect.
- When a driver callback occurs, the record is processed with the new value
  from the driver, updating VAL and RVAL accordingly.

**Ring buffer**: Driver callbacks are stored in a ring buffer to prevent data
loss when callbacks arrive faster than the record can process them. The default
ring buffer size is 10. This can be changed with the `asyn:FIFO` info tag:

```
record(busy, "$(P)Busy") {
    field(DTYP, "asynInt32")
    field(OUT,  "@asyn($(PORT),$(ADDR),$(TIMEOUT))BUSY_VALUE")
    info(asyn:FIFO, "20")
}
```

**Synchronous vs. asynchronous operation**: The device support queries the asyn
port's `canBlock` flag. If the port can block (`ASYN_CANBLOCK` is set), writes
are queued asynchronously -- PACT is set to 1, the write happens in a callback
thread, and the record is processed again when the write completes. If the port
cannot block, writes happen synchronously within `process()`.


## Common Use Cases

### Basic putNotify Signaling

The simplest use of the busy record: a Channel Access client writes 1 to
start an operation and an external agent writes 0 when it is done. The client
uses `ca_put_callback()` to be notified of completion.

```
record(busy, "$(P)MyBusy") {
    field(ZNAM, "Done")
    field(ONAM, "Busy")
}
```

The client side (pseudocode):

```
ca_put_callback(busy_pv, 1, my_done_callback)
# ... my_done_callback fires when some external code writes 0 to the record
```

A Python example using PyEpics is provided in
[busyExample.py](https://github.com/epics-modules/busy/blob/master/docs/busyExample.py).
It demonstrates a client that monitors a busy record and spawns a thread to
step a motor and read a detector when the record becomes busy.

### The bo + busy + calcout Pattern (No Asyn Driver)

This pattern uses standard EPICS database logic to coordinate a busy record
with a bo "acquire" record, without requiring an asyn port driver. It is useful
when the operation being tracked is managed by other EPICS records or by a CA
client that monitors the acquire record.

```
# The user-facing acquire control.
# Writing 1 here starts the operation.
record(bo, "$(P)Acquire") {
    field(ZNAM, "Done")
    field(ONAM, "Acquire")
    field(OUT,  "$(P)Acquire_RBV PP")
    field(FLNK, "$(P)SetBusy")
}

# Readback of the acquire state.
record(bi, "$(P)Acquire_RBV") {
    field(ZNAM, "Done")
    field(ONAM, "Acquire")
}

# Sets the busy record to 1 on a 0->1 transition of Acquire.
record(calcout, "$(P)SetBusy") {
    field(INPA, "$(P)Acquire NPP")
    field(CALC, "A")
    field(OOPT, "Transition To Non-zero")
    field(OUT,  "$(P)Busy PP")
    field(FLNK, "$(P)Reset")
}

# The busy record that holds the putNotify completion.
record(busy, "$(P)Busy") {
    field(ZNAM, "Done")
    field(ONAM, "Busy")
}

# After a 1-second delay, resets the Acquire readback to 0.
# Only runs when Busy is not 0 (SDIS/DISV).
record(seq, "$(P)Reset") {
    field(SDIS, "$(P)Busy")
    field(DISV, "0")
    field(DLY0, "1")
    field(LNK0, "$(P)Acquire_RBV PP")
    field(DOL0, "0")
}

# Clears the busy record when Acquire_RBV transitions to 0.
record(calcout, "$(P)ClearBusy") {
    field(INPA, "$(P)Acquire_RBV CP")
    field(CALC, "A")
    field(OOPT, "Transition To Zero")
    field(OUT,  "$(P)Busy PP")
}
```

How it works:

1. A `ca_put_callback()` writes 1 to `Acquire`.
2. `Acquire` writes 1 to `Acquire_RBV` via its OUT link, then forward-links
   to `SetBusy`.
3. `SetBusy` detects the 0-to-1 transition and writes 1 to `Busy` via a PP
   link. The busy record is now holding `putNotify` completion.
4. `SetBusy` forward-links to `Reset`. The `Reset` seq record waits 1 second,
   then writes 0 to `Acquire_RBV`. (It is disabled when `Busy` is already 0,
   preventing it from firing when it shouldn't.)
5. `ClearBusy` monitors `Acquire_RBV` via a CP link. When `Acquire_RBV`
   transitions to 0, `ClearBusy` writes 0 to `Busy`.
6. The busy record processes with VAL=0, executes its forward link, and
   `putNotify` sends the completion callback to the client.

### Using busy with an Asyn Port Driver

When using the busy record with an asyn port driver, the driver manages the
busy/done lifecycle through callbacks:

```
record(busy, "$(P)Busy") {
    field(DTYP, "asynInt32")
    field(OUT,  "@asyn($(PORT),$(ADDR),$(TIMEOUT))BUSY_VALUE")
    field(ZNAM, "Done")
    field(ONAM, "Busy")
}
```

The typical sequence:

1. A client writes 1 to the busy record via `ca_put_callback()`.
2. The record writes the value to the asyn driver via the `asynInt32` write
   method.
3. The driver starts the requested operation (e.g., an acquisition).
4. When the operation completes, the driver does a callback with value 0.
5. The device support receives the callback, updates VAL to 0, and processes
   the record. The record executes its forward link and `putNotify` sends the
   completion callback.

If you need both a user-facing bo record (for the control interface) and a
busy record (for `putNotify` holding), you can combine them using the calcout
pattern shown above, or use a bo record with the `asyn:READBACK` info tag
paired with a separate busy record. The test database `testBusyAsyn2.db` in
the source tree demonstrates this approach.
