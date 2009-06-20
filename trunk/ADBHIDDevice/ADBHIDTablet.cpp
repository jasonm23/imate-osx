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

#include "ADBHIDTablet.h"

#define KLASS ADBHIDTablet
#define SUPER ADBHIDDevice
OSDefineMetaClassAndStructors(KLASS, SUPER)

typedef struct {
  UInt8 report_id;
  UInt16 x;
  UInt16 y;
  UInt8 tip_pressure;
  SInt8 x_tilt;
  SInt8 y_tilt;
  UInt8 flags; 
} ADBHIDTabletStylusReport;

typedef struct {
  UInt8 report_id;
  UInt16 x;
  UInt16 y;
  UInt8 flags; 
} ADBHIDTabletPuckReport;

typedef struct {
  UInt8 report_id;
  UInt8 key_index;
} ADBHIDTabletButtonsReport;

UInt8 KLASS::_reportDescriptor[] = {
  0x05, 0x0d,                    // USAGE_PAGE (Digitizers)
  0x09, 0x01,                    // Usage (Digitizer)
  0xa1, 0x01,                    // COLLECTION (Application)
  0x85, 0x01,                    //   REPORT_ID (1)
  0x05, 0x0d,                    //   USAGE_PAGE (Digitizers)
  0x09, 0x20,                    //   USAGE (Stylus)
  0xa1, 0x00,                    //   COLLECTION (Physical)
  0x09, 0x00,                    //     USAGE (Undefined)
  0x95, 0x01,                    //     REPORT_COUNT (1)
  0x75, 0x08,                    //     REPORT_SIZE (8)
  0x81, 0x03,                    //     INPUT (Cnst,Var,Abs)
  0x05, 0x01,                    //     USAGE_PAGE (Generic Desktop)
  0x09, 0x30,                    //     USAGE (X)
  0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
  0x26, 0x00, 0x50,              //     LOGICAL_MAXIMUM (10240)
  0x35, 0x00,                    //     PHYSICAL_MINIMUM (0)
  0x46, 0x00, 0x50,              //     PHYSICAL_MAXIMUM (10240)
  0x65, 0x11,                    //     UNIT (SI Lin:Distance)
  0x55, 0x0d,                    //     UNIT_EXPONENT (-3)
  0x95, 0x01,                    //     REPORT_COUNT (1)
  0x75, 0x10,                    //     REPORT_SIZE (16)
  0x81, 0x02,                    //     INPUT (Data,Var,Abs)
  0x09, 0x31,                    //     USAGE (Y)
  0x26, 0x00, 0x3c,              //     LOGICAL_MAXIMUM (10240)
  0x46, 0x00, 0x3c,              //     PHYSICAL_MAXIMUM (10240)
  0x95, 0x01,                    //     REPORT_COUNT (1)
  0x75, 0x10,                    //     REPORT_SIZE (16)
  0x81, 0x02,                    //     INPUT (Data,Var,Abs)
  0x05, 0x0d,                    //     USAGE_PAGE (Digitizers)
  0x09, 0x30,                    //     USAGE (Tip Pressure)
  0x15, 0x80,                    //     LOGICAL_MINIMUM (-128)
  0x25, 0x7f,                    //     LOGICAL_MAXIMUM (127)
  0x35, 0x00,                    //     PHYSICAL_MINIMUM (0)
  0x45, 0xff,                    //     PHYSICAL_MAXIMUM (255)
  0x66, 0x11, 0xe1,              //     UNIT (SI Lin:Force)
  0x55, 0x0d,                    //     UNIT_EXPONENT (-30)
  0x95, 0x01,                    //     REPORT_COUNT (1)
  0x75, 0x08,                    //     REPORT_SIZE (8)
  0x81, 0x02,                    //     INPUT (Data,Var,Abs)
  0x09, 0x3d,                    //     USAGE (X Tilt)
  0x09, 0x3e,                    //     USAGE (Y Tilt)
  0x15, 0x80,                    //     LOGICAL_MINIMUM (-128)
  0x25, 0x7f,                    //     LOGICAL_MAXIMUM (127)
  0x35, 0xc4,                    //     PHYSICAL_MINIMUM (-60)
  0x45, 0x3c,                    //     PHYSICAL_MAXIMUM (60)
  0x65, 0x14,                    //     UNIT (Eng Rot:Angular Pos)
  0x55, 0x00,                    //     UNIT_EXPONENT (0)
  0x95, 0x02,                    //     REPORT_COUNT (2)
  0x75, 0x08,                    //     REPORT_SIZE (8)
  0x81, 0x02,                    //     INPUT (Data,Var,Abs)
  0x09, 0x32,                    //     USAGE (In Range)
  0x09, 0x42,                    //     USAGE (Tip Switch)
  0x09, 0x44,                    //     USAGE (Barrel Switch)
  0x09, 0x45,                    //     USAGE (Eraser)
  0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
  0x25, 0x01,                    //     LOGICAL_MAXIMUM (1)
  0x35, 0x00,                    //     PHYSICAL_MINIMUM (0)
  0x45, 0x01,                    //     PHYSICAL_MAXIMUM (1)
  0x65, 0x00,                    //     UNIT (None)
  0x95, 0x04,                    //     REPORT_COUNT (4)
  0x75, 0x01,                    //     REPORT_SIZE (1)
  0x81, 0x02,                    //     INPUT (Data,Var,Abs)
  0x09, 0x38,                    //     USAGE (Transducer Index)
  0x25, 0x0f,                    //     Logical Maximum 15
  0x95, 0x01,                    //     REPORT_COUNT (1)
  0x75, 0x04,                    //     REPORT_SIZE (4)
  0x81, 0x02,                    //     INPUT (Data,Var,Abs)
  0xc0,                          //   END_COLLECTION
  0x85, 0x02,                    //   REPORT_ID (2)
  0x05, 0x0d,                    //   USAGE_PAGE (Digitizers)
  0x09, 0x21,                    //   USAGE (Puck)
  0xa1, 0x00,                    //   COLLECTION (Physical)
  0x09, 0x00,                    //     USAGE (Undefined)
  0x95, 0x01,                    //     REPORT_COUNT (1)
  0x75, 0x08,                    //     REPORT_SIZE (8)
  0x81, 0x03,                    //     INPUT (Cnst,Var,Abs)
  0x05, 0x01,                    //     USAGE_PAGE (Generic Desktop)
  0x09, 0x30,                    //     USAGE (X)
  0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
  0x26, 0x00, 0x50,              //     LOGICAL_MAXIMUM (10240)
  0x95, 0x01,                    //     REPORT_COUNT (1)
  0x75, 0x10,                    //     REPORT_SIZE (16)
  0x81, 0x02,                    //     INPUT (Data,Var,Abs)
  0x09, 0x31,                    //     USAGE (Y)
  0x26, 0x00, 0x3c,              //     LOGICAL_MAXIMUM (10240)
  0x95, 0x01,                    //     REPORT_COUNT (1)
  0x75, 0x10,                    //     REPORT_SIZE (16)
  0x81, 0x02,                    //     INPUT (Data,Var,Abs)
  0x05, 0x09,                    //     USAGE_PAGE (Button)
  0x19, 0x01,                    //     USAGE_MINIMUM (Button 1)
  0x29, 0x04,                    //     USAGE_MAXIMUM (Button 4)
  0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
  0x25, 0x01,                    //     LOGICAL_MAXIMUM (1)
  0x95, 0x04,                    //     REPORT_COUNT (4)
  0x75, 0x01,                    //     REPORT_SIZE (1)
  0x81, 0x02,                    //     INPUT (Data,Var,Abs)  
  0x05, 0x0d,                    //     USAGE_PAGE (Digitizers)
  0x09, 0x38,                    //     USAGE (Transducer Index)
  0x25, 0x0f,                    //     Logical Maximum 15
  0x95, 0x01,                    //     REPORT_COUNT (1)
  0x75, 0x04,                    //     REPORT_SIZE (4)
  0x81, 0x02,                    //     INPUT (Data,Var,Abs)
  0xc0,                          //   END_COLLECTION
  0x85, 0x03,                    //   REPORT_ID (3)
  0x05, 0x0d,                    //   USAGE_PAGE (Digitizers)
  0x09, 0x39,                    //   USAGE (Tablet Function Keys)
  0xa1, 0x00,                    //   COLLECTION (Physical)
  0x09, 0x00,                    //     USAGE (Undefined)
  0x95, 0x01,                    //     REPORT_COUNT (1)
  0x75, 0x08,                    //     REPORT_SIZE (8)
  0x81, 0x03,                    //     INPUT (Cnst,Var,Abs)
  0x09, 0x39,                    //     USAGE (Tablet Function Keys)
  0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
  0x25, 0x20,                    //     LOGICAL_MAXIMUM (32)
  0x95, 0x01,                    //     REPORT_COUNT (1)
  0x75, 0x08,                    //     REPORT_SIZE (8)
  0x81, 0x02,                    //     INPUT (Data,Var,Abs)
  0xc0,                          //   END_COLLECTION
  0xc0                           // END_COLLECTION
};

