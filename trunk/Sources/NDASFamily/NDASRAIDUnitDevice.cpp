/*
 *  NDASRAIDUnitDevice.cpp
 *  NDASFamily
 *
 *  Created by ?? ? on 05. 11. 29.
 *  Copyright 2005 __MyCompanyName__. All rights reserved.
 *
 */

#include <IOKit/IOMessage.h>

#include "NDASRAIDUnitDevice.h"
#include "NDASRAIDUnitDeviceController.h"
#include "NDASLogicalDevice.h"
#include "NDASCommand.h"
#include "NDASBusEnumerator.h"

#include "Hash.h"
#include "scrc32.h"
#include "NDASBusEnumeratorUserClientTypes.h"
#include "LanScsiDiskInformationBlock.h"
#include "NDASDIB.h"
#include "NDASFamilyIOMessage.h"

#include "Utilities.h"

// Define my superclass
#define super com_ximeta_driver_NDASUnitDevice

// REQUIRED! This macro defines the class's constructors, destructors,
// and several other methods I/O Kit requires. Do NOT use super as the
// second parameter. You must use the literal name of the superclass.

OSDefineMetaClassAndStructors(com_ximeta_driver_NDASRAIDUnitDevice, com_ximeta_driver_NDASUnitDevice)

bool com_ximeta_driver_NDASRAIDUnitDevice::init(OSDictionary *dict)
{
	bool res = super::init(dict);
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Initializing\n"));
			
	fCountOfUnitDevices = 0;
	fController = NULL;
	fNDASCommand = NULL;
	fRAIDBlockSize = 0;
	
	setNRRWHosts(0);
	setNRROHosts(0);
	
	fRAIDStatus = kNDASRAIDStatusNotReady;
	
	for (int count = 0; count < NDAS_MAX_UNITS_IN_V2; count++)
	{
		fUnitDeviceArray[count] = NULL;
		fSubUnitStatus[count] = kNDASRAIDSubUnitStatusNotPresent;
	}
	
	fBufferForRecovery = NULL;
	fSizeofBufferForRecovery = 0;
	fDirtyBitmap = NULL;
	fDirty = false;
	fInRecoveryProcess = false;
		
	fPreProcessDone = true;			
	
	lock = IOLockAlloc();
	if (!lock) {
		DbgIOLogNC(DEBUG_MASK_NDAS_ERROR, ("init: Can't alloc lock.\n"));
		
		return false;
	}
	
	fPnpCommand = NULL;

	fWillTerminate = false;

    return res;
}

void com_ximeta_driver_NDASRAIDUnitDevice::free(void)
{
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Freeing\n"));
	
	if ( fBufferForRecovery ) {
		IOFree(fBufferForRecovery,fSizeofBufferForRecovery);
		fBufferForRecovery = NULL;
	}

	if (fDirtyBitmap) {
		IOFree(fDirtyBitmap, NDAS_BLOCK_SIZE_BITMAP * sizeof(NDAS_OOS_BITMAP_BLOCK));
		fDirtyBitmap = NULL;
	}

	if (lock) {
		IOLockFree(lock);
	}
	
	super::free();
}

IOService *com_ximeta_driver_NDASRAIDUnitDevice::probe(IOService *provider, SInt32 *score)
{
    IOService *res = super::probe(provider, score);
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Probing\n"));
    return res;
}

bool com_ximeta_driver_NDASRAIDUnitDevice::start(IOService *provider)
{
    bool res = super::start(provider);
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Starting\n"));
    return res;
}

void com_ximeta_driver_NDASRAIDUnitDevice::stop(IOService *provider)
{
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Stopping\n"));
    super::stop(provider);
}

IOReturn com_ximeta_driver_NDASRAIDUnitDevice::message( 
													   UInt32		mess, 
													   IOService	* provider,
													   void			* argument 
													   )
{
	
	DbgIOLog(DEBUG_MASK_PNP_TRACE, ("message: Entered.\n"));

	switch ( mess ) {
		case kIOMessageServiceIsRequestingClose:
			fWillTerminate = true;
		case kIOMessageServiceIsResumed:
		case kIOMessageServiceIsSuspended:
		case kIOMessageServiceIsTerminated:
			messageClients(mess);
			return kIOReturnSuccess;
		default:
			break;
	}

    return IOService::message(mess, provider, argument );
}

bool com_ximeta_driver_NDASRAIDUnitDevice::willTerminate(
														IOService * provider,
														IOOptionBits options 
														)
{
	DbgIOLog(DEBUG_MASK_PNP_TRACE, ("willTerminate: Entered.\n"));
		
	return true;
}

void	com_ximeta_driver_NDASRAIDUnitDevice::setType(UInt32 type) 
{
	fType = type;
	
	// Set Properties of Unit Device.
	setProperty(NDASDEVICE_PROPERTY_KEY_UNIT_DEVICE_TYPE, fType, 32);

}

UInt32 com_ximeta_driver_NDASRAIDUnitDevice::Type()
{
	return fType;
}

void	com_ximeta_driver_NDASRAIDUnitDevice::setIndex(UInt32 index)
{
	fIndex = index;
	
	setProperty(NDASDEVICE_PROPERTY_KEY_INDEX, fIndex, 32);
}

UInt32	com_ximeta_driver_NDASRAIDUnitDevice::Index()
{
	return fIndex;
}

void	com_ximeta_driver_NDASRAIDUnitDevice::setConfigID(PGUID ID)
{
	memcpy(&fConfigID, ID, sizeof(GUID));
	setProperty(NDASDEVICE_PROPERTY_KEY_RAID_CONFIG_ID, &fConfigID, 128);
}

PGUID	com_ximeta_driver_NDASRAIDUnitDevice::ConfigID()
{
	return &fConfigID;
}

void	com_ximeta_driver_NDASRAIDUnitDevice::setRMDUSN(UInt32 usn)
{
	fRMDUSN = usn;
	
	setProperty(NDASDEVICE_PROPERTY_KEY_RAID_RMD_SEQUENCE, fRMDUSN, 32);
}

UInt32	com_ximeta_driver_NDASRAIDUnitDevice::RMDUSN()
{
	return fRMDUSN;
}

bool com_ximeta_driver_NDASRAIDUnitDevice::IsWorkable()
{
	DbgIOLog(DEBUG_MASK_RAID_TRACE, ("IsWorkable: Entered.\n"));

	// Check Supported TYPE?
	if ( NMT_INVALID == Type() ) {
		return false;
	}
			
	switch(Type()) {
		case NMT_AGGREGATE:
		case NMT_RAID0:
		case NMT_RAID0R2:
		{
			for (int count = 0; count < fCountOfUnitDevices; count++) {
				if (!fUnitDeviceArray[count]) {
					// Missing Unit Device.
					return false;
				}
			}
		}
			break;
		case NMT_MIRROR:
		case NMT_RAID1:
		case NMT_RAID1R2:
		case NMT_RAID1R3:
		{
			if (IsInRunningStatus()) {
				for (int count = 0; count < fCountOfUnitDevices; count++) {
					if (fUnitDeviceArray[count]) {
						// Missing Unit Device.
						return true;
					}
				}				
			} else {
				for (int count = 0; count < fCountOfUnitDevices; count++) {
					if (!fUnitDeviceArray[count]) {
						// Missing Unit Device.
						return false;
					}
				}
				
			}

		}
			break;
		default:
			return false;
	}
	
	return true;
}

bool com_ximeta_driver_NDASRAIDUnitDevice::IsWritable()
{
	bool	writable = true;
	
	DbgIOLog(DEBUG_MASK_RAID_TRACE, ("IsWritable: Entered.\n"));

	for (int count = 0; count < fCountOfUnitDevices; count++) {
		if (fUnitDeviceArray[count]) {
			if (false == fUnitDeviceArray[count]->IsWritable()) {
				writable = false;
			}
		}
	}
	
	setProperty(NDASDEVICE_PROPERTY_KEY_WRITE_RIGHT, writable);

	return true;
}

UInt64	com_ximeta_driver_NDASRAIDUnitDevice::getNumberOfSectors()
{
	DbgIOLog(DEBUG_MASK_RAID_TRACE, ("getNumberOfSectors: Entered.\n"));

	UInt64	NumberOfSectors = 0;
	
	if (!IsWorkable()) {
		DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("getNumberOfSectors: Missing Unit Device.\n"));
		
		return 0;
	}

	switch (fType) {
	case NMT_AGGREGATE:
	{
		for (int count = 0; count < fCountOfUnitDevices; count++) {
			if (fUnitDeviceArray[count]) {
				NumberOfSectors += fUnitDeviceArray[count]->Capacity();
			} else {
				DbgIOLog(DEBUG_MASK_RAID_ERROR, ("getNumberOfSectors: No Unit Device!!!!!\n"));			
			}
		}		
	}
		break;
	case NMT_RAID0:
	case NMT_RAID0R2:
	{
		// Find Smallest Size.
		for (int count = 0; count < fCountOfUnitDevices; count++) {
			
			if (count == 0) {
				NumberOfSectors = fUnitDeviceArray[0]->Capacity();
			} else {
				if ( NumberOfSectors > fUnitDeviceArray[count]->Capacity()) {
					NumberOfSectors = fUnitDeviceArray[count]->Capacity();
				}
				
			}
		}		
		
		NumberOfSectors *= fCountOfUnitDevices;
	}
		break;
	case NMT_MIRROR:
	case NMT_RAID1:
	case NMT_RAID1R2:
	case NMT_RAID1R3:
	{
		// Find Smallest Size.
		for (int count = 0; count < fCountOfUnitDevices; count++) {
			
			if (fUnitDeviceArray[count]) {
				
				if (NumberOfSectors == 0) {
					NumberOfSectors = fUnitDeviceArray[count]->Capacity();
				} else {
					if ( NumberOfSectors > fUnitDeviceArray[count]->Capacity()) {
						NumberOfSectors = fUnitDeviceArray[count]->Capacity();
					}
					
				}
			}
		}		
	}
		break;		
	case NMT_RAID4R3:
	{
		// Find Smallest Size.
		for (int count = 0; count < fCountOfUnitDevices; count++) {
			
			if (count == 0) {
				NumberOfSectors = fUnitDeviceArray[0]->Capacity();
			} else {
				if ( NumberOfSectors > fUnitDeviceArray[count]->Capacity()) {
					NumberOfSectors = fUnitDeviceArray[count]->Capacity();
				}
				
			}
		}		
		
		NumberOfSectors *= CountOfUnitDevices();
	}
		break;		
	default:
	{
		DbgIOLog(DEBUG_MASK_RAID_WARNING, ("getNumberOfSectors: Unsuppoerted RAID Type.\n"));		
		
		NumberOfSectors = 0;
	}
		break;
	}	
	
	return NumberOfSectors;
}

UInt32	com_ximeta_driver_NDASRAIDUnitDevice::getNRRWHosts()
{
	UInt32 hosts = 0;
	
	if (IsWorkable()) {

		for (int count = 0; count < fCountOfUnitDevices; count++) {
			if (fUnitDeviceArray[count]) {
				
				if (kNDASUnitStatusNotPresent == fUnitDeviceArray[count]->Status()) {
					return 0;
				}
				
				if (hosts == 0) {
					hosts = fUnitDeviceArray[count]->NRRWHosts();
				} else {
					if (hosts < fUnitDeviceArray[count]->NRRWHosts()) {
						hosts = fUnitDeviceArray[count]->NRRWHosts();
					}
				}
				
			}
		}
		
		return hosts;

	} else {
		if ( NMT_RAID1R3 == DiskArrayType() ){
			
			int tempVaule = 0;
			
			if (fUnitDeviceArray[0]) {
				
				tempVaule = fUnitDeviceArray[0]->NRRWHosts();
				
			} else if (tempVaule == 0
					   && fUnitDeviceArray[1]) {
				
				return fUnitDeviceArray[1]->NRRWHosts();

			} else {
				return 0;
			}
			
		} else {
			return 0;
		}
	}
}

UInt32	com_ximeta_driver_NDASRAIDUnitDevice::getNRROHosts()
{
	UInt32 hosts = 0;
	
	if (IsWorkable()) {
		
		for (int count = 0; count < fCountOfUnitDevices; count++) {
			if (fUnitDeviceArray[count]) {
				
				if (kNDASUnitStatusNotPresent == fUnitDeviceArray[count]->Status()) {
					return 0;
				}
				
				if (hosts == 0) {
					hosts = fUnitDeviceArray[count]->NRROHosts();
				} else {
					if (hosts < fUnitDeviceArray[count]->NRROHosts()) {
						hosts = fUnitDeviceArray[count]->NRROHosts();
					}
				}
				
			} 
		}
		
		return hosts;
		
	} else {
		if ( NMT_RAID1R3 == DiskArrayType() ){
			
			int tempVaule = 0;
			
			if (fUnitDeviceArray[0]) {
				
				tempVaule = fUnitDeviceArray[0]->NRROHosts();
				
			} else if (fUnitDeviceArray[1]
					   && tempVaule < fUnitDeviceArray[1]->NRROHosts()) {
				
				return fUnitDeviceArray[1]->NRROHosts();
				
			} else {
				return 0;
			}			
		} else {
			return 0;
		}
	}
}

void	com_ximeta_driver_NDASRAIDUnitDevice::setConfigurationOfSubUnits(UInt32 configuration)
{
	for (int count = 0; count < fCountOfUnitDevices + fCountOfSpareDevices; count++) {
		if (fUnitDeviceArray[count]) {
			fUnitDeviceArray[count]->setConfiguration(configuration);
		}
	}	
}

void	com_ximeta_driver_NDASRAIDUnitDevice::setConfiguration(UInt32 configuration)
{
	DbgIOLog(DEBUG_MASK_RAID_TRACE, ("setConfiguration: Entered.\n"));
	
	switch(configuration) {
		case kNDASUnitConfigurationMountRW:
		case kNDASUnitConfigurationMountRO:
		{
			if (kNDASRAIDStatusGood != RAIDStatus()
				&& kNDASRAIDStatusReadyToRecovery != RAIDStatus())
			{
				return;
			}
		}
			break;
		case kNDASUnitConfigurationUnmount:
		{
			if (!fPreProcessDone) {
				
				fPreProcessDone = true;
				
				if (!preProcessingforDisconnectRW()) {
					DbgIOLog(DEBUG_MASK_RAID_ERROR, ("setConfiguration: preProcessingforDisconnectRW Failed!!!\n"));				
				}
			}
		}
			break;
		default:
			break;		
	}
	
	setConfigurationOfSubUnits(configuration);
	
	super::setConfiguration(configuration);
}

