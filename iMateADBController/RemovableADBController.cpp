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
#include "RemovableADBController.h"
#include "Debug.h"
#include <IOKit/adb/IOADBDevice.h>
#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/pwr_mgt/IOPM.h>

#define KLASS RemovableADBController
#define SUPER IOADBController
#define SUPERSUPER IOADBBus

#define kTenSeconds 10000000

OSDefineMetaClass(KLASS, SUPER)
OSDefineAbstractStructors(KLASS, SUPER)


bool 
KLASS::init(OSDictionary * properties)
{
  if (!SUPERSUPER::init(properties))
    return false;

  fWorkLoop = NULL;
	fCommandGate = NULL;
	_busProbingThread = NULL;
	_busProbed = false;
  
  _shuttingDown = false;
  _outstandingIO = 0;
  _deferredTerminationNub = NULL;
  _deferredTerminationOptions = 0;

  return true;
}

// Startup.  We need to redefine the way the superclass does this in order that we can
// be unloaded cleanly.
bool 
KLASS::start(IOService * nub)
{
  DEBUG_IOLog(3,"%s(%p)::start\n", getName(), this);
	
	if (!(SUPERSUPER::start(nub))) {
		IOLog("%s(%p)::start failed to call SUPERSUPER::start \n", getName(), this);
    return false;
	}
	
  // Create a workloop.  We need to retain this.
  if((fWorkLoop = IOWorkLoop::workLoop()) == NULL) {
    DEBUG_IOLog(1,"%s(%p)::start getWorkLoop failed\n", getName(), this);
    goto cleanup;
  }
  fWorkLoop->retain();
  
  // And a command gate for the workloop so we can gate our calls
  if ((fCommandGate = IOCommandGate::commandGate(this)) == NULL) {
    DEBUG_IOLog(1,"%s(%p)::start - create commandGate failed\n", getName(), this);
    fWorkLoop->release();
    goto cleanup;
  }
  if (fWorkLoop->addEventSource(fCommandGate) != kIOReturnSuccess) {
    DEBUG_IOLog(1,"%s(%p)::start - addEventSource fCommandGate to WorkLoop failed\n", getName(), this);
    fWorkLoop->release();
    goto cleanup;
  }
  fCommandGate->enable();
  
	// Allocate a thread for probing the bus
	_busProbingThread = thread_call_allocate((thread_call_func_t)probeADBBus, (thread_call_param_t)this);
  if (_busProbingThread == NULL) {
    IOLog("%s(%p)::start fails to call thread_call_allocate \n", getName(), this);
    stop(nub);
    return false;
  }
	
	return true;
cleanup:
  stop(nub);
  return false;
}

bool 
KLASS::willTerminate(IOService * nub, IOOptionBits options)
{
  DEBUG_IOLog(3,"%s(%p)::willTerminate\n", getName(), this);
  _shuttingDown = true;
  return SUPERSUPER::willTerminate(nub, options);
}

bool 
KLASS::didTerminate(IOService * nub, IOOptionBits options, bool * defer) 
{
  DEBUG_IOLog(3,"%s(%p)::didTerminate, defer is %d\n", getName(), this, *defer);
  if (_outstandingIO) {
    DEBUG_IOLog(7,"%s(%p)::didTerminate deferring shutdown as I have %d outstanding IOs\n", getName(), this, _outstandingIO);
    _deferredTerminationNub = nub;
    _deferredTerminationOptions = options;
    *defer = true;
    return false;
  }
  closeDevices();
  return SUPERSUPER::didTerminate(nub, options, defer);
}

// Shutdown.  Again, we redefine the superclass way of doing things
void 
KLASS::stop(IOService * nub)
{	
  DEBUG_IOLog(3,"%s(%p)::stop\n", getName(), this);
  
	if (_busProbingThread) {
    DEBUG_IOLog(4,"%s(%p)::stop killing bus probing thread\n", getName(), this);
		thread_call_cancel(_busProbingThread);
		thread_call_free(_busProbingThread);
		_busProbingThread = NULL;
  }
	  
  // Tear down ADB
  tearDownADBStack();
    
	SUPERSUPER::stop(nub);
}

void
KLASS::free()
{
  DEBUG_IOLog(3,"%s(%p)::free\n", getName(), this);
  if (fCommandGate) {
    DEBUG_IOLog(4,"%s(%p)::free killing command gate\n", getName(), this);
    if (fWorkLoop) {
      fWorkLoop->removeEventSource(fCommandGate);
    }
    fCommandGate->release();
    fCommandGate = NULL;
  }
  if (fWorkLoop) {
    DEBUG_IOLog(4,"%s(%p)::free killing event loop\n", getName(), this);
    fWorkLoop->release();
    fWorkLoop = NULL;
  }  
  
  SUPERSUPER::free();
}

