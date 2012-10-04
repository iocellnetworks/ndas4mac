/***************************************************************************
*
*  NDASLogicalDevice.cpp
*
*    NDAS Logical Device
*
*    Copyright (c) 2004 XiMeta, Inc. All rights reserved.
*
**************************************************************************/

#include <IOKit/IOLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOBSD.h>

//#include <uuid/uuid.h>

#include "NDASLogicalDevice.h"

#include "NDASRAIDUnitDevice.h"
#include "NDASPhysicalUnitDevice.h"
#include "NDASBusEnumerator.h"

#include "NDASFamilyIOMessage.h"
#include "NDASBusEnumeratorUserClientTypes.h"

#include "NDASDeviceNotificationCommand.h"

#include "NDASLogicalDeviceController.h"

#include "Utilities.h"

extern "C" {
#include <pexpert/pexpert.h>//This is for debugging purposes ONLY
}

// Define my superclass
#define super com_ximeta_driver_NDASUnitDevice


// REQUIRED! This macro defines the class's constructors, destructors,
// and several other methods I/O Kit requires. Do NOT use super as the
// second parameter. You must use the literal name of the superclass.

OSDefineMetaClassAndStructors(com_ximeta_driver_NDASLogicalDevice, com_ximeta_driver_NDASUnitDevice)

bool com_ximeta_driver_NDASLogicalDevice::init(OSDictionary *dict)
{
	bool res = super::init(dict);
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Initializing\n"));
	
	fUnitDevice = NULL;
					
    return res;
}

void com_ximeta_driver_NDASLogicalDevice::free(void)
{
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Freeing\n"));
		
	if(fBSDName) {
		fBSDName->release();
	}
	
	super::free();
}

IOService *com_ximeta_driver_NDASLogicalDevice::probe(IOService *provider, SInt32 *score)
{
    IOService *res = super::probe(provider, score);
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Probing\n"));
    return res;
}

bool com_ximeta_driver_NDASLogicalDevice::start(IOService *provider)
{
    bool res = super::start(provider);
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Starting\n"));
    return res;
}

void com_ximeta_driver_NDASLogicalDevice::stop(IOService *provider)
{
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Stopping\n"));
    super::stop(provider);
}