void	com_ximeta_driver_NDASRAIDUnitDevice::setController(com_ximeta_driver_NDASRAIDUnitDeviceController *controller)
{
	if(fController) {
		fController->release();
	}
	
	if (controller) {
		controller->retain();
	}
	
	fController = controller;
}

com_ximeta_driver_NDASRAIDUnitDeviceController *com_ximeta_driver_NDASRAIDUnitDevice::Controller()
{
	return fController;
}

bool com_ximeta_driver_NDASRAIDUnitDevice::readSectorsOnce(UInt64 location, UInt32 sectors, char *buffer)
{
	UInt64	endSector;
	int		count;
	
	DbgIOLog(DEBUG_MASK_RAID_TRACE, ("readSectorsOnce: Entered. Location %lld, Sectors %d\n", location, sectors));

	if (!IsWorkable()) {
		return false;
	}
		
	// Check End Sector.
	endSector = NumberOfSectors() - 1;
	
	if (endSector < (location + sectors - 1)) {
		DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("readSectorsOnce: Invald parameter!\n"));
		return false;
	}
	
	switch(fType) {
		case NMT_AGGREGATE:
		{
			UInt64	endSector;
			UInt64	offset, reqSectors, reqLocation;
			
			offset = 0;
			reqSectors = sectors; 
			reqLocation = location;
			
			for (count = 0; count < fCountOfUnitDevices; count++) {
				if (fUnitDeviceArray[count]) {
					endSector = fUnitDeviceArray[count]->NumberOfSectors() - 1;
					
					if (reqLocation <= endSector) {
						
						UInt64	actualSectors;
						bool	result;
						
						if (endSector < reqLocation + reqSectors - 1) {
							actualSectors = endSector - reqLocation + 1;
						} else {
							actualSectors = reqSectors;
						}
						
						reqSectors -= actualSectors;
						
						result = fUnitDeviceArray[count]->readSectorsOnce(
																		  reqLocation,
																		  actualSectors,
																		  &buffer[offset * SECTOR_SIZE]
																		  );
						if (!result) {
							goto ErrorOut;
						}
						
						
						if (0 == reqSectors) {
							// Done.
							break;
						}
						
						offset += actualSectors;
						
						reqLocation += actualSectors;
					}
										
				} else {
					DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("readSectorsOnce: No Unit Device!!!\n"));
				}
				
				reqLocation -= endSector + 1;
			}
		}
			break;
		case NMT_RAID0R2:
		{			
			if (0 == RAIDBlocksPerSector()) {
				DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("readSectorsOnce: RAID Block Size is 0!!!\n"));
				break;
			}
			
			UInt64	offset, reqSectors, reqLocation;
							
			offset = 0;
			reqSectors = sectors; 
			reqLocation = location;
			
			while (reqSectors) {
				bool	result;
				UInt32	diskSequence;
				UInt64	newLocation;
				UInt32	actualSectors;
			
				diskSequence = (reqLocation / RAIDBlocksPerSector()) % CountOfUnitDevices();
				newLocation = ((reqLocation / CountOfUnitDevices() / RAIDBlocksPerSector()) * RAIDBlocksPerSector()) 
							   + (location % RAIDBlocksPerSector());	
				actualSectors = (reqSectors > (RAIDBlocksPerSector() - (reqLocation % RAIDBlocksPerSector()))) ? 
								 (RAIDBlocksPerSector()  - (reqLocation % RAIDBlocksPerSector())): 
								 reqSectors;
							   
				DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("readSectorsOnce: disk Seq %d, newLoc %lld, actual %d\n",
												 diskSequence, newLocation, actualSectors));
				
				result = fUnitDeviceArray[diskSequence]->readSectorsOnce(
																		 newLocation,
																		 actualSectors,
																		 &buffer[offset * SECTOR_SIZE]
																		 );
				if (!result) {
					goto ErrorOut;
				}
				
				offset += actualSectors;
				reqSectors -= actualSectors;
				reqLocation += actualSectors;
			}
		}
			break;		
		case NMT_RAID1R3:
		{
			bool result;
			
			for (count = 0; count < fCountOfUnitDevices; count++) {
				if (fUnitDeviceArray[count] &&
					fUnitDeviceArray[count]->Status() != kNDASUnitStatusNotPresent) {
					
					result = fUnitDeviceArray[count]->readSectorsOnce(
																	  location,
																	  sectors,
																	  buffer
																	  );
					
					if (true == result) {
						return true;
					} else {
						//this->setStatusOfSubUnit(count, kNDASRAIDSubUnitStatusFailure);
						goto ErrorOut;
					}
					
				} else {
					DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("readSectorsOnce: Unit Device is inactive %d\n", count));
				}
			}
			
			goto ErrorOut;
		}
			break;
		default:
			return false;
			break;
	}
	
	return true;

ErrorOut:

	if (IsInRunningStatus()) {

		//this->setStatusOfSubUnit(count, kNDASRAIDSubUnitStatusFailure);
		updateRAIDStatus();
	}
	
	return false;
}

bool com_ximeta_driver_NDASRAIDUnitDevice::writeSectorsOnce(UInt64 location, UInt32 sectors, char *buffer)
{
	int count;
	
	DbgIOLog(DEBUG_MASK_RAID_TRACE, ("writeSectorsOnce: Entered.\n"));
	
	if (!IsWorkable()) {
		return false;
	}
	
	switch(fType) {
		case NMT_AGGREGATE:
		{
			UInt64	endSector;
			UInt64	offset, reqSectors, reqLocation;
			
			offset = 0;
			reqSectors = sectors; 
			reqLocation = location;
			
			for (count = 0; count < fCountOfUnitDevices; count++) {
				if (fUnitDeviceArray[count]) {
					endSector = fUnitDeviceArray[count]->NumberOfSectors() - 1;
					
					if (reqLocation <= endSector) {
						
						UInt64	actualSectors;
						bool	result;
						
						if (endSector < reqLocation + reqSectors - 1) {
							actualSectors = endSector - reqLocation + 1;
						} else {
							actualSectors = reqSectors;
						}
						
						reqSectors -= actualSectors;
						
						result = fUnitDeviceArray[count]->writeSectorsOnce(
																		  reqLocation,
																		  actualSectors,
																		  &buffer[offset * SECTOR_SIZE]
																		  );
						if (!result) {
								goto ErrorOut;
						}
						
						
						if (0 == reqSectors) {
							// Done.
							break;
						}
						
						offset += actualSectors;
						
						reqLocation += actualSectors;
					}
					
				} else {
					DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("writeSectorsOnce: No Unit Device!!!\n"));
				}
				
				reqLocation -= endSector + 1;
			}
		}
			break;
		case NMT_RAID0R2:
		{			
			if (0 == RAIDBlocksPerSector()) {
				DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("writeSectorsOnce: RAID Block Size is 0!!!\n"));
				break;
			}
			
			UInt64	offset, reqSectors, reqLocation;
			
			offset = 0;
			reqSectors = sectors; 
			reqLocation = location;
			
			while (reqSectors) {
				bool	result;
				UInt32	diskSequence;
				UInt64	newLocation;
				UInt32	actualSectors;
				
				diskSequence = (reqLocation / RAIDBlocksPerSector()) % CountOfUnitDevices();
				newLocation = ((location / CountOfUnitDevices() / RAIDBlocksPerSector()) * RAIDBlocksPerSector()) 
					+ (location % RAIDBlocksPerSector());	
				actualSectors = (reqSectors > (RAIDBlocksPerSector() - (reqLocation % RAIDBlocksPerSector()))) ? 
					(RAIDBlocksPerSector()  - (reqLocation % RAIDBlocksPerSector())): 
					reqSectors;				
				
				DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("writeSectorsOnce: disk Seq %d, newLoc %lld, actual %d\n",
												 diskSequence, newLocation, actualSectors));
				
				result = fUnitDeviceArray[diskSequence]->writeSectorsOnce(
																		 newLocation,
																		 actualSectors,
																		 &buffer[offset * SECTOR_SIZE]
																		 );
				if (!result) {
						goto ErrorOut;
				}
				
				offset += actualSectors;
				reqSectors -= actualSectors;
				reqLocation += actualSectors;
			}
		}
			break;				
		case NMT_RAID1R3:
		{
			bool result;
			
			for (count = 0; count < fCountOfUnitDevices; count++) {
				if (fUnitDeviceArray[count] &&
					fUnitDeviceArray[count]->Status() != kNDASUnitStatusNotPresent) {
					
					result = fUnitDeviceArray[count]->writeSectorsOnce(
																	  location,
																	  sectors,
																	  buffer
																	  );
					if(false == result) {
						
						goto ErrorOut;
					}
					
				} else {
					DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("writeSectorsOnce: Unit Device is inactive %d\n", count));
				}
			}
		}
			break;			
		default:
			return false;
			break;
	}
	
	return true;
	
ErrorOut:
		
		if (IsInRunningStatus()) {
			
			//this->setStatusOfSubUnit(count, kNDASRAIDSubUnitStatusFailure);
			updateRAIDStatus();
		}
	
	return false;
	
}

bool com_ximeta_driver_NDASRAIDUnitDevice::processSRBCommand(com_ximeta_driver_NDASCommand *command)
{
	DbgIOLog(DEBUG_MASK_RAID_TRACE, ("processSRBCommand: Entered.\n"));

	if (!command) {
		DbgIOLog(DEBUG_MASK_RAID_ERROR, ("processSRBCommand: No Command!!!\n"));
		
		return false;
	}
	
	if(!fController) { 
		DbgIOLog(DEBUG_MASK_RAID_ERROR, ("processSRBCommand: No Controller!!!\n"));
		
		if (UpperUnitDevice()) {
			UpperUnitDevice()->completeCommand(command);
		}		
		
		return false;
	}

	// Check RAID Status.
	switch(RAIDStatus()) {
		case kNDASRAIDStatusBroken:
		case kNDASRAIDStatusNotReady:
		case kNDASRAIDStatusUnsupported:
		{
			DbgIOLog(DEBUG_MASK_RAID_ERROR, ("processSRBCommand: Bad RAID Status %d\n", RAIDStatus()));
			
			if (UpperUnitDevice()) {
				UpperUnitDevice()->completeCommand(command);
			}
			
			return false;
		}
			break;
		default:
			break;
	}
		
	if(NULL != NDASCommand()) {
		DbgIOLog(DEBUG_MASK_RAID_ERROR, ("processSRBCommand: Command assigned already!!!\n"));
		
		if (UpperUnitDevice()) {
			UpperUnitDevice()->completeCommand(command);
		}		
		
		return false;		
	}
		
	setNDASCommand(command);
	
	fController->enqueueCommand(command);

	return true;
}

void com_ximeta_driver_NDASRAIDUnitDevice::completeRecievedCommand()
{
	DbgIOLog(DEBUG_MASK_RAID_TRACE, ("completeRecievedCommand: Entered\n"));
	
	com_ximeta_driver_NDASCommand *receivedCommand = NDASCommand();
	
	if (NULL != receivedCommand) {
		setNDASCommand(NULL);
		
		UpperUnitDevice()->completeCommand(receivedCommand);
	}
}

// It was called by lower Unit Device.
void com_ximeta_driver_NDASRAIDUnitDevice::completeCommand(com_ximeta_driver_NDASCommand *command)
{	
	command->convertSRBToSRBCompletedCommand();
	
	fController->enqueueCommand(command);
}

bool	com_ximeta_driver_NDASRAIDUnitDevice::IsConnected()
{
	DbgIOLog(DEBUG_MASK_RAID_TRACE, ("IsConnected: Entered.\n"));

	return fConnected;
}

void	com_ximeta_driver_NDASRAIDUnitDevice::setConnected(bool connected)
{
	DbgIOLog(DEBUG_MASK_RAID_TRACE, ("setConnected: Entered.\n"));

	fConnected = connected;
	
	setProperty(NDASDEVICE_PROPERTY_KEY_UNIT_CONNECTED, fConnected);
}

bool	com_ximeta_driver_NDASRAIDUnitDevice::setUnitDevice(SInt32 index, com_ximeta_driver_NDASUnitDevice *device)
{
	DbgIOLog(DEBUG_MASK_RAID_ERROR, ("setUnitDevice: Entered. device [%d] 0x%x\n", index, device));

	if(index < 0 || index >= NDAS_MAX_UNITS_IN_V2) {
		
		DbgIOLog(DEBUG_MASK_RAID_ERROR, ("setUnitDevice: Bad Sequence. %d\n", index));

		return false;
	}
	
	if (fUnitDeviceArray[index]) {
		fUnitDeviceArray[index]->release();
	}
	
	if (device) {
		device->retain();
	}
	
	fUnitDeviceArray[index] = device;
	
	updateRAIDStatus();
	
	char buffer[128] = { 0 };
	
	if (device) {
		
		UInt64	temp;
		
		sprintf(buffer, NDASDEVICE_PROPERTY_KEY_RAID_UNIT_DISK_LOCATION_OF_SUB_UNIT, index);
		
		memcpy(&temp, device->UnitDiskLocation(), sizeof(UInt64));
		
		setProperty(buffer, temp, 64);		
		
		// Set SubUnit Type.
		UInt32 SubUnitType = kNDASRAIDSubUnitTypeNormal; 
		
		if (NMT_RAID1R3 == device->DiskArrayType()) {
			
			if (NDAS_UNIT_META_BIND_STATUS_SPARE & getRMDStatusOfUnit(device->RMD(), index)) {
				SubUnitType = kNDASRAIDSubUnitTypeSpare;
			} 
			if (NDAS_UNIT_META_BIND_STATUS_REPLACED_BY_SPARE & getRMDStatusOfUnit(device->RMD(), index)) {
				SubUnitType = kNDASRAIDSubUnitTypeReplacedBySpare;
			}
		}
		
		setTypeOfSubUnit(index, SubUnitType);
	}
	
	return true;
}

