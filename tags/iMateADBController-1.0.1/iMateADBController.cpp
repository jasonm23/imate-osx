/*
 * Copyright (c) 2008-2009 Simon Stapleton
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

#include "iMateADBController.h"
#include "Debug.h"
#include <stdarg.h>

// How long we should wait, in ms, for a response from a device on the ADB bus
#define kADBTimeoutMilliseconds 30
// How long does a reset take?  ADB spec says 300ms, we allow 310.
#define kADBResetMilliseconds 310
// How long does a bus probe take?
// 300ms reset + ADB_DEVICES * (probe + move + probe + move) =~ 400ms. Tell IOKit
// we need up to a second
#define kiMateProbeTimeMicroseconds 1000000

#define SUPER RemovableADBController
#define KLASS iMateADBController

OSDefineMetaClassAndStructors(KLASS, SUPER)

enum {
  kiMatePowerStateOff = 0,
  kiMatePowerStateRestart = 1,
  kiMatePowerStateSleep = 2, 
  kiMatePowerStateOn = 3,
  kiMateNumberPowerStates = 4
};

// Not sure why we're bothering with stuff other than On and Off, but just in case.
static IOPMPowerState _myPowerStates[kiMateNumberPowerStates] = {
{kIOPMPowerStateVersion1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  // Power Off
{kIOPMPowerStateVersion1, kIOPMRestartCapability, kIOPMRestart, kIOPMRestart, 0, 0, 0, 0, 0, 0, 0, 0},
{kIOPMPowerStateVersion1, kIOPMSleepCapability, kIOPMSleep, kIOPMSleep, 0, 0, 0, 0, 0, 0, 0, 0},
{kIOPMPowerStateVersion1, kIOPMDeviceUsable, kIOPMPowerOn, kIOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0}, 
};


bool 
KLASS::init(OSDictionary * properties)
{
  if (!SUPER::init(properties))
    return false;
  
  fInterface = NULL;
  fInterruptPipe = NULL;
  fInterruptPipeMDP = NULL;
  fUSBData = NULL;
  autopollControlBlock.timerEventSource = NULL;
  commandControlBlock.timerEventSource = NULL;
  pollList = 0;
  autopollOn = false;
  waitingForData = 0xff;
  autoPollMilliseconds = 11;
  startingUp = false;
  
  return true;
}


// **********************************************************************************
// start
//
// **********************************************************************************
bool 
KLASS::start ( IOService * provider )
{
  DEBUG_IOLog(3,"%s(%p)::start iMate Driver\n", getName(), this);
  IOUSBFindEndpointRequest epReq;
  
  // Make sure we get out of here alive
  startingUp = true;
  incrementOutstandingIO();
  
  // And an endpoint request, pick up the "in" interrupt endpoint 
  epReq.type = kUSBInterrupt;
  epReq.direction = kUSBIn;
	      
  // Superclass starter
  if( !SUPER::start(provider)) {
    DEBUG_IOLog(1,"%s(%p)::start superclass start failed\n", getName(), this);
    goto error_exit;
  }
  
  // Grab our interface
  if ((fInterface = OSDynamicCast(IOUSBInterface, provider)) == NULL) {
    DEBUG_IOLog(1,"%s(%p)::start did not get an IOUSBInterface\n", getName(), this);
    goto cleanup;
  }
  
  // Now timer event sources for ADB timeouts
  if ((autopollControlBlock.timerEventSource = IOTimerEventSource::timerEventSource(reinterpret_cast<OSObject *>(this), 
                                                                                    reinterpret_cast<IOTimerEventSource::Action>(&(KLASS::autopollTimeoutHandler)))) == NULL) {
    DEBUG_IOLog(1,"%s(%p)::start - create autopoll timer event source failed\n", getName(), this);
    goto cleanup;
  }
  if (workLoop()->addEventSource(autopollControlBlock.timerEventSource) != kIOReturnSuccess) {
    DEBUG_IOLog(1,"%s(%p)::start - addEventSource autopollControlBlock.timerEventSource to WorkLoop failed\n", getName(), this);
    goto cleanup;
  }
  if ((commandControlBlock.timerEventSource = IOTimerEventSource::timerEventSource(reinterpret_cast<OSObject *>(this), 
                                                                                    reinterpret_cast<IOTimerEventSource::Action>(&(KLASS::commandTimeoutHandler)))) == NULL) {
    DEBUG_IOLog(1,"%s(%p)::start - create command timer event source failed\n", getName(), this);
    goto cleanup;
  }
  if (workLoop()->addEventSource(commandControlBlock.timerEventSource) != kIOReturnSuccess) {
    DEBUG_IOLog(1,"%s(%p)::start - addEventSource commandControlBlock.timerEventSource to WorkLoop failed\n", getName(), this);
    goto cleanup;
  }
  
  // Now open the interface
  if (!fInterface->open(this)) {
    DEBUG_IOLog(1,"%s(%p)::start - could not open interface\n", getName(), this);
    goto cleanup;
  }
    
  // Find our interrupt pipe, finally
  // Interrupt data comes in on the pipe
  if ((fInterruptPipe = fInterface->FindNextPipe(NULL, &epReq)) == NULL) {
    DEBUG_IOLog(1,"%s(%p)::start - could not get interrupt pipe\n", getName(), this);
    goto cleanup;
  }
  fInterruptPipe->retain();
  DEBUG_IOLog(3,"%s(%p)::start - interrupt pipe found, maxPacketSize:%d interval:%d\n", getName(), this, epReq.maxPacketSize, epReq.interval );
  packetSize = epReq.maxPacketSize;
  
  // Allocate memory blocks
  if ((fInterruptPipeMDP = IOBufferMemoryDescriptor::withCapacity(packetSize, kIODirectionIn)) == NULL) {
    DEBUG_IOLog(1,"%s(%p)::start - could not allocate a memory descriptor\n", getName(), this);
    goto cleanup;
  }
  fInterruptPipeMDP->setLength(packetSize);
  fUSBData = (UInt8 *)fInterruptPipeMDP->getBytesNoCopy();

  fReadThread = thread_call_allocate(readThreadHandler, (thread_call_param_t)this);
  // Set up our callback for reads
  fReadCompletionInfo.target = this;
  fReadCompletionInfo.action = &(KLASS::readCompleteCallback);
  fReadCompletionInfo.parameter = NULL;
  
  // Set up power management stuff
  bringUpPowerManagement(provider);
  
  return true;
    
cleanup:
  stop(provider);
error_exit:
  decrementOutstandingIO();
  return false;
}

bool
KLASS::willTerminate( IOService * nub, IOOptionBits options )
{
  DEBUG_IOLog(3,"%s(%p)::willTerminate\n", getName(), this);
  cancelAllIO();
  return SUPER::willTerminate(nub, options);
}

bool
KLASS::didTerminate( IOService * nub, IOOptionBits options, bool * defer )
{
  DEBUG_IOLog(3,"%s(%p)::didTerminate\n", getName(), this);
  return SUPER::didTerminate(nub, options, defer);
}

// **********************************************************************************
// stop
//
// **********************************************************************************
void KLASS::stop ( IOService *provider )
{
  DEBUG_IOLog(3,"%s(%p)::stop\n", getName(), this);
  
  if (autopollControlBlock.timerEventSource) {
    autopollControlBlock.timerEventSource->cancelTimeout();
    if (workLoop()) {
      workLoop()->removeEventSource(autopollControlBlock.timerEventSource);
    }
    autopollControlBlock.timerEventSource->release();
    autopollControlBlock.timerEventSource = NULL;
  }
  if (commandControlBlock.timerEventSource) {
    commandControlBlock.timerEventSource->cancelTimeout();
    if (workLoop()) {
      workLoop()->removeEventSource(commandControlBlock.timerEventSource);
    }
    commandControlBlock.timerEventSource->release();
    commandControlBlock.timerEventSource = NULL;
  }
  
  if (fReadThread) {
    DEBUG_IOLog(4,"%s(%p)::didTerminate killing read thread\n", getName(), this);
    thread_call_cancel(fReadThread);
    thread_call_free(fReadThread);
    fReadThread = NULL;
  }
  
  fUSBData = NULL;
  if (fInterruptPipeMDP) {
    DEBUG_IOLog(4,"%s(%p)::didTerminate killing interrupt MDP\n", getName(), this);
    fInterruptPipeMDP->release();
    fInterruptPipeMDP = NULL;
  }
  
  if (fInterruptPipe) {
    DEBUG_IOLog(4,"%s(%p)::didTerminate killing interrupt pipe\n", getName(), this);
    fInterruptPipe->release();
    fInterruptPipe = NULL;
  }  
  
  SUPER::stop(provider);
  
  // We're done accessing the device
  tearDownPowerManagement();
}

IOReturn
KLASS::closeDevices() {
  DEBUG_IOLog(3,"%s(%p)::closeDevices\n", getName(), this);
  if (fInterface) {
    DEBUG_IOLog(4,"%s(%p)::closeDevices killing interface\n", getName(), this);
    fInterface->close(this);
    fInterface = NULL;
  }
  return SUPER::closeDevices();
}

// probeBus - probe ADB bus.
// called in gated context
IOReturn KLASS::probeBus ()
{
  DEBUG_IOLog(3,"%s(%p)::probeBus\n", getName(), this);
  IOReturn rc;

  iMateWrite(kUSBOut, 0x01, 0x0003, 0x0000, 0x0000, NULL);  // turn address 3 conversion on
  rc = SUPER::probeBus();
  iMateWrite(kUSBOut, 0x01, 0x0003, 0x0001, 0x0000, NULL);  // and off again
  
  // Have we just finished starting up?  If so, decrement our IO counter to account for the 
  // increment we used in start().  
  if (startingUp) {
    DEBUG_IOLog(4,"%s(%p)::probeBus decrementing outstanding IO\n", getName(), this);
    startingUp = false;
    decrementOutstandingIO();
  }
  return rc;
}


/* static */ void
KLASS::autopollTimeoutHandler(OSObject * arg, IOTimerEventSource * source)
{
  DEBUG_IOLog(3,"autopollTimeoutHandler\n");
  KLASS * myThis;
  if (myThis = OSDynamicCast(KLASS, arg)) {
    myThis->retain();
    myThis->autopollTimeout();
    myThis->release();
  } else {
    DEBUG_IOLog(1,"autopollTimeoutHandler could not cast to KLASS\n");
  }
}