// Defines for Wacom structures 
#define kWacomPFKeyMask     0xd0
#define kWacomPFKeyFlags    0x90
#define kWacomProximityMask 0x40
#define kWacomToolTypeMask  0x20
#define kWacomClickMask     0x10
#define kWacomButtonMask    0x0f

// Stylus per Wacom
#define kWacomTipSwitchMask       0x01
#define kWacomSideSwitch1Mask     0x02
#define kWacomSideSwitch2Mask     0x04
#define kWacomEraserProximityFlag 0x04

// Flags for the stylus report
// Sub-byte measures fill low order first, so we get the proximity flag first, 
// then the tip switch, etc etc
#define kStylusProximityFlag  0x01
#define kStylusTipSwitchFlag  0x02
#define kStylusSideSwitchFlag 0x04
#define kStylusEraserFlag     0x08

bool
KLASS::handleStart(IOService * nub)
{
  IOLog("%s::handleStart()\n", getName());
  if (!SUPER::handleStart(nub)) {
    return false;
  }
  _stylusReport = IOBufferMemoryDescriptor::withCapacity(sizeof(ADBHIDTabletStylusReport), kIODirectionIn);
  _puckReport = IOBufferMemoryDescriptor::withCapacity(sizeof(ADBHIDTabletPuckReport), kIODirectionIn);
  _buttonsReport = IOBufferMemoryDescriptor::withCapacity(sizeof(ADBHIDTabletButtonsReport), kIODirectionIn);
  return true;
}