void	com_ximeta_driver_NDASRAIDUnitDevice::setCountOfUnitDevices(UInt32 count)
{
	DbgIOLog(DEBUG_MASK_RAID_TRACE, ("setCountOfUnitDevices: Entered.\n"));

	fCountOfUnitDevices = count;
	
	DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("setCountOfUnitDevices: Count of Unit Devices %d\n", fCountOfUnitDevices));
	
	setProperty(NDASDEVICE_PROPERTY_KEY_RAID_DEVICE_COUNTOFUNITS, fCountOfUnitDevices, 32);
}

UInt32	com_ximeta_driver_NDASRAIDUnitDevice::CountOfUnitDevices()
{
	DbgIOLog(DEBUG_MASK_RAID_TRACE, ("CountOfUnitDevices: Entered.\n"));

	return fCountOfUnitDevices;
}

UInt32	com_ximeta_driver_NDASRAIDUnitDevice::CountOfRegistetedUnitDevices()
{
	DbgIOLog(DEBUG_MASK_RAID_TRACE, ("CountOfRegistetedUnitDevices: Entered.\n"));

	UInt32 countOfUnits = 0;
	
	for (int count = 0; count < fCountOfUnitDevices + fCountOfSpareDevices; count++) {
		if (fUnitDeviceArray[count]) {
			countOfUnits++;
		}
	}
	
	return countOfUnits;
}

void	com_ximeta_driver_NDASRAIDUnitDevice::setCountOfSpareDevices(UInt32 count)
{
	DbgIOLog(DEBUG_MASK_RAID_TRACE, ("setCountOfSpareDevices: Entered.\n"));
	
	fCountOfSpareDevices = count;
	
	DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("setCountOfSpareDevices: Count of Unit Devices %d\n", fCountOfSpareDevices));
	
	setProperty(NDASDEVICE_PROPERTY_KEY_RAID_DEVICE_COUNTOFSPARES, fCountOfSpareDevices, 32);
}

UInt32	com_ximeta_driver_NDASRAIDUnitDevice::CountOfSpareDevices()
{
	DbgIOLog(DEBUG_MASK_RAID_TRACE, ("CountOfSpareDevices: Entered.\n"));
	
	return fCountOfSpareDevices;
}

void	com_ximeta_driver_NDASRAIDUnitDevice::setSenseKey(UInt32 senseKey)
{
	fSenseKey = senseKey;
}

UInt32	com_ximeta_driver_NDASRAIDUnitDevice::senseKey()
{
	return fSenseKey;
}

com_ximeta_driver_NDASUnitDevice	*com_ximeta_driver_NDASRAIDUnitDevice::UnitDeviceAtIndex(UInt32 index)
{
	if (index > NDAS_MAX_UNITS_IN_V2) {
		return NULL;
	}
	
	return fUnitDeviceArray[index];
}

void	com_ximeta_driver_NDASRAIDUnitDevice::setNDASCommand(com_ximeta_driver_NDASCommand *command)
{
	if(fNDASCommand) {
		fNDASCommand->release();
	}
	
	if (command) {
		command->retain();
	}
	
	fNDASCommand = command;	
}

com_ximeta_driver_NDASCommand *com_ximeta_driver_NDASRAIDUnitDevice::NDASCommand()
{
	return fNDASCommand;
}

UInt32 com_ximeta_driver_NDASRAIDUnitDevice::maxRequestSectors()
{
	UInt32	tempValue = 0;

	// Find Smallest size.
	for (int count = 0; count < fCountOfUnitDevices; count++) {
		if (fUnitDeviceArray[count]) {
			if (tempValue == 0) {
				tempValue = fUnitDeviceArray[count]->maxRequestSectors();
			} else {
				if (tempValue > fUnitDeviceArray[count]->maxRequestSectors()) {
					tempValue = fUnitDeviceArray[count]->maxRequestSectors();
				}
			}
		}
	}
	
	switch(fType) {
		case NMT_AGGREGATE:
		case NMT_RAID0R2:
		{
			tempValue *= CountOfUnitDevices();
		}
			break;
		case NMT_RAID1R3:
		{
		}
			break;
		default:
			tempValue = 64;
			
			break;
	}
	
	DbgIOLog(DEBUG_MASK_RAID_INFO, ("maxRequestSectors: return value is %d\n", tempValue));
	
	return tempValue;
}

bool	com_ximeta_driver_NDASRAIDUnitDevice::unbind()
{
	UInt64	location;
	char	buffer[SECTOR_SIZE] = { 0 };
	bool	result;
	
	// Check UnitDevice status.
	if (kNDASUnitConfigurationUnmount != Configuration() ||
		IsInRunningStatus()) {
		
		DbgIOLog(DEBUG_MASK_RAID_ERROR, ("unbind: Not Unmounted Status.\n"));
		return false;
	}
	
	for (int count = 0; count < CountOfUnitDevices() + CountOfSpareDevices(); count++) {
		if (UnitDeviceAtIndex(count)) {
			
			location = UnitDeviceAtIndex(count)->NumberOfSectors() + NDAS_BLOCK_LOCATION_DIB_V2;

			// Delete DIB_v2.
			result = UnitDeviceAtIndex(count)->writeSectorsOnce(location, 1,  buffer);
			
			location = UnitDeviceAtIndex(count)->NumberOfSectors() + NDAS_BLOCK_LOCATION_DIB_V1;
			
			DbgIOLog(DEBUG_MASK_RAID_ERROR, ("unbind: location %lld\n", location));
			
			// Delete DIB_v1.
			result = UnitDeviceAtIndex(count)->writeSectorsOnce(location, 1,  buffer);			
			
			// Delete RMD.
			location = UnitDeviceAtIndex(count)->NumberOfSectors() + NDAS_BLOCK_LOCATION_RMD;

			result = UnitDeviceAtIndex(count)->writeSectorsOnce(location, 1,  buffer);			
			
			switch(Type()) {
				case NMT_AGGREGATE:
				case NMT_RAID0:
				case NMT_RAID0R2:
				{
					// Clear MBR
					if(!writeSectorsOnce(NDAS_BLOCK_LOCATION_MBR, 1, buffer)) {
						DbgIOLog(DEBUG_MASK_RAID_ERROR, ("unbind: Can't Write MBR Location %lld\n", NDAS_BLOCK_LOCATION_MBR));
					}
					
					// Clear Partition 1 for DOS.
					if(!writeSectorsOnce(NDAS_BLOCK_LOCATION_USER, 1, buffer)) {
						DbgIOLog(DEBUG_MASK_RAID_ERROR, ("unbind: Can't Write Partition 1 Location %lld\n", NDAS_BLOCK_LOCATION_USER));
					}		
					
					// For Apple.
					for (int count = 1; count < 10; count++) {
						if(!writeSectorsOnce(count, 1, buffer)) {
							DbgIOLog(DEBUG_MASK_RAID_ERROR, ("unbind: Can't Write Apple Partition Info Location %lld\n", count));
						}		
					}
				}
					break;
			}
		}
	}
		
	return result;
}

bool	com_ximeta_driver_NDASRAIDUnitDevice::rebind()
{
	UInt64				location;
	bool				result = true;
	uint32_t			count;
	NDAS_DIB_V2			dib_v2 = { 0 };
	NDAS_RAID_META_DATA	rmd = { 0 };
	bool				hasReplacedUnit = false;
	UInt32				replacedUnitIndex;

	// Check UnitDevice status.
	if (kNDASUnitConfigurationUnmount != Configuration() ||
		IsInRunningStatus()) {
		
		DbgIOLog(DEBUG_MASK_RAID_ERROR, ("rebind: Not Unmounted Status.\n"));
		return false;
	}
		
	if (kNDASRAIDStatusGoodWithReplacedUnit == RAIDStatus()
		|| kNDASRAIDStatusReadyToRecoveryWithReplacedUnit == RAIDStatus()) {

		// Check All SubUnit.
		for (count = 0; count < CountOfUnitDevices(); count++) {
			if (!UnitDeviceAtIndex(count)) {
				DbgIOLog(DEBUG_MASK_RAID_ERROR, ("rebind: No SubUnit at index %d.\n", count));
				
				return false;
			}
		}
		
		memcpy(&dib_v2, UnitDeviceAtIndex(0)->DIB2(), sizeof(NDAS_DIB_V2));
		memcpy(&rmd, UnitDeviceAtIndex(0)->RMD(), sizeof(NDAS_RAID_META_DATA));
		
		// Check Replaceing of Spare disk.				
		for(count = 0; count < CountOfUnitDevices() + CountOfSpareDevices(); count++) {
			
			UInt32 index = getRMDIndexOfUnit(&rmd, count);

			DbgIOLog(DEBUG_MASK_RAID_ERROR, ("rebind: Sequence %d, index %d.\n", count, index));

			if (kNDASRAIDSubUnitTypeReplacedBySpare == TypeOfSubUnit(index)) {
				// It is Replaced by Spare disk.					   
				hasReplacedUnit = true;
				replacedUnitIndex = index;
				
				DbgIOLog(DEBUG_MASK_RAID_ERROR, ("rebind: [Replaced Unit] Sequence %d, index %d.\n", count, index));
			} 				
			
			if (count != index
				&& UnitDeviceAtIndex(index)) {

				UnitDeviceAtIndex(index)->DIB2()->iSequence = NDASSwap32HostToLittle(index);
				
				int innerCount;
				
				for (innerCount = 0; innerCount <  CountOfUnitDevices() + CountOfSpareDevices(); innerCount++) {
					
					if (UnitDeviceAtIndex(innerCount)) {
						
						// DIB
						memcpy(&UnitDeviceAtIndex(innerCount)->DIB2()->UnitDisks[index], UnitDeviceAtIndex(index)->UnitDiskLocation(), sizeof(UInt64));
						
						// RMD
						UnitDeviceAtIndex(innerCount)->RMD()->UnitMetaData[index].iUnitDeviceIdx = NDASSwap16HostToLittle(index);
						UnitDeviceAtIndex(innerCount)->RMD()->UnitMetaData[index].UnitDeviceStatus = getRMDStatusOfUnit(&rmd, index);
					}
				} 
			}
		}
		
		if (hasReplacedUnit) {
			for(count = 0; count < CountOfUnitDevices() + CountOfSpareDevices(); count++) {

				if (UnitDeviceAtIndex(count)) {
					UnitDeviceAtIndex(count)->DIB2()->nSpareCount = NDASSwap32HostToLittle(0);
				}
			}			
		}
	} else {
		
		// Check All SubUnit.
		for (count = 0; count < CountOfUnitDevices() + CountOfSpareDevices(); count++) {
			if (!UnitDeviceAtIndex(count)) {
				DbgIOLog(DEBUG_MASK_RAID_ERROR, ("rebind: No SubUnit at index %d.\n", count));
				
				return false;
			}
		}
		
		memcpy(&dib_v2, UnitDeviceAtIndex(0)->DIB2(), sizeof(NDAS_DIB_V2));
		memcpy(&rmd, UnitDeviceAtIndex(0)->RMD(), sizeof(NDAS_RAID_META_DATA));
		
		// Rebuild DIB.
		for (count = 0; count < CountOfUnitDevices() + CountOfSpareDevices(); count++) {

			int innerCount;
			
			for (innerCount = 0; innerCount < CountOfUnitDevices() + CountOfSpareDevices(); innerCount++) {
				
				memcpy(&UnitDeviceAtIndex(innerCount)->DIB2()->UnitDisks[count], 
					   UnitDeviceAtIndex(count)->UnitDiskLocation(), 
					   sizeof(UInt64));
				
			}
			
			UnitDeviceAtIndex(count)->DIB2()->iSequence = NDASSwap32HostToLittle(count);
		}
	}
	
	NDAS_uuid_t raidID;
	NDAS_uuid_t	configID;
				
	NDAS_uuid_generate(raidID);
	NDAS_uuid_generate(configID);
	
	// Rewrite DIB and RMD.
	for (count = 0; count < CountOfUnitDevices() + CountOfSpareDevices(); count++) {
		
		if (UnitDeviceAtIndex(count)) {
			if (hasReplacedUnit
				&& count == replacedUnitIndex) {

				memset(&dib_v2, 0, sizeof(NDAS_DIB_V2));
				
				location = UnitDeviceAtIndex(count)->NumberOfSectors() + NDAS_BLOCK_LOCATION_DIB_V2;
				
				DbgIOLog(DEBUG_MASK_RAID_ERROR, ("rebind: DIB location %lld\n", location));
				
				// Rewrite DIB.
				result = UnitDeviceAtIndex(count)->writeSectorsOnce(location, 1,  (char *)&dib_v2);
				if (!result) {
					DbgIOLog(DEBUG_MASK_RAID_ERROR, ("rebind: Can't Write DIB of SubUnit %d.\n", count));
				}
				
				memset(&rmd, 0, sizeof(NDAS_RAID_META_DATA));

				location = UnitDeviceAtIndex(count)->NumberOfSectors() + NDAS_BLOCK_LOCATION_RMD;
				
				DbgIOLog(DEBUG_MASK_RAID_ERROR, ("rebind: RMD location %lld\n", location));
				
				// Rewrite RMD.
				result = UnitDeviceAtIndex(count)->writeSectorsOnce(location, 1,  (char *)&rmd);
				if (!result) {
					DbgIOLog(DEBUG_MASK_RAID_ERROR, ("rebind: Can't Write RMD of SubUnit %d.\n", count));
				}
				
			} else {
				int innerCount;
				PNDAS_DIB_V2	pDIB_v2 = UnitDeviceAtIndex(count)->DIB2();
				PNDAS_RAID_META_DATA pRMD = &rmd;
				
				for(innerCount = CountOfUnitDevices(); innerCount < NDAS_MAX_UNITS_IN_V2; innerCount++) {
					memset(&pDIB_v2->UnitDisks[innerCount], 0, sizeof(pDIB_v2->UnitDisks[innerCount]));
					memset(&pRMD->UnitMetaData[innerCount], 0, sizeof(pRMD->UnitMetaData[innerCount]));
				}
				
				// Update Sequence and CRC.					
				SET_DIB_CRC(crc32_calc, *pDIB_v2);
				
				location = UnitDeviceAtIndex(count)->NumberOfSectors() + NDAS_BLOCK_LOCATION_DIB_V2;
				
				DbgIOLog(DEBUG_MASK_RAID_ERROR, ("rebind: DIB location %lld\n", location));
				
				// Rewrite DIB.
				result = UnitDeviceAtIndex(count)->writeSectorsOnce(location, 1,  (char *)pDIB_v2);
				if (!result) {
					DbgIOLog(DEBUG_MASK_RAID_ERROR, ("rebind: Can't Write DIB of SubUnit %d.\n", count));
				}
				
				// Update RMD.
				memcpy(pRMD, UnitDeviceAtIndex(count)->RMD(), sizeof(NDAS_RAID_META_DATA));
				
				memcpy(&pRMD->RaidSetId, &raidID, sizeof(GUID));
				memcpy(&pRMD->ConfigSetId, &configID, sizeof(GUID));
				pRMD->uiUSN = NDASSwap32HostToLittle(1);	// Initial value.		
				
				SET_RMD_CRC(crc32_calc, *pRMD);
				
				location = UnitDeviceAtIndex(count)->NumberOfSectors() + NDAS_BLOCK_LOCATION_RMD;
				
				DbgIOLog(DEBUG_MASK_RAID_ERROR, ("rebind: RMD location %lld\n", location));
				
				// Rewrite RMD.
				result = UnitDeviceAtIndex(count)->writeSectorsOnce(location, 1,  (char *)pRMD);
				if (!result) {
					DbgIOLog(DEBUG_MASK_RAID_ERROR, ("rebind: Can't Write RMD of SubUnit %d.\n", count));
				}
			}
		}
	}
	
	return result;
}

