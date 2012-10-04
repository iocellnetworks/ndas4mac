/***************************************************************************
*
*  NDASProtocolTransportNub.cpp
*
*    NDAS Protocol Transport Nub
*
*    Copyright (c) 2004 XiMeta, Inc. All rights reserved.
*
**************************************************************************/

#include <IOKit/IOLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOBSD.h>

#include "NDASProtocolTransportNub.h"

#include "NDASProtocolTransport.h"
#include "NDASLogicalDevice.h"

#include "NDASBusEnumeratorUserClientTypes.h"

#include "Utilities.h"

extern "C" {
#include <pexpert/pexpert.h>//This is for debugging purposes ONLY
}

// Define my superclass
#define super IOService


// REQUIRED! This macro defines the class's constructors, destructors,
// and several other methods I/O Kit requires. Do NOT use super as the
// second parameter. You must use the literal name of the superclass.

OSDefineMetaClassAndStructors(com_ximeta_driver_NDASProtocolTransportNub, IOService)

bool com_ximeta_driver_NDASProtocolTransportNub::init(OSDictionary *dict)
{
	bool res = super::init(dict);
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Initializing\n"));
	
	fCommandGate = NULL;
	fCommand = NULL;
	fTransport = NULL;
		
	if(dict != NULL) {
		OSDictionary *iconDict = (OSDictionary *)dict->getObject("Icon");
		
		setProperty("IOMediaIcon", iconDict);
	}	
	
    return res;
}

void com_ximeta_driver_NDASProtocolTransportNub::free(void)
{
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Freeing\n"));
		
	super::free();
}

IOService *com_ximeta_driver_NDASProtocolTransportNub::probe(IOService *provider, SInt32 *score)
{
    IOService *res = super::probe(provider, score);
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Probing\n"));
    return res;
}

bool com_ximeta_driver_NDASProtocolTransportNub::start(IOService *provider)
{
    bool res = super::start(provider);
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Starting\n"));
    return res;
}

void com_ximeta_driver_NDASProtocolTransportNub::stop(IOService *provider)
{
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Stopping\n"));
    super::stop(provider);
}

IOReturn com_ximeta_driver_NDASProtocolTransportNub::message( 
													   UInt32		mess, 
													   IOService	* provider,
													   void			* argument 
													   )
{
	DbgIOLog(DEBUG_MASK_PNP_TRACE, ("message: Entered. mess is 0x%x\n", kIOMessageServiceIsResumed));

    // Propagate power and eject messages
    if ( kIOMessageServiceIsResumed == mess ||
		 kIOMessageServiceIsSuspended == mess ||
		 kIOMessageServiceIsRequestingClose == mess ||
		 kIOMessageServiceIsTerminated == mess
		 ) 
    {		
        messageClients( mess );
        return kIOReturnSuccess;
    }
	
    return IOService::message(mess, provider, argument );
}

bool com_ximeta_driver_NDASProtocolTransportNub::willTerminate(
														IOService * provider,
														IOOptionBits options 
														)
{
	DbgIOLog(DEBUG_MASK_PNP_ERROR, ("willTerminate: Entered.\n"));
	
	// Cleanup Pending IO.
	if(fCommand) {
		
		DbgIOLog(DEBUG_MASK_PNP_ERROR, ("willTerminate: Cleanup Pending IO.\n"));

		PSCSICommand	pScsiCommand;
		com_ximeta_driver_NDASCommand *command;
				
		command = fCommand;
		fCommand = NULL;
		
		pScsiCommand = command->scsiCommand();

		pScsiCommand->CmdData.results = kIOReturnNotAttached;
		pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_DeviceNotPresent;

		if(NULL != pScsiCommand->MemoryDescriptor_ptr) {
			pScsiCommand->MemoryDescriptor_ptr->release();
		}
				
		if (fTransport) {
			fTransport->completeCommand(command);
		} else {
			
			DbgIOLog(DEBUG_MASK_DISK_ERROR, ("willTerminate: No Transport!!!\n"));			
			
			fCommandGate->runCommand((void *)kNDASProtocolTransportRequestCompleteCommand, command->scsiCommand());
			
			command->release();
		}
	}
	
	return true;
}

void	com_ximeta_driver_NDASProtocolTransportNub::setLogicalDevice(com_ximeta_driver_NDASLogicalDevice	*device)
{
	if (fLogicalDevice) {
		fLogicalDevice->release();
	}
	
	fLogicalDevice = device;
	
	if (fLogicalDevice) {
		fLogicalDevice->retain();
	}
}

com_ximeta_driver_NDASLogicalDevice	*com_ximeta_driver_NDASProtocolTransportNub::LogicalDevice() 
{
	return fLogicalDevice;
}

void	com_ximeta_driver_NDASProtocolTransportNub::setTransport(com_ximeta_driver_NDASProtocolTransport *transport)
{	
	fTransport = transport;
}

