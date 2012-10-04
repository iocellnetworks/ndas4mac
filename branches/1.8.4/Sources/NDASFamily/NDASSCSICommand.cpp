/*
 *  NDASSCSICommand.cpp
 *  NDASFamily
 *
 *  Created by Jung-gyun Ahn on 09. 07. 07.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#include <IOKit/IOLib.h>

#include "NDASSCSICommand.h"
#include "LanScsiProtocol.h"
#include "NDASUnitDevice.h"

#include "Utilities.h"

// Define my superclass
#define super com_ximeta_driver_NDASCommand


// REQUIRED! This macro defines the class's constructors, destructors,
// and several other methods I/O Kit requires. Do NOT use super as the
// second parameter. You must use the literal name of the superclass.

OSDefineMetaClassAndStructors(com_ximeta_driver_NDASSCSICommand, com_ximeta_driver_NDASCommand)

bool com_ximeta_driver_NDASSCSICommand::init()
{
	super::init();
	
	fCommand = kNDASCommandSRB;
	
	memset(&fScsiCommand, 0, sizeof(fScsiCommand));
	
	return true;
}

void com_ximeta_driver_NDASSCSICommand::free(void)
{	
	super::free();
}

void com_ximeta_driver_NDASSCSICommand::setCommand(PSCSICommand	command)
{	
	memcpy(&(fScsiCommand), command, sizeof(SCSICommand));
}

PSCSICommand com_ximeta_driver_NDASSCSICommand::scsiCommand()
{
	return &fScsiCommand;
}

void	com_ximeta_driver_NDASSCSICommand::convertSRBToSRBCompletedCommand()
{
	if (kNDASCommandSRB == fCommand) {
		fCommand = kNDASCommandSRBCompleted;
	}
}

void	com_ximeta_driver_NDASSCSICommand::convertSRBCompletedToSRBCommand()
{
	if (kNDASCommandSRBCompleted == fCommand) {
		fCommand = kNDASCommandSRB;
	}	
}