//*********************************************************************************
// tearDownADBStack
//
// When we're shutting down, we need to tear down the ADB stack
//*********************************************************************************
void 
KLASS::tearDownADBStack() 
{
  DEBUG_IOLog(3,"%s(%p)::tearDownADBStack\n", getName(), this);
  for (size_t i = 1; i < ADB_DEVICE_COUNT; i++ ) {
    if( adbDevices[ i ] != NULL ) {
      if ( adbDevices[ i ]->nub ) {
        adbDevices[ i ]->nub->terminate(kIOServiceRequired | kIOServiceSynchronous);
        adbDevices[ i ]->nub->release();
      }
      IOFree( adbDevices[ i ], sizeof (ADBDeviceControl));
      adbDevices[ i ] = NULL;
    }
  }
  _busProbed = false;
}

//*********************************************************************************
// bringUpADBStack
//
// kick off a bus probe if necessry
//*********************************************************************************
void 
KLASS::bringUpADBStack() 
{
  DEBUG_IOLog(3,"%s(%p)::bringUpADBStack\n", getName(), this);
  if (!_busProbed) {
    thread_call_enter(_busProbingThread);
    _busProbed = true;
  }
}

void 
KLASS::bringUpPowerManagement(IOService * nub)
{
  DEBUG_IOLog(3,"%s(%p)::bringUpPowerManagement\n", getName(), this);
  PMinit();
  nub->joinPMtree(this);
  registerPowerDriver(this, myPowerStates(), myNumberOfPowerStates());
}

void 
KLASS::tearDownPowerManagement()
{
  DEBUG_IOLog(3,"%s(%p)::tearDownPowerManagement\n", getName(), this);
  PMstop();
}

//*********************************************************************************
// setPowerState
//
// Set our power state as required.
//*********************************************************************************
IOReturn
KLASS::setPowerState(unsigned long powerStateOrdinal, IOService * device)
{
  return IOPMAckImplied;
}

// static entry point for ADB bus probing
void
KLASS::probeADBBus(thread_call_param_t me, thread_call_param_t arg2)
{
	((KLASS*)me)->probeBus();
	((KLASS*)me)->acknowledgeSetPowerState();
}

#pragma mark Redefined from IOADBController to fix endian issues 

// **********************************************************************************
// setHandlerID
//
// **********************************************************************************
IOReturn 
KLASS::setHandlerID ( ADBDeviceControl * deviceInfo, UInt8 handlerID )
{
  DEBUG_IOLog(3,"%s(%p)::setHandlerID\n", getName(), this);
  IOReturn	err;
  UInt8		value[2];
  IOByteCount	        length;
  IOADBAddress	addr = deviceInfo->address;
  
  length = 2;
  err = readFromDevice(addr,3,value,&length);
  
  if ( err ) {
    return err;
  }
  
  value[0] = addr | (value[0] & 0xf0);
  value[1] = handlerID;
  length = 2;
  err = writeToDevice(addr,3,value,&length);
  
  length = sizeof(value);
  err = readFromDevice(addr,3,value,&length);
  
  if ( err == kIOReturnSuccess ) {
    deviceInfo->handlerID = value[1];
    DEBUG_IOLog(3,"%s(%p)::setHandlerID set to 0x%02x \n", getName(), this, deviceInfo->handlerID);
  }
  
  if ( deviceInfo->handlerID == handlerID ) {
    deviceInfo->nub->setProperty(ADBhandlerIDProperty, handlerID, 8);
    err = kIOReturnSuccess;
  }
  else {
    err = kIOReturnNoResources;
  }
  
  return err;
}


// **********************************************************************************
// probeBus
//
// **********************************************************************************
IOReturn KLASS::probeBus ()
{
  DEBUG_IOLog(3,"%s(%p)::probeBus\n", getName(), this);
  int 		i;
  UInt32		unresolvedAddrs;
  UInt32		freeAddrs;
  IOADBAddress	freeNum, devNum;
  
  /* Kill the auto poll until a new dev id's have been setup */
  setAutoPollEnable(false);
  
  /*
   * Send a ADB bus reset - reply is sent after bus has reset,
   * so there is no need to wait for the reset to complete.
   */
  
  resetBus();
  
  /*
   * Okay, now attempt reassign the
   * bus
   */
  
  unresolvedAddrs = 0;
  freeAddrs = 0xfffe;
  
  /* Skip 0 -- it's special! */
  for (i = 1; i < ADB_DEVICE_COUNT; i++) {
    if( probeAddress(i) ) {
      unresolvedAddrs |= ( 1 << i );
      freeAddrs &= ~( 1 << i );
    }
  }
  
  /* Now attempt to reassign the addresses */
  while( unresolvedAddrs) {
    if( !freeAddrs) {
      panic("ADB: Cannot find a free ADB slot for reassignment!");
    }
    
    freeNum = firstBit(freeAddrs);
    devNum = firstBit(unresolvedAddrs);
    
    if( !moveDeviceFrom(devNum, freeNum, true) ) {
      
      /* It didn't move.. bad! */
      IOLog("WARNING : ADB DEVICE %d having problems "
            "probing!\n", devNum);
    }
    else {
      if( probeAddress(devNum) ) {
        /* Found another device at the address, leave
         * the first device moved to one side and set up
         * newly found device for probing
         */
        freeAddrs &= ~( 1 << freeNum );
        
        devNum = 0;
        
      }
      else {
        /* no more at this address, good !*/
        /* Move it back.. */
        moveDeviceFrom(freeNum,devNum,false);
      }
    }
    if(devNum) {
      unresolvedAddrs &= ~( 1 << devNum );
    }
  }
  
  IOLog("%s(%p)::probeBus shows ADB present:%lx\n", getName(), this, (freeAddrs ^ 0xfffe));
  
  setAutoPollList(freeAddrs ^ 0xfffe);
  
  setAutoPollPeriod(11111);
  
  setAutoPollEnable(true);
  
  return kIOReturnSuccess;
}

