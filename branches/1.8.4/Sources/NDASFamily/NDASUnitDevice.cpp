/*
 *  NDASUnitDevice.cpp
 *  NDASFamily
 *
 *  Created by ?? ? on 05. 11. 29.
 *  Copyright 2005 __MyCompanyName__. All rights reserved.
 *
 */
#include "NDASUnitDevice.h"

#include "NDASBusEnumeratorUserClientTypes.h"
#include "LanScsiDiskInformationBlock.h"
#include "Hash.h"
#include "scrc32.h"
#include "NDASDIB.h"
#include "NDASFamilyIOMessage.h"

#include "NDASBusEnumerator.h"
#include "NDASLogicalDevice.h"
#include "NDASRAIDUnitDevice.h"

#include "Utilities.h"

// Define my superclass
#define super IOService

// REQUIRED! This macro defines the class's constructors, destructors,
// and several other methods I/O Kit requires. Do NOT use super as the
// second parameter. You must use the literal name of the superclass.

OSDefineMetaClassAndAbstractStructors(com_ximeta_driver_NDASUnitDevice, IOService)

bool com_ximeta_driver_NDASUnitDevice::init(OSDictionary *dict)
{
	bool res = super::init(dict);
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("[Unit]Initializing\n"));
	
	fUpperUnitDevice = NULL;
	fConfiguration = kNDASUnitConfigurationUnmount;
	fStatus = kNDASUnitStatusNotPresent;
	fDiskArrayType = NMT_INVALID;	
	
	fNRRWHosts = -1;
	fNRROHosts = -1;

	setNRRWHosts(0);
	setNRROHosts(0);
	
	setNumberOfSectors(0);

	setRAIDFLAG(0xffffffff, false);

	setProperty(NDASDEVICE_PROPERTY_KEY_CONFIGURATION, fConfiguration, 32);
	setProperty(NDASDEVICE_PROPERTY_KEY_STATUS, fStatus, 32);

    return res;
}

void	com_ximeta_driver_NDASUnitDevice::setID(PGUID ID)
{
	memcpy(&fGUID, ID, sizeof(GUID));
	setProperty(NDASDEVICE_PROPERTY_KEY_UNIT_DEVICE_ID, &fGUID, 128);
}

PGUID	com_ximeta_driver_NDASUnitDevice::ID()
{
	return &fGUID;
}

void	com_ximeta_driver_NDASUnitDevice::setUnitDiskLocation(PUNIT_DISK_LOCATION location)
{
	memcpy(&fUnitDiskLocation, location, sizeof(UNIT_DISK_LOCATION));
	
	setProperty(NDASDEVICE_PROPERTY_KEY_UNIT_UNIT_DISK_LOCATION, *((uint64_t *)&fUnitDiskLocation), 64);
}

PUNIT_DISK_LOCATION	com_ximeta_driver_NDASUnitDevice::UnitDiskLocation()
{
	return &fUnitDiskLocation;
}

void	com_ximeta_driver_NDASUnitDevice::setConfiguration(uint32_t configuration)
{
	fConfiguration = configuration;
	
	setProperty(NDASDEVICE_PROPERTY_KEY_UNIT_CONFIGURATION, fConfiguration, 32);
}

uint32_t	com_ximeta_driver_NDASUnitDevice::Configuration()
{
	return fConfiguration;
}

void	com_ximeta_driver_NDASUnitDevice::setStatus(uint32_t status)
{
	DbgIOLog(DEBUG_MASK_NDAS_TRACE, ("setStatus: Entered. %d\n", status));
	
	fStatus = status;	
	
	setProperty(NDASDEVICE_PROPERTY_KEY_UNIT_STATUS, fStatus, 32);
}

uint32_t	com_ximeta_driver_NDASUnitDevice::Status()
{
	DbgIOLog(DEBUG_MASK_NDAS_TRACE, ("Status: Entered. %d\n", fStatus));

	return fStatus;
}

void	com_ximeta_driver_NDASUnitDevice::setCapacity(uint64_t sectors, uint32_t align)
{
	if (sectors == 0 || align == 0) {
		return;
	}
	
	fCapacity = sectors - (sectors % align);
	
	setProperty(NDASDEVICE_PROPERTY_KEY_UNIT_CAPACITY, fCapacity, 64);
}

uint64_t	com_ximeta_driver_NDASUnitDevice::Capacity() 
{
	return fCapacity;
}

void	com_ximeta_driver_NDASUnitDevice::setNRRWHosts(uint32_t hosts)
{
	if (fNRRWHosts != hosts) {
		fNRRWHosts = hosts;
		
		setProperty(NDASDEVICE_PROPERTY_KEY_UNIT_NR_RW_HOST, fNRRWHosts, 32);
		
		// Send Message to BunEnumerator.
		sendNDASFamilyIOMessage(kNDASFamilyIOMessageUnitDevicePropertywasChanged, ID(), sizeof(GUID));		
	}
}

uint32_t	com_ximeta_driver_NDASUnitDevice::NRRWHosts()
{
	return fNRRWHosts;
}

void	com_ximeta_driver_NDASUnitDevice::setNRROHosts(uint32_t hosts)
{
	if (fNRROHosts != hosts) {
		
		fNRROHosts = hosts;
		
		setProperty(NDASDEVICE_PROPERTY_KEY_UNIT_NR_RO_HOST, fNRROHosts, 32);
		
		// Send Message to BunEnumerator.
		sendNDASFamilyIOMessage(kNDASFamilyIOMessageUnitDevicePropertywasChanged, ID(), sizeof(GUID));		
	}
}

uint32_t	com_ximeta_driver_NDASUnitDevice::NRROHosts()
{
	return fNRROHosts;
}

void	com_ximeta_driver_NDASUnitDevice::setNumberOfSectors(uint64_t NumberOfSectors)
{
	fNumberOfSectors = NumberOfSectors;	
	
	setProperty(NDASDEVICE_PROPERTY_KEY_UNIT_NUMBER_OF_SECTORS, fNumberOfSectors, 64);
}

uint64_t	com_ximeta_driver_NDASUnitDevice::NumberOfSectors()
{
	return fNumberOfSectors;
}

void	com_ximeta_driver_NDASUnitDevice::setBlockSize(uint32_t blockSize)
{
	fBlockSize = blockSize;
	
	setProperty(NDASDEVICE_PROPERTY_KEY_UNIT_BLOCK_SIZE, fBlockSize, 32);
}

