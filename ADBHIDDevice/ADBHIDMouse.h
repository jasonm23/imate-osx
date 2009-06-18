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
#ifndef _ADBHIDMOUSE_H_
#define _ADBHIDMOUSE_H_

#include "ADBHIDDevice.h"

class ADBHIDMouse : public ADBHIDDevice 
{
  OSDeclareDefaultStructors(ADBHIDMouse)
public:
  virtual IOService * probe(IOService * provider, SInt32 * score);
  virtual bool handleStart (IOService * nub);
  virtual void free ();
  
  virtual IOReturn handleADBPacket(UInt8 adbCommand, IOByteCount length, UInt8 * adbData);

  virtual void * reportDescriptorBytes() const;
  virtual size_t reportDescriptorSize() const;
  
  virtual OSString * newManufacturerString() const { return OSString::withCString("Tufty"); };
  virtual OSNumber * newPrimaryUsageNumber() const { return OSNumber::withNumber(0x02,8); };
  virtual OSNumber * newPrimaryUsagePageNumber() const { return OSNumber::withNumber(0x01,8); };
  virtual OSNumber * newProductIDNumber() const {return OSNumber::withNumber(0x0101,16);};
  virtual OSString * newProductString() const { return OSString::withCString("ADB Mouse"); };
  virtual OSNumber * newVendorIDNumber() const { return OSNumber::withNumber(0xfeed,16); };
  virtual OSNumber * newLocationIDNumber() const {return OSNumber::withNumber(0x0302,16); };
  
protected:
  static UInt8 _reportDescriptor[];
  IOBufferMemoryDescriptor * _hidReport;
  
};

#endif