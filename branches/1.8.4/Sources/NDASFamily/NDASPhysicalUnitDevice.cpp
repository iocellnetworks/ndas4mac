/*
 *  NDASPhysicalUnitDevice.cpp
 *  NDASFamily
 *
 *  Created by ?? ? on 05. 11. 29.
 *  Copyright 2005 __MyCompanyName__. All rights reserved.
 *
 */

#include <IOKit/IOMessage.h>

#include "NDASDevice.h"
#include "NDASPhysicalUnitDevice.h"
#include "NDASRAIDUnitDevice.h"
#include "NDASCommand.h"
#include "NDASManagingIOCommand.h"
#include "NDASSCSICommand.h"
#include "NDASDeviceController.h"
#include "NDASLogicalDevice.h"
#include "NDASBusEnumerator.h"

#include "NDASBusEnumeratorUserClientTypes.h"
#include "LanScsiDiskInformationBlock.h"
#include "NDASFamilyIOMessage.h"

#include "Utilities.h"

// Define my superclass
#define super com_ximeta_driver_NDASUnitDevice

// REQUIRED! This macro defines the class's constructors, destructors,
// and several other methods I/O Kit requires. Do NOT use super as the
// second parameter. You must use the literal name of the superclass.

OSDefineMetaClassAndStructors(com_ximeta_driver_NDASPhysicalUnitDevice, com_ximeta_driver_NDASUnitDevice)

bool com_ximeta_driver_NDASPhysicalUnitDevice::init(OSDictionary *dict)
{
	bool res = super::init(dict);
    
	DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Initializing\n"));
		
	memset(&fTargetData, 0, sizeof(TARGET_DATA));
	memset(&fTargetInfo, 0, sizeof(TargetInfo));
	
//	fDataBuffer = NULL;
//	fDevice = NULL;
	fDisconnectedTime = 0;

    return res;
}

void com_ximeta_driver_NDASPhysicalUnitDevice::free(void)
{
    DbgIOLog(DEBUG_MASK_PNP_ERROR, ("Freeing\n"));
		
	// Free Buffer.
	if(NULL != fDataBuffer) {
		IOFree(fDataBuffer, fDataBufferSize);

		fDataBuffer = NULL;
	}
	
	super::free();
}

IOService *com_ximeta_driver_NDASPhysicalUnitDevice::probe(IOService *provider, SInt32 *score)
{
    IOService *res = super::probe(provider, score);
    DbgIOLog(DEBUG_MASK_PNP_ERROR, ("Probing\n"));
    return res;
}

bool com_ximeta_driver_NDASPhysicalUnitDevice::start(IOService *provider)
{
    bool res = super::start(provider);
    DbgIOLog(DEBUG_MASK_PNP_ERROR, ("Starting\n"));
    return res;
}

void com_ximeta_driver_NDASPhysicalUnitDevice::stop(IOService *provider)
{
    DbgIOLog(DEBUG_MASK_PNP_ERROR, ("Stopping\n"));
    super::stop(provider);
}

