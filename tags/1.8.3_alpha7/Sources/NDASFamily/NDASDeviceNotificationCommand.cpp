/*
 *  NDASDeviceNotificationCommand.cpp
 *  NDASFamily
 *
 *  Created by Jung-gyun Ahn on 09. 07. 07.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#include <IOKit/IOLib.h>

#include "NDASDeviceNotificationCommand.h"
#include "NDASCommand.h"
#include "LanScsiProtocol.h"
#include "NDASUnitDevice.h"

#include "Utilities.h"

// Define my superclass
#define super com_ximeta_driver_NDASCommand


// REQUIRED! This macro defines the class's constructors, destructors,
// and several other methods I/O Kit requires. Do NOT use super as the
// second parameter. You must use the literal name of the superclass.

OSDefineMetaClassAndStructors(com_ximeta_driver_NDASDeviceNotificationCommand, com_ximeta_driver_NDASCommand)

bool com_ximeta_driver_NDASDeviceNotificationCommand::init()
{
	super::init();
	
	fCommand = kNDASCommandDeviceNotification;
	
	return true;
}

void com_ximeta_driver_NDASDeviceNotificationCommand::free(void)
{	
	if(fDevice) {
		fDevice->release();
	}
	
	super::free();
}

void com_ximeta_driver_NDASDeviceNotificationCommand::setCommand(kNDASDeviceNotificationType notificationType, com_ximeta_driver_NDASUnitDevice *device)
{
	
	fNotificationType = notificationType;
	
	if(fDevice) {
		fDevice->release();
	}
	
	fDevice = device;
	
	if(fDevice) {
		fDevice->retain();
	}
}

kNDASDeviceNotificationType com_ximeta_driver_NDASDeviceNotificationCommand::notificationType()
{
	return  fNotificationType;
}

com_ximeta_driver_NDASUnitDevice *com_ximeta_driver_NDASDeviceNotificationCommand::device()
{
	return fDevice;
}
