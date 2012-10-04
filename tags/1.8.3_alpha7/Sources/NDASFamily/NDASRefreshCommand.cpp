/*
 *  NDASRefreshCommand.cpp
 *  NDASFamily
 *
 *  Created by Jung-gyun Ahn on 09. 07. 07.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#include <IOKit/IOLib.h>

#include "NDASRefreshCommand.h"
#include "NDASCommand.h"
#include "LanScsiProtocol.h"
#include "NDASUnitDevice.h"

#include "Utilities.h"

// Define my superclass
#define super com_ximeta_driver_NDASCommand


// REQUIRED! This macro defines the class's constructors, destructors,
// and several other methods I/O Kit requires. Do NOT use super as the
// second parameter. You must use the literal name of the superclass.

OSDefineMetaClassAndStructors(com_ximeta_driver_NDASRefreshCommand, com_ximeta_driver_NDASCommand)

bool com_ximeta_driver_NDASRefreshCommand::init()
{
	super::init();
	
	fCommand = kNDASCommandRefresh;
	
	return true;
}

void com_ximeta_driver_NDASRefreshCommand::free(void)
{	
	super::free();
}

void com_ximeta_driver_NDASRefreshCommand::setCommand(uint32_t TargetNumber)
{	
	fRefreshCommand.fTargetNumber = TargetNumber;
}

PRefreshCommand com_ximeta_driver_NDASRefreshCommand::refreshCommand()
{
	return &fRefreshCommand;
}