IOReturn com_ximeta_driver_NDASPhysicalUnitDevice::message( 
													   UInt32		mess, 
													   IOService	* provider,
													   void			* argument 
													   )
{
	DbgIOLog(DEBUG_MASK_PNP_ERROR, ("message: Entered. %d\n", (uint32_t)mess));

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

bool com_ximeta_driver_NDASPhysicalUnitDevice::willTerminate(
														IOService * provider,
														IOOptionBits options 
														)
{
	DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Entered.\n"));
			
	// Cleanup Pending Command.
	if (PendingCommand()) {
		setPendingCommand(NULL);
	}
	
	return true;
}

void com_ximeta_driver_NDASPhysicalUnitDevice::setWritable(bool writable) 
{
	fWritable = writable;
	
	setProperty(NDASDEVICE_PROPERTY_KEY_WRITE_RIGHT, fWritable);
}

uint32_t com_ximeta_driver_NDASPhysicalUnitDevice::Type()
{
	if(fTargetData.bPacket) {
		return NMT_CDROM;
	} else {
		return NMT_SINGLE;
	}
}

bool com_ximeta_driver_NDASPhysicalUnitDevice::IsWritable()
{
	return fWritable;
}

void	com_ximeta_driver_NDASPhysicalUnitDevice::setSlotNumber(int32_t slotNumber)
{
	fSlotNumber = slotNumber;
}

int32_t	com_ximeta_driver_NDASPhysicalUnitDevice::slotNumber()
{
	return fSlotNumber;
}

void	com_ximeta_driver_NDASPhysicalUnitDevice::setUnitNumber(uint32_t unitNumber)
{
	fUnitNumber = unitNumber;
	
	setProperty(NDASDEVICE_PROPERTY_KEY_UNIT_NUMBER, fUnitNumber, 32);
}

uint32_t	com_ximeta_driver_NDASPhysicalUnitDevice::unitNumber()
{
	return fUnitNumber;
}

void com_ximeta_driver_NDASPhysicalUnitDevice::setNeedRefresh(bool refresh)
{
	fNeedRefresh = refresh;
}

bool	com_ximeta_driver_NDASPhysicalUnitDevice::IsNeedRefresh()
{
	return fNeedRefresh;
}

void	com_ximeta_driver_NDASPhysicalUnitDevice::setTargetData(TARGET_DATA *Target)
{
	// Set Values.
	memcpy(&fTargetData, Target, sizeof(TARGET_DATA));
		
	// Set Properties.
	setProperty(NDASDEVICE_PROPERTY_KEY_UNIT_MODEL, fTargetData.model);
	setProperty(NDASDEVICE_PROPERTY_KEY_UNIT_FIRMWARE, fTargetData.firmware);
	setProperty(NDASDEVICE_PROPERTY_KEY_UNIT_SERIALNUMBER, fTargetData.serialNumber);
	setProperty(NDASDEVICE_PROPERTY_KEY_UNIT_TRANSFER_MODE, fTargetData.SelectedTransferMode, 8);
	
	if(!fTargetData.bPacket) {
		
		setProperty(NDASDEVICE_PROPERTY_KEY_UNIT_TYPE, kNDASUnitMediaTypeHDD, 8);
		
	} else {
		setProperty(NDASDEVICE_PROPERTY_KEY_UNIT_TYPE, kNDASUnitMediaTypeODD, 8);
		
	}	
	
}

TARGET_DATA	*com_ximeta_driver_NDASPhysicalUnitDevice::TargetData()
{
	return &fTargetData;
}

TargetInfo	*com_ximeta_driver_NDASPhysicalUnitDevice::ConnectionData()
{
	return &fTargetInfo;
}

bool com_ximeta_driver_NDASPhysicalUnitDevice::createDataBuffer(int bufferSize)
{
	// Allocate Data buffer.
	fDataBuffer = (char *)IOMalloc(bufferSize);
	if(0 == fDataBuffer) {
		DbgIOLog(DEBUG_MASK_LS_ERROR, ("createUnitDevice: Can't Allocate Data buffer\n"));
		
		return false;
	} 
	
	fDataBufferSize = bufferSize;
	
	return true;
}

char	*com_ximeta_driver_NDASPhysicalUnitDevice::DataBuffer()
{
	return fDataBuffer;
}

bool com_ximeta_driver_NDASPhysicalUnitDevice::readSectorsOnce(uint64_t location, uint32_t sectors, char *buffer)
{
	ManagingIOCommand						managingCommand;
	com_ximeta_driver_NDASManagingIOCommand			*NDASCommand;
	com_ximeta_driver_NDASDeviceController	*deviceController;
	bool									result;
	
	DbgIOLog(DEBUG_MASK_NDAS_TRACE, ("readSectorsOnce: Entered.\n"));

	managingCommand.device = this;
	managingCommand.command = kNDASManagingIOReadSectors;
	managingCommand.IsSuccess = false;
	managingCommand.Location = location;
	managingCommand.sectors = sectors;
	managingCommand.buffer = buffer;
	
	// Create NDAS Command.
	NDASCommand = OSTypeAlloc(com_ximeta_driver_NDASManagingIOCommand);
	
	if(NDASCommand == NULL || !NDASCommand->init()) {
		DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("readSectorsOnce: failed to alloc command class\n"));
		if (NDASCommand) {
			NDASCommand->release();
		}
		
		return false;
	}
	
	NDASCommand->setCommand(&managingCommand);
	
	// Send Command.
	deviceController = OSDynamicCast(com_ximeta_driver_NDASDeviceController, 
						getParentEntry(gIOServicePlane));
	if (NULL == deviceController) {
		DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("readSectorsOnce: Can't Find controller.\n"));

		NDASCommand->release();

		return false;		
	}
	
	this->retain();

	if (false == deviceController->executeAndWaitCommand(NDASCommand)) {
		DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("readSectorsOnce: Can't execute command.\n"));

		NDASCommand->release();

		return false;
	}
	
	result = NDASCommand->managingIOCommand()->IsSuccess;
	if (false == result) {
		DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("readSectorsOnce: Managing IO failed.\n"));	
	}
	
	this->release();
	
	NDASCommand->release();
	
	return result;
}

