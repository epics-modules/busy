#!/usr/bin/env python

"""
demonstration of client use of the EPICS busy record

Uses these records from a running synApps XXX (style) IOC:

======  =================  ==================
record  PV used here       URL
======  =================  ==================
busy    ``xxx:mybusy``     https://github.com/epics-modules/busy
motor   ``xxx:m1``         https://github.com/epics-modules/motor
swait   ``xxx:userCalc1``  https://github.com/epics-modules/calc
======  =================  ==================

Description:

#. Run this python program in a command shell.
#. From a GUI or other client do these things:
#. set the busy record.VAL to 1
#. watch the motor record .VAL, .RBV, and .DMOV fields
#. watch the swait record .CALC and .VAL fields
#. wait for busy record to return to 0
#. To end the python program orderly:
#. set the swait.CALC field to 0 (changes the calculation)
#. set the swait.PROC field to 1 (recalculate a new value)
#. To end the python program abruptly, press ^C in the terminal

EXAMPLE::

    user@localhost ~ $ ./busyExample.py 
    INFO:root:starting the process in a thread
    INFO:root:process() start
    (0.0, 0.1270313572899977)
    (2.1, 0.9285114824139773)
    (4.2, 0.578789959563592)
    (6.3, 0.46387426565957124)
    (8.4, 0.30080109864957655)
    INFO:root:process() complete
    INFO:root:starting the process in a thread
    INFO:root:process() start
    (0.0, 0.3134508278019379)
    (2.1, 0.7051956969558252)
    (4.2, 0.2409094377050431)
    INFO:root:process() interrupted
    INFO:root:process() complete
    INFO:root:starting the process in a thread
    INFO:root:process() start
    (0.0, 0.5009384298466468)
    (2.1, 0.11929503318837263)
    (4.2, 0.06889448386358435)
    (6.3, 0.8060425726710918)
    (8.4, 0.8368200198367285)
    INFO:root:process() complete


"""


import epics        # http://cars9.uchicago.edu/software/python/pyepics3
import logging
import threading

logging.basicConfig(level=logging.INFO) 

PREFIX = "xxx:"     # xxx: is from a synApps xxx IOC
BUSY_PV = PREFIX + "mybusy"     # busy record
MOTOR_PV = PREFIX + "m1"        # motor record
CALC_PV = PREFIX + "userCalc1"  # swait record


class Demonstrator(object):
    
    def __init__(self, busy_pv_name, motor_pv_name, calc_pv_name):
        self.busy = epics.PV(busy_pv_name)
        
        # motor: the independent variable
        self.motor = epics.Motor(motor_pv_name)
        
        # calc: the detector signal
        self.calc = epics.PV(calc_pv_name)
        self.calc_proc = epics.PV(calc_pv_name + ".PROC")
        epics.caput(calc_pv_name+".CALC", "RNDM")   # report random numbers
        self.calc_proc.put(1)   # get a new value

        self.cb_index = None
        self.processing = False
        
        # arbitrary choices for a demo program
        self.num_steps = 5
        self.step_size = 2.1
        self.origin = 0

    def monitor(self):
        if self.cb_index is None:
            self.cb_index = self.busy.add_callback(self.busy_callback)

    def unmonitor(self):
        if self.cb_index is not None:
            self.busy.remove_callback(self.cb_index)
            self.cb_index = None

    def busy_callback(self, *args, **kwargs):
        if not self.processing and self.busy.value == 1:
            logging.info("starting the process in a thread")
            thread = threading.Thread(target=self.process)
            thread.start()
        # else:
        #     for k, v in sorted(kwargs.items()):
        #         print(k, v)

    def process(self):
        if self.processing:
            return
        logging.info("process() start")
        self.processing = True
        self.motor.move(self.origin, wait=True)
        for step_number in range(self.num_steps):
            if self.busy.value != 1:
                logging.info("process() interrupted")
                break
            target = self.origin + step_number * self.step_size
            self.motor.move(target, wait=True)
            self.calc_proc.put(1)   # get a new value
            print(self.motor.readback, self.calc.value)
        self.busy.put(0)
        self.processing = False
        logging.info("process() complete")


def main():
    process = Demonstrator(BUSY_PV, MOTOR_PV, CALC_PV)
    process.monitor()
    while process.calc.value != 0:
        pass
    process.unmonitor()     # not really needed, we're quitting anyway


if __name__ == "__main__":
    main()
