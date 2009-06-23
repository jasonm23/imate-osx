/*
 * Copyright (c) 2009 Simon Stapleton. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#ifndef _IOKIT_IOREMOVABLEADBCONTROLLER_H_
#define _IOKIT_IOREMOVABLEADBCONTROLLER_H_

#include <IOKit/IOService.h>
#include <IOKit/adb/IOADBController.h>
#include <IOKit/IOLib.h>
#include <IOKit/pwr_mgt/RootDomain.h>


class RemovableADBController : public IOADBController
  {
    OSDeclareAbstractStructors(RemovableADBController)
    
  public:
    virtual bool init(OSDictionary * properties);
    virtual bool start(IOService * nub);
    virtual bool willTerminate(IOService * nub, IOOptionBits options);
    virtual bool didTerminate(IOService * nub, IOOptionBits options, bool * defer);
    virtual void stop(IOService * nub);
    virtual void free();
    
    virtual IOReturn powerStateWillChangeTo ( IOPMPowerFlags theFlags, unsigned long, IOService*);
    virtual IOReturn powerStateDidChangeTo ( IOPMPowerFlags theFlags, unsigned long, IOService*);
    
    inline IOWorkLoop * workLoop() { return fWorkLoop; };
    inline IOCommandGate * commandGate() { return fCommandGate; };
    
  protected:
    virtual void tearDownADBStack();
    virtual void bringUpADBStack();
    virtual void bringUpPowerManagement(IOService * nub);
    virtual void tearDownPowerManagement();
    
    // Override in subclasses to define allowed power states
    virtual IOPMPowerState * myPowerStates() = 0;
    virtual unsigned long myNumberOfPowerStates() = 0;
    virtual IOReturn setPowerState(unsigned long powerStateOrdinal, IOService * device);
    
    virtual IOReturn incrementOutstandingIO();
    virtual IOReturn decrementOutstandingIO();
    static IOReturn changeOutstandingIOAction(OSObject * owner, void * direction, void *, void *, void *);
    virtual bool shouldDoDeferredShutdown() { return ((_outstandingIO == 0) && (_shuttingDown)); };
    virtual bool shuttingDown() { return _shuttingDown; };
    virtual IOReturn closeDevices() { return kIOReturnSuccess; };
    
  public:
    virtual IOReturn setHandlerID ( ADBDeviceControl * deviceInfo, UInt8 handlerID );
    
  protected:
		// static thread entry method for probing the ADB bus
		static void probeADBBus(thread_call_param_t me, thread_call_param_t arg2);
    
    static IOReturn probeBusAction(OSObject * owner, void * arg0, void * arg1, void * arg2, void * arg3);
    virtual IOReturn probeBus();
    
    virtual void wakeADBDevices();
    virtual bool probeAddress ( IOADBAddress addr );
    virtual bool moveDeviceFrom ( IOADBAddress from, IOADBAddress to, bool check );
    virtual unsigned int firstBit ( unsigned int mask );
    
  protected:
		bool _busProbed;
		thread_call_t _busProbingThread;
    
    bool _shuttingDown;
    UInt32 _outstandingIO;
    IOService * _deferredTerminationNub;
    IOOptionBits _deferredTerminationOptions;
    
    IOWorkLoop			* fWorkLoop;		  // holds the workloop for this driver
    IOCommandGate		* fCommandGate;		// and the command gate
    
  };
#endif