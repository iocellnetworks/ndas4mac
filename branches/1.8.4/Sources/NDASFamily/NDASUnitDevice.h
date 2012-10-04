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
class com_ximeta_driver_NDASSCSICommand;
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
	
	uint32_t	fConfiguration;
	uint32_t	fStatus;
	
	uint64_t	fCapacity;
	uint64_t	fNumberOfSectors;
	
	uint32_t	fBlockSize;
	
	uint32_t	fDiskArrayType;
	
	SInt32	fNRRWHosts;
	SInt32	fNRROHosts;
	
	com_ximeta_driver_NDASUnitDevice	*fUpperUnitDevice;
	
	NDAS_DIB_V2				fDIB2;
	NDAS_RAID_META_DATA		fRMD;
	
	uint32_t					fRAIDFlags;

	bool					fConnected;
	
	uint32_t					fSequence;			// Vaild if it is sub Unit of RAID.
	uint32_t					fSubUnitIndex;		// Vaild if it is sub Unit of RAID.
		
public:

	// IOObject.
	virtual bool init(OSDictionary *dictionary = 0);

	// Virtual Unit Device.
	virtual uint32_t	Type() = 0;
	virtual bool	IsWritable() = 0;
	virtual	bool	readSectorsOnce(uint64_t location, uint32_t sectors, char *buffer) = 0;
	virtual bool	writeSectorsOnce(uint64_t location, uint32_t sectors, char *buffer) = 0;
	virtual	bool	processSRBCommand(com_ximeta_driver_NDASSCSICommand *command) = 0;
	virtual void	completeCommand(com_ximeta_driver_NDASSCSICommand *command) = 0;
	virtual uint32_t	maxRequestSectors() = 0;
	virtual uint32_t	updateStatus() = 0;
	virtual bool	sendNDASFamilyIOMessage(UInt32 type, void * argument = 0, vm_size_t argSize = 0 ) = 0;
	
	// Uint Device Only.
	void	setID(PGUID);
	PGUID	ID();

	void	setUnitDiskLocation(PUNIT_DISK_LOCATION);
	PUNIT_DISK_LOCATION	UnitDiskLocation();

	virtual void	setConfiguration(uint32_t configuration);
	uint32_t	Configuration();

	void	setStatus(uint32_t status);
	uint32_t	Status();

	void	setNRRWHosts(uint32_t hosts);
	uint32_t	NRRWHosts();	

	void	setNRROHosts(uint32_t hosts);
	uint32_t	NRROHosts();
	
	void	setCapacity(uint64_t sectors, uint32_t align);
	uint64_t	Capacity();

	void	setNumberOfSectors(uint64_t NumberOfSectors);
	uint64_t	NumberOfSectors();

	void	setBlockSize(uint32_t blockSize);
	uint32_t	blockSize();

	void	setDiskArrayType(uint32_t raidType);
	uint32_t	DiskArrayType();
	
	void	setUpperUnitDevice(com_ximeta_driver_NDASUnitDevice *upperDevice);
	com_ximeta_driver_NDASUnitDevice *UpperUnitDevice();

	bool	IsConnected();
	void	setConnected(bool connected);
		
//	void setDIB2(PNDAS_DIB_V2 dib2);
	PNDAS_DIB_V2 DIB2();
	PNDAS_RAID_META_DATA RMD();
	
	bool	checkDiskArrayType();
	bool	checkRMD();
		
	bool	bind(uint32_t	type, uint32_t sequence, uint32_t	NumberOfUnitDevices, com_ximeta_driver_NDASLogicalDevice *LogicalDevices[], NDAS_uuid_t raidID, NDAS_uuid_t configID, bool sync);

	bool	ReadBitMapFromUnitDevice(PNDAS_OOS_BITMAP_BLOCK pBitmap, uint32_t	BitMapSectorPoint);
	bool	WriteBitMapToUnitDevice(PNDAS_OOS_BITMAP_BLOCK pBitmap, uint32_t	BitMapSectorPoint);

	bool	IsInRunningStatus();
	
	void	setRAIDFLAG(uint32_t flag, bool value);
	bool	RAIDFlag(uint32_t flag);
	
	// For the Sub Unit of RAID set.
	void	setSequence(uint32_t sequence);
	uint32_t	Sequence();

	void	setSubUnitIndex(uint32_t index);
	uint32_t	SubUnitIndex();

};

#endif
