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

#include "ADBHIDDevice.h"

#define SUPER IOHIDDevice
#define KLASS ADBHIDDevice

#define kADBHIDDeviceNewHandlerKey "newHandler"

OSDefineMetaClass(KLASS, SUPER)
OSDefineAbstractStructors(KLASS, SUPER)

enum {
  kADBHIDPowerStateOff = 0,
  kADBHIDPowerStateOn = 1,
  kADBHIDNumberPowerstates = 2
};

static IOPMPowerState _myPowerStates[kADBHIDNumberPowerstates] = {
{kIOPMPowerStateVersion1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  // Power Off
{kIOPMPowerStateVersion1, kIOPMDeviceUsable, kIOPMPowerOn, kIOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0}, 
};

bool 
KLASS::init(OSDictionary * properties)
{
  if (!SUPER::init(properties))
    return false;
  
  _workLoop = NULL;
	_commandGate = NULL;
	_adbDevice = NULL;
  
  return true;
}

bool
KLASS::handleStart(IOService * nub)
{
  if (!SUPER::handleStart(nub)) {
    return false;
  }
  
  if ((_adbDevice = OSDynamicCast(IOADBDevice, nub)) == NULL) {
    return false;
  }
      
  if ((_workLoop = IOWorkLoop::workLoop()) == NULL) {
    goto cleanup;
  }
  _workLoop->retain();
  
  if ((_commandGate = IOCommandGate::commandGate(this)) == NULL) {
    goto cleanup;
  }
  if (_workLoop->addEventSource(_commandGate) != kIOReturnSuccess) {
    goto cleanup;
  }
  _commandGate->enable();

  PMinit();
  nub->joinPMtree(this);
  registerPowerDriver(this, myPowerStates(), myNumberOfPowerStates());  
  
  return true;
cleanup:
  if (_workLoop) {
    _workLoop->release();
  }
  bringDownADBDevice();
  return false;
}

bool 
KLASS::willTerminate(IOService * provider, IOOptionBits options)
{
  bringDownADBDevice();
  return SUPER::willTerminate(provider, options);
}

bool 
KLASS::didTerminate(IOService * nub, IOOptionBits options, bool * defer)
{
  return SUPER::didTerminate(nub, options, defer);
}

void 
KLASS::handleStop (IOService * nub) {
  SUPER::handleStop(nub);
  PMstop();
}

void
KLASS::free()
{
  if (_commandGate) {
    if (_workLoop) {
      _workLoop->removeEventSource(_commandGate);
    }
    _commandGate->release();
    _commandGate = NULL;
  }
  if (_workLoop) {
    _workLoop->release();
    _workLoop = NULL;
  }  
  
  SUPER::free();
}


//
//// power management
IOReturn 
KLASS::setPowerState(unsigned long powerStateOrdinal, IOService * device) 
{
  retain();
  IOReturn rc = commandGate()->runAction(setPowerStateAction, (void*)powerStateOrdinal, (void*)device, NULL, NULL);
  release();
  return rc;
}
IOReturn 
KLASS::setPowerStateAction(OSObject * owner, void * arg0, void * arg1, void * arg2, void * arg3)
{
  KLASS * myThis;
  if ((myThis = OSDynamicCast(KLASS,owner)) != NULL) {
    return myThis->setPowerStateGated((UInt32)arg0, (IOService *) arg1);
  } else {
    return kIOReturnError;
  }
}
IOReturn 
KLASS::setPowerStateGated(unsigned long powerStateOrdinal, IOService * device)
{
  switch (powerStateOrdinal) {
    case kADBHIDPowerStateOff:
      bringDownADBDevice();
      break;
    case kADBHIDPowerStateOn:
      bringUpADBDevice();
      registerService();
      break;
  }
  return IOPMAckImplied;
}

IOPMPowerState * KLASS::myPowerStates() { return _myPowerStates; };
unsigned long KLASS::myNumberOfPowerStates() { return kADBHIDNumberPowerstates; };
//
// ADB packet handling
/* static */ void 
KLASS::adbPacketInterrupt(IOService * target, UInt8 adbCommand, IOByteCount length, UInt8 * adbData)
{
  KLASS * myThis;
  if ((myThis = OSDynamicCast(KLASS, target)) == NULL) {
  } else {
    myThis->retain();
    myThis->commandGate()->runAction(adbPacketAction, (void *) adbCommand, (void *)length, (void *) adbData, NULL);
    myThis->release();
  }
}

/* static */ IOReturn
KLASS::adbPacketAction(OSObject * owner, void * arg0, void * arg1, void * arg2, void *)
{
  KLASS * myThis;
  if ((myThis = OSDynamicCast(KLASS, owner)) == NULL) {
    return kIOReturnError;
  } else {
    return myThis->handleADBPacket(*((UInt8*)(&arg0)),  (IOByteCount)arg1, (UInt8*) arg2);
  }
}

// Returns the HID descriptor for this device
IOReturn 
KLASS::newReportDescriptor(IOMemoryDescriptor **descriptor) const
{
  IOBufferMemoryDescriptor *buffer;
  
  buffer = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task, 0, reportDescriptorSize());
  if(buffer==NULL) return kIOReturnNoResources;
  buffer->writeBytes(0, reportDescriptorBytes(), reportDescriptorSize());
  *descriptor=buffer;
  return kIOReturnSuccess;
}


bool 
KLASS::bringUpADBDevice() {
  if (!_adbDevice->seizeForClient(this, KLASS::adbPacketInterrupt)) {
    return false;
  }
  
  // We now have a hold of the tablet, time to configure it if we need to
  if (getProperty(kADBHIDDeviceNewHandlerKey)) {
    UInt8 targetHandler = OSDynamicCast( OSNumber, getProperty(kADBHIDDeviceNewHandlerKey))->unsigned8BitValue();
    // Check if we need to change handler ID
    if (_adbDevice->handlerID() != targetHandler) {    
      // Set the handler id to required ID. 
      if (_adbDevice->setHandlerID(targetHandler) != kIOReturnSuccess) {
        return false;
      }
    }
  }
  return true;
}

void
KLASS::bringDownADBDevice() {
  if (_adbDevice) {
    _adbDevice->setHandlerID(_adbDevice->defaultHandlerID());
    _adbDevice->releaseFromClient(this);
    _adbDevice = NULL;
  }
}
