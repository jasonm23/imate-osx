/*
 *  ADBHIDUltraSlate.cpp
 *  UltraSlate
 *
 *  Created by Leonardo Hirokazu Hamada on 25/11/10.
 *  Copyright 2010 __MyCompanyName__. All rights reserved.
 *  sudo xcodebuild -project UltraSlate.xcodeproj  -configuration Release DSTROOT=/ install
 *  xcodebuild -project UltraSlate.xcodeproj  -configuration Release DSTROOT=/ clean build
 */

#include "ADBHIDUltraSlate.h"

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

//#include "ADBHIDUltraSlate.h"

#define KLASS ADBHIDUltraSlate
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
} ADBHIDUltraSlateStylusReport;

typedef struct {
  UInt8 report_id;
  UInt16 x;
  UInt16 y;
  UInt8 flags; 
} ADBHIDUltraSlatePuckReport;

typedef struct {
  UInt8 report_id;
  UInt8 button;
} ADBHIDUltraSlateButtonsReport;

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
  0x09, 0x3c,                    //     USAGE (Invert)
  0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
  0x25, 0x01,                    //     LOGICAL_MAXIMUM (1)
  0x35, 0x00,                    //     PHYSICAL_MINIMUM (0)
  0x45, 0x01,                    //     PHYSICAL_MAXIMUM (1)
  0x65, 0x00,                    //     UNIT (None)
  0x95, 0x05,                    //     REPORT_COUNT (5)
  0x75, 0x01,                    //     REPORT_SIZE (1)
  0x81, 0x02,                    //     INPUT (Data,Var,Abs)
  0x09, 0x38,                    //     USAGE (Transducer Index)
  0x25, 0x07,                    //     Logical Maximum 7
  0x95, 0x01,                    //     REPORT_COUNT (1)
  0x75, 0x03,                    //     REPORT_SIZE (3)
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
  0x35, 0x00,                    //     PHYSICAL_MINIMUM (0)
  0x46, 0x00, 0x50,              //     PHYSICAL_MAXIMUM (10240)
  0x65, 0x11,                    //     UNIT (SI Lin:Distance)
  0x55, 0x0d,                    //     UNIT_EXPONENT (-3)
  0x95, 0x01,                    //     REPORT_COUNT (1)
  0x75, 0x10,                    //     REPORT_SIZE (16)
  0x81, 0x02,                    //     INPUT (Data,Var,Abs)
  0x09, 0x31,                    //     USAGE (Y)
  0x26, 0x00, 0x3c,              //     LOGICAL_MAXIMUM (10240)
  0x46, 0x00, 0x50,              //     PHYSICAL_MAXIMUM (10240)
  0x95, 0x01,                    //     REPORT_COUNT (1)
  0x75, 0x10,                    //     REPORT_SIZE (16)
  0x81, 0x02,                    //     INPUT (Data,Var,Abs)
  0x05, 0x09,                    //     USAGE_PAGE (Button)
  0x19, 0x01,                    //     USAGE_MINIMUM (Button 1)
  0x29, 0x04,                    //     USAGE_MAXIMUM (Button 4)
  0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
  0x25, 0x01,                    //     LOGICAL_MAXIMUM (1)
  0x35, 0x00,                    //     PHYSICAL_MINIMUM (0)
  0x45, 0x01,                    //     PHYSICAL_MAXIMUM (1)
  0x65, 0x00,                    //     UNIT (None)
  0x55, 0x00,                    //     UNIT_EXPONENT (0)
  0x95, 0x04,                    //     REPORT_COUNT (4)
  0x75, 0x01,                    //     REPORT_SIZE (1)
  0x81, 0x02,                    //     INPUT (Data,Var,Abs) 
  0x09, 0x32,                    //     USAGE (In Range)
  0x95, 0x01,                    //     REPORT_COUNT (1)
  0x75, 0x01,                    //     REPORT_SIZE (1)
  0x81, 0x02,                    //     INPUT (Data,Var,Abs)  
  0x05, 0x0d,                    //     USAGE_PAGE (Digitizers)
  0x09, 0x38,                    //     USAGE (Transducer Index)
  0x25, 0x07,                    //     Logical Maximum 7
  0x95, 0x01,                    //     REPORT_COUNT (1)
  0x75, 0x03,                    //     REPORT_SIZE (3)
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