uint32_t	com_ximeta_driver_NDASUnitDevice::blockSize() 
{
	return fBlockSize;
}

void	com_ximeta_driver_NDASUnitDevice::setDiskArrayType(uint32_t raidType)
{
	fDiskArrayType = raidType;
	setProperty(NDASDEVICE_PROPERTY_KEY_UNIT_DISK_ARRAY_TYPE, fDiskArrayType, 32);
}

uint32_t	com_ximeta_driver_NDASUnitDevice::DiskArrayType() 
{
	return fDiskArrayType;
}

bool	com_ximeta_driver_NDASUnitDevice::IsConnected()
{
	return fConnected;
}

void	com_ximeta_driver_NDASUnitDevice::setConnected(bool connected)
{
	fConnected = connected;
	
	setProperty(NDASDEVICE_PROPERTY_KEY_UNIT_CONNECTED, fConnected);
}

void print_dib_v1(PNDAS_DIB_V1 dib1) {
	
	if (NULL == dib1) {
		return;
	}
	
	// Signature.
	DbgIOLogNC(DEBUG_MASK_NDAS_ERROR, ("print_dib_v1: Signature 0x%x\n", NDASSwap32BigToHost(dib1->Signature)));
	
	if (NDAS_DIB_SIGNATURE_V1 == dib1->Signature)
	{
		// Version.
		DbgIOLogNC(DEBUG_MASK_NDAS_ERROR, ("print_dib_v1: Major Version %d\n", dib1->MajorVersion));
		DbgIOLogNC(DEBUG_MASK_NDAS_ERROR, ("print_dib_v1: Minor Version %d\n", dib1->MinorVersion));
				
		// Media Type.
		DbgIOLogNC(DEBUG_MASK_NDAS_ERROR, ("print_dib_v1: Disk Type %d\n", dib1->DiskType));
		
		// Disk count.
		DbgIOLogNC(DEBUG_MASK_NDAS_ERROR, ("print_dib_v1: usageType %d\n", dib1->UsageType));
		
		// Sequence.
		DbgIOLogNC(DEBUG_MASK_NDAS_ERROR, ("print_dib_v1: Sequence %d\n", NDASSwap32BigToHost(dib1->Sequence)));
		
		DbgIOLogNC(DEBUG_MASK_NDAS_ERROR, ("print_dib_v1: [My ID] %02x:%02x:%02x:%02x:%02x:%02x[%d] \n", 
										   dib1->EtherAddress[0], dib1->EtherAddress[1], dib1->EtherAddress[2],
										   dib1->EtherAddress[3], dib1->EtherAddress[4], dib1->EtherAddress[5],
										   dib1->UnitNumber));
		
		DbgIOLogNC(DEBUG_MASK_NDAS_ERROR, ("print_dib_v1: [Peer ID] %02x:%02x:%02x:%02x:%02x:%02x[%d] \n", 
										   dib1->PeerAddress[0], dib1->PeerAddress[1], dib1->PeerAddress[2],
										   dib1->PeerAddress[3], dib1->PeerAddress[4], dib1->PeerAddress[5],
										   dib1->PeerUnitNumber));		
	}
}

void print_dib_v2(PNDAS_DIB_V2 dib2) {
	
	if (NULL == dib2) {
		return;
	}
	
	// Signature.
	DbgIOLogNC(DEBUG_MASK_NDAS_ERROR, ("print_dib_v2: Signature %llx\n", NDASSwap64LittleToHost(dib2->Signature)));
	
	if (NDAS_DIB_SIGNATURE_V2 == dib2->Signature
		&& IS_DIB_CRC_VALID(crc32_calc, *dib2))
	{
		// Version.
		DbgIOLogNC(DEBUG_MASK_NDAS_ERROR, ("print_dib_v2: Major Version %d\n", NDASSwap32LittleToHost(dib2->MajorVersion)));
		DbgIOLogNC(DEBUG_MASK_NDAS_ERROR, ("print_dib_v2: Minor Version %d\n", NDASSwap32LittleToHost(dib2->MinorVersion)));
		
		// Size of X area.
		DbgIOLogNC(DEBUG_MASK_NDAS_ERROR, ("print_dib_v2: Size of X area %lld\n", NDASSwap64LittleToHost(dib2->sizeXArea)));
		
		// Size of User Space.
		DbgIOLogNC(DEBUG_MASK_NDAS_ERROR, ("print_dib_v2: Size of User Space %lld\n", NDASSwap64LittleToHost(dib2->sizeUserSpace)));
		
		// Sectors per Bit.
		DbgIOLogNC(DEBUG_MASK_NDAS_ERROR, ("print_dib_v2: Bits per Sectors %d\n", NDASSwap32LittleToHost(dib2->iSectorsPerBit)));
		
		// Media Type.
		DbgIOLogNC(DEBUG_MASK_NDAS_ERROR, ("print_dib_v2: Media Type %d\n", NDASSwap32LittleToHost(dib2->iMediaType)));
		
		// Disk count.
		DbgIOLogNC(DEBUG_MASK_NDAS_ERROR, ("print_dib_v2: Disk Count %d\n", NDASSwap32LittleToHost(dib2->nDiskCount)));
		
		// Sequence.
		DbgIOLogNC(DEBUG_MASK_NDAS_ERROR, ("print_dib_v2: Sequence %d\n", NDASSwap32LittleToHost(dib2->iSequence)));
		
		// BACLSize
		DbgIOLogNC(DEBUG_MASK_NDAS_ERROR, ("print_dib_v2: BACLSize %d\n", NDASSwap32LittleToHost(dib2->BACLSize)));

		// nSpareCount
		DbgIOLogNC(DEBUG_MASK_NDAS_ERROR, ("print_dib_v2: nSpareCount %d\n", NDASSwap32LittleToHost(dib2->nSpareCount)));		
		
		// Units.
		if (1 < NDASSwap32HostToLittle(dib2->nDiskCount)) {
			for (int count = 0; count <= NDASSwap32LittleToHost(dib2->nDiskCount); count++) {
				
				if (count == NDAS_MAX_UNITS_IN_V2) {
					break;
				}
				
				DbgIOLogNC(DEBUG_MASK_NDAS_ERROR, ("print_dib_v2: Unit[%d] %02x:%02x:%02x:%02x:%02x:%02x[%d], 0x%x \n", 
												   count,
												   dib2->UnitDisks[count].MACAddr[0], dib2->UnitDisks[count].MACAddr[1], dib2->UnitDisks[count].MACAddr[2],
												   dib2->UnitDisks[count].MACAddr[3], dib2->UnitDisks[count].MACAddr[4], dib2->UnitDisks[count].MACAddr[5],
												   dib2->UnitDisks[count].UnitNumber, dib2->UnitDisks[count].VID));
			}
		}
	}
}

