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

//#define USBLOG 

//#define LOGGING
//#define LOG_LEVEL 7

//#define DATA_LOGGING
//#define DATA_LOG_LEVEL 3

#ifndef LOGGING
#define DEBUG_IOLog( ARGS... )
#else             /* LOGGING */
#ifdef USBLOG
#include <IOKit/usb/IOUSBLog.h>
#define DEBUG_IOLog( ARGS... ) USBLog (ARGS)
#else
#ifndef LOG_LEVEL
#define LOG_LEVEL 7
#endif
#define DEBUG_IOLog( LEVEL, ...) { if( (LEVEL) <= (LOG_LEVEL) ) IOLog( __VA_ARGS__ ); }
#endif          /* SYSLOG */
#endif            /* LOGGING */

#ifndef DATA_LOGGING
#define DATA_IOLog( ARGS... )
#else             /* DATA_LOGGING */
#ifdef USBLOG
#include <IOKit/usb/IOUSBLog.h>
#define DATA_IOLog( ARGS... )	USBLog (ARGS)
#else
#ifndef DATA_LOG_LEVEL
#define DATA_LOG_LEVEL 7
#endif
#define DATA_IOLog( LEVEL, ... ) { if( (LEVEL) <= (DATA_LOG_LEVEL) ) IOLog( __VA_ARGS__ ); }
#endif          /* SYSLOG */
#endif            /* DATA_LOGGING */

