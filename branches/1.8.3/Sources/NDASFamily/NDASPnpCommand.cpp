/*
 *  NDASPnpCommand.cpp
 *  NDASFamily
 *
 *  Created by Jung-gyun Ahn on 09. 07. 07.
 *  Copyright 2009 XIMETA, Inc. All rights reserved.
 *
 */
#include <IOKit/IOLib.h>

#include "NDASPnpCommand.h"
#include "NDASCommand.h"
#include "LanScsiProtocol.h"
#include "NDASUnitDevice.h"

#include "Utilities.h"

// Define my superclass
#define super com_ximeta_driver_NDASCommand


// REQUIRED! This macro defines the class's constructors, destructors,
// and several other methods I/O Kit requires. Do NOT use super as the
// second parameter. You must use the literal name of the superclass.

OSDefineMetaClassAndStructors(com_ximeta_driver_NDASPnpCommand, com_ximeta_driver_NDASCommand)

bool com_ximeta_driver_NDASPnpCommand::init()
{
	super::init();

	fCommand = kNDASCommandReceivePnpMessage;
	
	return true;
}

void com_ximeta_driver_NDASPnpCommand::free(void)
{	
	super::free();
}

void com_ximeta_driver_NDASPnpCommand::setCommand(uint32_t interfaceArrayIndex, char *address, uint32_t receivedTime, PPNP_MESSAGE message)
{	

	fPnpCommand.fInterfaceArrayIndex = interfaceArrayIndex;
	
	if (address) {
		memcpy(fPnpCommand.fAddress, address, 6);
	} else {
		memset(fPnpCommand.fAddress, 0, 6);
	}
	
	fPnpCommand.fReceivedTime = receivedTime;
	memcpy(&fPnpCommand.fMessage, message, sizeof(PNP_MESSAGE));
}


PPNPCommand com_ximeta_driver_NDASPnpCommand::pnpCommand()
{
	return &fPnpCommand;
}