bool	com_ximeta_driver_NDASUnitDevice::checkDiskArrayType()
{
    uint64_t			location;
    NDAS_DIB_V2			*dib_v2;
	NDAS_DIB_V1			*dib_v1;
	char				*sectorBuffer;
	
	//
	// Check Packet Device.
	//
	if (NMT_CDROM == Type()) {
		
		setDiskArrayType(NMT_CDROM);

		return true;
	}
	
    /*** Scrutiny of x area of a newly attached disk */	
	
    /**
		*  1. Read an NDAS_DIB_V2 structure from the NDAS Device at
     *     NDAS_BLOCK_LOCATION_DIB_V2.
     */
	
	if (NumberOfSectors() <= 0) {
		DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("Sector size if 0!!!\n"));

		setDiskArrayType(NMT_INVALID);
		
		return false;
	}
	
	sectorBuffer = (char *)IOMalloc(blockSize());
	if (sectorBuffer == NULL) {
		DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("Can't alloc buffer!!!\n"));

		setDiskArrayType(NMT_INVALID);
		
		return false;
	}
	
	//ND_LOG("1st..\n");
    location = NumberOfSectors() + NDAS_BLOCK_LOCATION_DIB_V2;
	
	DbgIOLog(DEBUG_MASK_RAID_INFO, ("Location of DIB_v2 %lld\n", location));
	
	// Set Capacity by default value.
	setCapacity((NumberOfSectors() - NDAS_BLOCK_SIZE_XAREA), NDAS_USER_SPACE_ALIGN);
	
    if (false == readSectorsOnce(location, 1, sectorBuffer))
    {
        DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("Reading DIB V2 of X area is failed.\n"));
		
		setDiskArrayType(NMT_INVALID);
		setStatus(kNDASUnitStatusFailure);
		
		IOFree(sectorBuffer, blockSize());
		
        return false;
    }

	dib_v2 = (NDAS_DIB_V2*)sectorBuffer;

//	print_dib_v2(& dib_v2);
	
    /**
		*  2. Check Signature, Version and CRC information in NDAS_DIB_V2 and
     *     accept if all the information is correct.
     */
	
	//ND_LOG("2nd..\n");
    if (NDAS_DIB_SIGNATURE_V2 == dib_v2->Signature
        && IS_DIB_CRC_VALID(crc32_calc, *dib_v2))
    {
        if ( ((NDAS_DIB_VERSION_MAJOR_V2 < NDASSwap32LittleToHost(dib_v2->MajorVersion)) ||
			  (NDAS_DIB_VERSION_MAJOR_V2 == NDASSwap32LittleToHost(dib_v2->MajorVersion) &&
			   NDAS_DIB_VERSION_MINOR_V2 < NDASSwap32LittleToHost(dib_v2->MinorVersion))) )
        {
            /* UNKNOWN DIB VERSION */
            /* There is nothing to do yet. */
			DbgIOLog(DEBUG_MASK_RAID_INFO, ("checkDiskArrayType: Unknown DIB_v2 Version Maj %d Mir %d\n",
											 NDASSwap32LittleToHost(dib_v2->MajorVersion),
											 NDASSwap32LittleToHost(dib_v2->MinorVersion)));
			
			setDiskArrayType(NMT_INVALID);

			IOFree(sectorBuffer, blockSize());

			return false;
        }
		
		
        /**
		*  3. Read additional NDAS Device location information at
         *     NDAS_BLOCK_LOCATION_ADD_BIND in case of more than 32 NDAS Unit
         *     devices exist.
         */
		
		//ND_LOG("3rd..\n");
        if (NDASSwap32LittleToHost(dib_v2->nDiskCount) > 1)
        {
            // My DIB?			
			if ( memcmp(getDIBUnitDiskLocation(dib_v2, NDASSwap32LittleToHost(dib_v2->iSequence)),
						UnitDiskLocation(),
						7) != 0) {	// except VID.
				// Not Mine.
				DbgIOLog(DEBUG_MASK_RAID_WARNING, ("checkDiskArrayType: Not my DIB2.\n"));
												
				setRAIDFLAG(UNIT_DEVICE_RAID_FLAG_NDAS_ADDR_MISMATCH, true);				
			} else {
				setRAIDFLAG(UNIT_DEVICE_RAID_FLAG_NDAS_ADDR_MISMATCH, false);
			}
        }
		
        /* Copy DIB V2 */
        memcpy((void *) &fDIB2, (void *)dib_v2, sizeof (*dib_v2));
		
        /* Get a disk type */
		setDiskArrayType(NDASSwap32LittleToHost(dib_v2->iMediaType));
		DbgIOLog(DEBUG_MASK_NDAS_INFO, ("checkDiskArrayType: DIB2. 0x%x\n", NDASSwap32LittleToHost(dib_v2->iMediaType)));
		
		uint32_t sectorsPerBit;
		
		if (0 == getDIBSectorsPerBit(dib_v2)) {
			sectorsPerBit = NDAS_USER_SPACE_ALIGN;
		} else {
			sectorsPerBit = getDIBSectorsPerBit(dib_v2);
		}
		
		setCapacity(getDIBSizeOfUserSpace(dib_v2), sectorsPerBit);
		
		if (false == checkRMD()) {
			
			if (NMT_RAID1R3 == NDASSwap32LittleToHost(dib_v2->iMediaType)) {
				// Bad RMD
				DbgIOLog(DEBUG_MASK_RAID_WARNING, ("checkDiskArrayType: Bad RMD DIB2.\n"));
				
				setRAIDFLAG(UNIT_DEVICE_RAID_FLAG_BAD_RMD, true);				

				setDiskArrayType(NMT_INVALID);
				
				IOFree(sectorBuffer, blockSize());

				return false;				
			} else {
				// Make dummy RMD for GUID.
				// 
				// Write RMD
				//
				NDAS_RAID_META_DATA RMD = { 0 };
				
				// Create RMD Info.
				RMD.Signature = NDAS_RAID_META_DATA_SIGNATURE;
				
				memcpy(&(RMD.RaidSetId.guid[0]),& dib_v2->UnitDisks[0], sizeof(UNIT_DISK_LOCATION));
				memcpy(&(RMD.RaidSetId.guid[sizeof(UNIT_DISK_LOCATION)]), & dib_v2->UnitDisks[1], sizeof(UNIT_DISK_LOCATION));
				
				SET_RMD_CRC(crc32_calc, RMD);

				memcpy(&fRMD, &RMD, sizeof(NDAS_RAID_META_DATA));
			}
		}
		
		setRAIDFLAG(UNIT_DEVICE_RAID_FLAG_BAD_RMD, false);				
		
		goto good;
    } 
	
    /**
		*  4. Read an NDAS_DIB_V1 information at NDAS_BLOCK_LOCATION_DIB_V1 if
     *     NDAS_DIB_V2 information is not acceptable.
     */
	
	//ND_LOG("4th..\n");
   location = NumberOfSectors() + NDAS_BLOCK_LOCATION_DIB_V1;
	
	DbgIOLog(DEBUG_MASK_RAID_INFO, ("checkDiskArrayType: Read DIB_V1 Location %lld\n", location));
	
    if (false == readSectorsOnce(location, 1, sectorBuffer))
    {
        DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("checkDiskArrayType: Reading DIB V1 of X area is failed.\n"));
		
		setDiskArrayType(NMT_INVALID);
		setStatus(kNDASUnitStatusFailure);

		IOFree(sectorBuffer, blockSize());

        return false;
    }
	
	dib_v1 = (NDAS_DIB_V1*)sectorBuffer;
	
