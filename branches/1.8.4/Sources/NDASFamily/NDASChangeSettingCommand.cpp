/*
 *  NDASChangeSettingCommand.cpp
 *  NDASFamily
 *
 *  Created by Jung-gyun Ahn on 09. 07. 07.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#include <IOKit/IOLib.h>

#include "NDASChangeSettingCommand.h"
#include "NDASCommand.h"
#include "LanScsiProtocol.h"
#include "NDASUnitDevice.h"

#include "Utilities.h"

// Define my superclass
#define super com_ximeta_driver_NDASCommand


// REQUIRED! This macro defines the class's constructors, destructors,
// and several other methods I/O Kit requires. Do NOT use super as the
// second parameter. You must use the literal name of the superclass.

OSDefineMetaClassAndStructors(com_ximeta_driver_NDASChangeSettingCommand, com_ximeta_driver_NDASCommand)

bool com_ximeta_driver_NDASChangeSettingCommand::init()
{
	super::init();
	
	fCommand = kNDASCommandChangedSetting;
	
	return true;
}

void com_ximeta_driver_NDASChangeSettingCommand::free(void)
{	
	super::free();
}

void com_ximeta_driver_NDASChangeSettingCommand::setCommand(uint32_t interfaceArrayIndex, char *address, uint64_t linkSpeed, bool interfaceChanged)
{	
	fChangeSettingCommand.fInterfaceArrayIndex = interfaceArrayIndex;
	if (address) {
		memcpy(fChangeSettingCommand.fAddress, address, 6);
	} else {
		memset(fChangeSettingCommand.fAddress, 0, 6);
	}	
	fChangeSettingCommand.fLinkSpeed = linkSpeed;
	
	fChangeSettingCommand.fInterfaceChanged = interfaceChanged;
}

PChangeSettingCommand com_ximeta_driver_NDASChangeSettingCommand::changeSettingCommand()
{
	return &fChangeSettingCommand;
}

