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
#include "NDASUnitDevice.h"

#include "Utilities.h"

// Define my superclass
#define super IOCommand


// REQUIRED! This macro defines the class's constructors, destructors,
// and several other methods I/O Kit requires. Do NOT use super as the
// second parameter. You must use the literal name of the superclass.

OSDefineMetaClassAndAbstractStructors(com_ximeta_driver_NDASCommand, IOCommand)

bool com_ximeta_driver_NDASCommand::init()
{
	super::init();
	
	fSema = NULL;
	fResult = 0;
		
	return true;
}

void com_ximeta_driver_NDASCommand::free(void)
{	
	if (fSema) {		
		semaphore_destroy(current_task(), fSema);
	}
		
	super::free();
}

uint32_t com_ximeta_driver_NDASCommand::command()
{
	return fCommand;
}

IOReturn com_ximeta_driver_NDASCommand::result()
{
	return fResult;
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

void	com_ximeta_driver_NDASCommand::setToComplete(IOReturn result) 
{
	DbgIOLogNC(DEBUG_MASK_NDAS_TRACE, ("Entered.\n"));
	
	fResult = result;
	
	if (fSema) {
		semaphore_signal(fSema);
	}
}