//	print_dib_v1(& dib_v1);
	
	if (NDAS_DIB_SIGNATURE_V1 == dib_v1->Signature) {
		//IOLog("OK, THIS IS NDAS DIB V1..\n");
		if (NDAS_DIB_VERSION_MAJOR_V1 < dib_v1->MajorVersion
			|| (NDAS_DIB_VERSION_MAJOR_V1 == dib_v1->MajorVersion
				&& NDAS_DIB_VERSION_MINOR_V1 < dib_v1->MinorVersion))
		{
			/* UNKNOWN DIB VERSION */
			/* There is nothing to do yet. */
			DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("checkDiskArrayType: Unknown DIB_v1 Version Maj %d Mir %d\n",
											 dib_v1->MajorVersion,
											 dib_v1->MinorVersion));

			setDiskArrayType(NMT_INVALID);
			IOFree(sectorBuffer, blockSize());

			return false;
		}
		
		switch(dib_v1->DiskType) {
			case NDAS_DIB_DISK_TYPE_SINGLE:
			{
				setDiskArrayType(NMT_SINGLE);
				
				goto good;
			} 
				break;
			case NDAS_DIB_DISK_TYPE_AGGREGATION_FIRST:
			case NDAS_DIB_DISK_TYPE_AGGREGATION_SECOND:
			case NDAS_DIB_DISK_TYPE_MIRROR_MASTER:
			case NDAS_DIB_DISK_TYPE_MIRROR_SLAVE:
			{
				/** Create a new DIB ver. 2 */
				
				NDAS_DIB_V1	*backupDIB;
				
				backupDIB = (NDAS_DIB_V1 *)IOMalloc(sizeof(NDAS_DIB_V1));
				if(backupDIB == NULL) {
					break;
				}
				
				memcpy(backupDIB, dib_v1, sizeof(NDAS_DIB_V1));
				
				dib_v2 = (NDAS_DIB_V2*)sectorBuffer;
				
				bzero((void *)dib_v2, sizeof(NDAS_DIB_V2));
				
				dib_v2->Signature = NDAS_DIB_SIGNATURE_V2;
				dib_v2->MajorVersion = NDASSwap32HostToLittle(NDAS_DIB_VERSION_MAJOR_V2);
				dib_v2->MinorVersion = NDASSwap32HostToLittle(NDAS_DIB_VERSION_MINOR_V2);
				dib_v2->sizeXArea = NDASSwap64HostToLittle(NDAS_BLOCK_SIZE_XAREA);
				dib_v2->iSectorsPerBit = 0;    /* No backup information */
				/* In case of mirror, use primary disk size */
				dib_v2->sizeUserSpace = NDASSwap64HostToLittle(NumberOfSectors() - NDAS_BLOCK_SIZE_XAREA);
				
				UNIT_DISK_LOCATION * pUnitDiskLocation0, * pUnitDiskLocation1;
				
				if (backupDIB->DiskType == NDAS_DIB_DISK_TYPE_MIRROR_MASTER ||
					backupDIB->DiskType == NDAS_DIB_DISK_TYPE_AGGREGATION_FIRST)
				{
					pUnitDiskLocation0 = & dib_v2->UnitDisks[0];
					pUnitDiskLocation1 = & dib_v2->UnitDisks[1];
				}
				else
				{
					pUnitDiskLocation0 = & dib_v2->UnitDisks[1];
					pUnitDiskLocation1 = & dib_v2->UnitDisks[0];
				}
				
				/** Store the MAC address into each UnitDisk */
				memcpy(& pUnitDiskLocation0->MACAddr, backupDIB->EtherAddress, sizeof(pUnitDiskLocation0->MACAddr));
				pUnitDiskLocation0->UnitNumber = backupDIB->UnitNumber;
				pUnitDiskLocation0->VID = NDAS_VID_GENERAL;
				
				memcpy(pUnitDiskLocation1->MACAddr, backupDIB->PeerAddress, sizeof(pUnitDiskLocation1->MACAddr));
				pUnitDiskLocation1->UnitNumber = backupDIB->UnitNumber;
				pUnitDiskLocation1->VID = NDAS_VID_GENERAL;
				
				dib_v2->nDiskCount = NDASSwap32HostToLittle(2);
				
				switch (backupDIB->DiskType)
				{
					case NDAS_DIB_DISK_TYPE_MIRROR_MASTER:
						dib_v2->iMediaType = NDASSwap32HostToLittle(NMT_MIRROR);
						dib_v2->iSequence = 0;
						break;
						
					case NDAS_DIB_DISK_TYPE_MIRROR_SLAVE:
						dib_v2->iMediaType = NDASSwap32HostToLittle(NMT_MIRROR);
						dib_v2->iSequence = NDASSwap32HostToLittle(1);
						break;
						
					case NDAS_DIB_DISK_TYPE_AGGREGATION_FIRST:
						dib_v2->iMediaType = NDASSwap32HostToLittle(NMT_AGGREGATE);
						dib_v2->iSequence = 0;
						break;
						
					case NDAS_DIB_DISK_TYPE_AGGREGATION_SECOND:
						dib_v2->iMediaType = NDASSwap32HostToLittle(NMT_AGGREGATE);
						dib_v2->iSequence = NDASSwap32HostToLittle(1);
						break;
						
					default:
						DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("checkDiskArrayType: Unknown disk type.\n"));

						setDiskArrayType(NMT_INVALID);
						
						IOFree(backupDIB, sizeof(*backupDIB));
						IOFree(sectorBuffer, blockSize());
						return false;
				}
				
				/* Make CRC codes */				
				SET_DIB_CRC(crc32_calc, *dib_v2);

				setDiskArrayType(NDASSwap32LittleToHost(dib_v2->iMediaType));
				
				memcpy((void *) &fDIB2, (void *)dib_v2, sizeof (*dib_v2));
				
				// Make dummy RMD for GUID.
				// 
				// Write RMD
				//
				NDAS_RAID_META_DATA RMD = { 0 };
				
				// Create RMD Info.
				RMD.Signature = NDAS_RAID_META_DATA_SIGNATURE;
				
				memcpy(&(RMD.RaidSetId.guid[0]),& dib_v2->UnitDisks[0], sizeof(UNIT_DISK_LOCATION));
				memcpy(&(RMD.RaidSetId.guid[sizeof(UNIT_DISK_LOCATION)]), & dib_v2->UnitDisks[1], sizeof(UNIT_DISK_LOCATION));
				
				SET_RMD_CRC(crc32_calc, RMD);

				memcpy(&fRMD, &RMD, sizeof(NDAS_RAID_META_DATA));
				
				// My DIB?			
				if ( memcmp(getDIBUnitDiskLocation(dib_v2, NDASSwap32LittleToHost(dib_v2->iSequence)),
							UnitDiskLocation(),
							7) != 0) {	// except VID.
										// Not Mine.
					DbgIOLog(DEBUG_MASK_RAID_WARNING, ("checkDiskArrayType: Not my DIB2.\n"));
					
					setRAIDFLAG(UNIT_DEVICE_RAID_FLAG_NDAS_ADDR_MISMATCH, true);				
				} else {
					setRAIDFLAG(UNIT_DEVICE_RAID_FLAG_NDAS_ADDR_MISMATCH, false);
				}

				IOFree(backupDIB, sizeof(*backupDIB));

				goto good;
			}
			case NDAS_DIB_DISK_TYPE_INVALID:
			{
				// Since the DIB_v2 is invalid, It is single Disk.
				break;
			}
			default:
			{
				DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("checkDiskArrayType: DIB_v1 Unknown type %d \n", dib_v1->DiskType));

				setDiskArrayType(NMT_INVALID);
				
				IOFree(sectorBuffer, blockSize());

				return false;
			}
				break;
		}		
	}	
	
	DbgIOLog(DEBUG_MASK_NDAS_INFO, ("checkDiskArrayType: No DIB_v1 and DIB_v2. So it's Single disk.\n"));
	
	setDiskArrayType(NMT_SINGLE);
	