// Wake the ADB devices on the bus, moved from probe so that subclasses can
// finish any additional probe handling before they wake all the devices up
void
KLASS::wakeADBDevices()
{
  IOADBDevice *	newDev;
  OSDictionary * 	newProps;
  char		nameStr[ 10 ];
  const OSNumber * object;
  const OSSymbol * key;

  // publish the nubs
  for ( size_t i = 1; i < ADB_DEVICE_COUNT; i++ ) {
    if( 0 == adbDevices[ i ] ) {
      continue;
    }
    newDev = new IOADBDevice;			// make a nub
    if ( newDev == NULL ) {
      continue;
    }
    adbDevices[ i ]->nub = newDev;			// keep a pointer to it
    
    newProps = OSDictionary::withCapacity( 10 );	// create a property table for it
    if ( newProps == NULL ) {
      newDev->free();
      continue;
    }
    
    key = OSSymbol::withCString(ADBaddressProperty);	// make key/object for address
    if ( key == NULL ) {
      newDev->free();
      newProps->free();
      continue;
    }
    
    object = OSNumber::withNumber((unsigned long long)adbDevices[i]->address,8);
    if ( object == NULL ) {
      key->release();
      newDev->free();
      newProps->free();
      continue;
    }
    newProps->setObject(key, (OSObject *)object); 		// put it in newProps
    key->release();
    object->release();
    
    key = OSSymbol::withCString(ADBhandlerIDProperty);	// make key/object for handlerID
    if ( key == NULL ) {
      newDev->free();
      newProps->free();
      continue;
    }
    object = OSNumber::withNumber((unsigned long long)adbDevices[i]->handlerID,8);
    if ( object == NULL ) {
      key->release();
      newDev->free();
      newProps->free();
      continue;
    }
    newProps->setObject(key, (OSObject *)object); 		// put it in newProps
    key->release();
    object->release();
    
    key = OSSymbol::withCString(ADBdefAddressProperty);	// make key/object for default addr
    if ( key == NULL ) {
      newDev->free();
      newProps->free();
      continue;
    }
    object = OSNumber::withNumber((unsigned long long)adbDevices[i]->defaultAddress,8);
    if ( object == NULL ) {
      key->release();
      newDev->free();
      newProps->free();
      continue;
    }
    newProps->setObject(key, (OSObject *)object); 		// put it in newProps
    key->release();
    object->release();
    
    key = OSSymbol::withCString(ADBdefHandlerProperty);	// make key/object for default h id
    if ( key == NULL ) {
      newDev->free();
      newProps->free();
      continue;
    }
    object = OSNumber::withNumber((unsigned long long)adbDevices[i]->defaultHandlerID,8);
    if ( object == NULL ) {
      key->release();
      newDev->free();
      newProps->free();
      continue;
    }
    newProps->setObject(key, (OSObject *)object);	 	// put it in newProps
    key->release();
    object->release();
    
    if ( ! newDev->init(newProps,adbDevices[i]) ) {		// give it to our new nub
      kprintf("adb nub init failed\n");
      newDev->release();
      continue;
    }
    
    snprintf(nameStr, 10, "%x-%02x", adbDevices[i]->defaultAddress,adbDevices[i]->handlerID);
    newDev->setName(nameStr);
    snprintf(nameStr, 10, "%x", adbDevices[i]->defaultAddress);
    newDev->setLocation(nameStr);
    
    newProps->release();				// we're done with it
    if ( !newDev->attach(this) ) {
      kprintf("adb nub attach failed\n");
      newDev->release();
      continue;
    }
    newDev->start(this);
    newDev->registerService();
    newDev->waitQuiet();
  }							// repeat loop
}