#define STYLUS_LOGICAL_MAX_X_LO  29
#define STYLUS_LOGICAL_MAX_X_HI  30
#define STYLUS_PHYSICAL_MAX_X_LO 34
#define STYLUS_PHYSICAL_MAX_X_HI 35
#define STYLUS_LOGICAL_MAX_Y_LO  49
#define STYLUS_LOGICAL_MAX_Y_HI  50
#define STYLUS_PHYSICAL_MAX_Y_LO 52
#define STYLUS_PHYSICAL_MAX_Y_HI 53
#define LOGICAL_MINIMUM_PRESSURE 65
#define PUCK_LOGICAL_MAX_X_LO    165
#define PUCK_LOGICAL_MAX_X_HI    166
#define PUCK_PHYSICAL_MAX_X_LO   170
#define PUCK_PHYSICAL_MAX_X_HI   171
#define PUCK_LOGICAL_MAX_Y_LO    185
#define PUCK_LOGICAL_MAX_Y_HI    186
#define PUCK_PHYSICAL_MAX_Y_LO   188
#define PUCK_PHYSICAL_MAX_Y_HI   189



// Defines for UltraSlate structures 
//#define kUltraSlatePFKeyMask     0xd0
//#define kUltraSlatePFKeyFlags    0x90
#define kUltraSlateProximityMask 0x80
#define kToolProximityFlag       0x80

#define kUltraSlateToolTypeMask  0x07
#define kStylusToolFlag       0
#define kMouseToolFlag        0x01
#define k16ButtonToolFlag     0x03

#define kUltraSlateClickMask     0x10
#define kUltraSlateButtonMask    0x1e

// Stylus per UltraSlate
#define kUltraSlateTipSwitchMask       0x01
#define kUltraSlateSideSwitch1Mask     0x02
#define kUltraSlateSideSwitch2Mask     0x04
//#define kUltraSlateEraserProximityFlag 0x04

// Flags for the stylus report
// Sub-byte measures fill low order first, so we get the proximity flag first, 
// then the tip switch, etc etc
#define kStylusProximityFlag  0x01
#define kStylusTipSwitchFlag  0x02
#define kStylusSideSwitchFlag 0x04
#define kStylusEraserFlag     0x08
#define kStylusInvertFlag     0x10



// Puck report
#define kPuckProximityFlag    0x10

IOService *
KLASS::probe(IOService * provider, SInt32 * score) {
  DEBUG_IOLog(3, "%s::probe()\n", getName());
  if (OSDynamicCast(IOADBDevice, provider)) {
    DEBUG_IOLog(3, "%s::probe()\n", getName());
    *score = 1000000;
    return this;
  }
  return NULL;
}

bool
KLASS::handleStart(IOService * nub)
{
  DEBUG_IOLog(3, "%s::handleStart()\n", getName());
  if (!SUPER::handleStart(nub)) {
    return false;
  }
  _stylusReport = IOBufferMemoryDescriptor::withCapacity(sizeof(ADBHIDUltraSlateStylusReport), kIODirectionIn);
  _puckReport = IOBufferMemoryDescriptor::withCapacity(sizeof(ADBHIDUltraSlatePuckReport), kIODirectionIn);
  _buttonsReport = IOBufferMemoryDescriptor::withCapacity(sizeof(ADBHIDUltraSlateButtonsReport), kIODirectionIn);
    
  return true;
}