bool	com_ximeta_driver_NDASRAIDUnitDevice::convertBindType(UInt32 newType)
{
	UInt64					location;
	char					buffer[512] = { 0 };
	PNDAS_DIB_V1			pDIB_v1;
	PNDAS_DIB_V2			pDIB_v2;
	PNDAS_RAID_META_DATA	pRMD;
	bool					result;
	int						FaultDisk = -1;
	int						count;
	
	DbgIOLog(DEBUG_MASK_RAID_ERROR, ("convertBindType: Entered.\n"));

	// Check UnitDevice status.
	if (kNDASUnitConfigurationUnmount != Configuration() ||
		IsInRunningStatus()) {
		
		DbgIOLog(DEBUG_MASK_RAID_ERROR, ("convertBindType: Not Unmounted Status.\n"));
		return false;
	}
	
	switch(newType) {
		case NMT_RAID1R3:
		{
			switch (Type()) {
			case NMT_MIRROR:
			{
				// No fault tolerant existed. Assume disk is not in sync.
				FaultDisk = 1;
			}
				break;
			case NMT_RAID1:
			{
				// Check which node's bitmap is clean. Clean one is defective one.
				// If bitmap is recorded, that disk is correct disk.
				for (count = 0; count < CountOfUnitDevices(); count++) {
					for (int sectorPointer = 0;  sectorPointer < NDAS_BLOCK_SIZE_BITMAP_REV1; sectorPointer++) {
						
						location = UnitDeviceAtIndex(count)->NumberOfSectors() + NDAS_BLOCK_LOCATION_BITMAP + sectorPointer;

						if (UnitDeviceAtIndex(count)->readSectorsOnce(location, 1, buffer)) {

							DbgIOLog(DEBUG_MASK_RAID_INFO, ("convertBindType: RAID1 Read bitmap sequence %d, sectorOffset %d.\n", count, sectorPointer));

							int bytePointer;
							
							for (bytePointer = 0; bytePointer < SECTOR_SIZE; bytePointer++) {
								if(buffer[bytePointer]) {
									
									DbgIOLog(DEBUG_MASK_RAID_INFO, ("convertBindType: RAID1 Dirty bitmap. sequence %d, sectorOffset %d. byte %d, value 0x%x\n", count, sectorPointer, bytePointer, buffer[bytePointer]));

									break;
								}
							}
							
							if (bytePointer != SECTOR_SIZE) {
								// Bitmap is not clean, which means the other disk is fault.
								FaultDisk = (count == 0) ? 1 : 0;
								
								goto RAID1_out;
							}
						} else {
							DbgIOLog(DEBUG_MASK_RAID_INFO, ("convertBindType: RAID1 Can't Read bitmap sequence %d, sectorOffset %lld.\n", count, sectorPointer));						
						}
					}
				}
			}
RAID1_out:
				break;
			case NMT_RAID1R2:
			{
				SInt32	subUnitHasMaxUSN = -1;
				UInt32	highUSN = 0;
				
				for (count = 0; count < CountOfUnitDevices(); count++) {
					if (!UnitDeviceAtIndex(count)->checkRMD()) {
						// Not a valid RMD.
						return false;
					}
					
					for (int subUnitIndex = 0; subUnitIndex < CountOfUnitDevices(); subUnitIndex++) {
						if (NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED & getRMDStatusOfUnit(UnitDeviceAtIndex(count)->RMD(), subUnitIndex)) {
							if (FaultDisk == -1) {
								FaultDisk = subUnitIndex;
							} else if (FaultDisk == subUnitIndex) {
								// Same disk is marked as fault. Okay.
							} else {
								// RAID status is not consistent. Cannot migrate.
								return false;
							}
						}
					}
					
					if (getRMDUSN(UnitDeviceAtIndex(count)->RMD()) >= highUSN) {
						highUSN = getRMDUSN(UnitDeviceAtIndex(count)->RMD());
						subUnitHasMaxUSN = count;
					}
				}	
				
				if (FaultDisk == -1) {
					// Check clean-unmount
					if (NDAS_RAID_META_DATA_STATE_MOUNTED & getRMDStatus(UnitDeviceAtIndex(subUnitHasMaxUSN)->RMD())) {
						// Select any non-spare disk.
						FaultDisk = 1;	// Select Second disk.
					}
				}
			}
				break;
			default:
				return false;
			}
		}
			break;
		default:
			return false;
	}

	// Create GUID for RMD.
	NDAS_uuid_t raidID;
	NDAS_uuid_t	configID;
				
	NDAS_uuid_generate(raidID);
	NDAS_uuid_generate(configID);
				
	for (count = 0; count < CountOfUnitDevices(); count++) {
		if (UnitDeviceAtIndex(count)) {
						
			pDIB_v2 = (PNDAS_DIB_V2)&buffer[0];
			
			memcpy(pDIB_v2, UnitDeviceAtIndex(count)->DIB2(), sizeof(NDAS_DIB_V2));
			
			pDIB_v2->iMediaType = NDASSwap32HostToLittle(newType);		
			pDIB_v2->iSectorsPerBit = NDASSwap32HostToLittle(NDAS_BITMAP_SECTOR_PER_BIT_DEFAULT);
						
			SET_DIB_CRC(crc32_calc ,*pDIB_v2);
			
			location = UnitDeviceAtIndex(count)->NumberOfSectors() + NDAS_BLOCK_LOCATION_DIB_V2;

			DbgIOLog(DEBUG_MASK_RAID_ERROR, ("convertBindType: location of DIB_v2 %lld\n", location));

			result = UnitDeviceAtIndex(count)->writeSectorsOnce(location, 1, (char *)pDIB_v2);
			if(!result) {
				DbgIOLog(DEBUG_MASK_RAID_ERROR, ("convertBindType: Can't write new DIB to location %lld\n", location));		
				
				break;
			}

			// Write RMD.
			pRMD = (PNDAS_RAID_META_DATA)&buffer[0];
			
			memcpy(pRMD, UnitDeviceAtIndex(count)->RMD(), sizeof(NDAS_RAID_META_DATA));
			
			if (NMT_MIRROR == Type()
				|| NMT_RAID1 == Type()
				|| NMT_RAID1R2 == Type()) {
				// There wasn't RMD.				
				memcpy(&pRMD->RaidSetId, &raidID, sizeof(GUID));
				memcpy(&pRMD->ConfigSetId, &configID, sizeof(GUID));
				pRMD->uiUSN = NDASSwap32HostToLittle(1);	// Initial value.
							
				if (FaultDisk != -1) {
					pRMD->UnitMetaData[FaultDisk].UnitDeviceStatus |= NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED; 
				}
				
				pRMD->state = NDASSwap32HostToLittle(NDAS_RAID_META_DATA_STATE_UNMOUNTED);
			}
			
			SET_RMD_CRC(crc32_calc, *pRMD);

			location = UnitDeviceAtIndex(count)->NumberOfSectors() + NDAS_BLOCK_LOCATION_RMD;
			
			DbgIOLog(DEBUG_MASK_RAID_ERROR, ("convertBindType: location of RMD %lld\n", location));
			
			result = UnitDeviceAtIndex(count)->writeSectorsOnce(location, 1, (char *)pRMD);
			if(!result) {
				DbgIOLog(DEBUG_MASK_RAID_ERROR, ("convertBindType: Can't write new RMD to location %lld\n", location));		
				
				break;
			}
			
			// Invaildate DIB V1.
			pDIB_v1 = (PNDAS_DIB_V1)&buffer[0];
			
			memset(pDIB_v1, 0, sizeof(NDAS_DIB_V1));
			
			NDAS_DIB_V1_INVALIDATE(*pDIB_v1);
			
			location = UnitDeviceAtIndex(count)->NumberOfSectors() + NDAS_BLOCK_LOCATION_DIB_V1;
			
			DbgIOLog(DEBUG_MASK_RAID_ERROR, ("convertBindType: location of DIB_v1 %lld\n", location));
			
			if(!UnitDeviceAtIndex(count)->writeSectorsOnce(location, 1, (char *)pDIB_v1)) {
				DbgIOLog(DEBUG_MASK_RAID_ERROR, ("convertBindType: Can't delete old DIB v1 to location %lld\n", location));		
			}
		}
	}
	
	if (-1 != FaultDisk) {
		PNDAS_OOS_BITMAP_BLOCK pBitmap = (PNDAS_OOS_BITMAP_BLOCK)&buffer[0];
		
		// I'm laze to convert old version's bitmap to current version's bitmap
		// Just mark all bit dirty.
		for (count = 0; count < CountOfUnitDevices(); count++) {
			pBitmap->SequenceNumHead = 0;
			pBitmap->SequenceNumTail = 0;
			memset(pBitmap->Bits, 0xff, sizeof(pBitmap->Bits));

			location = UnitDeviceAtIndex(count)->NumberOfSectors() + NDAS_BLOCK_LOCATION_BITMAP;

			for (int sectorPointer = 0; sectorPointer < NDAS_BLOCK_SIZE_BITMAP; sectorPointer++) {

				if (!UnitDeviceAtIndex(count)->writeSectorsOnce(location + sectorPointer, 1, (char *)pBitmap)) {
					return false;
				}
			}
		}
	}
	
	return true;
}

void com_ximeta_driver_NDASRAIDUnitDevice::setName(char *Name)
{
	memcpy(&fName[0], Name, MAX_NDAS_NAME_SIZE);
	
	//setProperty(NDASDEVICE_PROPERTY_KEY_NAME, fName);
	
	setProperty(NDASDEVICE_PROPERTY_KEY_NAME, OSData::withBytesNoCopy(fName, MAX_NDAS_NAME_SIZE));
}

void	com_ximeta_driver_NDASRAIDUnitDevice::setUnitDevices(PNDAS_DIB_V2 dib2, PNDAS_RAID_META_DATA rmd)
{
	UInt32	count;
	
	for (count = 0; count < (getDIBCountOfUnitDisks(dib2) + getDIBCountOfSpareDevices(dib2)); count++) {
	
		char	buffer[256];
		UInt64	temp;
		SInt32	index;
		
		sprintf(buffer, NDASDEVICE_PROPERTY_KEY_RAID_UNIT_DISK_LOCATION_OF_SUB_UNIT, count);
				
		if (NMT_RAID1R3 ==  NDASSwap32LittleToHost(dib2->iMediaType)) {
			if (-1 == (index = getRMDIndexOfUnit(rmd, count))) {
				index = count;
			}
		} else {
			index = count;
		}
		
		memcpy(&temp, getDIBUnitDiskLocation(dib2, index), sizeof(UInt64));

		setProperty(buffer, temp, 64);		
		
		setStatusOfSubUnit(index, kNDASRAIDSubUnitStatusNotPresent);

		// Set SubUnit Type.
		UInt32 SubUnitType = kNDASRAIDSubUnitTypeNormal; 
		
		if (NMT_RAID1R3 ==  NDASSwap32LittleToHost(dib2->iMediaType)) {

			if (NDAS_UNIT_META_BIND_STATUS_SPARE & getRMDStatusOfUnit(rmd, index)) {
				SubUnitType = kNDASRAIDSubUnitTypeSpare;
			} 
			if (NDAS_UNIT_META_BIND_STATUS_REPLACED_BY_SPARE & getRMDStatusOfUnit(rmd, index)) {
				SubUnitType = kNDASRAIDSubUnitTypeReplacedBySpare;
			}
		}
		
		setTypeOfSubUnit(index, SubUnitType);
	}
}

void	com_ximeta_driver_NDASRAIDUnitDevice::setStatusOfSubUnit(UInt32 index, UInt32 status)
{
	char	buffer[256];

	if (index < CountOfUnitDevices() + CountOfSpareDevices()) {
		
		fSubUnitStatus[index] = status;
		
		sprintf(buffer, NDASDEVICE_PROPERTY_KEY_RAID_STATUS_OF_SUB_UNIT, index);
		
		setProperty(buffer, fSubUnitStatus[index], 32);	
	}	
}

UInt32	com_ximeta_driver_NDASRAIDUnitDevice::StatusOfSubUnit(UInt32 index) 
{
	if (index < CountOfUnitDevices() + CountOfSpareDevices()) {
		return fSubUnitStatus[index];	
	}
	
	return kNDASRAIDSubUnitStatusNotPresent;
}

void	com_ximeta_driver_NDASRAIDUnitDevice::setTypeOfSubUnit(UInt32 index, UInt32 type)
{
	char	buffer[256];
	
	if (index < CountOfUnitDevices() + CountOfSpareDevices()) {
		
		fSubUnitType[index] = type;
		
		sprintf(buffer, NDASDEVICE_PROPERTY_KEY_RAID_TYPE_OF_SUB_UNIT, index);
		
		setProperty(buffer, fSubUnitType[index], 32);	
	}		
}

