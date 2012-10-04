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
class	com_ximeta_driver_NDASChangeSettingCommand;
class	com_ximeta_driver_NDASSCSICommand;
class	com_ximeta_driver_NDASRAIDUnitDeviceController;

typedef struct _WrittenSector {
	uint64_t	location;
	uint32_t	sectors;
} WrittenSector, *PWrttenSector;

class com_ximeta_driver_NDASRAIDUnitDevice : public com_ximeta_driver_NDASUnitDevice
{
	OSDeclareDefaultStructors(com_ximeta_driver_NDASRAIDUnitDevice)
	
private:
	
	bool								fWillTerminate;

	uint32_t								fIndex;

	uint32_t								fType;
				
	bool								fConnected;
	
	uint32_t								fCountOfUnitDevices;
	uint32_t								fCountOfSpareDevices;
	
	com_ximeta_driver_NDASRAIDUnitDeviceController	*fController;
	
	uint32_t								fSenseKey;

	com_ximeta_driver_NDASUnitDevice	*fUnitDeviceArray[NDAS_MAX_UNITS_IN_V2];
	uint32_t								fSubUnitStatus[NDAS_MAX_UNITS_IN_V2];
	uint32_t								fSubUnitType[NDAS_MAX_UNITS_IN_V2];
	
	com_ximeta_driver_NDASSCSICommand		*fNDASCommand;
	
	uint32_t	fRAIDBlockSize;				// By sector
	
	char								fName[MAX_NDAS_NAME_SIZE];
	
	uint32_t								fNRRWHost;
	uint32_t								fNRROHost;
		
	GUID								fConfigID;
	uint32_t								fRMDUSN;

	uint32_t								fRAIDStatus;
	
	uint64_t								fSectorOfRecovery;
	
	char								*fBufferForRecovery;
	uint32_t								fSizeofBufferForRecovery;
	
	char								*fDirtyBitmap;
		
	bool								fDirty;
	
	bool								fInRecoveryProcess;
	
	IOLock								*lock;
	
	bool								fPreProcessDone;

	com_ximeta_driver_NDASCommand*	fPnpCommand;

	void	setConfigurationOfSubUnits(uint32_t configuration);

public:

	void	setType(uint32_t type);
	
	void	setIndex(uint32_t index);
	uint32_t	Index();
	
	bool	IsWorkable();
			
	bool	IsConnected();
	void	setConnected(bool connected);
		
	void	setCountOfUnitDevices(uint32_t count);
	uint32_t	CountOfUnitDevices();
	
	uint32_t	CountOfRegistetedUnitDevices();
	
	uint64_t	getNumberOfSectors();
	
//	uint32_t	getState();
	uint32_t	getNRRWHosts();
	uint32_t	getNRROHosts();
	
	void	setConfiguration(uint32_t configuration);
	
	void	setController(com_ximeta_driver_NDASRAIDUnitDeviceController *controller);
	com_ximeta_driver_NDASRAIDUnitDeviceController *Controller();
	
	void	setCountOfSpareDevices(uint32_t count);
	uint32_t	CountOfSpareDevices();
		
	void	setSenseKey(uint32_t senseKey);
	uint32_t	senseKey();
	
	com_ximeta_driver_NDASUnitDevice	*UnitDeviceAtIndex(uint32_t index);
	
	void	setNDASCommand(com_ximeta_driver_NDASSCSICommand *command);
	com_ximeta_driver_NDASSCSICommand *NDASCommand();
	
	void	completeRecievedCommand();
			
	uint32_t	RAIDBlocksPerSector();
	
	void	setConfigID(PGUID);
	PGUID	ConfigID();

	void	setRMDUSN(uint32_t USN);
	uint32_t	RMDUSN();
		
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
	virtual uint32_t Type();
	
	virtual bool	IsWritable();
	
	virtual	bool readSectorsOnce(uint64_t location, uint32_t sectors, char *buffer);
	virtual bool writeSectorsOnce(uint64_t location, uint32_t sectors, char *buffer);
	virtual	bool processSRBCommand(com_ximeta_driver_NDASSCSICommand *command);
	virtual void completeCommand(com_ximeta_driver_NDASSCSICommand *command);
	virtual uint32_t maxRequestSectors();
	virtual uint32_t	updateStatus();
	virtual bool	sendNDASFamilyIOMessage(UInt32 type, void * argument = 0, vm_size_t argSize = 0 );

	bool	setUnitDevice(int32_t index, com_ximeta_driver_NDASUnitDevice *device);
	
	bool	unbind();
	bool	rebind();
	bool	convertBindType(uint32_t newType);

	uint32_t	blockSize();

	void	setName(char *Name);

	void	setUnitDevices(PNDAS_DIB_V2 dib2, PNDAS_RAID_META_DATA rmd);
	
	void	setStatusOfSubUnit(uint32_t index, uint32_t status);
	uint32_t	StatusOfSubUnit(uint32_t index);
	
	void	setTypeOfSubUnit(uint32_t index, uint32_t type);
	SInt32	TypeOfSubUnit(uint32_t index);

	void	setRAIDStatus(uint32_t status);
	uint32_t	RAIDStatus();
	void	updateRAIDStatus();
		
	bool	IsDirtyBitSet(uint64_t sector, bool *dirty);
	bool	SetBitmap(uint64_t location, uint32_t sectors, bool dirty);
	bool	ClearBitmapFromFirstSector(uint64_t location); 
	bool	UpdateBitMapBit(uint64_t location, uint32_t sectors, bool dirty);
	
	void	recovery();
	
	bool	allocDirtyBitmap();
	char*	dirtyBitmap();
	
	void	setDirty(bool newValue);
	bool	IsDirty();
	
	bool	postProcessingforConnectRW();
	bool	preProcessingforDisconnectRW();
	
	void	receivePnPMessage();
	void	noPnPMessage();
	void	completeChangeSettingMessage(com_ximeta_driver_NDASChangeSettingCommand *command);
	
	bool	ReplaceToSpare();
};