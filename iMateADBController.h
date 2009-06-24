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

#include "RemovableADBController.h"
#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/IOTimerEventSource.h>

#define ADB_CMD(command, address, register)  command | (address << kADBAddressField) | register

typedef struct {
  IOReturn             adbMessageResult;
  size_t               adbDataLength;
  UInt8                adbPacket[10];
  IOTimerEventSource * timerEventSource;
} ADBControlBlock;

class iMateADBController : public RemovableADBController
{
  OSDeclareDefaultStructors(iMateADBController)

  enum {
    kADBAddressField = 4
  };
  
  enum {
    kADBResetDevice	= 0x00,
    kADBFlush	= 0x01,
    kADBListen	= 0x08,
    kADBTalk     = 0x0C,
    kADBRWMask	= 0x0C
  };  

protected:
  
  IOUSBInterface  * fInterface;     // The interface
  IOUSBPipe       * fInterruptPipe; // Incoming data pipe
  IOBufferMemoryDescriptor * fInterruptPipeMDP;
  IOUSBCompletionWithTimeStamp   fReadCompletionInfo;
    
  UInt8           * fUSBData;
  
  ADBControlBlock autopollControlBlock;
  ADBControlBlock commandControlBlock;
  
  thread_call_t     fReadThread;

  UInt32		        pollList;		// ADB autopoll device bitmap
  UInt32            autoPollMilliseconds;
  UInt32            packetSize; // Packet size as returned from the device
  bool              autopollOn;
  UInt8             waitingForData;
  
  bool              startingUp;
	
protected:

  IOReturn sendADBCommandToIMate(UInt8 direction, UInt8 command, UInt8 * outData, IOByteCount * outLength, bool waitForResults = false);
  IOReturn iMateWrite(UInt8 direction, UInt8 bRequest, UInt16 wValue, UInt16 wIndex, UInt16 wLength, void * pData, bool waitForResults = false);
  IOReturn iMateWriteLowLevel(UInt8 bmRequestType, UInt8 bRequest, UInt16 wValue, UInt16 wIndex, UInt16 wLength, void * pData, bool waitForResults = false);

  static void autopollTimeoutHandler(OSObject * object, IOTimerEventSource * source);
  void autopollTimeout(); 
  static void commandTimeoutHandler(OSObject * object, IOTimerEventSource * source);
  void commandTimeout(); 
  
  void signalData(ADBControlBlock * controlBlock);
  
  static void readThreadHandler(thread_call_param_t owner, void *);
  static void readCompleteCallback(void * owner, void * parameter, IOReturn rc, UInt32 bufferSizeRemaining, AbsoluteTime timestamp);
  static IOReturn readCompleteAction(OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3);
  IOReturn readComplete(IOReturn rc, UInt32 bufferSizeRemaining);

  virtual IOReturn probeBus();
  
  // Power management
  virtual IOPMPowerState * myPowerStates();
  virtual unsigned long myNumberOfPowerStates();
  virtual IOReturn setPowerState(unsigned long powerStateOrdinal, IOService * device);
  static IOReturn setPowerStateAction(OSObject * owner, void * arg0, void * arg1, void * arg2, void * arg3);
  virtual IOReturn setPowerStateGated(unsigned long powerStateOrdinal, IOService * device);
  
  virtual IOReturn closeDevices();
public:
  IOReturn queueRead();
  
  // General housekeeping
  virtual bool init(OSDictionary * properties);
  virtual bool start ( IOService * );
  virtual bool willTerminate(IOService * provider, IOOptionBits options);
  virtual bool didTerminate(IOService * provider, IOOptionBits options, bool * defer);
  virtual void stop ( IOService * );
  
  virtual IOReturn setAutoPollPeriod ( int microseconds );
  virtual IOReturn getAutoPollPeriod ( int * microseconds );
  virtual IOReturn setAutoPollList ( UInt16 activeAddressMask );
  virtual IOReturn getAutoPollList ( UInt16 * activeAddressMask );
  virtual IOReturn setAutoPollEnable ( bool enable );
  virtual IOReturn resetBus ( void );
  virtual IOReturn cancelAllIO ( void );
  virtual IOReturn flushDevice ( IOADBAddress address );
  virtual IOReturn readFromDevice ( IOADBAddress address, IOADBRegister adbRegister, UInt8 * data, IOByteCount * length );
  virtual IOReturn writeToDevice ( IOADBAddress address, IOADBRegister adbRegister, UInt8 * data, IOByteCount * length );
  
  // static gate stubs
  static IOReturn setAutoPollListAction (OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3);
  static IOReturn setAutoPollEnableAction (OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3);
  static IOReturn resetBusAction (OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3);
  static IOReturn flushDeviceAction (OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3);
  static IOReturn readFromDeviceAction (OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3);
  static IOReturn writeToDeviceAction (OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3);
  
  // And finally, the underlying gated methods
  virtual IOReturn setAutoPollListGated ( UInt32 activeAddressMask );
  virtual IOReturn setAutoPollEnableGated ( bool enable );
  virtual IOReturn resetBusGated ( void );
  virtual IOReturn flushDeviceGated ( UInt32 address );
  virtual IOReturn readFromDeviceGated ( UInt32 address, UInt32 adbRegister, UInt8 * data, IOByteCount * length );
  virtual IOReturn writeToDeviceGated ( UInt32 address, UInt32 adbRegister, UInt8 * data, IOByteCount * length );
  
};
