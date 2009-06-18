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

#include "ADBHIDMouse.h"

#define KLASS ADBHIDMouse
#define SUPER ADBHIDDevice

typedef struct {
  UInt8 buttons;
  SInt8 x;
  SInt8 y;
} ADBHIDMouseReport;

OSDefineMetaClassAndStructors(KLASS, SUPER)

UInt8 KLASS::_reportDescriptor[56] = {
0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
0x0b, 0x02, 0x00, 0x01, 0x00,  // USAGE (Generic Desktop:Mouse)
0xa1, 0x01,                    // COLLECTION (Application)
0x0b, 0x01, 0x00, 0x01, 0x00,  //   USAGE (Generic Desktop:Pointer)
0xa1, 0x00,                    //   COLLECTION (Physical)
0x05, 0x09,                    //     USAGE_PAGE (Button)
0x19, 0x01,                    //     USAGE_MINIMUM (Button 1)
0x29, 0x03,                    //     USAGE_MAXIMUM (Button 3)
0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
0x25, 0x01,                    //     LOGICAL_MAXIMUM (1)
0x95, 0x03,                    //     REPORT_COUNT (3)
0x75, 0x01,                    //     REPORT_SIZE (1)
0x81, 0x02,                    //     INPUT (Data,Var,Abs)
0x95, 0x01,                    //     REPORT_COUNT (1)
0x75, 0x05,                    //     REPORT_SIZE (5)
0x81, 0x03,                    //     INPUT (Cnst,Var,Abs)
0x05, 0x01,                    //     USAGE_PAGE (Generic Desktop)
0x09, 0x30,                    //     USAGE (X)
0x09, 0x31,                    //     USAGE (Y)
0x15, 0x80,                    //     LOGICAL_MINIMUM (-128)
0x25, 0x7f,                    //     LOGICAL_MAXIMUM (127)
0x75, 0x08,                    //     REPORT_SIZE (8)
0x95, 0x02,                    //     REPORT_COUNT (2)
0x81, 0x06,                    //     INPUT (Data,Var,Rel)
0xc0,                          //   END_COLLECTION
0xc0                           // END_COLLECTION
};

IOService *
KLASS::probe(IOService * provider, SInt32 * score) {
  IOLog("%s::probe()\n", getName());
  if (OSDynamicCast(IOADBDevice, provider)) {
    IOLog("%s::probe()\n", getName());
    *score = 1000000;
    return this;
  }
  return NULL;
}

bool
KLASS::handleStart(IOService * nub)
{
  IOLog("%s::handleStart()\n", getName());
  if (!SUPER::handleStart(nub)) {
    return false;
  }
  _hidReport = IOBufferMemoryDescriptor::withCapacity(sizeof(ADBHIDMouseReport), kIODirectionIn);
  return true;
}

IOReturn 
KLASS::handleADBPacket(UInt8 adbCommand, IOByteCount length, UInt8 * adbData) {
  ADBHIDMouseReport * report = (ADBHIDMouseReport*)(_hidReport->getBytesNoCopy());
  bzero(report, sizeof(ADBHIDMouseReport));
  report->y        = (adbData[0] & 0x40) ? (adbData[0] & 0x7f) | 0x80 : adbData[0] & 0x7f; 
  report->x        = (adbData[1] & 0x40) ? (adbData[1] & 0x7f) | 0x80 : adbData[1] & 0x7f;
  // Implementing button handling is left as an exercise for the reader (hint : adbData[0] & 0x80)
  report->buttons  = 0x00;   
  handleReport(_hidReport,kIOHIDReportTypeInput, 0);
  return kIOReturnSuccess;
}

void * KLASS::reportDescriptorBytes() const { return KLASS::_reportDescriptor; }
size_t KLASS::reportDescriptorSize() const { return sizeof(KLASS::_reportDescriptor); };