void
KLASS::free()
{
  IOLog("%s::free()\n", getName());
  if (_stylusReport) {
    _stylusReport->release();
    _stylusReport = NULL;
  }
  if (_buttonsReport) {
    _buttonsReport->release();
    _buttonsReport = NULL;
  }
  if (_puckReport) {
    _puckReport->release();
    _puckReport = NULL;
  }
  SUPER::free();
}

IOReturn 
KLASS::handleADBPacket(UInt8 adbCommand, IOByteCount length, UInt8 * adbData) {
  // Our packets should always be 8 bytes long
  if (length != 8) {
    IOLog("%s::handleADBPacket() got a packet of length %l, expecting 8\n", getName(), length);
    return kIOReturnError;
  }
  // Determine what type of packet we're dealing with.
  if ((adbData[0] & kWacomPFKeyMask) == kWacomPFKeyFlags) {  // Tablet function key packet
    IOLog("%s::handleADBPacket() got a function key packet\n", getName());

  } else {                            // either stylus or puck
    bool proximity = adbData[0] & kWacomProximityMask;
    bool stylus       = adbData[0] & kWacomToolTypeMask;
    bool click        = adbData[0] & kWacomClickMask;
    UInt8 buttons     = adbData[0] & kWacomButtonMask;
    UInt16 x          = OSSwapHostToLittleInt16( adbData[1] * 256 + adbData[2] );
    UInt16 y          = OSSwapHostToLittleInt16( adbData[3] * 256 + adbData[4] );
    if (stylus) {
      ADBHIDTabletStylusReport * report = (ADBHIDTabletStylusReport*)(_stylusReport->getBytesNoCopy());
      bzero(report, sizeof(ADBHIDTabletStylusReport));
      report->report_id    = 1;
      
      report->x            = x;
      report->y            = y;
      report->tip_pressure = (SInt8)adbData[5];
      report->x_tilt       = (SInt8)adbData[6];
      report->y_tilt       = (SInt8)adbData[7];
      report->flags        = 0x10; // Transducer 1
      if (proximity) {
        report->flags |= kStylusProximityFlag;
        // Determine stylus attitude if this is the first entry into proximity
        if (!_stylusInProximity) {
          _stylusInverted = buttons & kWacomEraserProximityFlag;
        }
        if (_stylusInverted) report->flags |= kStylusEraserFlag;
        if (buttons & kWacomTipSwitchMask) report->flags |= kStylusTipSwitchFlag;
        if (buttons & kWacomSideSwitch1Mask) report->flags |= kStylusSideSwitchFlag;
      }
      _stylusInProximity = proximity;
      handleReport(_stylusReport, kIOHIDReportTypeInput, 0);
    } else {
      ADBHIDTabletPuckReport * report = (ADBHIDTabletPuckReport*)(_puckReport->getBytesNoCopy());
      bzero(report, sizeof(ADBHIDTabletPuckReport));
      report->report_id    = 2;
      report->x            = x;
      report->y            = y;
      report->flags        = 0x20; // Transducer 2
      report->flags        |= buttons;
      handleReport(_puckReport, kIOHIDReportTypeInput, 0);
    }
  }
  
  
  
  return kIOReturnSuccess;
}

void * KLASS::reportDescriptorBytes() const { return KLASS::_reportDescriptor; }
size_t KLASS::reportDescriptorSize() const { return sizeof(KLASS::_reportDescriptor); };


