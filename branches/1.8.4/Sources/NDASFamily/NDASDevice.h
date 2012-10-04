/***************************************************************************
*
*  NDASDevice.h
*
*    NDAS Physical Device
*
*    Copyright (c) 2004 XiMeta, Inc. All rights reserved.
*
**************************************************************************/

#include <IOKit/IOService.h>

#include "lpx.h"
#include "LanScsiProtocol.h"

#include "NDASBusEnumeratorUserClientTypes.h"

#define NDAS_DEVICE_CLASS	"com_ximeta_driver_NDASDevice"

//
// Flow Control.
//
/*
#define FLOW_UNIT_SIZE					1
#define FLOW_NUMBER_OF_REQUEST_SIZE		120	// 8, 16, 24, ... 128
#define FLOW_MIN_REQUEST_SIZE			8	// 4KB
#define CURRENT_REQUEST_BLOCKS(_index_)	(FLOW_MIN_REQUEST_SIZE + (_index_ * FLOW_UNIT_SIZE))
*/

#define FLOW_UNIT_SIZE					1
#define FLOW_NUMBER_OF_REQUEST_SIZE		120		// 1KB to 64KB by 512bytes.
#define FLOW_MIN_REQUEST_SIZE			4096	// 1KB
#define CURRENT_REQUEST_BYTES(_index_, _blocksize_)		(FLOW_MIN_REQUEST_SIZE + (_index_ * _blocksize_))
#define CURRENT_REQUEST_BLOCKS(_index_, _blocksize_)	((FLOW_MIN_REQUEST_SIZE / _blocksize_) + _index_)

#define MAX_STABLE_COUNT				0

#define RETRANSMIT_TIME_NANO			200 * 1000 * 1000	// 200 ms

class com_ximeta_driver_NDASDeviceController;
class com_ximeta_driver_NDASCommand;
class com_ximeta_driver_NDASChangeSettingCommand;
class com_ximeta_driver_NDASPhysicalUnitDevice;

typedef struct _SCSICommand *PSCSICommand;
typedef struct _PNPCommand *PPNPCommand;
typedef struct _ManagingIOCommand *PManagingIOCommand;

class com_ximeta_driver_NDASDevice : public IOService
{
	OSDeclareDefaultStructors(com_ximeta_driver_NDASDevice)

private:
	
	bool					fWillTerminate;
	// Address. 
	int32_t					fSlotNumber;
	struct sockaddr_lpx		fAddress;
	char					fName[MAX_NDAS_NAME_SIZE];
	char					fDeviceIDString[MAX_NDAS_NAME_SIZE];
	UInt8					fVID;
	
	bool					fAutoRegister;
	
	// Configuration.
	bool					fHasWriteRight;
	uint32_t					fConfiguration;
	
	char					fUserPassword[8];
	char					fSuperPassword[8];
	
	// Device status.
	uint32_t					fStatus;
			
	// Device Infomation.
	UInt8					fType;
	UInt8					fVersion;

	int						fNRTarget;

//	uint32_t				fMaxRequestBlocks;
	uint32_t				fMaxRequestBytes;
	
	// Misc. Infomation.
	uint32_t			fLastTimeRecevingPnPMessage;	// secs.
	uint32_t			fIntervalPnPMessage;			// secs.
		
	// Flow control.
	uint64_t			fReadSpeed[FLOW_NUMBER_OF_REQUEST_SIZE + 1];
	uint64_t			fWriteSpeed[FLOW_NUMBER_OF_REQUEST_SIZE + 1];
	uint16_t			fCurrentReadRequestSizeIndex;
	uint16_t			fCurrentWriteRequestSizeIndex;
	SInt8				fReadStableCount;
	SInt8				fWriteStableCount;	

	//
	// Interface.
	//
	uint32_t				fInterfaceArrayIndex;
	uint64_t				fLinkSpeed;
	struct sockaddr_lpx	fInterfaceAddress;
	
	com_ximeta_driver_NDASCommand*	fPnpCommand;
	