void
KLASS::autopollTimeout() 
{
  autopollControlBlock.adbMessageResult = kIOReturnTimeout;
}

/* static */ void
KLASS::commandTimeoutHandler(OSObject * arg, IOTimerEventSource * source)
{
  DEBUG_IOLog(3,"commandTimeoutHandler\n");
  KLASS * myThis;
  if (myThis = OSDynamicCast(KLASS, arg)) {
    myThis->retain();
    myThis->commandTimeout();
    myThis->release();
  } else {
    DEBUG_IOLog(1,"commandTimeoutHandler could not cast to KLASS\n");
  }
}

void
KLASS::commandTimeout() 
{
  commandControlBlock.adbMessageResult = kIOReturnTimeout;
  decrementOutstandingIO();
  commandGate()->commandWakeup(&commandControlBlock);
}


/* static */ void 
KLASS::readThreadHandler(thread_call_param_t owner, void *) 
{
  DEBUG_IOLog(3,"readThreadHandler\n");
  KLASS * myThis;
  if (myThis = OSDynamicCast(KLASS, (OSObject*)owner)) {
    myThis->retain();
    myThis->queueRead();
    myThis->release();
  } else {
    DEBUG_IOLog(1,"readThread could not cast to KLASS\n");
  }
}

IOReturn 
KLASS::queueRead()
{
  DEBUG_IOLog(3,"%s(%p)::queueRead\n", getName(), this);
  incrementOutstandingIO();
  
  IOReturn rc;
  int retries = 0;
  
  fInterruptPipeMDP->setLength(packetSize);
  while (2 > ++retries && ((rc = fInterruptPipe->Read(fInterruptPipeMDP, 0, 0, packetSize, &fReadCompletionInfo)) != kIOReturnSuccess))
  {
    DEBUG_IOLog(3,"%s(%p)::queueRead could not queue\n", getName(), this);
    switch (rc) {
      case kIOUSBPipeStalled:          
        DEBUG_IOLog(3,"%s(%p)::queueRead Stalled - resetting and retrying\n", getName(), this);
        fInterruptPipe->ClearStall();
        break;
      default:
        DEBUG_IOLog(3,"%s(%p)::queueRead Error was %s\n", getName(), this, stringFromReturn(rc));
        retries = 3;
    }
  }
  DEBUG_IOLog(3,"%s(%p)::queueRead - read queued\n", getName(), this);
  IOReturn rc2 = fInterruptPipe->GetPipeStatus();
  if (rc2 != kIOReturnSuccess) {
    DEBUG_IOLog(3,"%s(%p)::queueRead - rc2 was kIOUSBPipeStalled\n", getName(), this);
  }
  DEBUG_IOLog(3,"%s(%p)::queueRead - pipe status is 0x%08x (%s)\n", getName(), this, rc2, stringFromReturn(rc2));
  
  return rc;
}