bool com_ximeta_driver_NDASPhysicalUnitDevice::writeSectorsOnce(uint64_t location, uint32_t sectors, char *buffer)
{
	ManagingIOCommand						managingCommand;
	com_ximeta_driver_NDASManagingIOCommand			*NDASCommand;
	com_ximeta_driver_NDASDeviceController	*deviceController;
	bool									result;
	
	if (false ==IsWritable()) {
		DbgIOLog(DEBUG_MASK_DISK_ERROR, ("writeSectorsOnce: No Write right!\n"));
		
		return false;
	}
	
	managingCommand.device = this;
	managingCommand.command = kNDASManagingIOWriteSectors;
	managingCommand.IsSuccess = false;
	managingCommand.Location = location;
	managingCommand.sectors = sectors;
	managingCommand.buffer = buffer;
	
	// Create NDAS Command.
	NDASCommand = OSTypeAlloc(com_ximeta_driver_NDASManagingIOCommand);
	
	if(NDASCommand == NULL || !NDASCommand->init()) {
		DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("writeSectorsOnce: failed to alloc command class\n"));
		if (NDASCommand) {
			NDASCommand->release();
		}
		
		return false;
	}
	
	NDASCommand->setCommand(&managingCommand);
	
	// Send Command.
	deviceController = OSDynamicCast(com_ximeta_driver_NDASDeviceController, 
									 getParentEntry(gIOServicePlane));
	if (NULL == deviceController) {
		DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("writeSectorsOnce: Can't Find controller.\n"));
		
		return false;		
	}
	
	this->retain();
	
	if (false == deviceController->executeAndWaitCommand(NDASCommand)) {
		DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("writeSectorsOnce: Can't execute command.\n"));
		
		return false;
	}
	
	result = NDASCommand->managingIOCommand()->IsSuccess;
	if (false == result) {
		DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("writeSectorsOnce: Managing IO failed.\n"));	
	}
	
	this->release();
	
	NDASCommand->release();
	
	return result;
}

bool com_ximeta_driver_NDASPhysicalUnitDevice::processSRBCommand(com_ximeta_driver_NDASSCSICommand *command)
{
	DbgIOLog(DEBUG_MASK_NDAS_TRACE, ("Entered. command %p, this %p\n", command, this));

	com_ximeta_driver_NDASDeviceController *deviceController;
	
	// Send Command.
	deviceController = OSDynamicCast(com_ximeta_driver_NDASDeviceController, getProvider());
	if (NULL == deviceController) {
		DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("Can't Find controller.\n"));
		
		return false;		
	}
		
	// Set Target Number.
	command->scsiCommand()->targetNo = unitNumber();
	command->scsiCommand()->device = this;
	
	deviceController->enqueueCommand(command);
	
	return true;
}

void com_ximeta_driver_NDASPhysicalUnitDevice::completeCommand(com_ximeta_driver_NDASSCSICommand *command)
{
	DbgIOLog(DEBUG_MASK_NDAS_TRACE, ("Entered. command %p, this %p\n", command, this));
	
	if (UpperUnitDevice()) {
		UpperUnitDevice()->completeCommand(command);
	} else {
		DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("No Device set!!!\n"));
	}	
}

void com_ximeta_driver_NDASPhysicalUnitDevice::setPendingCommand(com_ximeta_driver_NDASSCSICommand* command)
{
	DbgIOLog(DEBUG_MASK_NDAS_WARNING, ("Entered. Command %p\n", command));
	
	if (command &&
		command->scsiCommand()->completeWhenFailed) {
		
		completeCommand(command);

		DbgIOLog(DEBUG_MASK_NDAS_WARNING, ("Completed.\n"));

		return;
	}
	
	if (command) {
		command->retain();
	}

	if (fTargetInfo.fTargetPendingCommand) {
		fTargetInfo.fTargetPendingCommand->release();
	}

	fTargetInfo.fTargetPendingCommand = command;
}

