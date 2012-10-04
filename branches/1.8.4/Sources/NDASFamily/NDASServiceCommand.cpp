/*
 *  NDASServiceCommand.cpp
 *  NDASFamily
 *
 *  Created by Jung-gyun Ahn on 09. 07. 07.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#include <IOKit/IOLib.h>

#include "NDASServiceCommand.h"
#include "NDASCommand.h"
#include "LanScsiProtocol.h"
#include "NDASUnitDevice.h"

#include "Utilities.h"

// Define my superclass
#define super com_ximeta_driver_NDASCommand


// REQUIRED! This macro defines the class's constructors, destructors,
// and several other methods I/O Kit requires. Do NOT use super as the
// second parameter. You must use the literal name of the superclass.

OSDefineMetaClassAndStructors(com_ximeta_driver_NDASServiceCommand, com_ximeta_driver_NDASCommand)

bool com_ximeta_driver_NDASServiceCommand::init()
{
	super::init();
	
	fCommand = kNDASCommandService;
	
	return true;
}

void com_ximeta_driver_NDASServiceCommand::free(void)
{	
	super::free();
}

void com_ximeta_driver_NDASServiceCommand::setCommand(uint32_t serviceType, char *password, uint8_t	 vid, bool bWritable)
{	
	fServiceCommand.serviceType = serviceType;

	switch (fServiceCommand.serviceType) {
		case kNDASServiceCommandChangeUserPassword:
		{
			memcpy(fServiceCommand.password, password, 8);
			fServiceCommand.vid = vid;
			fServiceCommand.bWritable = bWritable;
		}
			break;
		default:
			break;
	}
}

PServiceCommand com_ximeta_driver_NDASServiceCommand::serviceCommand()
{
	return &fServiceCommand;
}