// static method handling read completion 
/* static */ void
KLASS::readCompleteCallback(void * owner, void * parameter, IOReturn rc, UInt32 bufferSizeRemaining, AbsoluteTime timestamp)
{
  DEBUG_IOLog(3,"readCompleteCallback\n");
  KLASS * myThis;
  if (myThis = OSDynamicCast(KLASS,(OSObject*)owner)) {
    myThis->retain();
//    myThis->commandGate()->runAction(readCompleteAction, (void*)rc, (void*)bufferSizeRemaining, NULL, NULL);
    myThis->readComplete(rc, bufferSizeRemaining);
    myThis->release();
  } else {
    DEBUG_IOLog(1,"readCompleteCallback could not cast to KLASS\n");
  }
}

/* static */ IOReturn
KLASS::readCompleteAction(OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3)
{
  KLASS * myThis;
  if (myThis = OSDynamicCast(KLASS,owner)) {
    return myThis->readComplete((IOReturn)arg0, (UInt32)arg1);
  } else {
    DEBUG_IOLog(1,"readCompleteAction could not cast to KLASS\n");
    return kIOReturnError;
  }
}

IOReturn 
KLASS::readComplete(IOReturn rc, UInt32 bufferSizeRemaining)
{
  DEBUG_IOLog(3,"%s(%p)::readComplete, rc is %s, bufferSizeRemaining is %u\n", getName(), this, stringFromReturn(rc), bufferSizeRemaining);

    // We got some data
  if (rc == kIOReturnSuccess) {
    // USB packet is 8 bytes, optionally followed by a second 8.
    // byte 0 == ADB command
    // byte 1 ==
    // byte 2 == packet number, 0x99 = packet 1, 0x98 = packet 2
    // byte 3 == number of bytes total

#ifdef DATA_LOGGING
    DATA_IOLog(7,"%s(%p)::readComplete USB control data was %02x %02x %02x %02x\n", getName(), this, fUSBData[0], fUSBData[1], fUSBData[2], fUSBData[3]);
#endif
    
    // Switch what control block we use depending on the ADB command (and thus source)
    // autopoll data comes in with a command of 0x00, hopefully all other commands
    // are serialised such that there's no conflict
    ADBControlBlock * adbControlBlock = (fUSBData[0] == 0 || fUSBData[0] != waitingForData) ? &autopollControlBlock : &commandControlBlock;
    DEBUG_IOLog(7, "%s(%p)::readComplete received a %s packet\n", getName(), this, (fUSBData[0] == 0 || fUSBData[0] != waitingForData) ? "autopoll" : "command reply"); 
    // Cancel our timer.
    adbControlBlock->timerEventSource->cancelTimeout();
    
    switch (fUSBData[2]) {
      case 0x99:    // First block of USB Data
        adbControlBlock->adbPacket[8] = fUSBData[0];                               // Store ADB command
        adbControlBlock->adbDataLength = adbControlBlock->adbPacket[9] = fUSBData[3];               // Total number of bytes sent
        if (adbControlBlock->adbDataLength > 4) {                                  // we expect a second block to follow
          memcpy(adbControlBlock->adbPacket, fUSBData + 4, 4);                     // Copy data
          adbControlBlock->timerEventSource->setTimeoutMS(kADBTimeoutMilliseconds); // Reset the timeout for the next packet
        } else {
          memcpy(adbControlBlock->adbPacket, fUSBData + 4, adbControlBlock->adbDataLength);         // Copy data
#ifdef DATA_LOGGING
          DATA_IOLog(3,"%s(%p)::readComplete adb data block 1 was ", getName(), this);
          for (int i = 0; i < adbControlBlock->adbDataLength; i++)
            DATA_IOLog(3,"0x%02x ", adbControlBlock->adbPacket[i]);
          DATA_IOLog(3,"\n");
#endif
          signalData(adbControlBlock);
        }
        break;
      case 0x98:    // Second block of adb data
        memcpy(adbControlBlock->adbPacket + 4, fUSBData + 4, adbControlBlock->adbDataLength - 4);   // Copy data
#ifdef DATA_LOGGING
        DATA_IOLog(3,"%s(%p)::readComplete adb data block 2 was ", getName(), this);
        for (int i = 0; i < adbControlBlock->adbDataLength; i++)
          DATA_IOLog(3,"0x%02x ", adbControlBlock->adbPacket[i]);
        DATA_IOLog(3,"\n");
#endif
        signalData(adbControlBlock);
        break;
      default:
        decrementOutstandingIOGated();  // we received a packet, even if we don't know what to do with it
        DEBUG_IOLog(7,"%s(%p)::readComplete got a data packet with a header byte of %02x\n", getName(), this, fUSBData[2]);
#ifdef DATA_LOGGING
        DATA_IOLog(3,"%s(%p)::readComplete odd adb data block was ", getName(), this);
        for (int i = 0; i < fUSBData[3]; i++)
          DATA_IOLog(3,"0x%02x ", fUSBData[i + 4]);
        DATA_IOLog(3,"\n");
#endif
        break;
    }
  } else {
    // We got into here after an error, usually after unplugging, but there may be a "hanging" 
    // outstanding IO from a message sent to the iMate. Get rid of it if so.
    if (waitingForData) {
      decrementOutstandingIOGated();
    }
  }
  // This is the IO from the read
  decrementOutstandingIOGated();
  if ((rc != kIOReturnAborted) && (rc != kIOReturnNotResponding)) {
    thread_call_enter(fReadThread);                               // Reschedule a read
  }
  return rc;
}