com_ximeta_driver_NDASSCSICommand* com_ximeta_driver_NDASPhysicalUnitDevice::PendingCommand()
{
	return fTargetInfo.fTargetPendingCommand;
}

uint32_t com_ximeta_driver_NDASPhysicalUnitDevice::maxRequestSectors()
{
	return fMaxRequestBytes / blockSize();
}

uint32_t	com_ximeta_driver_NDASPhysicalUnitDevice::updateStatus()
{
	DbgIOLog(DEBUG_MASK_NDAS_TRACE, ("Entered.\n"));

	uint32_t	newStatus;
	uint32_t	prevStatus;
//	bool	bDataConnected;
/*	
	if (!fDevice) {
		DbgIOLog(DEBUG_MASK_NDAS_WARNING, ("updateStatus: Device is not present\n"));
		return kNDASUnitStatusNotPresent;
	}
*/	
	// Check Data Connection.
//	fDevice->checkDataConnection(this);
//	bDataConnected = IsConnected();

	prevStatus = newStatus = Status();

	// Check Present of Unit Device.
	if (!IsPresent()) {
		
		switch(Status()) {
			case kNDASUnitStatusConnectedRO:
			case kNDASUnitStatusConnectedRW:
			case kNDASUnitStatusReconnectRO:
			case kNDASUnitStatusReconnectRW:
				DbgIOLog(DEBUG_MASK_NDAS_WARNING, ("Unit Device is present. But not connected\n"));

				break;
			default:
				DbgIOLog(DEBUG_MASK_NDAS_WARNING, ("Unit Device is Not present.\n"));

				newStatus = kNDASUnitStatusNotPresent;
		}
	} else {
		switch(Status()) {
			case kNDASUnitStatusNotPresent:
			{
				if (IsPresent()) {
					newStatus = kNDASUnitStatusDisconnected;
				}
			}
				break;
			case kNDASUnitStatusFailure:
			{
			}
				break;
			case kNDASUnitStatusDisconnected:
			{
				switch(Configuration()) {
					case kNDASUnitConfigurationMountRO:
						newStatus = kNDASUnitStatusTryConnectRO;
						NDAS_clock_get_system_value(&fStartTimeOfConnecting);
						break;
					case kNDASUnitConfigurationMountRW:
						newStatus = kNDASUnitStatusTryConnectRW;
						NDAS_clock_get_system_value(&fStartTimeOfConnecting);
						break;
					default:
						break;
				}
			}
				break;
			case kNDASUnitStatusTryConnectRO:
			{
				if (IsConnected()) {
					newStatus = kNDASUnitStatusConnectedRO;
				} else {
					uint32_t	currentTime;
					
					NDAS_clock_get_system_value(&currentTime);
					
					if (NDAS_MAX_WAIT_TIME_FOR_TRY_MOUNT <= (currentTime - fStartTimeOfConnecting) ) {
						newStatus = kNDASUnitStatusTryDisconnectRO;
					}
				}
			}
				break;
			case kNDASUnitStatusTryConnectRW:
			{
				if (IsConnected()) {
					newStatus = kNDASUnitStatusConnectedRW;
				} else {
					uint32_t	currentTime;
					
					NDAS_clock_get_system_value(&currentTime);
					
					if (NDAS_MAX_WAIT_TIME_FOR_TRY_MOUNT <= (currentTime - fStartTimeOfConnecting) ) {
						newStatus = kNDASUnitStatusTryDisconnectRW;
					}
				}
				
			}
				break;
			case kNDASUnitStatusConnectedRO:
			{
				if (!IsConnected()) {
					NDAS_clock_get_system_value(&fDisconnectedTime);
					newStatus = kNDASUnitStatusReconnectRO;
				} else if (kNDASUnitConfigurationMountRO != Configuration()) {
					newStatus = kNDASUnitStatusTryDisconnectRO;
				}
			}
				break;
				
			case kNDASUnitStatusConnectedRW:
			{
				if (!IsConnected()) {
					DbgIOLog(DEBUG_MASK_NDAS_INFO, ("updateStatus: kNDASUnitStatusConnectedRW : !IsConnected()\n"));
					NDAS_clock_get_system_value(&fDisconnectedTime);
					newStatus = kNDASUnitStatusReconnectRW;
				} else if (kNDASUnitConfigurationMountRW != Configuration()) {
					DbgIOLog(DEBUG_MASK_NDAS_INFO, ("updateStatus: kNDASUnitStatusConnectedRW : IsConnected() && kNDASUnitConfigurationMountRW != Configuration()\n"));
					newStatus = kNDASUnitStatusTryDisconnectRW;
				}
			}
				break;
			case kNDASUnitStatusReconnectRO:
			case kNDASUnitStatusReconnectRW:
			{
				uint32_t	currentTime;
				
				NDAS_clock_get_system_value(&currentTime);
				
				if (IsConnected()) {
					if (kNDASUnitStatusReconnectRO == Status()) {
						DbgIOLog(DEBUG_MASK_NDAS_WARNING, ("updateStatus: kNDASUnitStatusReconnectRW. currentTime(%d), fDisconnectedTime(%d)\n", currentTime, fDisconnectedTime));
						newStatus = kNDASUnitStatusConnectedRO;
					} else {
						DbgIOLog(DEBUG_MASK_NDAS_WARNING, ("updateStatus: kNDASUnitStatusReconnectRW. currentTime(%d), fDisconnectedTime(%d)\n", currentTime, fDisconnectedTime));
						newStatus = kNDASUnitStatusConnectedRW;
					}
					
					fDisconnectedTime = 0;
					
				} else if ((currentTime - fDisconnectedTime) > NDAS_MAX_WAIT_TIME_FOR_TRY_MOUNT
						   || kNDASUnitConfigurationUnmount == Configuration()) {
					
					if (kNDASUnitStatusReconnectRO == Status()) {
						DbgIOLog(DEBUG_MASK_NDAS_WARNING, ("updateStatus: kNDASUnitStatusReconnectRW. currentTime(%d), fDisconnectedTime(%d)\n", currentTime, fDisconnectedTime));
						newStatus = kNDASUnitStatusTryDisconnectRO;
					} else {
						DbgIOLog(DEBUG_MASK_NDAS_WARNING, ("updateStatus: kNDASUnitStatusReconnectRW. currentTime(%d), fDisconnectedTime(%d)\n", currentTime, fDisconnectedTime));
						newStatus = kNDASUnitStatusTryDisconnectRW;
					}
					
					fDisconnectedTime = 0;
				}				
			}
				break;
			case kNDASUnitStatusTryDisconnectRO:
			case kNDASUnitStatusTryDisconnectRW:
			{
				if (!IsConnected()) {
					newStatus = kNDASUnitStatusDisconnected;
				}		
			}
				break;
			default:
				break;
		}
	}

	setStatus(newStatus);

	if (prevStatus != newStatus) {
		DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("Change Status from %d to %d\n", prevStatus, newStatus));
		
		// Send Message to BunEnumerator.
		sendNDASFamilyIOMessage(kNDASFamilyIOMessagePhysicalUnitDeviceStatusWasChanged, ID(), sizeof(GUID));
	}
		
	return Status();
}

