//  Copyright (c) 2009, Simon Stapleton
//  All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without modification, 
//  are permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright notice, this 
//    list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright notice, 
//    this list of conditions and the following disclaimer in the documentation 
//    and/or other materials provided with the distribution.
//  * Neither the name of the Simon Stapleton or any other contributor may 
//    be used to endorse or promote products derived from this software without 
//    specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
//  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
//  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
//  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE 
//  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
//  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
//  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
//  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, 
//  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
//  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef _ADBHIDDEVICE_H_
#define _ADBHIDDEVICE_H_

#include <IOKit/IOService.h>
#include <IOKit/IOLib.h>
#include <IOKit/adb/IOADBDevice.h>
#include <IOKit/hid/IOHIDDevice.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandGate.h>
#include "Debug.h"

class ADBHIDDevice : public IOHIDDevice 
{
  OSDeclareDefaultStructors(ADBHIDDevice)
  
public:
  
  // Uusual lifecycle methods
  virtual bool init(OSDictionary * properties);
  virtual IOService * probe(IOService * nub, SInt32 * score);
  virtual bool handleStart (IOService * nub);
  virtual bool willTerminate (IOService * nub, IOOptionBits options);
  virtual bool didTerminate(IOService * nub, IOOptionBits options, bool * defer);
  virtual void handleStop (IOService * nub);
  virtual void free ();
  
  // power management
  virtual IOReturn setPowerState(unsigned long powerStateOrdinal, IOService * device);
  static IOReturn setPowerStateAction(OSObject * owner, void * arg0, void * arg1, void * arg2, void * arg3);
  // This is the method to override if you need to do specific handling based on the power
  // state you're moving to, it's called in workloop context
  virtual IOReturn setPowerStateGated(unsigned long powerStateOrdinal, IOService * device);
  
  // Override in subclasses to define allowed power states if you need more than "on" and "off"
  virtual IOPMPowerState * myPowerStates();
  virtual unsigned long myNumberOfPowerStates();
  
  // ADB packet handling
  static void adbPacketInterrupt(IOService * target, UInt8 adbCommand, IOByteCount length, UInt8 * adbData);
  static IOReturn adbPacketAction(OSObject * owner, void * arg0, void * arg1, void * arg2, void *);
  
  // Pure virtual adb packet handler.  Clients override this to do their thing.  Always called
  // in workloop context.
  virtual IOReturn handleADBPacket(UInt8 adbCommand, IOByteCount length, UInt8 * adbData) = 0;

  // Workloop, gating, underlying device access
  inline IOWorkLoop * workLoop() { return _workLoop; };
  inline IOCommandGate * commandGate() { return _commandGate; };
  inline IOADBDevice * adbDevice() { return _adbDevice; };
  
  // Hooks for clients to bring up or down the ADB device.
  // Note that the device is brought up only when power management comes up. 
  virtual bool bringUpADBDevice();
  virtual void bringDownADBDevice();

  // HIDDevice stuff
  // You may also want to either implement new<Property><type>, or set stuff in Info.plist
  virtual IOReturn newReportDescriptor(IOMemoryDescriptor **descriptor) const;
  virtual IOReturn setReport(IOMemoryDescriptor *report,IOHIDReportType reportType,IOOptionBits options) { return kIOReturnUnsupported; };
  virtual IOReturn getReport(IOMemoryDescriptor *report,IOHIDReportType reportType,IOOptionBits options) { return kIOReturnUnsupported; };
  virtual OSString * newTransportString() const { return OSString::withCString("ADB"); };
  virtual OSString * newManufacturerString() const;
  virtual OSNumber * newPrimaryUsageNumber() const;
  virtual OSNumber * newPrimaryUsagePageNumber() const;
  virtual OSNumber * newProductIDNumber() const;
  virtual OSString * newProductString() const;
  virtual OSNumber * newVendorIDNumber() const;
  virtual OSNumber * newLocationIDNumber() const;
  
  // Pure virtuals giving access to a block of memory containaing a subclass' 
  // report descriptor
  virtual void * reportDescriptorBytes() const = 0;
  virtual size_t reportDescriptorSize() const = 0;
  
protected:
  IOWorkLoop * _workLoop;
  IOCommandGate * _commandGate;
  IOADBDevice * _adbDevice;
};

#endif