SInt32	com_ximeta_driver_NDASRAIDUnitDevice::TypeOfSubUnit(UInt32 index)
{
	if (index < CountOfUnitDevices() + CountOfSpareDevices()) {
		return fSubUnitType[index];	
	}
	
	return -1;
}

void	com_ximeta_driver_NDASRAIDUnitDevice::setRAIDStatus(UInt32 status)
{
	setProperty(NDASDEVICE_PROPERTY_KEY_RAID_STATUS, status, 32);
	
	fRAIDStatus = status;
}

UInt32	com_ximeta_driver_NDASRAIDUnitDevice::RAIDStatus()
{
	return fRAIDStatus;
}

UInt32	com_ximeta_driver_NDASRAIDUnitDevice::RAIDBlocksPerSector()
{
	return  blocksize() / SECTOR_SIZE;
}

bool	com_ximeta_driver_NDASRAIDUnitDevice::IsDirtyBitSet(UInt64 sector, bool *dirty)
{
	DbgIOLog(DEBUG_MASK_RAID_TRACE, ("IsDirtyBitSet: Entered. sector %lld\n", sector));
	
	// Check UnitDevice status.
	if (kNDASUnitStatusConnectedRW != Status()) {		
		DbgIOLog(DEBUG_MASK_RAID_ERROR, ("IsDirtyBitSet: Not Mount RW Status.\n"));
		return false;
	}
	
	// Check Disk Array Type.
	if (NMT_RAID1R3 != Type() ) {
		DbgIOLog(DEBUG_MASK_RAID_ERROR, ("IsDirtyBitSet: No RAID1R.\n"));
		return false;
	}
	
	// Check parameter Sector.
	if (sector >= Capacity()) {
		DbgIOLog(DEBUG_MASK_RAID_ERROR, ("IsDirtyBitSet: Exceeded Sector %lld\n", sector));
		return false;
	}

	com_ximeta_driver_NDASUnitDevice *GoodSubUnit;
	
	if ( kNDASRAIDSubUnitStatusGood == StatusOfSubUnit(0) ) {
		GoodSubUnit = UnitDeviceAtIndex(0);
	} else if ( kNDASRAIDSubUnitStatusGood == StatusOfSubUnit(1) ) {
		GoodSubUnit = UnitDeviceAtIndex(1);
	} else {
		DbgIOLog(DEBUG_MASK_RAID_ERROR, ("IsDirtyBitSet: No Good Sub Unit.\n"));
		return false;		
	}
	
	UInt32	sectorsPerBit = getDIBSectorsPerBit(GoodSubUnit->DIB2());
	UInt8	BitMapBitPoint = (sector / sectorsPerBit) % 8;
	UInt32	BitMapSectorPoint = (sector / sectorsPerBit) / NDAS_BIT_PER_OOS_BITMAP_BLOCK; 
	UInt32	BitMapBytePoint = (sector / sectorsPerBit / 8) % NDAS_BYTE_PER_OOS_BITMAP_BLOCK; 

	DbgIOLog(DEBUG_MASK_RAID_INFO, ("IsDirtyBitSet: Pointer sec %d, Byte %d, Bit %d\n", BitMapSectorPoint, BitMapBytePoint, BitMapBitPoint));

	PNDAS_OOS_BITMAP_BLOCK	pBitmap;

	if (fDirtyBitmap != NULL) {

		pBitmap = (PNDAS_OOS_BITMAP_BLOCK)&(fDirtyBitmap[BitMapSectorPoint * sizeof(NDAS_OOS_BITMAP_BLOCK)]);
		
	} else {
		
		// Read BitMap.
		NDAS_OOS_BITMAP_BLOCK	tempBitmap;
		
		if(GoodSubUnit->ReadBitMapFromUnitDevice(&tempBitmap, BitMapSectorPoint)) {
			
			pBitmap = &tempBitmap;

		} else {
			// BUG BUG BUG!!!
			DbgIOLog(DEBUG_MASK_RAID_ERROR, ("IsDirtyBitSet: No Bitmap and Can't read bitmap from Good Sub Unit.\n"));

			return false;
		}
	}

	if (pBitmap->Bits[BitMapBytePoint] & (1 << BitMapBitPoint)) {
		*dirty = true;
	} else {
		*dirty = false;
	}
		
	return true;
	
}

bool com_ximeta_driver_NDASRAIDUnitDevice::SetBitmap(UInt64 location, UInt32 sectors, bool dirty)
{
	if (sectors == 0) {
		return false;
	}
	
	return UpdateBitMapBit(location, sectors, true);
}

bool com_ximeta_driver_NDASRAIDUnitDevice::ClearBitmapFromFirstSector(UInt64 location) 
{
	return UpdateBitMapBit(0, location + 1, false);
}


bool	com_ximeta_driver_NDASRAIDUnitDevice::UpdateBitMapBit(UInt64 location, UInt32 sectors, bool dirty)
{
	DbgIOLog(DEBUG_MASK_RAID_TRACE, ("UpdateBitMapBit: Entered.\n"));

	if (dirty) {
		setDirty(true);
	}
	
	// Check UnitDevice status.
	if (kNDASUnitStatusConnectedRW != Status()) {		
		DbgIOLog(DEBUG_MASK_RAID_ERROR, ("UpdateBitMapBit: Not Mount RW Status.\n"));
		return false;
	}
	
	// Check Disk Array Type.
	if (NMT_RAID1R3 != Type() ) {
		DbgIOLog(DEBUG_MASK_RAID_ERROR, ("UpdateBitMapBit: No RAID1R.\n"));
		return false;
	}
	
	// Check parameter Sector.
	if ((location + sectors - 1) > Capacity()) {
		DbgIOLog(DEBUG_MASK_RAID_ERROR, ("UpdateBitMapBit: Exceeded Sector %lld\n", location));
		return false;
	}
		
	// Read BitMap.
	UInt32	sectorsPerBit;
	UInt64	nomalizedLocation;

	UInt64	dirtyBitSector;
	
	com_ximeta_driver_NDASUnitDevice *GoodSubUnit;
				
	if ( kNDASRAIDSubUnitStatusGood == StatusOfSubUnit(0) ) {
		GoodSubUnit = UnitDeviceAtIndex(0);
	} else if ( kNDASRAIDSubUnitStatusGood == StatusOfSubUnit(1) ) {
		GoodSubUnit = UnitDeviceAtIndex(1);
	} else {
		DbgIOLog(DEBUG_MASK_RAID_ERROR, ("UpdateBitMapBit: No Good Sub Unit.\n"));
		return false;		
	}
				
	sectorsPerBit = getDIBSectorsPerBit(GoodSubUnit->DIB2());

	nomalizedLocation = (location / sectorsPerBit) * sectorsPerBit;
	
	while (nomalizedLocation <= (location + sectors - 1)) {
	
		if (false == dirty && ((nomalizedLocation + sectorsPerBit) > (location + sectors - 1))) {
			break;
		}
		
		UInt8	BitMapBitPoint = (nomalizedLocation / sectorsPerBit) % 8;
		UInt32	BitMapSectorPoint = (nomalizedLocation / sectorsPerBit) / NDAS_BIT_PER_OOS_BITMAP_BLOCK; 
		UInt32	BitMapBytePoint = (nomalizedLocation / sectorsPerBit / 8) % NDAS_BYTE_PER_OOS_BITMAP_BLOCK; 
		bool	bPrevious;
		
		dirtyBitSector = GoodSubUnit->NumberOfSectors() + NDAS_BLOCK_LOCATION_BITMAP + BitMapSectorPoint;

		DbgIOLog(DEBUG_MASK_RAID_INFO, ("UpdateBitMapBit: Pointer sec %d, Byte %d, Bit %d\n", BitMapSectorPoint, BitMapBytePoint, BitMapBitPoint));
		
		PNDAS_OOS_BITMAP_BLOCK	pBitmap;

		if (fDirtyBitmap) {
			pBitmap = (PNDAS_OOS_BITMAP_BLOCK)&fDirtyBitmap[BitMapSectorPoint * sizeof(NDAS_OOS_BITMAP_BLOCK)];
		} else {
			// Read BitMap.
			NDAS_OOS_BITMAP_BLOCK	tempBitmap;
			
			if(GoodSubUnit->ReadBitMapFromUnitDevice(&tempBitmap, BitMapSectorPoint)) {
				
				pBitmap = &tempBitmap;
				
			} else {
				// BUG BUG BUG!!!
				DbgIOLog(DEBUG_MASK_RAID_ERROR, ("UpdateBitMapBit: No bitmap and Can't read bitmap from goodsubUnit.\n"));

				return false;
			}
		}
		
		bPrevious = pBitmap->Bits[BitMapBytePoint] & ( 1 << BitMapBitPoint);
		
		if (bPrevious != dirty) {
			if (dirty) {
				pBitmap->Bits[BitMapBytePoint] |= ( 1 << BitMapBitPoint);
			} else {
				pBitmap->Bits[BitMapBytePoint] &= ~( 1 << BitMapBitPoint);
			}				
			
			// Update Bitmap sector.
			if(!GoodSubUnit->WriteBitMapToUnitDevice(pBitmap, BitMapSectorPoint)) {
				DbgIOLog(DEBUG_MASK_RAID_ERROR, ("UpdateBitMapBit: Write BitMap failed.\n"));
				return false;		
			}
			
		} else {
			// No touch.
			DbgIOLog(DEBUG_MASK_RAID_INFO, ("UpdateBitMapBit: No need Write BitMap.\n"));
		}
		
		nomalizedLocation += sectorsPerBit;		
	}
	
	return true;
}