IOReturn com_ximeta_driver_NDASLogicalDevice::message(		
													   UInt32		mess, 
													   IOService	* provider,
													   void			* argument 
													   )
{
	DbgIOLog(DEBUG_MASK_PNP_TRACE, ("message: Entered.\n"));

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

uint32_t	com_ximeta_driver_NDASLogicalDevice::Index()
{
	return fIndex;
}

void	com_ximeta_driver_NDASLogicalDevice::setIndex(uint32_t index)
{
	fIndex = index;
	
	setProperty(NDASDEVICE_PROPERTY_KEY_INDEX, fIndex, 32);
}

uint32_t	com_ximeta_driver_NDASLogicalDevice::maxRequestSectors()
{
	if(fUnitDevice) {
		return fUnitDevice->maxRequestSectors();
	}
	
	return 0;
}

com_ximeta_driver_NDASUnitDevice	*com_ximeta_driver_NDASLogicalDevice::UnitDevice()
{
	return fUnitDevice;
}

void	com_ximeta_driver_NDASLogicalDevice::setUnitDevice(com_ximeta_driver_NDASUnitDevice	*device)
{	
	if (NULL != device) {		
		device->retain();
	}

	if (fUnitDevice) {
		fUnitDevice->release();
	}
	
	fUnitDevice = device;
	
	if (fUnitDevice) {
		// Set Properties of Unit Device.
		setProperty(NDASDEVICE_PROPERTY_KEY_UNIT_DEVICE_TYPE, fUnitDevice->Type(), 32);
		
		switch( fUnitDevice->Type() ) {
			case NMT_SINGLE:
			case NMT_CDROM:
			{
				com_ximeta_driver_NDASPhysicalUnitDevice *physicalDevice;
				
				physicalDevice = OSDynamicCast(com_ximeta_driver_NDASPhysicalUnitDevice, device);
				
				if (NULL != physicalDevice) {
					
					setProperty( NDASDEVICE_PROPERTY_KEY_PHYSICAL_DEVICE_SLOT_NUMBER , physicalDevice->slotNumber(), 32);
					setProperty( NDASDEVICE_PROPERTY_KEY_PHYSICAL_DEVICE_UNIT_NUMBER , physicalDevice->unitNumber(), 32);
				}
			}
				break;
			default:
				break;
		}
	
		setProperty(NDASDEVICE_PROPERTY_KEY_LOGICAL_DEVICE_LOWER_UNIT_DEVICE_ID, fUnitDevice->ID(), 128);
	}

}

bool com_ximeta_driver_NDASLogicalDevice::processSRBCommand(com_ximeta_driver_NDASSCSICommand *command)
{
	DbgIOLog(DEBUG_MASK_DISK_TRACE, ("Entered.\n"));
	
	return fUnitDevice->processSRBCommand(command);
}

void com_ximeta_driver_NDASLogicalDevice::completeCommand(com_ximeta_driver_NDASSCSICommand *command)
{
	DbgIOLog(DEBUG_MASK_DISK_TRACE, ("Entered.\n"));
	
	com_ximeta_driver_NDASLogicalDeviceController *controller = OSDynamicCast(com_ximeta_driver_NDASLogicalDeviceController, getClient());

	controller->completeCommand(command);
}

bool com_ximeta_driver_NDASLogicalDevice::readSectorsOnce(uint64_t location, uint32_t sectors, char *buffer)
{
	return false;
}

bool com_ximeta_driver_NDASLogicalDevice::writeSectorsOnce(uint64_t location, uint32_t sectors, char *buffer)
{
	return false;
}

uint32_t	com_ximeta_driver_NDASLogicalDevice::NRRWHosts()
{
	return fUnitDevice->NRRWHosts();
}

uint32_t	com_ximeta_driver_NDASLogicalDevice::NRROHosts()
{
	return fUnitDevice->NRROHosts();
}

OSString *com_ximeta_driver_NDASLogicalDevice::BSDName()
{
	return fBSDName;
}

void com_ximeta_driver_NDASLogicalDevice::setBSDName(OSString *newBSDname)
{
	DbgIOLog(DEBUG_MASK_DISK_TRACE, ("Entered.\n"));
	
	if(fBSDName) {
		fBSDName->release();
	}
	
	if(newBSDname) {
		
		fBSDName = OSString::withString(newBSDname);
		
		setProperty(NDASDEVICE_PROPERTY_KEY_UNIT_DEVICE_BSD_NAME, fBSDName);
	}
	
	return;
}


void com_ximeta_driver_NDASLogicalDevice::setConfiguration(uint32_t configuration)
{
	uint32_t	prevConfiguration = Configuration();
	
	if (fUnitDevice) {
		// Change the configuration of Unit Device.
		fUnitDevice->setConfiguration(configuration);
	}
	
	super::setConfiguration(configuration);
	
	if (prevConfiguration != configuration) {
	
		sendNDASFamilyIOMessage(kNDASFamilyIOMessageLogicalDeviceConfigurationWasChanged, UnitDevice()->ID(), sizeof(GUID));
	}
}

bool com_ximeta_driver_NDASLogicalDevice::IsMounted()
{
	if (!fUnitDevice) {
		return false;
	}
	
	// Update Status.
	updateStatus();
	
	switch(Status()) {
		case kNDASUnitStatusConnectedRO:
		case kNDASUnitStatusConnectedRW:
		case kNDASUnitStatusReconnectRO:
		case kNDASUnitStatusReconnectRW:
		{
			return true;
		}
			break;
		default:
			return false;
	}
}

bool com_ximeta_driver_NDASLogicalDevice::IsWritable()
{
	bool writable = false;
	
	if (fUnitDevice) {
		writable = fUnitDevice->IsWritable();
	}
	
	setProperty(NDASDEVICE_PROPERTY_KEY_WRITE_RIGHT, writable);
	
	return writable;
}

uint32_t	com_ximeta_driver_NDASLogicalDevice::Type()
{
	return kNDASUnitDeviceTypeLogical;
}

bool	com_ximeta_driver_NDASLogicalDevice::bind(uint32_t	type, 
												  uint32_t	sequence, 
												  uint32_t	NumberOfUnitDevices, 
												  com_ximeta_driver_NDASLogicalDevice *LogicalDevices[],
												  NDAS_uuid_t	raidID,
												  NDAS_uuid_t	configID,
												  bool		sync)
{
	if(UnitDevice()) {
		return UnitDevice()->bind(type, sequence, NumberOfUnitDevices, LogicalDevices, raidID, configID, sync);
	} else {
		return false;
	}
}

uint32_t com_ximeta_driver_NDASLogicalDevice::updateStatus()
{
	uint32_t prevStatus, newStatus;
	
	prevStatus = newStatus = Status();

	switch(prevStatus) {
		case kNDASUnitStatusNotPresent:
		case kNDASUnitStatusFailure:
		{
			newStatus = fUnitDevice->Status();
		}
			break;
		case kNDASUnitStatusDisconnected:
		{
			switch(fUnitDevice->Status()) {
				case kNDASUnitStatusConnectedRO:
				{
					newStatus = kNDASUnitStatusTryConnectRO;
				}
					break;
				case kNDASUnitStatusConnectedRW:
				{
					newStatus = kNDASUnitStatusTryConnectRW;
				}
					break;
				case kNDASUnitStatusFailure:
				case kNDASUnitStatusNotPresent:
				{
					newStatus = fUnitDevice->Status();
				}
					break;
			}
		}
			break;
		case kNDASUnitStatusTryConnectRO:
		{
			if (kNDASUnitStatusConnectedRO == fUnitDevice->Status()) {
				newStatus = kNDASUnitStatusConnectedRO;
			}
		}
			break;
		case kNDASUnitStatusTryConnectRW:
		{
			if (kNDASUnitStatusConnectedRW == fUnitDevice->Status()) {
				newStatus = kNDASUnitStatusConnectedRW;
			}
		}
			break;
		case kNDASUnitStatusConnectedRO:
		{
			switch(fUnitDevice->Status()) {
				case kNDASUnitStatusDisconnected:
				case kNDASUnitStatusFailure:
				case kNDASUnitStatusNotPresent:
					newStatus = kNDASUnitStatusTryDisconnectRO;
					break;
				case kNDASUnitStatusReconnectRO:
					newStatus = kNDASUnitStatusReconnectRO;
					break;
			}
		}
			break;
		case kNDASUnitStatusConnectedRW:
		{
			switch(fUnitDevice->Status()) {
				case kNDASUnitStatusDisconnected:
				case kNDASUnitStatusFailure:
				case kNDASUnitStatusNotPresent:
					newStatus = kNDASUnitStatusTryDisconnectRW;
					break;
				case kNDASUnitStatusReconnectRW:
					newStatus = kNDASUnitStatusReconnectRW;
					break;
			}
		}
			break;
		case kNDASUnitStatusTryDisconnectRO:
		case kNDASUnitStatusTryDisconnectRW:
		{
		
			com_ximeta_driver_NDASLogicalDeviceController *controller = OSDynamicCast(com_ximeta_driver_NDASLogicalDeviceController, getClient());
			DbgIOASSERT(controller);
			
			if (!controller->isProtocolTransportNubAttached()) {
				newStatus = kNDASUnitStatusDisconnected;
			}

		}
			break;
		case kNDASUnitStatusReconnectRO:
		{
			switch (fUnitDevice->Status()) {
			case kNDASUnitStatusConnectedRO:
				newStatus = kNDASUnitStatusConnectedRO;
				break;
			case kNDASUnitStatusDisconnected:
			case kNDASUnitStatusFailure:
			case kNDASUnitStatusNotPresent:
				newStatus = kNDASUnitStatusTryDisconnectRO;
				break;
			}
		}
			break;
		case kNDASUnitStatusReconnectRW:
		{
			switch (fUnitDevice->Status()) {
				case kNDASUnitStatusConnectedRW:
					newStatus = kNDASUnitStatusConnectedRW;
					break;
				case kNDASUnitStatusDisconnected:
				case kNDASUnitStatusFailure:
				case kNDASUnitStatusNotPresent:
					newStatus = kNDASUnitStatusTryDisconnectRW;
					break;
			}
		}
			break;
		default:
			break;
	}

	if (prevStatus != newStatus) {
		DbgIOLog(DEBUG_MASK_NDAS_INFO, ("Status changed from %d to %d. Status of Unit %d\n", Status(), newStatus, prevStatus));
		
		// Send Message to BunEnumerator.
		sendNDASFamilyIOMessage(kNDASFamilyIOMessageLogicalDeviceStatusWasChanged, UnitDevice()->ID(), sizeof(GUID));
	}
	
	setStatus(newStatus);
	
	return Status();
}

bool com_ximeta_driver_NDASLogicalDevice::sendNDASFamilyIOMessage(UInt32 type, void * argument, vm_size_t argSize)
{
	if(!UnitDevice()) {
		return false;
	}
	
	com_ximeta_driver_NDASBusEnumerator	*bus;
	
	bus = OSDynamicCast(com_ximeta_driver_NDASBusEnumerator, getParentEntry(gIOServicePlane));
	
	if (NULL == bus) {
		DbgIOLog(DEBUG_MASK_NDAS_INFO, ("Can't Find bus.\n"));
	} else {
		bus->messageClients(type, argument, argSize);
	}
	
	return true;
}	

bool com_ximeta_driver_NDASLogicalDevice::receiveDeviceNotification(kNDASDeviceNotificationType type)
{
	
	// Create NDAS Command.
	com_ximeta_driver_NDASDeviceNotificationCommand *command = new com_ximeta_driver_NDASDeviceNotificationCommand();
	if(command == NULL || !command->init()) {
		DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("failed to alloc command class\n"));
		if (command) {
			command->release();
		}
		
		return false;
	}
	
	command->setCommand(type, this);
	
	com_ximeta_driver_NDASLogicalDeviceController *controller = OSDynamicCast(com_ximeta_driver_NDASLogicalDeviceController, getClient());
		
	if(!controller) { 
		DbgIOLog(DEBUG_MASK_NDAS_INFO, ("Controller is not installed.\n"));
		
		command->release();
		
		return false;
	}
		
	controller->enqueueCommand(command);
	
	return true;
}

void com_ximeta_driver_NDASLogicalDevice::completeDeviceNotificationMessage(
																			com_ximeta_driver_NDASDeviceNotificationCommand *command
																			)
{
	DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Entered.\n"));
	
	command->release();
}