void KLASS::signalData(ADBControlBlock * controlBlock) {
  DEBUG_IOLog(3,"%s(%p)::signalData\n", getName(), this);
  
  controlBlock->adbMessageResult = kIOReturnSuccess;             // Successful return
  // decide what to do with the packet
  if (controlBlock == &autopollControlBlock) {  // Autopoll data
    if (waitingForData) {
      DEBUG_IOLog(4,"%s(%p)::signalData - was waiting for response from command %02x, got response %02x - treating as autopolled\n", getName(), this, waitingForData, controlBlock->adbPacket[8]);
      // reset the timeout for the ongoing ADB command, as this autopolled response may have held up the command response
      commandControlBlock.timerEventSource->cancelTimeout();
      commandControlBlock.timerEventSource->setTimeoutMS(kADBTimeoutMilliseconds);
    }
    autopollHandler(this, controlBlock->adbPacket[8], controlBlock->adbDataLength, controlBlock->adbPacket);
  } else {                                   // Requested packet
    decrementOutstandingIOGated();
    waitingForData = 0x00;                                         // No longer waiting
    commandGate()->commandWakeup(controlBlock); // Wake up and process
  }
}

#pragma mark Power Management

IOPMPowerState * 
KLASS::myPowerStates()
{
  return _myPowerStates;
}