void	com_ximeta_driver_NDASRAIDUnitDevice::recovery()
{
	DbgIOLog(DEBUG_MASK_RAID_TRACE, ("recovery: Entered.\n"));

	com_ximeta_driver_NDASUnitDevice *GoodSubUnit = NULL;
	com_ximeta_driver_NDASUnitDevice *BadSubUnit = NULL;

	// Check Mounting status.
	if (kNDASUnitStatusConnectedRW != Status()
		&& kNDASUnitStatusReconnectRW != Status() ) {
		DbgIOLog(DEBUG_MASK_RAID_ERROR, ("recovery: Not Mounted RW.\n"));
		
		fInRecoveryProcess = false;

		goto out;
	}
	
	updateRAIDStatus();

	if (fInRecoveryProcess
		&& (getRMDUSN(UnitDeviceAtIndex(0)->RMD()) != getRMDUSN(UnitDeviceAtIndex(1)->RMD()))) {
		
		DbgIOLog(DEBUG_MASK_RAID_ERROR, ("recovery: RMD is update!!!\n"));

		// Re Init.
		fInRecoveryProcess = false;
	}
	
	// Initilaizing.
	if (!fInRecoveryProcess) {
		
		fSectorOfRecovery = 0;
		
		// Export Recovering Sector for monitoring.
		setProperty(NDASDEVICE_PROPERTY_KEY_RAID_RECOVERING_SECTOR, fSectorOfRecovery, 64);
	
		// Find Good and Bad Sub Unit.
		if ( kNDASRAIDSubUnitStatusGood == StatusOfSubUnit(0) 
			 && kNDASRAIDSubUnitStatusNeedRecovery == StatusOfSubUnit(1) ) {
			GoodSubUnit = UnitDeviceAtIndex(0);
			BadSubUnit = UnitDeviceAtIndex(1);
			
			DbgIOLog(DEBUG_MASK_RAID_ERROR, ("recovery: Sub Unit 1 now recovering .\n"));
			
		} else if ( kNDASRAIDSubUnitStatusGood == StatusOfSubUnit(1) 
					&& kNDASRAIDSubUnitStatusNeedRecovery == StatusOfSubUnit(0)) {
			BadSubUnit = UnitDeviceAtIndex(0);
			GoodSubUnit = UnitDeviceAtIndex(1);
			
			DbgIOLog(DEBUG_MASK_RAID_ERROR, ("recovery: Sub Unit 0 now recovering .\n"));
			
		} else {
			DbgIOLog(DEBUG_MASK_RAID_ERROR, ("recovery: No Good Sub Unit.\n"));
			
			return;
		}
		
		if (kNDASUnitStatusConnectedRW != BadSubUnit->Status()) {
			DbgIOLog(DEBUG_MASK_RAID_ERROR, ("recovery: Status of Bad subunit is not MOUNT-RW.\n"));

			return;
		}
		
		// Create Dirty Bitmap.
		if (NULL == fDirtyBitmap) {
			
			if (false == allocDirtyBitmap()) {
				// BUG BUG BUG. -- No Mem.
				DbgIOLog(DEBUG_MASK_RAID_ERROR, ("recovery: No memory for Dirty Bitmap.\n"));			
			} else {
				// Read Bitmap.
				
				if (GoodSubUnit != NULL) {

					UInt64	dirtyBitSector = GoodSubUnit->NumberOfSectors() + NDAS_BLOCK_LOCATION_BITMAP;
					
					if(!GoodSubUnit->readSectorsOnce(dirtyBitSector, NDAS_BLOCK_SIZE_BITMAP, fDirtyBitmap)) {
						DbgIOLog(DEBUG_MASK_RAID_INFO, ("recovery: Can't read BitMap %lld\n", dirtyBitSector));
					}
				} else {
					IOFree(fDirtyBitmap, NDAS_BLOCK_SIZE_BITMAP * sizeof(NDAS_OOS_BITMAP_BLOCK));
					fDirtyBitmap = NULL;
				}
			}
		}
		
		// Update RMD.
		NDAS_RAID_META_DATA		tempRMD;
		uint32_t				tempUSN;
		
		memcpy(&tempRMD, BadSubUnit->RMD(), sizeof(NDAS_RAID_META_DATA));
		tempUSN = getRMDUSN(GoodSubUnit->RMD());
		
		tempRMD.uiUSN = NDASSwap32HostToLittle(tempUSN);
		
		// Update UMD.
		memcpy(tempRMD.UnitMetaData, GoodSubUnit->RMD()->UnitMetaData, sizeof(NDAS_UNIT_META_DATA) * NDAS_MAX_UNITS_IN_V2);
		
		SET_RMD_CRC(crc32_calc, tempRMD);

		// Update RMD.
		UInt64 location = BadSubUnit->NumberOfSectors() + NDAS_BLOCK_LOCATION_RMD;
						
		if(!BadSubUnit->writeSectorsOnce(location, 1,  (char *)&tempRMD)) {
			DbgIOLog(DEBUG_MASK_RAID_ERROR, ("recovery: Write RMD failedt to Bad SubUnit.\n"));
			
			goto out;
		}

		// Check RMD.
		BadSubUnit->checkRMD();
		
		updateRAIDStatus();

		fInRecoveryProcess = true;
	} 
			
	if (kNDASRAIDStatusRecovering != RAIDStatus() ) {
		 DbgIOLog(DEBUG_MASK_RAID_ERROR, ("recovery: Not recovering status.\n"));
		 
		 goto out;
	 }
		
	if (fSectorOfRecovery >= Capacity()) {
		DbgIOLog(DEBUG_MASK_RAID_ERROR, ("recovery: Exceed sectors.\n"));

		return;
	}
	
	switch(Type()) {
		case NMT_RAID1R3:
		{			
			// Read Bit Map including fSectorOfRecovery.
			
			// If Dirty write.
			bool dirty;
			int								GoodSubUnitIndex;
			int								BadSubUnitIndex;

			// Recovery
			
			// Search Good Sub Unit.
			if ( kNDASRAIDSubUnitStatusGood == StatusOfSubUnit(0) ) {
				GoodSubUnit = UnitDeviceAtIndex(0);
				GoodSubUnitIndex = 0;
				BadSubUnitIndex = 1;
				BadSubUnit = UnitDeviceAtIndex(1);
			} else if ( kNDASRAIDSubUnitStatusGood == StatusOfSubUnit(1) ) {
				GoodSubUnit = UnitDeviceAtIndex(1);
				GoodSubUnitIndex = 1;
				BadSubUnitIndex = 0;
				BadSubUnit = UnitDeviceAtIndex(0);
			} else {
				DbgIOLog(DEBUG_MASK_RAID_ERROR, ("recovery: No Good Sub Unit.\n"));
				
				return;		
			}
						
			UInt32 sectorsPerBit = getDIBSectorsPerBit(GoodSubUnit->DIB2());
			UInt32 maxRequestBlocks = 128;
			
			// Check Buffer For Recovery.
			if (NULL == fBufferForRecovery) {
				// Create Buffer.
				fSizeofBufferForRecovery =  maxRequestBlocks * SECTOR_SIZE;
				fBufferForRecovery = (char *)IOMalloc(fSizeofBufferForRecovery);
				if (NULL == fBufferForRecovery) {
					// BUG BUG BUG. -- No Mem.
					DbgIOLog(DEBUG_MASK_RAID_ERROR, ("recovery: No memory.\n"));

					return;
				}
				
			}
			
			if ( IsDirtyBitSet(fSectorOfRecovery, &dirty) && dirty) {
				
				DbgIOLog(DEBUG_MASK_RAID_INFO, ("recovery: DirtyBit set. Sector %lld, sectors %d\n", fSectorOfRecovery, sectorsPerBit));
				
				UInt32	sectorsForRecovery = min(maxRequestBlocks, Capacity() - fSectorOfRecovery);
				
				// Read From Good Disk.
				if (!GoodSubUnit->readSectorsOnce(fSectorOfRecovery, sectorsForRecovery, fBufferForRecovery)) {
					DbgIOLog(DEBUG_MASK_RAID_ERROR, ("recovery: Read Data failed.\n"));
					
					goto out;	
				}
				
				// Write to Bad Disk.
				if (!BadSubUnit->writeSectorsOnce(fSectorOfRecovery, sectorsForRecovery, fBufferForRecovery)) {
					
					UInt64	rollbackLocation;
					
					DbgIOLog(DEBUG_MASK_RAID_ERROR, ("recovery: Write Data failed.\n"));
					
					if (fSectorOfRecovery <= DISK_CACHE_SIZE_BY_BLOCK_SIZE) {
						rollbackLocation = 0;
					} else {
						rollbackLocation = fSectorOfRecovery - DISK_CACHE_SIZE_BY_BLOCK_SIZE;
					}
					
					// Set Write history.
					SetBitmap(rollbackLocation, DISK_CACHE_SIZE_BY_BLOCK_SIZE, true);

					goto out;	
				}				
				
				fSectorOfRecovery += sectorsForRecovery;
				
				// Reset dirty bit.
				ClearBitmapFromFirstSector(fSectorOfRecovery);				
				
			} else {
				fSectorOfRecovery += sectorsPerBit;
			}
			
			// Export Recovering Sector for monitoring.
			setProperty(NDASDEVICE_PROPERTY_KEY_RAID_RECOVERING_SECTOR, fSectorOfRecovery, 64);
			
			// If Reach the end? 
			if (fSectorOfRecovery >= Capacity()) {
				// Recovery is done.
				
				DbgIOLog(DEBUG_MASK_RAID_WARNING, ("recovery: Recovery Is Done.\n"));

				// Update RMD of GoodSubUnit.
				NDAS_RAID_META_DATA		tempRMD;
				uint32_t				tempUSN;
				uint32_t				tempStatus;
				UInt64					rmdLocation;
				
				memcpy(&tempRMD, GoodSubUnit->RMD(), sizeof(NDAS_RAID_META_DATA));
				tempUSN = getRMDUSN(GoodSubUnit->RMD()) + 1;
				tempRMD.uiUSN = NDASSwap32HostToLittle(tempUSN);
				
				tempStatus = getRMDStatus(GoodSubUnit->RMD()) & ~NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED;
				tempRMD.state = NDASSwap32HostToLittle(tempStatus);
								
				// Update UMD.
				tempRMD.UnitMetaData[BadSubUnitIndex].UnitDeviceStatus &= ~NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED;

				SET_RMD_CRC(crc32_calc, tempRMD);

				// Update RMD.
				rmdLocation = GoodSubUnit->NumberOfSectors() + NDAS_BLOCK_LOCATION_RMD;
				
				if(!GoodSubUnit->writeSectorsOnce(rmdLocation, 1,  (char *)&tempRMD)) {
					DbgIOLog(DEBUG_MASK_RAID_ERROR, ("recovery: Write RMD failedt to Bad SubUnit.\n"));
					
					goto out;
				} 
	
				// Update RMD of BadSubUnit.				
				memcpy(&tempRMD, BadSubUnit->RMD(), sizeof(NDAS_RAID_META_DATA));
				tempUSN = getRMDUSN(GoodSubUnit->RMD());
				tempRMD.uiUSN = NDASSwap32HostToLittle(tempUSN);
				
				tempStatus = getRMDStatus(GoodSubUnit->RMD()) & ~NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED;
				tempRMD.state = NDASSwap32HostToLittle(tempStatus);
								
				// Update UMD.
				tempRMD.UnitMetaData[BadSubUnitIndex].UnitDeviceStatus &= ~NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED;
				
				SET_RMD_CRC(crc32_calc, tempRMD);

				// Update RMD.
				rmdLocation = BadSubUnit->NumberOfSectors() + NDAS_BLOCK_LOCATION_RMD;

				if(!BadSubUnit->writeSectorsOnce(rmdLocation, 1,  (char *)&tempRMD)) {
					DbgIOLog(DEBUG_MASK_RAID_ERROR, ("recovery: Write RMD failedt to Bad SubUnit.\n"));
					
					goto out;
				}

				// Update RMD.
				GoodSubUnit->checkRMD();
				BadSubUnit->checkRMD();
				
				// Change Status.
				fInRecoveryProcess = false;

				// Free Buffer for Recovery.
				IOFree(fBufferForRecovery, fSizeofBufferForRecovery);
				fBufferForRecovery = NULL;
				
				// Free Bitmap.
				IOFree(fDirtyBitmap, NDAS_BLOCK_SIZE_BITMAP * sizeof(NDAS_OOS_BITMAP_BLOCK));
				fDirtyBitmap = NULL;				
				
				setDirty(false);
			}
		}
			break;
		default:
			break;
	}
			
out:
	// Check RAID Status.
	updateRAIDStatus();
	
	return;
}

bool	com_ximeta_driver_NDASRAIDUnitDevice::allocDirtyBitmap() {
	
	if (NMT_RAID1R3 != Type() ) {
		return false;
	}
	
	if (NULL == fDirtyBitmap) {
		
		fDirtyBitmap = (char *)IOMalloc(NDAS_BLOCK_SIZE_BITMAP * sizeof(NDAS_OOS_BITMAP_BLOCK));
		if (NULL == fDirtyBitmap) {
			
			// BUG BUG BUG. -- No Mem.
			DbgIOLog(DEBUG_MASK_RAID_ERROR, ("allocDirtyBitmap: No memory for Dirty Bitmap.\n"));			
			
			return false;
		} else {
			memset(fDirtyBitmap, 0, NDAS_BLOCK_SIZE_BITMAP * sizeof(NDAS_OOS_BITMAP_BLOCK));
			
			return true;
		}
	} else {
		return true;
	}
}

char*	com_ximeta_driver_NDASRAIDUnitDevice::dirtyBitmap()
{
	return fDirtyBitmap;
}

void	com_ximeta_driver_NDASRAIDUnitDevice::setDirty(bool newValue)
{
	fDirty = newValue;
}

bool	com_ximeta_driver_NDASRAIDUnitDevice::IsDirty()
{
	return fDirty;
}

UInt32	com_ximeta_driver_NDASRAIDUnitDevice::blocksize()
{
	UInt32	size;
	
	switch(Type()) {
		case NMT_RAID0:
		{
			size = SECTOR_SIZE * CountOfUnitDevices();
		}
			break;
		default:
			size = SECTOR_SIZE;
	}
	
	return size;
}