bool 
KLASS::bringUpADBDevice() 
{
  DEBUG_IOLog(3, "%s::bringUpADBDevice()\n", getName());
  UInt8 register_[8];
  IOByteCount adbDataLength;
  IOReturn rc;
  
  if (SUPER::bringUpADBDevice()) {
	// Read register 1 and perhaps register 2 to get base information
	// register_[0] contains the product type. Type = 1 is Ultraslate; type = 2 is ADB box; type = 3 creation station; type = 4 creation station pro, design station pro
	// GTCO CalComp ADB Function Spec says that devices before 7/1/98 enters the tablet (absolute) mode when talk r1 is received. Units afterwards switches with talk r2.
	adbDataLength = 0;
    if (((rc = adbDevice()->readRegister(1, register_, &adbDataLength)) != kIOReturnSuccess) || (adbDataLength != 5)) {
		DEBUG_IOLog(3, "%s failed to read register 1 in bringUpADBDevice, rc was %s, adbDataLength was %d\n, trying register 2...", getName(), stringFromReturn(rc) , adbDataLength);
		if (((rc = adbDevice()->readRegister(2, register_, &adbDataLength)) != kIOReturnSuccess) || (adbDataLength != 5)) {
			DEBUG_IOLog(3, "%s failed to read register 2 in bringUpADBDevice, rc was %s, adbDataLength was %d\n", getName(), stringFromReturn(rc) , adbDataLength);
			return false;
		}
    }
	
	// Set tilt defaults.
	_last_x_tilt = 0;
	_last_y_tilt = 0;
	// Set pressure to negative (-128) value. I think this shoul be read from the HID report default minimum value . Not hard coded. To be fixed.
    _last_tip_pressure = _reportDescriptor[LOGICAL_MINIMUM_PRESSURE];
	// Set to true to allow the driver to send pressure data. 
	_send_pressure_information = true;
	
    // Futz the descriptor to match our device
	_max_x = 1000 * ( register_[3] / 2 ) - 1;
	_max_y = 1000 * ( register_[4] / 2 ) - 1;
	
	UInt8 max_x_lo = (UInt8)(_max_x & 0xff);
	UInt8 max_x_hi = (UInt8)(_max_x >> 8);
	UInt8 max_y_lo = (UInt8)(_max_y & 0xff);
	UInt8 max_y_hi = (UInt8)(_max_y >> 8);

    _reportDescriptor[STYLUS_LOGICAL_MAX_X_LO] = max_x_lo;
    _reportDescriptor[STYLUS_LOGICAL_MAX_X_HI] = max_x_hi;
    _reportDescriptor[STYLUS_LOGICAL_MAX_Y_LO] = max_y_lo;
    _reportDescriptor[STYLUS_LOGICAL_MAX_Y_HI] = max_y_hi;
    _reportDescriptor[STYLUS_PHYSICAL_MAX_X_LO] = max_x_lo;
    _reportDescriptor[STYLUS_PHYSICAL_MAX_X_HI] = max_x_hi;
    _reportDescriptor[STYLUS_PHYSICAL_MAX_Y_LO] = max_y_lo;
    _reportDescriptor[STYLUS_PHYSICAL_MAX_Y_HI] = max_y_hi;
    _reportDescriptor[PUCK_LOGICAL_MAX_X_LO] = max_x_lo;
    _reportDescriptor[PUCK_LOGICAL_MAX_X_HI] = max_x_hi;
    _reportDescriptor[PUCK_LOGICAL_MAX_Y_LO] = max_y_lo;
    _reportDescriptor[PUCK_LOGICAL_MAX_Y_HI] = max_y_hi;
    _reportDescriptor[PUCK_PHYSICAL_MAX_X_LO] = max_x_lo;
    _reportDescriptor[PUCK_PHYSICAL_MAX_X_HI] = max_x_hi;
    _reportDescriptor[PUCK_PHYSICAL_MAX_Y_LO] = max_y_lo;
    _reportDescriptor[PUCK_PHYSICAL_MAX_Y_HI] = max_y_hi;
    
    return true;
  }
  return false;
}


