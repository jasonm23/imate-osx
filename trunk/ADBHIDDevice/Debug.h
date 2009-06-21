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

// Define one, other, or both, of LOGGING and DATA_LOGGING for log output.
//#define LOGGING
//#define LOG_LEVEL 7

//#define DATA_LOGGING
//#define DATA_LOG_LEVEL 3


// defining USBLOG will pass messages through the USB debug stack, where they can be picked up
// with USB Prober
//#define USBLOG 

// Defining FIRELOG requires the FireLog extension to be installed, and will pass messages
// through the firewire stack to this or another machine, for pickup.
// Must add com.apple.FireLog 2.0 to the OSBundleLibraries entry in Info.plist as well.
//#define FIRELOG

#ifdef USBLOG
#include <IOKit/usb/IOUSBLog.h>
#endif

#ifdef FIRELOG
#include "/System/Library/Frameworks/Kernel.framework/Headers/IOKit/firewire/FireLog.h"
#endif


#ifndef LOGGING   /* LOGGING */
#define DEBUG_IOLog( ARGS... )
#else             
#ifndef LOG_LEVEL
#define LOG_LEVEL 7
#endif
#ifdef FIRELOG    /* Firewire logging */
#define DEBUG_IOLog( LEVEL, ... ) { if( (LEVEL) <= (LOG_LEVEL) ) FireLog( __VA_ARGS__ ); }
#else
#ifdef USBLOG     /* USB Logging */
#define DEBUG_IOLog( ARGS... ) USBLog (ARGS)
#else
#define DEBUG_IOLog( LEVEL, ...) { if( (LEVEL) <= (LOG_LEVEL) ) IOLog( __VA_ARGS__ ); }
#endif            /* USB Logging */
#endif            /* Firewire Logging */
#endif            /* LOGGING */

#ifndef DATA_LOGGING
#define DATA_IOLog( ARGS... )
#else             
#ifndef LOG_LEVEL
#define LOG_LEVEL 7
#endif
#ifdef FIRELOG    /* Firewire logging */
#define DATA_IOLog( LEVEL, ... ) { if( (LEVEL) <= (LOG_LEVEL) ) FireLog( __VA_ARGS__ ); }
#else
#ifdef USBLOG     /* USB Logging */
#define DATA_IOLog( ARGS... ) USBLog (ARGS)
#else
#define DATA_IOLog( LEVEL, ...) { if( (LEVEL) <= (LOG_LEVEL) ) IOLog( __VA_ARGS__ ); }
#endif            /* USB Logging */
#endif            /* Firewire Logging */
#endif            /* DATA_LOGGING */