void  com_ximeta_driver_NDASRAIDUnitDevice::updateRAIDStatus()
{
	UInt32	prevRAIDStatus = 0;
	UInt32	newRAIDStatus = 0;
	int		count;
	bool	bMajorProblem = false;
	
	IOLockLock(lock);
	
	newRAIDStatus = prevRAIDStatus = RAIDStatus();
	
	// Update Status of Sub Units.
	for (count = 0; count < fCountOfUnitDevices + fCountOfSpareDevices; count++) {
		
		UInt32	newUnitStatus = StatusOfSubUnit(count);
		
		if(!fUnitDeviceArray[count]) {
			newUnitStatus = kNDASRAIDSubUnitStatusNotPresent;
		} else {
		
			if (fUnitDeviceArray[count]->RAIDFlag(UNIT_DEVICE_RAID_FLAG_NDAS_ADDR_MISMATCH)) {
				newUnitStatus = kNDASRAIDSubUnitStatusNDASAddressMismatch;
			} else if (fUnitDeviceArray[count]->RAIDFlag(UNIT_DEVICE_RAID_FLAG_BAD_RMD)) {
				newUnitStatus = kNDASRAIDSubUnitStatusBadRMD;
			} else {
				switch(fUnitDeviceArray[count]->Status()) {
					case kNDASUnitStatusNotPresent:
					{
						newUnitStatus = kNDASRAIDSubUnitStatusNotPresent;
					}
						break;
					case kNDASUnitStatusDisconnected:
					case kNDASUnitStatusTryConnectRO:
					case kNDASUnitStatusTryConnectRW:
					case kNDASUnitStatusConnectedRO:
					case kNDASUnitStatusConnectedRW:
					case kNDASUnitStatusTryDisconnectRO:
					case kNDASUnitStatusTryDisconnectRW:
					{
						newUnitStatus = kNDASRAIDSubUnitStatusGood;
					}
						break;
					case kNDASUnitStatusReconnectRO:
					case kNDASUnitStatusReconnectRW:
					{
						if (NMT_RAID1R3 == Type()) {
							newUnitStatus = kNDASRAIDSubUnitStatusFailure;
						} else {
							newUnitStatus = kNDASRAIDSubUnitStatusGood;
						}
					}
						break;					
					case kNDASUnitStatusFailure:
					default:
					{
						newUnitStatus = kNDASRAIDSubUnitStatusFailure;
					}
						break;
				}
			}
		}
		
		setStatusOfSubUnit(count, newUnitStatus);
	}
	
	for (int subUnitIndex = 0; subUnitIndex < CountOfUnitDevices(); subUnitIndex++) {
		
		if (StatusOfSubUnit(subUnitIndex) == kNDASRAIDSubUnitStatusNDASAddressMismatch
			|| StatusOfSubUnit(subUnitIndex) == kNDASRAIDSubUnitStatusBadRMD) {
			
			newRAIDStatus = kNDASRAIDStatusBadRAIDInformation;
			
			bMajorProblem = true;
			
			break;
		}
	}
	
	if (!IsWorkable()) {
		newRAIDStatus = kNDASRAIDStatusNotReady;
		goto Out;
	}
	
	if (!bMajorProblem) {
		//
		// Update RAID Status.
		//
		switch (Type()) {
			case NMT_AGGREGATE:
			case NMT_RAID0R2:
			{
				switch(prevRAIDStatus) {
					case kNDASRAIDStatusGood:
					{
						for (int subUnitIndex = 0; subUnitIndex < CountOfUnitDevices(); subUnitIndex++ ) {
							
							switch(StatusOfSubUnit(subUnitIndex)) {
								case kNDASRAIDSubUnitStatusGood:
								{
									// Okay.
								}
									break;
								default:
								{	
									DbgIOLog(DEBUG_MASK_RAID_ERROR, ("updateRAIDStatus: Broken status.\n"));
									newRAIDStatus = kNDASRAIDStatusBroken;
								}
									break;
							}
						}
					}
						break;
					case kNDASRAIDStatusBroken:
					case kNDASRAIDStatusNotReady:
					default:
					{
						bool	bGood = true;
						
						for (int subUnitIndex = 0; subUnitIndex < CountOfUnitDevices(); subUnitIndex++ ) {
							switch(StatusOfSubUnit(subUnitIndex)) {
								case kNDASRAIDSubUnitStatusGood:
								{
									// Okay.
								}
									break;
								default:
								{
									bGood = false;
								}
									break;
							}
						}
						
						if (bGood) {
							DbgIOLog(DEBUG_MASK_RAID_ERROR, ("updateRAIDStatus: Good status.\n"));
							
							newRAIDStatus = kNDASRAIDStatusGood;
						}					
					}
						break;
				}
			}
				break;
			case NMT_RAID1R3:
			{
				//
				// Update Status of Sub Units.
				//
				switch(prevRAIDStatus) {
					case kNDASRAIDStatusNotReady:
					case kNDASRAIDStatusBroken:
					case kNDASRAIDStatusReadyToRecovery:
					case kNDASRAIDStatusReadyToRecoveryWithReplacedUnit:
					case kNDASRAIDStatusRecovering:
					case kNDASRAIDStatusGoodButPartiallyFailed:
					case kNDASRAIDStatusBrokenButWorkable:
					case kNDASRAIDStatusGood:
					case kNDASRAIDStatusGoodWithReplacedUnit:
					{
						UInt32	firstUSN, secondUSN;
						
						if (kNDASRAIDSubUnitStatusGood == StatusOfSubUnit(0) 
							&& kNDASRAIDSubUnitStatusGood == StatusOfSubUnit(1)) {
							
							// Update RMD Info.
							int		loop;
							
							firstUSN = getRMDUSN(UnitDeviceAtIndex(0)->RMD());
							secondUSN = getRMDUSN(fUnitDeviceArray[1]->RMD()); 
							
							// Prevent too many IO's
							if (kNDASRAIDStatusRecovering != prevRAIDStatus
								&& firstUSN != secondUSN) {
								for (loop = 0; loop < 2; loop++) {
									if(!fUnitDeviceArray[loop]->checkRMD()) {
										setStatusOfSubUnit(loop, kNDASRAIDSubUnitStatusFailure);
									}
								}
							}
						}
						
						if (kNDASRAIDSubUnitStatusGood == StatusOfSubUnit(0) 
							&& kNDASRAIDSubUnitStatusGood == StatusOfSubUnit(1)) {
							
							if ( (getRMDStatus(fUnitDeviceArray[0]->RMD()) & NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED)
								 && (getRMDStatus(fUnitDeviceArray[1]->RMD()) & NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED) ) {
								
								setStatusOfSubUnit(0, kNDASRAIDSubUnitStatusFailure);
								setStatusOfSubUnit(1, kNDASRAIDSubUnitStatusFailure);
								
							} else {
								
								firstUSN = getRMDUSN(fUnitDeviceArray[0]->RMD());
								secondUSN = getRMDUSN(fUnitDeviceArray[1]->RMD()); 
								
								if ( firstUSN == secondUSN ) {
									
									if (fInRecoveryProcess) {
										if (NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED & getRMDStatusOfUnit(fUnitDeviceArray[0]->RMD(), 0)
											|| NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED & getRMDStatusOfUnit(fUnitDeviceArray[1]->RMD(), 0)) {
											setStatusOfSubUnit(0, kNDASRAIDSubUnitStatusRecovering);
										}
										if (NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED & getRMDStatusOfUnit(fUnitDeviceArray[0]->RMD(), 1)
											|| NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED & getRMDStatusOfUnit(fUnitDeviceArray[1]->RMD(), 1)) {
											setStatusOfSubUnit(1, kNDASRAIDSubUnitStatusRecovering);
										}									
									} else {
										if (NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED & getRMDStatusOfUnit(fUnitDeviceArray[0]->RMD(), 0)
											|| NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED & getRMDStatusOfUnit(fUnitDeviceArray[1]->RMD(), 0)) {
											setStatusOfSubUnit(0, kNDASRAIDSubUnitStatusNeedRecovery);
										}
										if (NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED & getRMDStatusOfUnit(fUnitDeviceArray[0]->RMD(), 1)
											|| NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED & getRMDStatusOfUnit(fUnitDeviceArray[1]->RMD(), 1)) {
											setStatusOfSubUnit(1, kNDASRAIDSubUnitStatusNeedRecovery);
										}
									}
									
								} else if ( firstUSN > secondUSN ) {
									
									if (fInRecoveryProcess) {
										setStatusOfSubUnit(1, kNDASRAIDSubUnitStatusRecovering);
									} else {
										setStatusOfSubUnit(1, kNDASRAIDSubUnitStatusNeedRecovery);
									}
									
								} else if ( firstUSN < secondUSN ) {
									
									if (fInRecoveryProcess) {
										setStatusOfSubUnit(0, kNDASRAIDSubUnitStatusRecovering);
									} else {
										setStatusOfSubUnit(0, kNDASRAIDSubUnitStatusNeedRecovery);
									}
									
								} else {
									// BUG BUG BUG!!!
									DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("updateRAIDStatus: BAD USN.\n"));
									setStatusOfSubUnit(0, kNDASRAIDSubUnitStatusFailure);
									setStatusOfSubUnit(1, kNDASRAIDSubUnitStatusFailure);							
								}
							}
						} else {
						
							if ( kNDASRAIDSubUnitStatusGood == StatusOfSubUnit(0)
								 && (getRMDStatus(fUnitDeviceArray[0]->RMD()) & NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED)
								 && (NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED & getRMDStatusOfUnit(fUnitDeviceArray[0]->RMD(), 1)) ) {

								setStatusOfSubUnit(0, kNDASRAIDSubUnitStatusGood);
								setStatusOfSubUnit(1, kNDASRAIDSubUnitStatusFailure);							
								
							} else if ( kNDASRAIDSubUnitStatusGood == StatusOfSubUnit(1)
										&& (getRMDStatus(fUnitDeviceArray[1]->RMD()) & NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED)
										&& (NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED & getRMDStatusOfUnit(fUnitDeviceArray[1]->RMD(), 0)) ) {
								
								setStatusOfSubUnit(0, kNDASRAIDSubUnitStatusFailure);
								setStatusOfSubUnit(1, kNDASRAIDSubUnitStatusGood);							

							} else {
							}
						}
					}
						break;
					default:
						DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("updateRAIDStatus: BAD RAID Status. %d\n", prevRAIDStatus));					
						break;
				}
				
				//
				// Update RAID Status.
				//
				if (kNDASRAIDSubUnitStatusGood == StatusOfSubUnit(0) 
					&& kNDASRAIDSubUnitStatusGood == StatusOfSubUnit(1)) {
					newRAIDStatus = kNDASRAIDStatusGood;
				} else if ((kNDASRAIDSubUnitStatusRecovering == StatusOfSubUnit(0) && 
							kNDASRAIDSubUnitStatusGood == StatusOfSubUnit(1))
						   || (kNDASRAIDSubUnitStatusRecovering == StatusOfSubUnit(1) && 
							   kNDASRAIDSubUnitStatusGood == StatusOfSubUnit(0))) 
				{
					newRAIDStatus = kNDASRAIDStatusRecovering;
				} else if ((kNDASRAIDSubUnitStatusNeedRecovery == StatusOfSubUnit(0) && 
							kNDASRAIDSubUnitStatusGood == StatusOfSubUnit(1))
						   || (kNDASRAIDSubUnitStatusNeedRecovery == StatusOfSubUnit(1) && 
							   kNDASRAIDSubUnitStatusGood == StatusOfSubUnit(0))) 
				{
					newRAIDStatus = kNDASRAIDStatusReadyToRecovery;
				} else if (kNDASRAIDSubUnitStatusGood == StatusOfSubUnit(0)
						   || kNDASRAIDSubUnitStatusGood == StatusOfSubUnit(1)) 
				{
					if (IsDirty()) {
						newRAIDStatus = kNDASRAIDStatusBrokenButWorkable;
					} else {
						newRAIDStatus = kNDASRAIDStatusGoodButPartiallyFailed;						
					}
				} else if (kNDASRAIDSubUnitStatusNotPresent == StatusOfSubUnit(0)
						   && kNDASRAIDSubUnitStatusNotPresent == StatusOfSubUnit(1))
				{
					newRAIDStatus = kNDASRAIDStatusNotReady;
				} else {
					newRAIDStatus = kNDASRAIDStatusBroken;
				}
				
				// Check Replaced Unit.
				if (0 < CountOfSpareDevices()
					&& kNDASRAIDSubUnitTypeReplacedBySpare == TypeOfSubUnit(2)) {
					
					switch(newRAIDStatus) {
						case kNDASRAIDStatusGood:
							newRAIDStatus = kNDASRAIDStatusGoodWithReplacedUnit;
							break;
						case kNDASRAIDStatusReadyToRecovery:
							newRAIDStatus = kNDASRAIDStatusReadyToRecoveryWithReplacedUnit;
							break;
					}
				}
			}
				break;
			default:
				DbgIOLog(DEBUG_MASK_RAID_INFO, ("updateRAIDStatus: Unsuppoered RAID type.\n"));
				
				newRAIDStatus = kNDASRAIDStatusUnsupported;
				
				break;
		}
	}
	
Out:
	
	setRAIDStatus(newRAIDStatus);

	if (prevRAIDStatus != newRAIDStatus) {
		DbgIOLog(DEBUG_MASK_RAID_ERROR, ("updateRAIDStatus: From %d to %d.\n", prevRAIDStatus, newRAIDStatus));
		
		if (kNDASRAIDStatusNotReady == prevRAIDStatus) {
			setNumberOfSectors(getNumberOfSectors());
			
			DbgIOLog(DEBUG_MASK_NDAS_INFO, ("updateRAIDStatus: set Sectors RAID device. %lld\n", NumberOfSectors()));											
		}
		
		// Temp change configuration.
		switch (Type()) {
			case NMT_AGGREGATE:
			case NMT_RAID0:
			{
				if ( kNDASRAIDStatusGood == prevRAIDStatus
					 && kNDASRAIDStatusGood != newRAIDStatus
					 && kNDASUnitConfigurationUnmount != Configuration() ) {
				
					setConfigurationOfSubUnits(kNDASUnitConfigurationUnmount);
				}

				// Restore Configuration.
				if ( kNDASRAIDStatusGood == newRAIDStatus
					 && kNDASRAIDStatusGood != prevRAIDStatus
					 && kNDASUnitConfigurationUnmount != Configuration() ) {
					
					setConfigurationOfSubUnits(Configuration());
				}				
			}
				break;
			default:
				break;
		}
		// Send Message to BunEnumerator.
		sendNDASFamilyIOMessage(kNDASFamilyIOMessageRAIDDeviceRAIDStatusWasChanged, ID(), sizeof(GUID));
	}
		
	IOLockUnlock(lock);

	return;
}

UInt32 com_ximeta_driver_NDASRAIDUnitDevice::updateStatus()
{
	DbgIOLog(DEBUG_MASK_RAID_TRACE, ("getStatus: updateStatus.\n"));
	
	UInt32	newStatus;	// Not assigned.
	UInt32	prevStatus;
	int count;
	
	newStatus = prevStatus = Status();
		
	// Update RAID Status first.
	updateRAIDStatus();
	
	switch(RAIDStatus()) {
		case kNDASRAIDStatusNotReady:
		{
			newStatus = kNDASUnitStatusNotPresent;
		}
			break;
		case kNDASRAIDStatusGood:
		case kNDASRAIDStatusGoodWithReplacedUnit:
		{
			bool endloop = false;
			
			for (count = 0; count < fCountOfUnitDevices; count++) {
				
				if(!fUnitDeviceArray[count]) {
					// Skip.
				} else {
					switch(fUnitDeviceArray[count]->Status()) {
						case kNDASUnitStatusFailure:
						case kNDASUnitStatusNotPresent:
						{
							newStatus = fUnitDeviceArray[count]->Status();
							endloop = true;
						}
							break;
						case kNDASUnitStatusTryConnectRO:
						case kNDASUnitStatusTryConnectRW:
						case kNDASUnitStatusTryDisconnectRO:
						case kNDASUnitStatusTryDisconnectRW:
						case kNDASUnitStatusReconnectRO:
						case kNDASUnitStatusReconnectRW:
						{
							newStatus = fUnitDeviceArray[count]->Status();
							endloop = true;
						}
							break;
						case kNDASUnitStatusDisconnected:
						case kNDASUnitStatusConnectedRO:
						case kNDASUnitStatusConnectedRW:
						{
							newStatus = fUnitDeviceArray[count]->Status();
						}
							break;
					}
				}
				
				if (endloop) {
					break;
				}
			}
			
			if (kNDASRAIDStatusGoodWithReplacedUnit == RAIDStatus()
				&& kNDASUnitStatusDisconnected == newStatus) {
				newStatus = kNDASUnitStatusFailure;
			}
		}
			break;
		case kNDASRAIDStatusGoodButPartiallyFailed:
		case kNDASRAIDStatusBrokenButWorkable:
		case kNDASRAIDStatusReadyToRecovery:
		case kNDASRAIDStatusReadyToRecoveryWithReplacedUnit:
		case kNDASRAIDStatusRecovering:
		{
			//
			// For RAID 1.
			//
			if (NMT_RAID1R3 == Type()) {
				
				if (kNDASRAIDSubUnitStatusGood == StatusOfSubUnit(0)
					&& fUnitDeviceArray[0]) {
					newStatus = fUnitDeviceArray[0]->Status();
				} else if (kNDASRAIDSubUnitStatusGood == StatusOfSubUnit(1)
						   && fUnitDeviceArray[1]) {
					newStatus = fUnitDeviceArray[1]->Status();
				} else {
					newStatus = kNDASUnitStatusFailure;
				}
			} else {
				// Not Implemented.
			}
		}
			break;
		case kNDASRAIDStatusBroken:
		default:
		{
			newStatus = kNDASUnitStatusFailure;			
		}
	}
	
Out:
			
	setStatus(newStatus);

	if (prevStatus != newStatus) {
		DbgIOLog(DEBUG_MASK_RAID_ERROR, ("updateStatus: From %d to %d.\n", prevStatus, newStatus));
		
		// Send Message to BunEnumerator.
		sendNDASFamilyIOMessage(kNDASFamilyIOMessageRAIDDeviceStatusWasChanged, ID(), sizeof(GUID));
	}
	
	// Need post processing for redundant RAID.
	if (NMT_RAID1R3 == Type()) {
		if ((prevStatus == kNDASUnitStatusTryConnectRW || prevStatus == kNDASUnitStatusDisconnected )&&
			newStatus == kNDASUnitStatusConnectedRW) {
			
			if(!postProcessingforConnectRW()) {
				DbgIOLog(DEBUG_MASK_RAID_ERROR, ("updateStatus: post processing for Connect failed!!!\n"));
			}
			
			fPreProcessDone = false;
		}
	}
	
	return Status();
}