good:
		
	if (kNDASUnitStatusFailure == Status()) {
		setStatus(kNDASUnitStatusDisconnected);
	}

	IOFree(sectorBuffer, blockSize());

	return true;
}

void print_RMD(PNDAS_RAID_META_DATA rmd) {
	
	if (NULL == rmd) {
		return;
	}
	
	// Signature.
	DbgIOLogNC(DEBUG_MASK_NDAS_ERROR, ("print_RMD: Signature 0x%llx\n", NDASSwap64LittleToHost(rmd->Signature)));
	
	if (NDAS_RAID_META_DATA_SIGNATURE == rmd->Signature
        && IS_RMD_CRC_VALID(crc32_calc, *rmd))
	{
		// GUID.
		DbgIOLogNC(DEBUG_MASK_NDAS_ERROR, ("print_RMD: GUID %2x-%2x-%2x-%2x-%2x-%2x-%2x-%2x-%2x-%2x-%2x-%2x-%2x-%2x-%2x-%2x\n", 
										   rmd->RaidSetId.guid[0], rmd->RaidSetId.guid[1], rmd->RaidSetId.guid[2], rmd->RaidSetId.guid[3], 
										   rmd->RaidSetId.guid[4], rmd->RaidSetId.guid[5], rmd->RaidSetId.guid[6], rmd->RaidSetId.guid[7],
										   rmd->RaidSetId.guid[8], rmd->RaidSetId.guid[9], rmd->RaidSetId.guid[10], rmd->RaidSetId.guid[11],
										   rmd->RaidSetId.guid[12], rmd->RaidSetId.guid[13], rmd->RaidSetId.guid[14], rmd->RaidSetId.guid[15]));
		// uiUSN.
		DbgIOLogNC(DEBUG_MASK_NDAS_ERROR, ("print_RMD: uiUSN %d\n", NDASSwap32LittleToHost(rmd->uiUSN)));
		
		// state.
		DbgIOLogNC(DEBUG_MASK_NDAS_ERROR, ("print_RMD: state %d\n", NDASSwap32LittleToHost(rmd->state)));
			
		// Config ID.
		DbgIOLogNC(DEBUG_MASK_NDAS_ERROR, ("print_RMD: Config ID %2x-%2x-%2x-%2x-%2x-%2x-%2x-%2x-%2x-%2x-%2x-%2x-%2x-%2x-%2x-%2x\n", 
										   rmd->ConfigSetId.guid[0], rmd->ConfigSetId.guid[1], rmd->ConfigSetId.guid[2], rmd->ConfigSetId.guid[3], 
										   rmd->ConfigSetId.guid[4], rmd->ConfigSetId.guid[5], rmd->ConfigSetId.guid[6], rmd->ConfigSetId.guid[7],
										   rmd->ConfigSetId.guid[8], rmd->ConfigSetId.guid[9], rmd->ConfigSetId.guid[10], rmd->ConfigSetId.guid[11],
										   rmd->ConfigSetId.guid[12], rmd->ConfigSetId.guid[13], rmd->ConfigSetId.guid[14], rmd->ConfigSetId.guid[15]));
		
		// Units.
		for (int count = 0; count < NDAS_MAX_UNITS_IN_V2; count++) {
			if (rmd->UnitMetaData[count].UnitDeviceStatus != 0) {
				DbgIOLogNC(DEBUG_MASK_NDAS_ERROR, ("print_RMD: Unit[%d] Device Index %d, Status %d\n", 
											   count, NDASSwap16LittleToHost(rmd->UnitMetaData[count].iUnitDeviceIdx), rmd->UnitMetaData[count].UnitDeviceStatus
											));
			}
		}
	}
}