bool com_ximeta_driver_NDASProtocolTransportNub::sendCommand(com_ximeta_driver_NDASCommand *command)
{
	PSCSICommand	pScsiCommand;
	
	DbgIOLog(DEBUG_MASK_DISK_TRACE, ("sendCommand: Entered.\n"));
		
	if(command == NULL) {
		return false;
	}
				
	pScsiCommand = command->scsiCommand();
	
	if ( !fLogicalDevice->IsInRunningStatus() ) {
		DbgIOLog(DEBUG_MASK_DISK_ERROR, ("sendCommand: Logical Device is not running status\n"));
		
		if (command) {
			
			pScsiCommand->CmdData.results = kIOReturnNoDevice;
			
			if (fTransport) {
				fTransport->completeCommand(command);
			} else {
				DbgIOLog(DEBUG_MASK_DISK_ERROR, ("sendCommand: fCommandGate Closed!!!\n"));			
			}
		}
		
		return false;
	}
	
	if(fCommand != NULL || fLogicalDevice == NULL) {
		DbgIOLog(DEBUG_MASK_DISK_INFO, ("sendCommand: ERROR fCommand 0x%x, fController 0x%x.\n", fCommand, fLogicalDevice));
		
		if (command) {
			
			pScsiCommand->CmdData.results = kIOReturnNoDevice;
			
			if (fTransport) {
				fTransport->completeCommand(command);
			} else {
				DbgIOLog(DEBUG_MASK_DISK_ERROR, ("sendCommand: fCommandGate Closed!!!\n"));			
			}
		}
		
		return false;
	}
	
	fCommand = command;
	
	if(NULL != pScsiCommand->MemoryDescriptor_ptr) {
		pScsiCommand->MemoryDescriptor_ptr->retain();
	}
	
	if (fLogicalDevice && !fLogicalDevice->isInactive()) {
		fLogicalDevice->processSRBCommand(fCommand);
		return true;
	} else {
		
		DbgIOLog(DEBUG_MASK_DISK_WARNING, ("completeCommand: No Logical Device.\n"));

		pScsiCommand = fCommand->scsiCommand();
		
		pScsiCommand->CmdData.results = kIOReturnNotAttached;
		pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_DeliveryFailure;

		completeCommand(fCommand);
		
		return false;
	}
}

void com_ximeta_driver_NDASProtocolTransportNub::completeCommand(com_ximeta_driver_NDASCommand *command)
{
	PSCSICommand	pScsiCommand;
	
	DbgIOLog(DEBUG_MASK_DISK_TRACE, ("completeCommand: Entered.\n"));
	
	if(fCommand) {
				
		if( fCommand != command) {
			DbgIOLog(DEBUG_MASK_DISK_ERROR, ("completeCommand: Bad Command!!!\n"));
		}
				
		pScsiCommand = command->scsiCommand();
		
		if(NULL != pScsiCommand->MemoryDescriptor_ptr) {
			pScsiCommand->MemoryDescriptor_ptr->release();
		}
		
		fCommand = NULL;
		
		if (fTransport) {
			fTransport->completeCommand(command);
		} else {
			DbgIOLog(DEBUG_MASK_DISK_ERROR, ("completeCommand: fCommandGate Closed!!!\n"));			
		}
		
	}
}

void com_ximeta_driver_NDASProtocolTransportNub::clearCommand()
{
	fCommand = NULL;	
}

IOCommandGate *com_ximeta_driver_NDASProtocolTransportNub::commandGate()
{
	return fCommandGate;
}

void com_ximeta_driver_NDASProtocolTransportNub::setCommandGate(IOCommandGate *commandGate)
{
	fCommandGate = commandGate;
}

bool com_ximeta_driver_NDASProtocolTransportNub::isBusy()
{			
	if(NULL == fCommand) {
		return false;
	} else {
		return true;
	}
}

bool com_ximeta_driver_NDASProtocolTransportNub::isWorkable()
{
	if (fLogicalDevice && !fLogicalDevice->isInactive() && fLogicalDevice->IsMounted()) {
		return true;
	} else {
		return false;
	}
}

OSString *com_ximeta_driver_NDASProtocolTransportNub::bsdName()
{
	OSString *name = NULL;
	
//	DbgIOLog(DEBUG_MASK_DISK_TRACE, ("bsdName: Entered.\n"));
	
	if(fTransport) {
		IORegistryEntry *fChild = fTransport->getChildEntry(gIOServicePlane);
		
		while (fChild) {
//			DbgIOLog(DEBUG_MASK_DISK_INFO, ("bsdName: while fChild %x\n", fChild));
			name = (OSString *)fChild->getProperty(kIOBSDNameKey, gIOServicePlane);
			
			if(name == NULL) {
				fChild = fChild->getChildEntry(gIOServicePlane);
			} else {
				break;
			}
		}
	}
	
//	if(name != NULL) {
//		DbgIOLog(DEBUG_MASK_DISK_INFO, ("bsdName: %s\n", name->getCStringNoCopy()));
//	}
	
	return name; 
}

UInt32	com_ximeta_driver_NDASProtocolTransportNub::maxRequestSectors() 
{
	return fLogicalDevice->maxRequestSectors();
}

UInt32	com_ximeta_driver_NDASProtocolTransportNub::blocksize()
{
	if (fLogicalDevice) {
		return fLogicalDevice->blocksize();
	} else {
		return DISK_BLOCK_SIZE;
	}
}