bool com_ximeta_driver_NDASRAIDUnitDevice::postProcessingforConnectRW() {

	DbgIOLog(DEBUG_MASK_RAID_TRACE, ("postProcessingforConnectRW: Enterted.\n"));
	
	if (NMT_RAID1R3 != Type()) {
		// nothing to do.
		return true;
	}
	
	int goodUnitIndex;
	int badUnitIndex;
	
	if (UnitDeviceAtIndex(0) && 
		UnitDeviceAtIndex(0)->IsInRunningStatus()
		&& UnitDeviceAtIndex(0)->IsConnected() 
		&& kNDASRAIDSubUnitStatusGood == StatusOfSubUnit(0) ) {
		
		goodUnitIndex = 0;
		badUnitIndex = 1;
		
	} else if (UnitDeviceAtIndex(1) && 
			   UnitDeviceAtIndex(1)->IsInRunningStatus()
			   && UnitDeviceAtIndex(1)->IsConnected() 
			   && kNDASRAIDSubUnitStatusGood == StatusOfSubUnit(1) ) {
		
		goodUnitIndex = 1;
		badUnitIndex = 0;
		
	} else {
		DbgIOLog(DEBUG_MASK_RAID_ERROR, ("postProcessingforConnectRW: no Good Unit Device!!.\n"));

		return false;
	}
		
	// Update RMD.
	NDAS_RAID_META_DATA					tempRMD;
	uint32_t							tempValue;
	uint32_t							tempUSN;
	com_ximeta_driver_NDASUnitDevice	*unitDevice;
	int									count;

	for (count = 0;  count < CountOfUnitDevices() + CountOfSpareDevices(); count++) {

		unitDevice = UnitDeviceAtIndex(count);
		
		if (unitDevice) {
		
			memcpy(&tempRMD, UnitDeviceAtIndex(count)->RMD(), sizeof(NDAS_RAID_META_DATA));

			// update flag.
			tempValue = getRMDStatus(&tempRMD) | NDAS_RAID_META_DATA_STATE_MOUNTED & ~NDAS_RAID_META_DATA_STATE_UNMOUNTED;
			tempRMD.state = NDASSwap32HostToLittle(tempValue);
			
			// sequence.
			tempUSN =  getRMDUSN(UnitDeviceAtIndex(count)->RMD()) + 1;
			tempRMD.uiUSN = NDASSwap32HostToLittle(tempUSN);
			
			// We don't support Arbiter.
			memset(&tempRMD.ArbiterInfo[0], 0, (sizeof(tempRMD.ArbiterInfo[0]) * NDAS_DRAID_ARBITER_ADDR_COUNT));
						
			SET_RMD_CRC(crc32_calc, tempRMD);
			
			UInt64 location = unitDevice->NumberOfSectors() + NDAS_BLOCK_LOCATION_RMD;
			
			if(!unitDevice->writeSectorsOnce(location, 1,  (char *)&tempRMD)) {
				DbgIOLog(DEBUG_MASK_RAID_ERROR, ("postProcessingforConnectRW: Write RMD failed for %dnd.\n", count));
			} else {
				unitDevice->checkRMD();
				DbgIOLog(DEBUG_MASK_RAID_WARNING, ("postProcessingforConnectRW: Write RMD to %lld USN %d for %dnd Unit.\n", location, getRMDUSN(&tempRMD), count));
			}
		}
	}
	
	return true;
}

bool com_ximeta_driver_NDASRAIDUnitDevice::preProcessingforDisconnectRW() {
	
	DbgIOLog(DEBUG_MASK_RAID_TRACE, ("preProcessingforDisconnectRW: Enterted.\n"));

	if (NMT_RAID1R3 != Type()) {
		// nothing to do.
		return true;
	}
	
	int goodUnitIndex;
	int badUnitIndex;
	
	if (UnitDeviceAtIndex(0) && 
		kNDASRAIDSubUnitStatusGood == StatusOfSubUnit(0) ) {
		
		goodUnitIndex = 0;
		badUnitIndex = 1;
		
	} else if (UnitDeviceAtIndex(1) && 
			   kNDASRAIDSubUnitStatusGood == StatusOfSubUnit(1) ) {
		
		goodUnitIndex = 1;
		badUnitIndex = 0;

	} else {
		
		DbgIOLog(DEBUG_MASK_RAID_ERROR, ("preProcessingforDisconnectRW: no Good Unit Device!!.\n"));

		return false;
	}
	
	// Update RMD.
	NDAS_RAID_META_DATA					tempRMD;
	uint32_t							tempValue;
	uint32_t							tempUSN;
	com_ximeta_driver_NDASUnitDevice	*unitDevice;
	int									count;
		
	for (count = 0;  count < CountOfUnitDevices() + CountOfSpareDevices(); count++) {
		
		unitDevice = UnitDeviceAtIndex(count);
		
		if (unitDevice) {
			
			memcpy(&tempRMD, UnitDeviceAtIndex(count)->RMD(), sizeof(NDAS_RAID_META_DATA));
			
			// update flag.
			tempValue = getRMDStatus(&tempRMD) & ~NDAS_RAID_META_DATA_STATE_MOUNTED | NDAS_RAID_META_DATA_STATE_UNMOUNTED;
			tempRMD.state = NDASSwap32HostToLittle(tempValue);
			
			// sequence.
			tempUSN =  getRMDUSN(UnitDeviceAtIndex(count)->RMD()) + 1;
			tempRMD.uiUSN = NDASSwap32HostToLittle(tempUSN);
			
			// We don't support Arbiter.
			memset(&tempRMD.ArbiterInfo[0], 0, (sizeof(tempRMD.ArbiterInfo[0]) * NDAS_DRAID_ARBITER_ADDR_COUNT));
						
			SET_RMD_CRC(crc32_calc, tempRMD);

			UInt64 location = unitDevice->NumberOfSectors() + NDAS_BLOCK_LOCATION_RMD;
			
			if(!unitDevice->writeSectorsOnce(location, 1,  (char *)&tempRMD)) {
				DbgIOLog(DEBUG_MASK_RAID_ERROR, ("postProcessingforConnectRW: Write RMD failed for %dnd.\n", count));
			} else {
				unitDevice->checkRMD();
				DbgIOLog(DEBUG_MASK_RAID_WARNING, ("postProcessingforConnectRW: Write RMD to %lld USN %d for %dnd Unit.\n", location, getRMDUSN(&tempRMD), count));
			}
		}
	}

	return true;	
}

void com_ximeta_driver_NDASRAIDUnitDevice::receivePnPMessage()
{
	com_ximeta_driver_NDASCommand	*command;
	
	DbgIOLog(DEBUG_MASK_NDAS_INFO, ("receivePnPMessage: Entered.\n"));
		
	if ( fWillTerminate ) {
		return;
	}
		
	if ( NULL == fPnpCommand ) {
		
		// Create NDAS Command.
		command = OSDynamicCast(com_ximeta_driver_NDASCommand, 
								OSMetaClass::allocClassWithName(NDAS_COMMAND_CLASS));
		
		if(command == NULL || !command->init()) {
			DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("receivePnPMessage: failed to alloc command class\n"));
			if (command) {
				command->release();
			}
			
			return;
		}
		
		command->setCommand(0, NULL, 0, true);
		
		if(!fController) { 
			DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("receivePnPMessage: Command gate is NULL!!!\n"));
			
			command->release();
			
			return;
		}
		
		fPnpCommand = command;
		
		fController->enqueueCommand(command);
	}
	
	return;
}

void com_ximeta_driver_NDASRAIDUnitDevice::noPnPMessage()
{
	com_ximeta_driver_NDASCommand	*command;
	
	DbgIOLog(DEBUG_MASK_NDAS_INFO, ("noPnPMessage: Entered.\n"));
	
	if ( fWillTerminate ) {
		return;
	}
	
	if ( NULL == fPnpCommand ) {
		
		// Create NDAS Command.
		command = OSDynamicCast(com_ximeta_driver_NDASCommand, 
								OSMetaClass::allocClassWithName(NDAS_COMMAND_CLASS));
		
		if(command == NULL || !command->init()) {
			DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("noPnPMessage: failed to alloc command class\n"));
			if (command) {
				command->release();
			}
			
			return;
		}
		
		command->setCommand(0, NULL, 0, false);
		
		if(!fController) { 
			DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("noPnPMessage: Command gate is NULL!!!\n"));
			
			command->release();
			
			return;
		}
		
		fPnpCommand = command;
		
		fController->enqueueCommand(command);
	}
	
	return;
}

void com_ximeta_driver_NDASRAIDUnitDevice::completePnPMessage(
															  com_ximeta_driver_NDASCommand *command
															  )
{
	if ( NULL != fPnpCommand ) {
		
		if ( fPnpCommand != command ) {
			DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("completePnPMessage: Command missmatch!!!\n"));
		}
		
		command->release();
		fPnpCommand = NULL;
	}
}

bool com_ximeta_driver_NDASRAIDUnitDevice::sendNDASFamilyIOMessage(UInt32 type, void * argument, vm_size_t argSize) 
{
	com_ximeta_driver_NDASBusEnumerator	*bus;
	
	bus = OSDynamicCast(com_ximeta_driver_NDASBusEnumerator, 
						getParentEntry(gIOServicePlane));
	
	if (NULL == bus) {
		DbgIOLog(DEBUG_MASK_NDAS_INFO, ("sendNDASFamilyIIOMessage: Can't Find bus.\n"));
		
		return false;
	} else {
		bus->messageClients(type, argument, argSize);
		
		return true;
	}
}	

bool	com_ximeta_driver_NDASRAIDUnitDevice::ReplaceToSpare()
{
	int indexOfGoodSubUnit;
	int	indexOfBrokenSubUnit;
	int indexOfSpareSubUnit;
	
	DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("ReplaceToSpare: Entered\n"));

	// Find broken Subunit and spare.
	if (kNDASRAIDSubUnitStatusFailure == StatusOfSubUnit(0)
		|| kNDASRAIDSubUnitStatusNotPresent == StatusOfSubUnit(0)) {
		indexOfBrokenSubUnit = 0;
		indexOfGoodSubUnit = 1;
	} else 	if (kNDASRAIDSubUnitStatusFailure == StatusOfSubUnit(1)
				|| kNDASRAIDSubUnitStatusNotPresent == StatusOfSubUnit(1)) {
		indexOfBrokenSubUnit = 1;
		indexOfGoodSubUnit = 0;
	} else {
		// No Bad Unit.
		DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("ReplaceToSpare: No Bad SubUnit.\n"));
		
		return true;
	}

	// BUG BUG BUG. It assumed that spare disk is only one.
	if (kNDASRAIDSubUnitStatusGood == StatusOfSubUnit(2)) {
		indexOfSpareSubUnit = 2;
	} else {
		// No Bad Unit.
		DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("ReplaceToSpare: No Spare SubUnit.\n"));
		
		return false;
	}
	
	PNDAS_RAID_META_DATA	tempRMD;
	NDAS_uuid_t				configID;
	UInt32					usn;
	
	NDAS_uuid_generate(configID);
	
	usn = getRMDUSN(UnitDeviceAtIndex(indexOfGoodSubUnit)->RMD()) + 1;
	
	setConfigID((PGUID)configID);
	setRMDUSN(usn);
	
	// Change RMD info first.
	for (int count = 0; count < CountOfUnitDevices() + CountOfSpareDevices(); count++) {
		
		if (UnitDeviceAtIndex(count)
			&& kNDASRAIDSubUnitStatusFailure != StatusOfSubUnit(count)
			&& kNDASRAIDSubUnitStatusNotPresent != StatusOfSubUnit(count)) {
						
			tempRMD = UnitDeviceAtIndex(count)->RMD();
			
			tempRMD->UnitMetaData[indexOfBrokenSubUnit].iUnitDeviceIdx = NDASSwap16HostToLittle(indexOfSpareSubUnit);
			tempRMD->UnitMetaData[indexOfBrokenSubUnit].UnitDeviceStatus = NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED;
			
			tempRMD->UnitMetaData[indexOfSpareSubUnit].iUnitDeviceIdx = NDASSwap16HostToLittle(indexOfBrokenSubUnit);
			tempRMD->UnitMetaData[indexOfSpareSubUnit].UnitDeviceStatus |= NDAS_UNIT_META_BIND_STATUS_REPLACED_BY_SPARE;
			
			UnitDeviceAtIndex(indexOfSpareSubUnit)->setSubUnitIndex(indexOfBrokenSubUnit);
			
			memcpy(&tempRMD->ConfigSetId, &configID, sizeof(GUID));

			// update USN.
			tempRMD->uiUSN = NDASSwap32HostToLittle( usn );
			
			SET_RMD_CRC(crc32_calc, *tempRMD);
			
			// Update RMD.
			UInt64 location = UnitDeviceAtIndex(count)->NumberOfSectors() + NDAS_BLOCK_LOCATION_RMD;
			
			if(!UnitDeviceAtIndex(count)->writeSectorsOnce(location, 1,  (char *)tempRMD)) {
				DbgIOLog(DEBUG_MASK_RAID_ERROR, ("ReplaceToSpare: Write RMD failed.\n"));
			} else {
				memcpy(UnitDeviceAtIndex(count)->RMD(), tempRMD, sizeof(NDAS_RAID_META_DATA));
			}
			
			UnitDeviceAtIndex(count)->checkRMD();
		}
	}
	
	// Write Bitmap.
	UpdateBitMapBit(0, Capacity(), true);
	
	if (fInRecoveryProcess) {
		fSectorOfRecovery = 0;
	}
	
	// Apply changed RMD info to RAID set.	
	setUnitDevice(indexOfBrokenSubUnit, UnitDeviceAtIndex(indexOfSpareSubUnit));
	setUnitDevice(indexOfSpareSubUnit, NULL);

	UInt64	temp;
	char	buffer[256];
	
	sprintf(buffer, NDASDEVICE_PROPERTY_KEY_RAID_UNIT_DISK_LOCATION_OF_SUB_UNIT, indexOfSpareSubUnit);
	
	memcpy(&temp, getDIBUnitDiskLocation(UnitDeviceAtIndex(indexOfGoodSubUnit)->DIB2(), indexOfBrokenSubUnit), sizeof(UInt64));
	
	setProperty(buffer, temp, 64);		
	
	setTypeOfSubUnit(indexOfSpareSubUnit, kNDASRAIDSubUnitTypeReplacedBySpare);
	
	
	return true;
}