bool	com_ximeta_driver_NDASUnitDevice::checkRMD()
{
	uint64_t				location;
    NDAS_RAID_META_DATA rmd;
	   
    /*** Scrutiny of x area of a newly attached disk */
	
    /**
		*  1. Read an NDAS_DIB_V2 structure from the NDAS Device at
     *     NDAS_BLOCK_LOCATION_DIB_V2.
     */
	
	//ND_LOG("1st..\n");
    location = NumberOfSectors() + NDAS_BLOCK_LOCATION_RMD;
	
	DbgIOLog(DEBUG_MASK_RAID_INFO, ("checkRMD: Location %lld\n", location));
	
    if (false == readSectorsOnce(location, 1, (char *)&rmd))
    {
        DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("checkRMD: Reading RMD area is failed.\n"));
		
        return false;
    }
	
//	print_RMD(& rmd);
	
	if (NDAS_RAID_META_DATA_SIGNATURE == rmd.Signature
		&& IS_RMD_CRC_VALID(crc32_calc, rmd))
    {
	
		memcpy(&fRMD, &rmd, sizeof(NDAS_RAID_META_DATA));
		
		return true;
	}		
	
	DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("checkRMD: Invalid RMD!!!!!!!\n"));

	return false;
}

void	com_ximeta_driver_NDASUnitDevice::setUpperUnitDevice(com_ximeta_driver_NDASUnitDevice *upperDevice)
{	
	if (fUpperUnitDevice) {
		fUpperUnitDevice->release();		
	}
	
	if (upperDevice) {		
		upperDevice->retain();
	}
	
	fUpperUnitDevice = upperDevice;
}

com_ximeta_driver_NDASUnitDevice *com_ximeta_driver_NDASUnitDevice::UpperUnitDevice()
{
	return fUpperUnitDevice;
}

/*
void com_ximeta_driver_NDASUnitDevice::setDIB2(PNDAS_DIB_V2 dib2)
{
	memcpy(&fDIB2, dib2, sizeof(NDAS_DIB_V2));
}
*/
PNDAS_DIB_V2 com_ximeta_driver_NDASUnitDevice::DIB2()
{
	return &fDIB2;
}

PNDAS_RAID_META_DATA com_ximeta_driver_NDASUnitDevice::RMD()
{
	return &fRMD;
}

