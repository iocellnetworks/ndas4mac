/*
 *  NDASManagingIOCommand.cpp
 *  NDASFamily
 *
 *  Created by Jung-gyun Ahn on 09. 07. 07.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#include <IOKit/IOLib.h>

#include "NDASManagingIOCommand.h"
#include "NDASCommand.h"
#include "LanScsiProtocol.h"
#include "NDASUnitDevice.h"

#include "Utilities.h"

// Define my superclass
#define super com_ximeta_driver_NDASCommand


// REQUIRED! This macro defines the class's constructors, destructors,
// and several other methods I/O Kit requires. Do NOT use super as the
// second parameter. You must use the literal name of the superclass.

OSDefineMetaClassAndStructors(com_ximeta_driver_NDASManagingIOCommand, com_ximeta_driver_NDASCommand)

bool com_ximeta_driver_NDASManagingIOCommand::init()
{
	super::init();
	
	fCommand = kNDASCommandManagingIO;
	
	return true;
}

void com_ximeta_driver_NDASManagingIOCommand::free(void)
{	
	super::free();
}

void com_ximeta_driver_NDASManagingIOCommand::setCommand(PManagingIOCommand command)
{
	fCommand = kNDASCommandManagingIO;
	
	memcpy(&(fManagingIOCommand), command, sizeof(ManagingIOCommand));
}

PManagingIOCommand com_ximeta_driver_NDASManagingIOCommand::managingIOCommand()
{
	return &fManagingIOCommand;
}