unsigned long 
KLASS::myNumberOfPowerStates() 
{
  return kiMateNumberPowerStates;
}

IOReturn 
KLASS::setPowerState(unsigned long powerStateOrdinal, IOService * device)
{
  DEBUG_IOLog(3,"%s(%p)::setPowerState 0x%08x\n", getName(), this, powerStateOrdinal);
  retain();
  IOReturn rc = commandGate()->runAction(setPowerStateAction, (void *)powerStateOrdinal, (void *)device, NULL, NULL);
  release();
  return rc;
}

IOReturn 
KLASS::setPowerStateAction(OSObject * owner, void * arg0, void * arg1, void * arg2, void * arg3)
{
  KLASS * myThis;
  if (myThis = OSDynamicCast(KLASS,owner)) {
    return myThis->setPowerStateGated((UInt32)arg0, (IOService *) arg1);
  } else {
    DEBUG_IOLog(1,"setPowerStateAction could not cast to KLASS\n");
    return kIOReturnError;
  }
}

IOReturn 
KLASS::setPowerStateGated(unsigned long powerStateOrdinal, IOService * device)
{
  switch (powerStateOrdinal) {
    case kiMatePowerStateOff:
    case kiMatePowerStateRestart:
    case kiMatePowerStateSleep:
      cancelAllIO();
      tearDownADBStack();
      break;
    case kiMatePowerStateOn:
      // Arm read thread
//      queueRead();
//      if (fReadThread) {
        thread_call_enter(fReadThread);
      bringUpADBStack();
      return kiMateProbeTimeMicroseconds;

//      } else {
//        DEBUG_IOLog(3,"%s(%p)::setPowerStateGated - fReadThread is nil\n", getName(), this);
//      }
  }
  return IOPMAckImplied;
}
  

#pragma mark Various ADB command implementations

// **********************************************************************************
// setAutoPollPeriod
// **********************************************************************************
IOReturn 
KLASS::setAutoPollPeriod ( int microseconds )
{
  DEBUG_IOLog(3,"%s(%p)::setAutoPollPeriod %d\n", getName(), this, microseconds);
  autoPollMilliseconds = (microseconds / 1000) & 0xff;
  if (autoPollMilliseconds == 0)
    autoPollMilliseconds = 1;
    
  return ADB_RET_OK;
}

