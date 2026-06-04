---
layout: default
title: Home
nav_order: 1
---


# synApps: busy
{: .no_toc}

## Table of contents
{: .no_toc .text-delta}

- TOC
{:toc}

The **busy** module contains the busy record, which allows EPICS Channel Access
clients to signal the completion of an operation in a way that works with the
EPICS `putNotify`/`ca_put_callback()` mechanism.

When `ca_put_callback()` is used to process a record, EPICS traces all
directly-resulting record processing and sends a callback when it completes.
However, if some of the work you need to wait for is *not* traced by EPICS
(e.g., it is triggered by a CA monitor, a separate thread, or an external
process), the busy record provides a way to hold the callback until that
external work is done.

GitHub repository:
[https://github.com/epics-modules/busy](https://github.com/epics-modules/busy)

Required Modules
----------------

| Module | Required? | Notes |
| - | - | - |
| [EPICS Base](https://epics-controls.org/) | Yes | 3.15+ or 7.0+ |
| [asyn](https://github.com/epics-modules/asyn) | Optional | Needed for `asynInt32` device support. The module builds without it; only soft-channel device support will be available. |
| [autosave](https://github.com/epics-modules/autosave) | Optional | Only needed for the included test IOC (`testBusyAsyn`). Not required for normal use. |

Installation and Building
--------------------------

1. Clone the repository (or download a release archive):

   ```bash
   git clone https://github.com/epics-modules/busy.git
   ```

2. Edit `configure/RELEASE` to set the paths to your installations of
   EPICS base, and optionally asyn and autosave:

   ```makefile
   SUPPORT=/path/to/support

   # Optional: needed for asyn device support
   ASYN=$(SUPPORT)/asyn-R4-44-2

   # Optional: needed for the test IOC only
   AUTOSAVE=$(SUPPORT)/autosave-R5-11

   EPICS_BASE=/path/to/base-7.0.8.1
   ```

3. Build:

   ```bash
   make
   ```

4. To use the busy record in your IOC application, add the following to your
   application's `configure/RELEASE`:

   ```makefile
   BUSY=/path/to/busy
   ```

   And in your application's `src/Makefile`, add:

   ```makefile
   myApp_DBD  += busySupport.dbd
   myApp_LIBS += busy
   ```

Suggestions and Comments to:
[Keenan Lang](mailto:klang@anl.gov) : (klang@anl.gov)