void com_ximeta_driver_NDASPhysicalUnitDevice::setIsPresent(bool present)
{
	fIsPresent = present;
}

bool com_ximeta_driver_NDASPhysicalUnitDevice::IsPresent()
{
	return fIsPresent;
}

bool com_ximeta_driver_NDASPhysicalUnitDevice::sendNDASFamilyIOMessage(UInt32 type, void * argument, vm_size_t argSize) 
{
	com_ximeta_driver_NDASDeviceController *controller;
	
	controller = OSDynamicCast(com_ximeta_driver_NDASDeviceController, getProvider());	
	if (!controller) {
		return false;
	}
	
	com_ximeta_driver_NDASBusEnumerator	*bus = controller->getBusEnumerator();
		
	if (NULL == bus) {
		DbgIOLog(DEBUG_MASK_NDAS_INFO, ("sendNDASFamilyIIOMessage: Can't Find bus.\n"));
		
		return false;
	} else {
		bus->messageClients(type, argument, argSize);
		
		return true;
	}
}	

void com_ximeta_driver_NDASPhysicalUnitDevice::setMaxRequestBytes(uint32_t MRB)
{
	fMaxRequestBytes = MRB;
}

uint32_t com_ximeta_driver_NDASPhysicalUnitDevice::maxRequestBytes()
{
	return fMaxRequestBytes;
}
