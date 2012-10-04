/*
 *  NDASRAIDUnitDevice.h
 *  NDASFamily
 *
 *  Created by Junggyun Ahn on 05. 11. 29.
 *  Copyright 2005 XIMETA, Inc. All rights reserved.
 *
 */

#include <IOKit/IOService.h>

#define NDAS_RAID_UNIT_DEVICE_CLASS	"com_ximeta_driver_NDASRAIDUnitDevice"

#include "NDASUnitDevice.h"
#include "NDASBusEnumeratorUserClientTypes.h"
#include "LanScsiDiskInformationBlock.h"

class	com_ximeta_driver_NDASCommand;
class	com_ximeta_driver_NDASRAIDUnitDeviceController;

typedef struct _WrittenSector {
	UInt64	location;
	UInt32	sectors;
} WrittenSector, *PWrttenSector;

class com_ximeta_driver_NDASRAIDUnitDevice : public com_ximeta_driver_NDASUnitDevice
{
	OSDeclareDefaultStructors(com_ximeta_driver_NDASRAIDUnitDevice)
	
private:
	
	bool								fWillTerminate;

	UInt32								fIndex;

	UInt32								fType;
				
	bool								fConnected;
	
	UInt32								fCountOfUnitDevices;
	UInt32								fCountOfSpareDevices;
	
	com_ximeta_driver_NDASRAIDUnitDeviceController	*fController;
	
	UInt32								fSenseKey;

	com_ximeta_driver_NDASUnitDevice	*fUnitDeviceArray[NDAS_MAX_UNITS_IN_V2];
	UInt32								fSubUnitStatus[NDAS_MAX_UNITS_IN_V2];
	UInt32								fSubUnitType[NDAS_MAX_UNITS_IN_V2];
	
	com_ximeta_driver_NDASCommand		*fNDASCommand;
	
	UInt32	fRAIDBlockSize;				// By sector
	
	char								fName[MAX_NDAS_NAME_SIZE];
	
	UInt32								fNRRWHost;
	UInt32								fNRROHost;
		
	GUID								fConfigID;
	UInt32								fRMDUSN;

	UInt32								fRAIDStatus;
	
	UInt64								fSectorOfRecovery;
	
	char								*fBufferForRecovery;
	UInt32								fSizeofBufferForRecovery;
	
	char								*fDirtyBitmap;
		
	bool								fDirty;
	
	bool								fInRecoveryProcess;
	
	IOLock								*lock;
	
	bool								fPreProcessDone;

	com_ximeta_driver_NDASCommand*	fPnpCommand;

	void	setConfigurationOfSubUnits(UInt32 configuration);

public:

	void	setType(UInt32 type);
	
	void	setIndex(UInt32 index);
	UInt32	Index();
	
	bool	IsWorkable();
			
	bool	IsConnected();
	void	setConnected(bool connected);
		
	void	setCountOfUnitDevices(UInt32 count);
	UInt32	CountOfUnitDevices();
	
	UInt32	CountOfRegistetedUnitDevices();
	
	UInt64	getNumberOfSectors();
	
//	UInt32	getState();
	UInt32	getNRRWHosts();
	UInt32	getNRROHosts();
	
	void	setConfiguration(UInt32 configuration);
	
	void	setController(com_ximeta_driver_NDASRAIDUnitDeviceController *controller);
	com_ximeta_driver_NDASRAIDUnitDeviceController *Controller();
	
	void	setCountOfSpareDevices(UInt32 count);
	UInt32	CountOfSpareDevices();
		
	void	setSenseKey(UInt32 senseKey);
	UInt32	senseKey();
	
	com_ximeta_driver_NDASUnitDevice	*UnitDeviceAtIndex(UInt32 index);
	
	void	setNDASCommand(com_ximeta_driver_NDASCommand *command);
	com_ximeta_driver_NDASCommand *NDASCommand();
	
	void	completeRecievedCommand();
			
	UInt32	RAIDBlocksPerSector();
	
	void	setConfigID(PGUID);
	PGUID	ConfigID();

	void	setRMDUSN(UInt32 USN);
	UInt32	RMDUSN();
		
	// IOService
	virtual bool init(OSDictionary *dictionary = 0);
	virtual void free(void);
	virtual IOService *probe(IOService *provider, SInt32 *score);
	virtual bool start(IOService *provider);
	virtual void stop(IOService *provider);
	virtual IOReturn message( 
							  UInt32	mess, 
							  IOService	* provider,
							  void		* argument 
							  );
	
	virtual bool willTerminate(
							   IOService * provider,
							   IOOptionBits options );
	
	// NDASUnitDevice
	virtual UInt32 Type();
	
	virtual bool	IsWritable();
	
	virtual	bool readSectorsOnce(UInt64 location, UInt32 sectors, char *buffer);
	virtual bool writeSectorsOnce(UInt64 location, UInt32 sectors, char *buffer);
	virtual	bool processSRBCommand(com_ximeta_driver_NDASCommand *command);
	virtual void completeCommand(com_ximeta_driver_NDASCommand *command);
	virtual UInt32 maxRequestSectors();
	virtual UInt32	updateStatus();
	virtual UInt32	blocksize();
	virtual bool	sendNDASFamilyIOMessage(UInt32 type, void * argument = 0, vm_size_t argSize = 0 );

	bool	setUnitDevice(SInt32 index, com_ximeta_driver_NDASUnitDevice *device);
	
	bool	unbind();
	bool	rebind();
	bool	convertBindType(UInt32 newType);

	void	setName(char *Name);

	void	setUnitDevices(PNDAS_DIB_V2 dib2, PNDAS_RAID_META_DATA rmd);
	
	void	setStatusOfSubUnit(UInt32 index, UInt32 status);
	UInt32	StatusOfSubUnit(UInt32 index);
	
	void	setTypeOfSubUnit(UInt32 index, UInt32 type);
	SInt32	TypeOfSubUnit(UInt32 index);

	void	setRAIDStatus(UInt32 status);
	UInt32	RAIDStatus();
	void	updateRAIDStatus();
		
	bool	IsDirtyBitSet(UInt64 sector, bool *dirty);
	bool	SetBitmap(UInt64 location, UInt32 sectors, bool dirty);
	bool	ClearBitmapFromFirstSector(UInt64 location); 
	bool	UpdateBitMapBit(UInt64 location, UInt32 sectors, bool dirty);
	
	void	recovery();
	
	bool	allocDirtyBitmap();
	char*	dirtyBitmap();
	
	void	setDirty(bool newValue);
	bool	IsDirty();
	
	bool	postProcessingforConnectRW();
	bool	preProcessingforDisconnectRW();
	
	void	receivePnPMessage();
	void	noPnPMessage();
	void	completePnPMessage(com_ximeta_driver_NDASCommand *command);
	
	bool	ReplaceToSpare();
};