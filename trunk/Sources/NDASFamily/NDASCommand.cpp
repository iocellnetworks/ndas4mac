/***************************************************************************
*
*  NDASCommand.cpp
*
*    NDAS Command 
*
*    Copyright (c) 2004 XiMeta, Inc. All rights reserved.
*
**************************************************************************/

#include <IOKit/IOLib.h>

#include "NDASCommand.h"
#include "LanScsiProtocol.h"

#include "Utilities.h"

// Define my superclass
#define super IOCommand


// REQUIRED! This macro defines the class's constructors, destructors,
// and several other methods I/O Kit requires. Do NOT use super as the
// second parameter. You must use the literal name of the superclass.

OSDefineMetaClassAndStructors(com_ximeta_driver_NDASCommand, IOCommand)

bool com_ximeta_driver_NDASCommand::init()
{
	super::init();
	
	fPrev = fNext = NULL;
	
	fSema = NULL;
	
	return true;
}

void com_ximeta_driver_NDASCommand::free(void)
{	
	if (fSema) {		
		semaphore_destroy(current_task(), fSema);
	}
	
	super::free();
}
void com_ximeta_driver_NDASCommand::setCommand(UInt32 interfaceArrayIndex, char *address, UInt32 receivedTime, PPNP_MESSAGE message)
{
	fCommand = kNDASCommandReceivePnpMessage;
	
	u.fPnpCommand.fInterfaceArrayIndex = interfaceArrayIndex;
	
	if (address) {
		memcpy(u.fPnpCommand.fAddress, address, 6);
	} else {
		memset(u.fPnpCommand.fAddress, 0, 6);
	}
	
	u.fPnpCommand.fReceivedTime = receivedTime;
	memcpy(&u.fPnpCommand.fMessage, message, sizeof(PNP_MESSAGE));
}

void com_ximeta_driver_NDASCommand::setCommand(UInt32 interfaceArrayIndex, char *address, UInt64 linkSpeed, bool interfaceChanged)
{
	fCommand = kNDASCommandChangedSetting;
	
	u.fPnpCommand.fInterfaceArrayIndex = interfaceArrayIndex;
	if (address) {
		memcpy(u.fPnpCommand.fAddress, address, 6);
	} else {
		memset(u.fPnpCommand.fAddress, 0, 6);
	}	
	u.fPnpCommand.fLinkSpeed = linkSpeed;
	
	u.fPnpCommand.fInterfaceChanged = interfaceChanged;
}

void com_ximeta_driver_NDASCommand::setCommand(UInt32 TargetNumber)
{
	fCommand = kNDASCommandRefresh;
	
	u.fRefreshCommand.fTargetNumber = TargetNumber;
}

void com_ximeta_driver_NDASCommand::setCommand(PSCSICommand	command)
{
	fCommand = kNDASCommandSRB;
	
	memcpy(&(u.fScsiCommand), command, sizeof(SCSICommand));
}

void com_ximeta_driver_NDASCommand::setCommand(PManagingIOCommand command)
{
	fCommand = kNDASCommandManagingIO;
	
	memcpy(&(u.fManagingIOCommand), command, sizeof(ManagingIOCommand));
}

void com_ximeta_driver_NDASCommand::setCommand(UInt32 notificationType, com_ximeta_driver_NDASUnitDevice *device)
{
	fCommand = kNDASCommandDeviceNotification;
	
	u.fDeviceNotificationCommand.notificationType = notificationType;
	u.fDeviceNotificationCommand.device = device;
}

void com_ximeta_driver_NDASCommand::setToTerminateCommand()
{
	fCommand = kNDASCommandTerminateThread;
}

UInt32 com_ximeta_driver_NDASCommand::command()
{
	return fCommand;
}

PSCSICommand com_ximeta_driver_NDASCommand::scsiCommand()
{
	return &u.fScsiCommand;
}

PPNPCommand com_ximeta_driver_NDASCommand::pnpCommand()
{
	return &u.fPnpCommand;
}

PRefreshCommand com_ximeta_driver_NDASCommand::refreshCommand()
{
	return &u.fRefreshCommand;
}

PManagingIOCommand com_ximeta_driver_NDASCommand::managingIOCommand()
{
	return &u.fManagingIOCommand;
}

PDeviceNotificationCommand com_ximeta_driver_NDASCommand::DeviceNotificationCommand()
{
	return &u.fDeviceNotificationCommand;
}

void	com_ximeta_driver_NDASCommand::convertSRBToSRBCompletedCommand()
{
	if (kNDASCommandSRB == fCommand) {
		fCommand = kNDASCommandSRBCompleted;
	}
}

void	com_ximeta_driver_NDASCommand::convertSRBCompletedToSRBCommand()
{
	if (kNDASCommandSRBCompleted == fCommand) {
		fCommand = kNDASCommandSRB;
	}	
}

bool	com_ximeta_driver_NDASCommand::setToWaitForCompletion()
{
	DbgIOLogNC(DEBUG_MASK_NDAS_TRACE, ("setToWaitForCompletion Entered.\n"));
	
	// Create Sema.
	if (NULL == fSema) {
		if(KERN_SUCCESS != semaphore_create(current_task(),
											&fSema,
											SYNC_POLICY_FIFO, 0)) {
			
			DbgIOLogNC(DEBUG_MASK_PNP_ERROR, ("setToWaitForCompletion::semaphore_create(), failed to Create Sema\n"));
			
			return false;
		}
	}
	
	return true;
}

void	com_ximeta_driver_NDASCommand::waitForCompletion()
{
	DbgIOLogNC(DEBUG_MASK_NDAS_TRACE, ("waitForCompletion Entered.\n"));
	
	if (NULL != fSema) {
		semaphore_wait(fSema);
	}
}

void	com_ximeta_driver_NDASCommand::setToComplete() 
{
	DbgIOLogNC(DEBUG_MASK_NDAS_TRACE, ("setToComplete Entered.\n"));
	
	if (fSema) {
		semaphore_signal(fSema);
	}
}
