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

  if (!bringUpADBDevice()) {
    goto cleanup;
  }
  
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
KLASS::bringUpADBDevice() {
  if (!_adbDevice->siezeForClient(this, KLASS::adbPacketInterrupt)) {
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

bool 
KLASS::willTerminate(IOService * provider, IOOptionBits options)
{
  bringDownADBDevice();
  return SUPER::willTerminate(provider, options);
}
//virtual bool didTerminate(IOService * nub, IOOptionBits options, bool * defer);

void
KLASS::bringDownADBDevice() {
  if (_adbDevice) {
    _adbDevice->setHandlerID(adbDevice->defaultHandlerID());
    _adbDevice->releaseFromClient(this);
    _adbDevice = NULL;
  }
}

void 
KLASS::handleStop (IOService * nub) {
  
  PMstop();
  SUPER::handleStop(nub);
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
//virtual IOReturn setPowerState(unsigned long powerStateOrdinal, IOService * device);
//virtual IOReturn powerStateWillChangeTo ( IOPMPowerFlags theFlags, unsigned long, IOService*);
//virtual IOReturn powerStateDidChangeTo ( IOPMPowerFlags theFlags, unsigned long, IOService*);
//// Override in subclasses to define allowed power states
//virtual IOPMPowerState * myPowerStates() = 0;
//virtual unsigned long myNumberOfPowerStates() = 0;
//
//// ADB packet handling
//static void adbPacketInterrupt(IOService * target, UInt8 adbCommand, IOByteCount length, UInt8 * data);
//virtual void handleADBPacket(UInt8 * adbData);