// **********************************************************************************
// getAutoPollPeriod
// **********************************************************************************
IOReturn 
KLASS::getAutoPollPeriod ( int * microseconds)
{
  DEBUG_IOLog(3,"%s(%p)::getAutoPollPeriod\n", getName(), this);
  *microseconds = autoPollMilliseconds * 1000;
  return ADB_RET_OK;
}

// **********************************************************************************
// setAutoPollList
// **********************************************************************************
IOReturn 
KLASS::setAutoPollList ( UInt16 PollBitField )
{  
  DEBUG_IOLog(3,"%s(%p)::setAutoPollList 0x%04x\n", getName(), this, PollBitField);
  retain();
  IOReturn rc = commandGate()->runAction(setAutoPollListAction, (void*)PollBitField, NULL, NULL, NULL);
  release();
  return rc;
}

/* static */ IOReturn 
KLASS::setAutoPollListAction (OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3)
{
  KLASS * myThis;
  if (myThis = OSDynamicCast(KLASS,owner)) {
    return myThis->setAutoPollListGated((UInt32)arg0);
  } else {
    DEBUG_IOLog(1,"setAutoPollListAction could not cast to KLASS\n");
    return kIOReturnError;
  }
}

IOReturn 
KLASS::setAutoPollListGated ( UInt32 PollBitField )
{
  IOReturn rc;
  pollList = (UInt16)PollBitField;				// remember the new poll list
  rc = iMateWrite(kUSBOut, 0x01, 0x0001,  OSSwapInt16(pollList), 0x0000, NULL);
  IOLog("Set Polling list to %04x\n", pollList);
  return rc;
}

// **********************************************************************************
// getAutoPollList
// **********************************************************************************
IOReturn 
KLASS::getAutoPollList ( UInt16 * PollBitField )
{
  DEBUG_IOLog(3,"%s(%p)::getAutoPollList\n", getName(), this);
  *PollBitField = pollList;
  return ADB_RET_OK;
}

// **********************************************************************************
// setAutoPollEnable
//
// **********************************************************************************
IOReturn 
KLASS::setAutoPollEnable ( bool enable )
{
  DEBUG_IOLog(3,"%s(%p)::setAutoPollEnable %s\n", getName(), this, enable ? "On" : "Off");
  IOReturn rc;
  
  retain();
  rc = commandGate()->runAction(setAutoPollEnableAction, (void*)enable, NULL, NULL, NULL);
  release();
  return rc;
}

/* static */ IOReturn 
KLASS::setAutoPollEnableAction (OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3)
{
  KLASS * myThis;
  if (myThis = OSDynamicCast(KLASS,owner)) {
    return myThis->setAutoPollEnableGated((bool)arg0);
  } else {
    DEBUG_IOLog(1,"setAutoPollEnableAction could not cast to KLASS\n");
    return kIOReturnError;
  }
}

IOReturn 
KLASS::setAutoPollEnableGated ( bool enable )
{
  IOReturn rc;
  autopollOn = enable;
  
  if (enable) {
    rc = iMateWrite(kUSBOut, 0x01, 0x0004, autoPollMilliseconds - 1, 0x0000, NULL);
  } else {
    rc = iMateWrite(kUSBOut, 0x01, 0x0004, 0x00ff, 0x0000, NULL);
  }
  return rc;
}

// **********************************************************************************
// resetBus
//
// **********************************************************************************
IOReturn 
KLASS::resetBus ( void )
{
  DEBUG_IOLog(3,"%s(%p)::resetBus\n", getName(), this);
  IOReturn rc;
  retain();
  rc = commandGate()->runAction(resetBusAction, NULL, NULL, NULL, NULL);
  release();
  return rc;
}
/* static */ IOReturn 
KLASS::resetBusAction (OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3)
{
  KLASS * myThis;
  if (myThis = OSDynamicCast(KLASS,owner)) {
    return myThis->resetBusGated();
  } else {
    DEBUG_IOLog(1,"resetBusAction could not cast to KLASS\n");
    return kIOReturnError;
  }
}
IOReturn 
KLASS::resetBusGated ( void )
{
  IOReturn rc;
  UInt32 length = 0;
  
  // iMate reset is a bit wierd.  We send a reset packet to the device, then
  // a specific message to device 3.
  if ((rc = iMateWrite(kUSBOut, 0x01, 0x0002, 0x0000, 0x0000, NULL)) == kIOReturnSuccess) {
    // We've reset the bus. so sleep a bit
    IOSleep(kADBResetMilliseconds);
    // Now send the second packet and wait for a response
    rc = sendADBCommandToIMate(kUSBOut, ADB_CMD(kADBResetDevice, 3, 0), NULL, &length, true);
  }
  return rc;
}