void
KLASS::free()
{
  DEBUG_IOLog(3, "%s::free()\n", getName());
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
  // Our packets should always be at least 6 bytes long, can be 8 buytes too, if enabled (how?).
  if (length < 6) {
    DEBUG_IOLog(3, "%s::handleADBPacket() got a packet of length %d, expecting 6\n", getName(), length);
    DEBUG_IOLog(3, "%s::handleADBPacket() - 0x%02x, 0x%02x\n", getName(), adbData[0], adbData[1]);
    return kIOReturnError;
  }
  // Determine what type of packet we're dealing with.
//  if ((adbData[0] & kWacomPFKeyMask) == kWacomPFKeyFlags) {  // Tablet function key packet
//    ADBHIDTabletButtonsReport * report = (ADBHIDTabletButtonsReport*)(_buttonsReport->getBytesNoCopy());
//    bzero(report, sizeof(ADBHIDTabletButtonsReport));
//    report->report_id = 3;
//    report->button    = adbData[2];
//    handleReport(_buttonsReport, kIOHIDReportTypeInput, 0);
//  } else {                            
	// Always either stylus or puck
    bool proximity = (adbData[0] & kToolProximityFlag) != 0;
	//Transducer (tool) type. 0b000 = pressure pen (stylus); 0b001 = mouse; 0b011 = 16 button cursor.
//    bool stylus       = ( adbData[5] & kUltraSlateToolTypeMask ) == kStylusToolFlag;
//    UInt8 buttons     = adbData[4] & kUltraSlateButtonMask;
    UInt8 buttons     = (adbData[4] >> 1) & 0x0f; 
    UInt16 x          = OSSwapHostToLittleInt16( adbData[2] * 256 + adbData[3] );
	// In this tablet, y grows away from user. Invert it.
    UInt16 y          = OSSwapHostToLittleInt16( _max_y - ( ( adbData[0] & 0x7f ) * 256 + adbData[1] ) );
//    if (stylus) {
    if (true) {
      ADBHIDUltraSlateStylusReport * report = (ADBHIDUltraSlateStylusReport*)(_stylusReport->getBytesNoCopy());
      bzero(report, sizeof(ADBHIDUltraSlateStylusReport));
      report->report_id    = 1;
      report->x            = x;
      report->y            = y; 
	  report->y_tilt = _last_y_tilt;
	  report->x_tilt = _last_x_tilt;
	  if (_send_pressure_information) {
		report->tip_pressure = _last_tip_pressure;
	  } else {
	    report->tip_pressure = _reportDescriptor[LOGICAL_MINIMUM_PRESSURE];		
	  }
	  if ((adbData[4] >> 6) == 2) {        // Tip pressure data.
		if (_send_pressure_information) { 
			// Update the pressure value.
			UInt16 pressure = (adbData[4] & 0x01) * 256 + adbData[5];
			// Mapping 512 pressure levels to 256.
			report->tip_pressure = ((SInt8)_reportDescriptor[LOGICAL_MINIMUM_PRESSURE]) + (pressure / 2); 
			_last_tip_pressure = report->tip_pressure;
		}
	  } else if ((adbData[4] >> 6) == 1) { // Pen height data.
		if (_send_pressure_information) { 
			// Mapping 256 height level as negative pressure levels, from 0 to -128.
			//report->tip_pressure = -((SInt8)(adbData[5] / 2));
			//_last_tip_pressure = report->tip_pressure;
			if (adbData[5] > 0) {
			    // If the stylus tip is not touching the drawing surface, report minimum negative pressure value.
				report->tip_pressure = _reportDescriptor[LOGICAL_MINIMUM_PRESSURE];	
				//report->tip_pressure = -((SInt8)(adbData[5] / 2));
				_last_tip_pressure = report->tip_pressure;
			}
		}		
	  } else if ((adbData[4] >> 6) == 0) { // Stylus tilt data.
	    if ((adbData[5] >> 7) == 0) {
          report->x_tilt       = (adbData[5] & 0x40) ? (SInt8)((adbData[5] & 0x7f)|0x80) : (adbData[5] & 0x7f);
		  report->x_tilt = 2 * report->x_tilt;
		  _last_x_tilt = report->x_tilt;
		} else {
		  // Tilt must be negative, when pen is tilting away from user.
          report->y_tilt       = (adbData[5] & 0x40) ? -((SInt8)((adbData[5] & 0x7f)|0x80)) : -((SInt8)(adbData[5] & 0x7f));
		  report->y_tilt = 2 * report->y_tilt;
		  _last_y_tilt = report->y_tilt;
	    }
	  }
      report->flags        = 0x20; // Transducer 1
      if (proximity) {
        report->flags |= kStylusProximityFlag;
        // Determine stylus attitude if this is the first entry into proximity
//        if (!_stylusInProximity) {
//          _stylusInverted = buttons & kWacomEraserProximityFlag;
//        }
//      if (_stylusInverted) report->flags |= kStylusInvertFlag;
        //report->flags |= kStylusInvertFlag;

		// When in pressure mode report, don't send tip click event.
        if ((buttons & kUltraSlateTipSwitchMask) && (!_send_pressure_information)) report->flags |= kStylusTipSwitchFlag;
        if (buttons & kUltraSlateSideSwitch1Mask) report->flags |= kStylusSideSwitchFlag;
        // This is a bit naughty, but IOHIDEventDriver maps the Eraser element to button 3 for some insane reason
        if (buttons & kUltraSlateSideSwitch2Mask) report->flags |= kStylusEraserFlag;
        //if (buttons & kUltraSlateSideSwitch2Mask) report->flags |= kStylusEraserFlag;
      } else {
	  // Special section to allow the user to enable/disable (toggle) tip pressure report.
	  // How it works: When the user is out of proximity and the pen is still in reach of the tablet,
	  // he/she can enable or disable tip pressure report by pressing side switch 1 on the stylus.
		if (buttons & kUltraSlateSideSwitch1Mask) _send_pressure_information = ! _send_pressure_information;
	  }
      _stylusInProximity = proximity;
      handleReport(_stylusReport, kIOHIDReportTypeInput, 0);
    } else {
      ADBHIDUltraSlatePuckReport * report = (ADBHIDUltraSlatePuckReport*)(_puckReport->getBytesNoCopy());
      bzero(report, sizeof(ADBHIDUltraSlatePuckReport));
      report->report_id = 2;
      report->x         = x;
      report->y         = y;
      report->flags     = 0x40; // Transducer 2
      report->flags     |= buttons;
      report->flags     |= proximity ? kPuckProximityFlag : 0x00;
      handleReport(_puckReport, kIOHIDReportTypeInput, 0);
    }
//}
  
  return kIOReturnSuccess;
}

void * KLASS::reportDescriptorBytes() const { return KLASS::_reportDescriptor; }
size_t KLASS::reportDescriptorSize() const { return sizeof(KLASS::_reportDescriptor); };