// **********************************************************************************
// probeAddress
//
// **********************************************************************************
bool 
KLASS::probeAddress ( IOADBAddress addr )
{
  DEBUG_IOLog(3,"%s(%p)::probeAddress\n", getName(), this);
  IOReturn		err;
  ADBDeviceControl *	deviceInfo;
  UInt8		value[2];
  IOByteCount		length;
  
  length = 2;
  err = readFromDevice(addr,3,value,&length);
  
  if (err == ADB_RET_OK) {
    if( NULL == (deviceInfo = adbDevices[ addr ])) {
      
      deviceInfo = (ADBDeviceControl *)IOMalloc(sizeof(ADBDeviceControl));
      bzero(deviceInfo, sizeof(ADBDeviceControl));
      
      adbDevices[ addr ] = deviceInfo;
      deviceInfo->defaultAddress = addr;
      deviceInfo->handlerID = deviceInfo->defaultHandlerID = (value[1]);
    }
    deviceInfo->address = addr;
  }
  return( (err == ADB_RET_OK));
}

// **********************************************************************************
// firstBit
//
// **********************************************************************************
unsigned int 
KLASS::firstBit ( unsigned int mask )
{
  DEBUG_IOLog(3,"%s(%p)::firstBit\n", getName(), this);
  int bit = 15;
  
  while( 0 == (mask & (1 << bit))) {
    bit--;
  }
  return(bit);
}

// **********************************************************************************
// moveDeviceFrom
//
// **********************************************************************************
bool 
KLASS::moveDeviceFrom ( IOADBAddress from, IOADBAddress to, bool check )
{
  DEBUG_IOLog(3,"%s(%p)::moveDeviceFrom 0x%02x to 0x%02x\n", getName(), this, from, to);
  IOReturn	err;
  UInt8		value[2];
  IOByteCount	length;
  bool		moved;
  
  length = 2;
  value[1] = ADB_DEVCMD_CHANGE_ID;
  value[0] = to;
  
  err = writeToDevice(from,3,value,&length);
  
  adbDevices[ to ] = adbDevices[ from ];
  
  moved = probeAddress(to);
  
  if( moved || (!check)) {
    adbDevices[ from ] = NULL;
  }
  else {
    adbDevices[ to ] = NULL;
  }
  
  return moved;
}

IOReturn 
KLASS::incrementOutstandingIO()
{
  DEBUG_IOLog(3,"%s(%p)::incrementOutstandingIO\n", getName(), this);
  if (commandGate()) {
    return commandGate()->runAction(changeOutstandingIOAction, NULL);
  }
  _outstandingIO++;
  DEBUG_IOLog(7,"%s(%p)::incrementOutstandingIO : outstandingIO is now %d\n", getName(), this, _outstandingIO);
  return kIOReturnSuccess;
}

IOReturn 
KLASS::decrementOutstandingIO()
{
  DEBUG_IOLog(3,"%s(%p)::decrementOutstandingIO\n", getName(), this);
  if (commandGate()) {
    return commandGate()->runAction(changeOutstandingIOAction, (void*)-1);
  }
  _outstandingIO--;
  if  (shouldDoDeferredShutdown()) {
    DEBUG_IOLog(4,"%s(%p)::decrementOutstandingIO  continuing deferred shutdown\n", getName(), this);
    closeDevices();
    bool defer = false;
    SUPERSUPER::didTerminate(_deferredTerminationNub, _deferredTerminationOptions, &defer);
  }
  DEBUG_IOLog(7,"%s(%p)::decrementOutstandingIO : outstandingIO is now %d\n", getName(), this, _outstandingIO);
  return kIOReturnSuccess;
}

IOReturn 
KLASS::changeOutstandingIOAction(OSObject * owner, void * param1, void *, void *, void *)
{
  KLASS * me = NULL;
  int direction = *((UInt8*)(&param1));
  
  if ((me = OSDynamicCast(KLASS, owner)) != NULL) {
    switch (direction) {
      case 0x00:
        me->_outstandingIO++;
        break;
      case 0xff:
        me->_outstandingIO--;
        if (me->shouldDoDeferredShutdown()) {
          DEBUG_IOLog(4,"%s(%p)::changeOutstandingIOAction continuing deferred shutdown\n", me->getName(), me);
          me->closeDevices();
          bool defer = false;
          me->SUPERSUPER::didTerminate(me->_deferredTerminationNub, me->_deferredTerminationOptions, &defer);
        }
        break;
      default:
        DEBUG_IOLog(1,"%s(%p)::changeOutstandingIOAction got a strange value of0x%02x\n", me->getName(), me, direction);
        break;
    }
  }
  DEBUG_IOLog(7,"%s(%p)::changeOutstandingIOAction : outstandingIO is now %d\n", me->getName(), me, me->_outstandingIO);
  return kIOReturnSuccess;
}