bool	com_ximeta_driver_NDASUnitDevice::bind(uint32_t								type, 
											   uint32_t								sequence,
											   uint32_t								NumberOfUnitDevices,
											   com_ximeta_driver_NDASLogicalDevice	*LogicalDevices[],
											   NDAS_uuid_t							raidID,
											   NDAS_uuid_t							configID,
											   bool									Sync)
{
	DbgIOLog(DEBUG_MASK_RAID_ERROR, ("bind: Entered.\n"));
	
	char			buffer[SECTOR_SIZE];
	PNDAS_DIB_V2	pDIB_v2 = NULL;
	uint64_t			userSize = 0;
	uint64_t			location;
	bool			result;
	int				count;
	uint32_t			sectorsPerBit = 128;
	
	//
	// Connect RW connection first.
	//
	
	
	//
	// Check RO count.
	//
	
	
	
	// Calc userSize.
	switch(type) {
		case NMT_AGGREGATE:
		{
			userSize = LogicalDevices[sequence]->UnitDevice()->Capacity();
		}
			break;
		case NMT_RAID0R2:
		{
			// Search Smallest Size.
			for (count = 0; count < NumberOfUnitDevices; count++) {
				if (count == 0) {
					userSize = LogicalDevices[count]->UnitDevice()->Capacity();
				} else {
					if (userSize > LogicalDevices[count]->UnitDevice()->Capacity()) {
						userSize = LogicalDevices[count]->UnitDevice()->Capacity();
					}
				}
			}			
		}
			break;
		case NMT_RAID1R3:
		{
			// Search Smallest Size.
			for (count = 0; count < NumberOfUnitDevices; count++) {
				if (count == 0) {
					userSize = LogicalDevices[count]->UnitDevice()->Capacity();
				} else {
					if (userSize > LogicalDevices[count]->UnitDevice()->Capacity()) {
						userSize = LogicalDevices[count]->UnitDevice()->Capacity();
					}
				}
			}
			
			//
			// Reduce user space by 0.5%. HDDs with same giga size labels have different sizes.
			//		Sometimes, it is up to 7.5% difference due to 1k !=1000.
			//		Once, I found Maxter 160G HDD size - Samsung 160G HDD size = 4G. 
			//		Even with same maker's HDD with same gig has different sector size.
			//	To do: Give user a option to select this margin.
			//
			userSize = userSize * 199/ 200;

			// Increase sectors per bit if user space is larger than default maximum.
			for(count = 16; TRUE; count++)
			{
				if(userSize  <= ( ((uint64_t)1 << count)) * NDAS_BLOCK_SIZE_BITMAP * NDAS_BIT_PER_OOS_BITMAP_BLOCK ) // 512 GB : 128 SPB
				{
					// Sector per bit is big enough to be covered by bitmap
					sectorsPerBit = 1 << count;
					break;
				}
				DbgIOASSERT(count <= 32); // protect overflow
			}
			
			// Trim user space that is out of bitmap align.
			userSize -= userSize % sectorsPerBit;
		}
			break;
	}

	// Write Bitmap.
	NDAS_OOS_BITMAP_BLOCK bitmapBuffer;
	int	loop;
	
	for (loop = 0; loop < NDAS_BLOCK_SIZE_BITMAP; loop++) {

		if (0 == sequence
			&& Sync) {
			// Set Dirty bits.
			memset(bitmapBuffer.Bits, 0xff, NDAS_BYTE_PER_OOS_BITMAP_BLOCK);
		} else {
			// Clear Bitmap.
			memset(bitmapBuffer.Bits, 0, NDAS_BYTE_PER_OOS_BITMAP_BLOCK);
		}
		
		bitmapBuffer.SequenceNumHead = bitmapBuffer.SequenceNumTail = NDASSwap64HostToLittle(1);
		
		location = NumberOfSectors() + NDAS_BLOCK_LOCATION_BITMAP + loop;
		
		DbgIOLog(DEBUG_MASK_RAID_ERROR, ("bind: Write Bitmap Location %lld\n", location));
		
		result = writeSectorsOnce(location, 1, (char*)&bitmapBuffer);
		if (false == result) {
			DbgIOLog(DEBUG_MASK_RAID_ERROR, ("bind: Can't write bitmap.\n"));
			
			return false;
		}
		
	}
			
	// Create DIB.
	
	pDIB_v2 = (PNDAS_DIB_V2)buffer;
	memset(pDIB_v2, 0, sizeof(NDAS_DIB_V2));
	
	pDIB_v2->Signature = NDAS_DIB_SIGNATURE_V2;
	pDIB_v2->MajorVersion = NDASSwap32HostToLittle(NDAS_DIB_VERSION_MAJOR_V2);
	pDIB_v2->MinorVersion = NDASSwap32HostToLittle(NDAS_DIB_VERSION_MINOR_V2);
	pDIB_v2->sizeXArea = NDASSwap64HostToLittle(NDAS_BLOCK_SIZE_XAREA);
	pDIB_v2->sizeUserSpace = NDASSwap64HostToLittle(userSize);
	pDIB_v2->iSectorsPerBit = NDASSwap32HostToLittle(sectorsPerBit);
	pDIB_v2->iMediaType = NDASSwap32HostToLittle(type);
	
	if (NMT_RAID1R3 != type
		|| 2 == NumberOfUnitDevices) {
		pDIB_v2->nDiskCount = NDASSwap32HostToLittle(NumberOfUnitDevices);
	} else {
		pDIB_v2->nDiskCount = NDASSwap32HostToLittle(2);
		pDIB_v2->nSpareCount = NDASSwap32HostToLittle(NumberOfUnitDevices - 2);
	}
	
	pDIB_v2->iSequence = NDASSwap32HostToLittle(sequence);
	
	for (int count = 0; count < NumberOfUnitDevices; count++) {
		memcpy(&(pDIB_v2->UnitDisks[count]), LogicalDevices[count]->UnitDevice()->UnitDiskLocation(), sizeof(uint64_t));
	}

	SET_DIB_CRC(crc32_calc, *pDIB_v2);

	// Write DIB.
	location = NumberOfSectors() + NDAS_BLOCK_LOCATION_DIB_V2;
	
	DbgIOLog(DEBUG_MASK_RAID_ERROR, ("bind: Write DIB 2 Location %lld\n", location));
	
	result = writeSectorsOnce(location, 1,  (char *)pDIB_v2);
	
	if (false == result) {
		return false;
	}
	
	// 
	// Write RMD
	//
	PNDAS_RAID_META_DATA pRMD = (PNDAS_RAID_META_DATA)buffer;
	memset(pRMD, 0, sizeof(NDAS_RAID_META_DATA));

	// Create RMD Info.
	pRMD->Signature = NDAS_RAID_META_DATA_SIGNATURE;
	memcpy(&pRMD->RaidSetId, raidID, 16);
	memcpy(&pRMD->ConfigSetId, configID, 16);
/*
	uint32_t	tempState;
	
	if (0 == sequence && Sync) {
		tempState = NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED;
	} else {
		tempState = 0;
	}	
	RMD.state = NDASSwap32HostToLittle(tempState);
*/	
	pRMD->uiUSN = NDASSwap32HostToLittle(1);
	
	for (count = 0; count < NumberOfUnitDevices; count++) {
		pRMD->UnitMetaData[count].iUnitDeviceIdx = NDASSwap16HostToLittle(count);
		
		if (NMT_RAID1R3 == type
			&& count >= 2) {
			pRMD->UnitMetaData[count].UnitDeviceStatus = NDAS_UNIT_META_BIND_STATUS_SPARE;
		} else {
			pRMD->UnitMetaData[count].UnitDeviceStatus = 0;
		}
	}

	if (Sync) {
		pRMD->UnitMetaData[1].UnitDeviceStatus = NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED;
	}
	
	SET_RMD_CRC(crc32_calc, *pRMD);
	
	// Write RMD.
	location = NumberOfSectors() + NDAS_BLOCK_LOCATION_RMD;
	
	DbgIOLog(DEBUG_MASK_RAID_ERROR, ("bind: Write RMD Location %lld\n", location));
	
	result = writeSectorsOnce(location, 1,  (char *)pRMD);
	if (false == result) {
		return false;
	}
		
	// Invaildate DIB V1.
	PNDAS_DIB_V1 pDIB_v1 = (PNDAS_DIB_V1)buffer;
	
	memset(pDIB_v1, 0, sizeof(NDAS_DIB_V1));
	
	if (NMT_AGGREGATE == type 
		&& 2 == NumberOfUnitDevices) {
		
		pDIB_v1->Signature = NDAS_DIB_SIGNATURE_V1;
		pDIB_v1->MajorVersion = NDAS_DIB_VERSION_MAJOR_V1;
		pDIB_v1->MinorVersion = NDAS_DIB_VERSION_MINOR_V1;
		
		if (0 == sequence) {
			pDIB_v1->DiskType = NDAS_DIB_DISK_TYPE_AGGREGATION_FIRST;
			
			memcpy(pDIB_v1->EtherAddress, LogicalDevices[0]->UnitDevice()->UnitDiskLocation()->MACAddr, 6);
			pDIB_v1->UnitNumber = LogicalDevices[0]->UnitDevice()->UnitDiskLocation()->UnitNumber;
			memcpy(pDIB_v1->PeerAddress, LogicalDevices[1]->UnitDevice()->UnitDiskLocation()->MACAddr, 6);
			pDIB_v1->PeerUnitNumber = LogicalDevices[1]->UnitDevice()->UnitDiskLocation()->UnitNumber;

		} else {
			pDIB_v1->DiskType = NDAS_DIB_DISK_TYPE_AGGREGATION_SECOND;
			
			memcpy(pDIB_v1->EtherAddress, LogicalDevices[1]->UnitDevice()->UnitDiskLocation()->MACAddr, 6);
			pDIB_v1->UnitNumber = LogicalDevices[1]->UnitDevice()->UnitDiskLocation()->UnitNumber;
			memcpy(pDIB_v1->PeerAddress, LogicalDevices[0]->UnitDevice()->UnitDiskLocation()->MACAddr, 6);
			pDIB_v1->PeerUnitNumber = LogicalDevices[0]->UnitDevice()->UnitDiskLocation()->UnitNumber;			
		}
		
	} else {
		NDAS_DIB_V1_INVALIDATE(*pDIB_v1);
	}
	
	location = NumberOfSectors() + NDAS_BLOCK_LOCATION_DIB_V1;
	
	DbgIOLog(DEBUG_MASK_RAID_ERROR, ("bind: location of DIB_v1 %lld\n", location));
	
	if(!writeSectorsOnce(location, 1, (char *)pDIB_v1)) {
		DbgIOLog(DEBUG_MASK_RAID_ERROR, ("bind: Can't delete old DIB v1 to location %lld\n", location));		
	}
	
	if (false == Sync) {
		// Clear Partition table.
		memset(buffer, 0, SECTOR_SIZE);

		// Clear MBR
		if(!writeSectorsOnce(NDAS_BLOCK_LOCATION_MBR, 1, (char *)buffer)) {
			DbgIOLog(DEBUG_MASK_RAID_ERROR, ("bind: Can't Write MBR Location %lld\n", NDAS_BLOCK_LOCATION_MBR));
			return false;
		}

		// Clear Partition 1 for DOS.
		if(!writeSectorsOnce(NDAS_BLOCK_LOCATION_USER, 1, (char *)buffer)) {
			DbgIOLog(DEBUG_MASK_RAID_ERROR, ("bind: Can't Write Partition 1 Location %lld\n", NDAS_BLOCK_LOCATION_USER));
			return false;
		}		
		
		// For Apple.
		for (int count = 1; count < 10; count++) {
			if(!writeSectorsOnce(count, 1, (char *)buffer)) {
				DbgIOLog(DEBUG_MASK_RAID_ERROR, ("bind: Can't Write Apple Partition Info Location %d\n", count));
				return false;
			}		
		}
	}
	
	return result;
}