// **********************************************************************************
// flushDevice
//
// **********************************************************************************
IOReturn KLASS::flushDevice ( IOADBAddress address )
{
  DEBUG_IOLog(3,"%s(%p)::flushDevice 0x%02x\n", getName(), this, address);
  IOReturn rc;
  retain();
  rc = commandGate()->runAction(flushDeviceAction, (void*)address, NULL, NULL, NULL);
  release();
  return rc;
}
/* static */ IOReturn
KLASS::flushDeviceAction (OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3)
{
  KLASS * myThis;
  if (myThis = OSDynamicCast(KLASS,owner)) {
    return myThis->flushDeviceGated((UInt32)arg0);
  } else {
    DEBUG_IOLog(1,"flushDeviceAction could not cast to KLASS\n");
    return kIOReturnError;
  }
}
IOReturn 
KLASS::flushDeviceGated ( UInt32 address )
{
  UInt32 length;
  return sendADBCommandToIMate(kUSBOut, ADB_CMD(kADBFlush, address, 0), NULL, &length);
}


// **********************************************************************************
// readFromDevice
//
// The length parameter is ignored on entry.  It is set on exit to reflect
// the number of bytes read from the device.
// **********************************************************************************
IOReturn 
KLASS::readFromDevice ( IOADBAddress address, IOADBRegister adbRegister,
                                    UInt8 * data, IOByteCount * length )
{
  DEBUG_IOLog(3,"%s(%p)::readFromDevice : 0x%02x register 0x%02x\n", getName(), this, address, adbRegister);
  IOReturn rc;
  retain();
  rc = commandGate()->runAction(readFromDeviceAction, (void*)address, (void*)adbRegister, (void*)data, (void*)length);
  release();
  return rc;
}
/* static */ IOReturn
KLASS::readFromDeviceAction (OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3)
{
  KLASS * myThis;
  if (myThis = OSDynamicCast(KLASS,owner)) {
    return myThis->readFromDeviceGated((UInt32)arg0, (UInt32) arg1, (UInt8 *) arg2, (IOByteCount *) arg3);
  } else {
    DEBUG_IOLog(1,"readFromDeviceAction could not cast to KLASS\n");
    return kIOReturnError;
  }
}
IOReturn 
KLASS::readFromDeviceGated(UInt32 address, UInt32 adbRegister,
                                        UInt8 * data, IOByteCount * length )
{
  IOReturn rc;
  rc = sendADBCommandToIMate(kUSBOut, ADB_CMD(kADBTalk, address, adbRegister), NULL, NULL, true);
  
  if (rc == kIOReturnSuccess) {
#ifdef DATA_LOGGING
    DATA_IOLog(3,"%s(%p)::readFromDevice data was ", getName(), this);
    for (int i = 0; i < commandControlBlock.adbDataLength; i++)
      DATA_IOLog(3,"0x%02x ", commandControlBlock.adbPacket[i]);
    DATA_IOLog(3,"\n");
#endif
    *length = commandControlBlock.adbDataLength;
    memcpy(data, commandControlBlock.adbPacket, commandControlBlock.adbDataLength);
    return rc;
  } else {
    return ADB_RET_NOTPRESENT;
  }
}


// **********************************************************************************
// writeToDevice
//
// **********************************************************************************
IOReturn 
KLASS::writeToDevice (IOADBAddress address, IOADBRegister adbRegister, UInt8 * data, IOByteCount * length )
{
  DEBUG_IOLog(3,"%s(%p)::writeToDevice : 0x%02x register 0x%02x\n", getName(), this, address, adbRegister);
  // Byte swap the data buffer if necessary
//  for (int i = 0; i < *length / 2; i++) {
//    ((UInt16*)data)[i] = OSSwapHostToBigInt16(((UInt16*)data)[i]);
//  }
#ifdef DATA_LOGGING
  if (*length > 0) {
    DATA_IOLog(7,"%s(%p)::writeToDevice data is ", getName(), this);
    for (int i = 0; i < *length; i++)
      DATA_IOLog(7,"0x%02x ", data[i]);
    DATA_IOLog(7,"\n");
  }
#endif
  IOReturn rc;
  retain();
  rc = commandGate()->runAction(writeToDeviceAction, (void*)address, (void*)adbRegister, (void*)data, (void*)length);
  release();
  return rc;
}
/* static */ IOReturn
KLASS::writeToDeviceAction (OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3)
{
  KLASS * myThis;
  if (myThis = OSDynamicCast(KLASS,owner)) {
    return myThis->writeToDeviceGated((UInt32)arg0, (UInt32) arg1, (UInt8 *) arg2, (IOByteCount *) arg3);
  } else {
    DEBUG_IOLog(1,"writeToDeviceAction could not cast to KLASS\n");
    return kIOReturnError;
  }
}
IOReturn 
KLASS::writeToDeviceGated (UInt32 address, UInt32 adbRegister, UInt8 * data, IOByteCount * length )
{
  IOReturn rc = sendADBCommandToIMate(kUSBOut, ADB_CMD(kADBListen, address, adbRegister), data, length);
  return rc;
}


