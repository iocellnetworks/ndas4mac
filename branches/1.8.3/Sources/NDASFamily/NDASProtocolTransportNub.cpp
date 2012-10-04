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
#include "NDASLogicalDeviceController.h"

#include "NDASBusEnumeratorUserClientTypes.h"

#include "Utilities.h"

#include "NDASSCSICommand.h"

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
	
	fCommand = NULL;
			
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
	DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Entered. mess is 0x%x\n", (uint32_t)mess));

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
	DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Entered.\n"));
	
	// Cleanup Pending IO.
	if(fCommand) {
		
		DbgIOLog(DEBUG_MASK_PNP_ERROR, ("Cleanup Pending IO.\n"));

		PSCSICommand	pScsiCommand;
		com_ximeta_driver_NDASSCSICommand *command;
				
		command = fCommand;
				
		pScsiCommand = command->scsiCommand();

		pScsiCommand->CmdData.results = kIOReturnNotAttached;
		pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_DeviceNotPresent;

		completeCommand(command);
	}
	
	return true;
}

bool com_ximeta_driver_NDASProtocolTransportNub::sendCommand(com_ximeta_driver_NDASSCSICommand *command)
{	
	DbgIOLog(DEBUG_MASK_DISK_TRACE, ("Entered.\n"));

	DbgIOASSERT(command);
	DbgIOASSERT(fCommand == NULL);
		
	com_ximeta_driver_NDASLogicalDeviceController *logicalDeviceController = OSDynamicCast(com_ximeta_driver_NDASLogicalDeviceController, getProvider());
	DbgIOASSERT(logicalDeviceController);

	fCommand = command;

	if(!logicalDeviceController->enqueueCommand(command)) {

		DbgIOLog(DEBUG_MASK_DISK_ERROR, ("Logical Device is not running status\n"));
		
		command->scsiCommand()->CmdData.results = kIOReturnNoDevice;
				
		return false;		
	}
		
	return true;
}

void com_ximeta_driver_NDASProtocolTransportNub::completeCommand(com_ximeta_driver_NDASSCSICommand *command)
{	
	com_ximeta_driver_NDASProtocolTransport *transport = OSDynamicCast(com_ximeta_driver_NDASProtocolTransport, getClient());
	DbgIOASSERT(transport);
	
	if( command == NULL) {
		DbgIOLog(DEBUG_MASK_DISK_ERROR, ("NULL Command!!! fCommand %p, command %p\n", fCommand, command));
		
		return;
	}
	
#if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= 1060
	if(OSCompareAndSwapPtr(command, NULL, (void * volatile *)&(fCommand))) {
#elif __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= 1050
	if(OSCompareAndSwap((UInt32)command, 0, (volatile UInt32*)&fCommand)) {
#else
	if(OSCompareAndSwap((UInt32)command, 0, (UInt32*)&fCommand)) {
#endif
		
		transport->completeCommand(command);		
	} else {
		DbgIOLog(DEBUG_MASK_DISK_ERROR, ("Bad Command!!! fCommand %p, command %p\n", fCommand, command));	
	}
}

void com_ximeta_driver_NDASProtocolTransportNub::clearCommand()
{
	DbgIOLog(DEBUG_MASK_DISK_TRACE, ("Entered. this %p, fCommand %p\n", this, fCommand));
		
	fCommand = NULL;	
}

bool com_ximeta_driver_NDASProtocolTransportNub::isBusy()
{			
	DbgIOLog(DEBUG_MASK_DISK_TRACE, ("Entered. this %p, fCommand %p\n", this, fCommand));
	
	bool bResult;
	
	if(NULL == fCommand) {
		bResult = false;
	} else {
		bResult = true;
	}
		
	return bResult;
}

bool com_ximeta_driver_NDASProtocolTransportNub::isWorkable()
{
	com_ximeta_driver_NDASLogicalDeviceController *logicalDeviceController = OSDynamicCast(com_ximeta_driver_NDASLogicalDeviceController, getProvider());
	DbgIOASSERT(logicalDeviceController);

	if (!logicalDeviceController->isInactive()) { // && logicalDeviceController->isWorkable()) {
		return true;
	} else {
		return false;
	}
}

OSString *com_ximeta_driver_NDASProtocolTransportNub::bsdName()
{
	OSString *name = NULL;
	
	DbgIOLog(DEBUG_MASK_DISK_TRACE, ("bsdName: Entered.\n"));
	
	com_ximeta_driver_NDASProtocolTransport* transport = OSDynamicCast(com_ximeta_driver_NDASProtocolTransport, getClient());
	
	if(transport) {
		IORegistryEntry *fChild = transport->getChildEntry(gIOServicePlane);
		
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

void com_ximeta_driver_NDASProtocolTransportNub::setMaxRequestSectors(uint32_t sectors)
{
	fMaxRequestSectors = sectors;
}

void com_ximeta_driver_NDASProtocolTransportNub::setBlockSize(uint32_t size)
{	
	fBlockSize = size;
}

uint32_t	com_ximeta_driver_NDASProtocolTransportNub::maxRequestSectors() 
{
	return fMaxRequestSectors;
}

uint32_t	com_ximeta_driver_NDASProtocolTransportNub::blocksize()
{
	return fBlockSize;
}