bool	com_ximeta_driver_NDASUnitDevice::ReadBitMapFromUnitDevice(PNDAS_OOS_BITMAP_BLOCK pBitmap, uint32_t	BitMapSectorPoint)
{
	if (!pBitmap) {		
		DbgIOLog(DEBUG_MASK_RAID_ERROR, ("ReadBitMapFromUnitDevice: No buffer.\n"));

		return false;
	}
	
	// Read BitMap.				
	uint64_t	dirtyBitSector = NumberOfSectors() + NDAS_BLOCK_LOCATION_BITMAP;
				
	dirtyBitSector += BitMapSectorPoint;
	DbgIOLog(DEBUG_MASK_RAID_INFO, ("ReadBitMapFromUnitDevice: Sector %lld, dirtyBitSector %lld\n", NumberOfSectors(), dirtyBitSector));
	
	if(!readSectorsOnce(dirtyBitSector, 1, (char*)pBitmap)) {
		DbgIOLog(DEBUG_MASK_RAID_ERROR, ("ReadBitMapFromUnitDevice: Read BitMap failed.\n"));
		return false;		
	}	
	
	// Validate Bitmap block.
	if (pBitmap->SequenceNumHead != pBitmap->SequenceNumTail) {
		// Corrupted bitmap!!!
		DbgIOLog(DEBUG_MASK_RAID_ERROR, ("ReadBitMapFromUnitDevice: Corrupted bitmap.\n"));

		return false;
	}
	
	return true;
}

bool	com_ximeta_driver_NDASUnitDevice::WriteBitMapToUnitDevice(PNDAS_OOS_BITMAP_BLOCK pBitmap, uint32_t	BitMapSectorPoint) {

	if (!pBitmap) {
		return false;
	}

	// Write BitMap.				
	uint64_t	dirtyBitSector = NumberOfSectors() + NDAS_BLOCK_LOCATION_BITMAP;
				
	dirtyBitSector += BitMapSectorPoint;
	DbgIOLog(DEBUG_MASK_RAID_INFO, ("ReadBitMapFromUnitDevice: Sector %lld, dirtyBitSector %lld\n", NumberOfSectors(), dirtyBitSector));
	
	// Update Sequence.
	pBitmap->SequenceNumHead++;
	pBitmap->SequenceNumTail = pBitmap->SequenceNumHead;
	
	if(!writeSectorsOnce(dirtyBitSector, 1, (char*)pBitmap)) {
		DbgIOLog(DEBUG_MASK_RAID_ERROR, ("WriteBitMapToUnitDevice: Write BitMap failed.\n"));
		return false;		
	}	
	
	return true;
}

bool	com_ximeta_driver_NDASUnitDevice::IsInRunningStatus() 
{
	if (kNDASUnitConfigurationUnmount == Configuration()) {
		return false;
	}
	
	switch(Status()) {
		case kNDASUnitStatusNotPresent:
		case kNDASUnitStatusFailure:
		case kNDASUnitStatusDisconnected:
		case kNDASUnitStatusTryConnectRO:
		case kNDASUnitStatusTryConnectRW:
		case kNDASUnitStatusTryDisconnectRO:
		case kNDASUnitStatusTryDisconnectRW:
			return false;
	}
	
	return true;
}

void	com_ximeta_driver_NDASUnitDevice::setRAIDFLAG(uint32_t flag, bool value) {
	
	if (value) {
		fRAIDFlags |= flag;
	} else {
		fRAIDFlags &= ~flag;
	}
}

bool	com_ximeta_driver_NDASUnitDevice::RAIDFlag(uint32_t flag) {
	if (flag & fRAIDFlags) {
		return true;
	} else {
		return false;
	}
}

void	com_ximeta_driver_NDASUnitDevice::setSequence(uint32_t sequence)
{
	fSequence = sequence;
}

uint32_t	com_ximeta_driver_NDASUnitDevice::Sequence()
{
	return fSequence;
}

void	com_ximeta_driver_NDASUnitDevice::setSubUnitIndex(uint32_t index)
{
	fSubUnitIndex = index;
}

uint32_t	com_ximeta_driver_NDASUnitDevice::SubUnitIndex()
{
	return fSubUnitIndex;
}

