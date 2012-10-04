/*
 *  NDASFamilyIOMessage.h
 *  NDASFamily
 *
 *  Created by 정균 안 on 07. 03. 08.
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef _NDAS_FAMILY_IO_MESSAGE_H_
#define _NDAS_FAMILY_IO_MESSAGE_H_

#include <IOKit/IOMessage.h>

#ifndef iokit_vendor_specific_msg
	#define sub_iokit_vendor_specific         err_sub(-2)
	#define iokit_vendor_specific_msg(message) (uint32_t)(sys_iokit|sub_iokit_vendor_specific|message)
#endif

#define kNDASFamilyIOMessageNDASDeviceWasCreated					iokit_vendor_specific_msg(0x001)	// Argument is SlotNumber.
#define kNDASFamilyIOMessageNDASDeviceWasDeleted					iokit_vendor_specific_msg(0x002)	// Argument is SlotNumber.
#define kNDASFamilyIOMessageNDASDeviceStatusWasChanged				iokit_vendor_specific_msg(0x003)	// Argument is SlotNumber.

#define kNDASFamilyIOMessagePhysicalUnitDeviceWasCreated			iokit_vendor_specific_msg(0x011)	// Argument is GUID.
#define kNDASFamilyIOMessagePhysicalUnitDeviceWasDeleted			iokit_vendor_specific_msg(0x012)	// Argument is GUID.
#define kNDASFamilyIOMessagePhysicalUnitDeviceStatusWasChanged		iokit_vendor_specific_msg(0x013)	// Argument is GUID.

#define kNDASFamilyIOMessageLogicalDeviceWasCreated					iokit_vendor_specific_msg(0x021)	// Argument is GUID.
#define kNDASFamilyIOMessageLogicalDeviceWasDeleted					iokit_vendor_specific_msg(0x022)	// Argument is GUID.
#define kNDASFamilyIOMessageLogicalDeviceStatusWasChanged			iokit_vendor_specific_msg(0x023)	// Argument is GUID.
#define kNDASFamilyIOMessageLogicalDeviceConfigurationWasChanged	iokit_vendor_specific_msg(0x024)	// Argument is GUID.

#define kNDASFamilyIOMessageRAIDDeviceWasCreated					iokit_vendor_specific_msg(0x031)	// Argument is GUID.
#define kNDASFamilyIOMessageRAIDDeviceWasDeleted					iokit_vendor_specific_msg(0x032)	// Argument is GUID.
#define kNDASFamilyIOMessageRAIDDeviceStatusWasChanged				iokit_vendor_specific_msg(0x033)	// Argument is GUID.
#define kNDASFamilyIOMessageRAIDDeviceRAIDStatusWasChanged			iokit_vendor_specific_msg(0x035)	// Argument is GUID.

#define kNDASFamilyIOMessageUnitDevicePropertywasChanged			iokit_vendor_specific_msg(0x100)	// Argument is GUID.

#endif