/*
 *  NDASUnitDevice.h
 *  NDASFamily
 *
 *  Created by Jung gyun Ahn on 05. 11. 29.
 *  Copyright 2005 XiMeta, Inc. All rights reserved.
 *
 */

#ifndef _NDAS_UNIT_DEVICE_H_
#define _NDAS_UNIT_DEVICE_H_

#include <IOKit/IOService.h>

//#include <uuid/uuid.h>

#include "LanScsiDiskInformationBlock.h"
#include "NDASID.h"

#include "Utilities.h"

class com_ximeta_driver_NDASCommand;
class com_ximeta_driver_NDASLogicalDevice;
class com_ximeta_driver_NDASRAIDUnitDevice;

//
//	RAID Flags.
//
#define UNIT_DEVICE_RAID_FLAG_DIB_VERSION_MISMATCH	0x00000001
#define UNIT_DEVICE_RAID_FLAG_NDAS_ADDR_MISMATCH	0x00000002
#define UNIT_DEVICE_RAID_FLAG_BAD_RMD				0x00000004

class com_ximeta_driver_NDASUnitDevice : public IOService
{
	OSDeclareAbstractStructors(com_ximeta_driver_NDASUnitDevice)
	
private:
	// GUID for Single Unit Device.
	//		00-00-00-00-00-00-00-00-eth0-eth1-eth2-eth3-eth4-eth5-eth6-unit#-00
	// GUID for RAID Unit Device.
	//		Stored in the RMD.
	GUID	fGUID;
	
	UNIT_DISK_LOCATION	fUnitDiskLocation;
	
	UInt32	fConfiguration;
	UInt32	fStatus;
	
	UInt64	fCapacity;
	UInt64	fNumberOfSectors;
	
	UInt32	fDiskArrayType;
	
	SInt32	fNRRWHosts;
	SInt32	fNRROHosts;
	
	com_ximeta_driver_NDASUnitDevice	*fUpperUnitDevice;
	
	NDAS_DIB_V2				fDIB2;
	NDAS_RAID_META_DATA		fRMD;
	
	UInt32					fRAIDFlags;

	bool					fConnected;
	
	UInt32					fSequence;			// Vaild if it is sub Unit of RAID.
	UInt32					fSubUnitIndex;		// Vaild if it is sub Unit of RAID.
		
public:

	// IOObject.
	virtual bool init(OSDictionary *dictionary = 0);

	// Virtual Unit Device.
	virtual UInt32	Type() = 0;
	virtual bool	IsWritable() = 0;
	virtual	bool	readSectorsOnce(UInt64 location, UInt32 sectors, char *buffer) = 0;
	virtual bool	writeSectorsOnce(UInt64 location, UInt32 sectors, char *buffer) = 0;
	virtual	bool	processSRBCommand(com_ximeta_driver_NDASCommand *command) = 0;
	virtual void	completeCommand(com_ximeta_driver_NDASCommand *command) = 0;
	virtual UInt32	maxRequestSectors() = 0;
	virtual UInt32	updateStatus() = 0;
	virtual UInt32	blocksize() = 0;
	virtual bool	sendNDASFamilyIOMessage(UInt32 type, void * argument = 0, vm_size_t argSize = 0 ) = 0;
	
	// Uint Device Only.
	void	setID(PGUID);
	PGUID	ID();

	void	setUnitDiskLocation(PUNIT_DISK_LOCATION);
	PUNIT_DISK_LOCATION	UnitDiskLocation();

	virtual void	setConfiguration(UInt32 configuration);
	UInt32	Configuration();

	void	setStatus(UInt32 status);
	UInt32	Status();

	void	setNRRWHosts(UInt32 hosts);
	UInt32	NRRWHosts();	

	void	setNRROHosts(UInt32 hosts);
	UInt32	NRROHosts();
	
	void	setCapacity(UInt64 sectors, UInt32 align);
	UInt64	Capacity();

	void	setNumberOfSectors(UInt64 NumberOfSectors);
	UInt64	NumberOfSectors();

	void	setDiskArrayType(UInt32 raidType);
	UInt32	DiskArrayType();
	
	void	setUpperUnitDevice(com_ximeta_driver_NDASUnitDevice *upperDevice);
	com_ximeta_driver_NDASUnitDevice *UpperUnitDevice();

	bool	IsConnected();
	void	setConnected(bool connected);
		
//	void setDIB2(PNDAS_DIB_V2 dib2);
	PNDAS_DIB_V2 DIB2();
	PNDAS_RAID_META_DATA RMD();
	
	bool	checkDiskArrayType();
	bool	checkRMD();
		
	bool	bind(UInt32	type, UInt32 sequence, UInt32	NumberOfUnitDevices, com_ximeta_driver_NDASLogicalDevice *LogicalDevices[], NDAS_uuid_t raidID, NDAS_uuid_t configID, bool sync);

	bool	ReadBitMapFromUnitDevice(PNDAS_OOS_BITMAP_BLOCK pBitmap, UInt32	BitMapSectorPoint);
	bool	WriteBitMapToUnitDevice(PNDAS_OOS_BITMAP_BLOCK pBitmap, UInt32	BitMapSectorPoint);

	bool	IsInRunningStatus();
	
	void	setRAIDFLAG(UInt32 flag, bool value);
	bool	RAIDFlag(UInt32 flag);
	
	// For the Sub Unit of RAID set.
	void	setSequence(UInt32 sequence);
	UInt32	Sequence();

	void	setSubUnitIndex(UInt32 index);
	UInt32	SubUnitIndex();

};

#endif