#pragma mark Low level data transfer

// Send a specific ADB command
IOReturn
KLASS::sendADBCommandToIMate(UInt8 dir, UInt8 command, UInt8 * outData, IOByteCount * outLength, bool waitForData)
{
  IOReturn rc;
  
  rc = iMateWrite(dir, 0x00, command, 0x0000, outLength ? *outLength : 0, outData, waitForData);
  return rc;
}

// Send a low level message
IOReturn
KLASS::iMateWrite(UInt8 dir, UInt8 bRequest, UInt16 wValue, UInt16 wIndex, UInt16 wLength, void * pData, bool waitForData)
{
  return iMateWriteLowLevel(USBmakebmRequestType(dir, kUSBVendor, kUSBInterface),
                            bRequest,
                            wValue,
                            wIndex,
                            wLength,
                            pData,
                            waitForData);
}

// Do the actual USB tranport send.
IOReturn
KLASS::iMateWriteLowLevel(UInt8 bmRequestType, UInt8 bRequest, UInt16 wValue, UInt16 wIndex, UInt16 wLength, void * pData, bool waitForData)
{
  IOUSBDevRequest devReq;
  IOReturn rc;
  DEBUG_IOLog(7,"%s(%p)::iMateWriteLowLevel 0x%02x : 0x%02x, 0x%04x, 0x%04x, 0x%04x, %08p, %s\n", getName(), this, bmRequestType, bRequest, wValue, wIndex, wLength, pData, waitForData ? "sleep" : "continue");
  devReq.bmRequestType = bmRequestType;
  devReq.bRequest = bRequest;
  devReq.wValue = wValue;
  devReq.wIndex = wIndex;
  devReq.wLength = wLength;
  devReq.pData = pData;
  devReq.wLenDone = 0;
  
  if (shuttingDown()) 
    return kIOReturnError;
  
  incrementOutstandingIOGated();
  
  rc = fInterface->DeviceRequest(&devReq, NULL);
  if (rc != kIOReturnSuccess) {
    DEBUG_IOLog(7,"%s(%p)::iMateWriteLowLevel returning 0x%08x : %s\n", getName(), this, rc, stringFromReturn(rc));
    rc = fInterface->DeviceRequest(&devReq, NULL);
    if (rc != kIOReturnSuccess) {
      DEBUG_IOLog(7,"%s(%p)::iMateWriteLowLevel returning 0x%08x : %s\n", getName(), this, rc, stringFromReturn(rc));
      DEBUG_IOLog(7,"%s(%p)::iMateWriteLowLevel sent 0x%08x bytes\n", getName(), this, devReq.wLenDone);
      decrementOutstandingIOGated();
      return rc;
    }
  }
  
  // Sleep the command gate waiting on results coming back or the timeout happening
  commandControlBlock.timerEventSource->setTimeoutMS(kADBTimeoutMilliseconds);  // x milliseconds timeout for ADB command to complete
  commandControlBlock.adbMessageResult = kIOReturnBusy;
  if (waitForData) {
    waitingForData = (UInt8)(wValue & 0xff);
    bzero(commandControlBlock.adbPacket, 10);  // this is still iffy, what if we receive unasked for data when we're asking for data
  } else {
    waitingForData = 0x00;
  }
  commandGate()->commandSleep(&commandControlBlock, THREAD_ABORTSAFE);
  if (!waitForData) commandControlBlock.adbMessageResult = kIOReturnSuccess;
  DEBUG_IOLog(7,"%s(%p)::iMateWriteLowLevel returning 0x%08x : %s\n", getName(), this, rc, stringFromReturn(rc));
  return commandControlBlock.adbMessageResult;
}


// **********************************************************************************
// cancelAllIO
// **********************************************************************************
IOReturn KLASS::cancelAllIO ( void )
{
  DEBUG_IOLog(3,"%s(%p)::cancelAllIO\n", getName(), this);
  fInterruptPipe->Abort();
  // TODO.  do we need to do more?
  
  return ADB_RET_OK;
}


