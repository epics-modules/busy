---
layout: default
title: Busy Record
nav_order: 2
---


# The Busy Record
{: .no_toc}

## Table of contents
{: .no_toc .text-delta}

- TOC
{:toc}

Overview
--------

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

{: .important}
> The write that sets a busy record to 1 must be done by a
> `ca_put_callback()` or a `PP` link so that `putNotify` tracing is active.
> Note that several records in the synApps package have output links capable
> of issuing a `ca_put_callback()` via device support calling
> `dbCaPutLinkCallback()` rather than `dbPutLink()`.

The write that resets the record to 0 can be done by a Channel Access client
(via `ca_put()` or a CA link) or by device support.


Differences from the BO Record
-------------------------------

The busy record is derived from the EPICS base bo (binary output) record. It
differs in two ways:

1. **Forward link suppression**: The process routine declines to call the
   forward link when VAL is 1 ("Busy"). This is the core mechanism that holds
   the `putNotify` completion. See [Record Processing](#record-processing) for
   the full logic.

2. **Default enum strings**: ZNAM defaults to "Done" and ONAM defaults to
   "Busy" (the bo record's defaults are empty strings).


Record Fields
-------------

The following tables list fields specific to the busy record.

See [Fields Common to All Record Types](https://docs.epics-controls.org/projects/base/en/latest/dbCommonRecord.html)
for fields such as NAME, DESC, SCAN, PINI, FLNK, PACT, and UDF.

See [Fields Common to Output Record Types](https://docs.epics-controls.org/projects/base/en/latest/dbCommonOutput.html)
for additional common output record fields.

### Value Fields

| Field | Summary | Type | Default | Access | CA PP |
| - | - | - | - | - | - |
| VAL | Current value | ENUM | 0 | R/W | Yes |
| RVAL | Raw value | ULONG | 0 | R/W | Yes |
| OVAL | Previous value | ULONG | 0 | R | No |
| ORAW | Previous raw value | ULONG | 0 | R | No |
| RBV | Readback value | ULONG | 0 | R | No |
| ORBV | Previous readback value | ULONG | 0 | R | No |
| MASK | Hardware mask | ULONG | 0 | R | No |
| MLST | Last value monitored | USHORT | 0 | R | No |
| LALM | Last value alarmed | USHORT | 0 | R | No |

**VAL** is the current value of the record: 0 means "Done" and 1 means "Busy".
Writing to VAL via Channel Access with the `PP` attribute causes the record to
process.

**RVAL** is the raw value written through the OUT link. If **MASK** is
non-zero, RVAL is set to the value of MASK when VAL=1, and to 0 when VAL=0.
If MASK is zero, RVAL is simply cast from VAL.

**OVAL** stores the value of VAL from the start of the current processing
cycle (before the write to device support). It is used internally to determine
whether the forward link should be executed. See
[Forward Link Behavior](#forward-link-behavior).

**RBV** is the readback value. It can be set by device support to reflect the
actual state of the hardware.

**MASK** is the hardware mask. It is used by "Raw Soft Channel" device support
to write a configurable integer value through the OUT link instead of just 0
or 1. MASK can only be set at database configuration time (DCT) -- it cannot
be changed at runtime.

**MLST**, **LALM**, **ORAW**, and **ORBV** are internal bookkeeping fields
used for monitor posting and alarm detection.

### Output Fields

| Field | Summary | Type | Default | Access | CA PP |
| - | - | - | - | - | - |
| OUT | Output specification | OUTLINK | | R/W | No |
| DOL | Desired output location | INLINK | | R/W | No |
| OMSL | Output mode select | MENU [menuOmsl] | supervisory | R/W | No |

**OUT** specifies where the record writes its value. For "Soft Channel" and
"Raw Soft Channel" device support, this is a database or channel access link.
For "asynInt32" device support, this specifies the asyn port connection
(e.g., `@asyn(port, addr, timeout)drvinfo`).

**DOL** and **OMSL** control closed-loop operation. When OMSL is set to
`closed_loop`, the record reads its desired value from the DOL link before
each processing cycle instead of using the value written by the caller.

### Timing Fields

| Field | Summary | Type | Default | Access | CA PP |
| - | - | - | - | - | - |
| HIGH | Seconds to hold high | DOUBLE | 0 | R/W | No |

When **HIGH** is greater than zero and VAL is set to 1, the record
automatically resets VAL to 0 and reprocesses itself after HIGH seconds. See
[Auto-Reset Timer](#auto-reset-timer-high-field).

### Display Fields

| Field | Summary | Type | Default | Access | CA PP |
| - | - | - | - | - | - |
| ZNAM | Zero name | STRING [26] | "Done" | R/W | Yes |
| ONAM | One name | STRING [26] | "Busy" | R/W | Yes |

**ZNAM** and **ONAM** are the display strings for the VAL=0 and VAL=1 states,
respectively. These default to "Done" and "Busy", unlike the bo record where
they default to empty strings.

### Alarm Fields

| Field | Summary | Type | Default | Access | CA PP |
| - | - | - | - | - | - |
| ZSV | Zero error severity | MENU [menuAlarmSevr] | NO_ALARM | R/W | Yes |
| OSV | One error severity | MENU [menuAlarmSevr] | NO_ALARM | R/W | Yes |
| COSV | Change of state severity | MENU [menuAlarmSevr] | NO_ALARM | R/W | Yes |

The record checks alarms in this order:

1. **UDF alarm**: If the record has never been initialized (UDF=TRUE), a
   UDF_ALARM is raised at INVALID_ALARM severity (or the UDFS field severity
   on EPICS base 3.15.0.2+).
2. **State alarm**: If VAL=0, a STATE_ALARM is raised at **ZSV** severity. If
   VAL=1, a STATE_ALARM is raised at **OSV** severity. (If the severity is
   NO_ALARM, no alarm is raised.)
3. **Change of state alarm**: If VAL differs from the last alarmed value, a
   COS_ALARM is raised at **COSV** severity.

### Simulation Fields

| Field | Summary | Type | Default | Access | CA PP |
| - | - | - | - | - | - |
| SIML | Simulation mode location | INLINK | | R/W | No |
| SIMM | Simulation mode | MENU [menuYesNo] | NO | R/W | No |
| SIOL | Simulation output specification | OUTLINK | | R/W | No |
| SIMS | Simulation mode alarm severity | MENU [menuAlarmSevr] | NO_ALARM | R/W | No |

When **SIMM** is YES (set directly or read from the link in **SIML**), the
record writes its value through the **SIOL** link instead of calling device
support, and raises a SIMM_ALARM at **SIMS** severity.

### Invalid Output Action Fields

| Field | Summary | Type | Default | Access | CA PP |
| - | - | - | - | - | - |
| IVOA | Invalid output action | MENU [menuIvoa] | Continue normally | R/W | No |
| IVOV | Invalid output value | USHORT | 0 | R/W | No |

**IVOA** controls what happens when the record has INVALID alarm severity and
needs to write an output:

| Choice | Description |
| - | - |
| Continue normally | Write the output as usual despite the invalid alarm. |
| Don't drive outputs | Skip the write entirely. |
| Set output to IVOV | Override VAL with the value of **IVOV**, convert to RVAL, and write that value instead. |


Record Processing
------------------

### Forward Link Behavior

The key behavior that distinguishes the busy record from a bo record is in the
forward link logic at the end of processing. After writing the value and
posting monitors, the record decides whether to execute its forward link based
on both the current value (VAL) and the value at the start of the processing
cycle (OVAL):

- **VAL=1, OVAL=1**: The record is "holding" -- the forward link is **not**
  executed and `putNotify` remains pending.
- **VAL=0, OVAL=0**: Normal idle state -- the forward link **is** executed.
- **VAL=0, OVAL=1**: The record was busy and has now completed -- the forward
  link **is** executed.
- **VAL=1, OVAL=0**: This case arises when the record receives a new write
  while still processing a previous one (via the RPRO reprocess mechanism). The
  forward link **is** executed to ensure `putNotify` tracing continues and the
  record is reprocessed with the new value.

In short, the forward link is suppressed only when both VAL and OVAL are 1.

### Auto-Reset Timer (HIGH Field)

When VAL is set to 1 and the HIGH field is greater than zero, the record
automatically schedules a delayed callback to reset itself:

1. At the end of processing, if VAL=1 and HIGH > 0, a delayed callback is
   requested at the record's priority level, with a delay of HIGH seconds.
2. When the callback fires:
   - If the record is still actively processing (PACT=TRUE) and VAL is still 1
     and HIGH is still > 0, the timer re-arms itself for another HIGH seconds.
   - If the record is not actively processing, the callback sets VAL to 0 and
     processes the record, which executes the forward link.

{: .note}
> The HIGH field auto-reset is useful when you want a busy record to
> automatically clear itself after a timeout, rather than relying on an
> external agent to reset it.


Device Support
--------------

The busy record supports three device support types. The DTYP field selects
which one is used.

### Soft Channel

**DTYP**: `"Soft Channel"`

The default device support. On each write, it puts the value of VAL through
the OUT link as DBR_USHORT:

- VAL=0 writes 0
- VAL=1 writes 1

This is useful when the OUT link points to another record that should receive
the busy/done state directly, or when no output link is needed at all (OUT is
left empty and the record is used purely for its `putNotify`-holding behavior).

### Raw Soft Channel

**DTYP**: `"Raw Soft Channel"`

Similar to Soft Channel, but writes RVAL (the raw value) through the OUT link
as DBR_LONG:

- VAL=0 writes RVAL, which is 0
- VAL=1 writes RVAL, which is the value of MASK (or 1 if MASK is 0)

This is useful when you want the busy record to write a configurable integer
value rather than just 0 or 1. For example, you can set MASK to a bit pattern
and have the busy record write that value to a `sel` record's SELN field.

{: .note}
> MASK can only be configured at database configuration time (it has the
> SPC_NOMOD special attribute and cannot be changed at runtime via Channel
> Access).

### asynInt32

**DTYP**: `"asynInt32"`

Asyn device support for use with asyn port drivers. This device support is
based on the bo record support in `devAsynInt32.c` from asyn R4-33. It uses
the asynInt32 interface to write values to and receive callbacks from an asyn
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
port's `canBlock` flag. If the port can block (ASYN_CANBLOCK is set), writes
are queued asynchronously -- PACT is set to 1, the write happens in a callback
thread, and the record is processed again when the write completes. If the port
cannot block, writes happen synchronously within the process routine.


Common Use Cases
----------------

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
2. The record writes the value to the asyn driver via the asynInt32 write
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