	void	setHWType(UInt8 type);
	void	setHWVersion(UInt8 version);
	
public:
	virtual bool init(OSDictionary *dictionary = 0);
	virtual void free(void);
	virtual bool attach( IOService * provider );
    virtual void detach( IOService * provider );
	
	virtual IOReturn message(UInt32 type, IOService * provider, void * argument = 0);
		
	void	setSlotNumber(uint32_t SlotNumber);
	uint32_t	slotNumber();

	void	setUserPassword(char *Password);
	char	*userPassword();

	void	setSuperPassword(char *Password);
	char	*superPassword();

	void	setAddress(struct sockaddr_lpx *Address);
	struct sockaddr_lpx * address();

	void	setName(char *Name);
	char	*name();

	void	setDeviceIDString(char *deviceIDString);
	char	*deviceIDString();

	void	setWriteRight(bool HasWriteRight);
	bool	writeRight();
	
	void	setConfiguration(uint32_t configuration);
	uint32_t	configuration();
	
	void	setStatus(uint32_t status);
	uint32_t	status();
			
//	void	setMaxRequestBlocks(uint32_t MRB);
//	uint32_t	MaxRequestBlocks();

	void	setMaxRequestBytes(uint32_t MRB);
	uint32_t	MaxRequestBytes();

	void	setVID(UInt8 vid);
	UInt8	VID();

	void	setAutoRegister(bool autoRegister);
	bool	autoRegister();
	
	int	targetType(uint32_t TargetNo);
	
	void setTargetNeedRefresh(uint32_t TargetNo);
				
	uint32_t lastTimeReceivingPnPMessage();
	uint64_t intervalPnPMessage();
	
	void	receivePnPMessage(uint32_t receivedTime, UInt8 type, UInt8 Version, uint32_t interfaceArrayIndex, char* address, uint64_t Speed);
	void	completeChangeSettingMessage(com_ximeta_driver_NDASChangeSettingCommand *command);
	void	receiveServiceRequest(uint32_t serviceType, char *password, uint8_t vid, bool bWritable);

	// Utility Command Part.
	int getTargetData(TARGET_DATA *Targets, com_ximeta_driver_NDASPhysicalUnitDevice *unitDevice);
	int discovery(TARGET_DATA *PerTarget);
	int getUnitDiskInfo(TARGET_DATA *Targets);
	
	int loginCheck(char *password);
	UInt8	checkVID();

	int changeUserPassword(char *newPassword, uint8_t vid, bool bChangeDevice);

	bool	initDataConnection(com_ximeta_driver_NDASPhysicalUnitDevice *device, bool writable);
	bool	cleanupDataConnection(com_ximeta_driver_NDASPhysicalUnitDevice *device, bool connectionError);
	void	checkDataConnection(com_ximeta_driver_NDASPhysicalUnitDevice *device);
	
	bool mount(com_ximeta_driver_NDASPhysicalUnitDevice *device);
	bool unmount(com_ximeta_driver_NDASPhysicalUnitDevice *device);
	bool reconnect(com_ximeta_driver_NDASPhysicalUnitDevice *device);
	
	bool	processSrbCommand(PSCSICommand pScsiCommand);
	bool	processSrbCommandNormal(PSCSICommand pScsiCommand);

	bool	processReadCommand(
								PSCSICommand pScsiCommand, 
								uint64_t LogicalBlockAddress, 
								uint32_t NumberOfBlocks,
								uint32_t BufferSize
								);
		
	bool	processWriteCommand(
						PSCSICommand pScsiCommand, 
						uint64_t LogicalBlockAddress, 
						uint32_t NumberOfBlocks,
						uint32_t BufferSize
								);
	
	bool	sendIDECommand(PSCSICommand pCommand, com_ximeta_driver_NDASPhysicalUnitDevice *physicalDevice);
	bool	processSrbCommandPacket(PSCSICommand pScsiCommand);
	
	bool	processManagingIOCommand(PManagingIOCommand pManagingIOCommand);	
};
