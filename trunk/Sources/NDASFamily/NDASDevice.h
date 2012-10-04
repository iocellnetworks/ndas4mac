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
#define FLOW_UNIT_SIZE					8
#define FLOW_NUMBER_OF_REQUEST_SIZE		16	// 8, 16, 24, ... 128
#define FLOW_MIN_REQUEST_SIZE			8	// 4KB
#define CURRENT_REQUEST_BLOCKS(_index_)	(FLOW_MIN_REQUEST_SIZE + (_index_ * FLOW_UNIT_SIZE))
#define MAX_STABLE_COUNT				0

#define RETRANSMIT_TIME_NANO			200 * 1000 * 1000

class com_ximeta_driver_NDASDeviceController;
class com_ximeta_driver_NDASCommand;
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
	UInt32					fSlotNumber;
	struct sockaddr_lpx		fAddress;
	char					fName[MAX_NDAS_NAME_SIZE];
	char					fDeviceIDString[MAX_NDAS_NAME_SIZE];
	UInt8					fVID;
	
	// Configuration.
	bool					fHasWriteRight;
	UInt32					fConfiguration;
	
	char					fUserPassword[8];
	char					fSuperPassword[8];
	
	// Device status.
	UInt32					fStatus;
			
	// Device Infomation.
	UInt8					fType;
	UInt8					fVersion;

	int						fNRTarget;

	UInt32					fMaxRequestBlocks;
	
//	TargetInfo				fTargetInfos[MAX_NR_OF_TARGETS_PER_DEVICE];
	
//	TARGET_DATA						fTargets[MAX_NR_OF_TARGETS_PER_DEVICE];
	
	/*struct LanScsiSession			fTargetDataSessions[MAX_NR_OF_TARGETS_PER_DEVICE];
	char							*fTargetDataBuffer[MAX_NR_OF_TARGETS_PER_DEVICE];
	int								fTargetSenseKey[MAX_NR_OF_TARGETS_PER_DEVICE];
	bool							fTargetConnected[MAX_NR_OF_TARGETS_PER_DEVICE];
	com_ximeta_driver_NDASCommand*	fTargetPendingCommand[MAX_NR_OF_TARGETS_PER_DEVICE];
	*/
	// Misc. Infomation.
	uint32_t			fLastTimeRecevingPnPMessage;	// secs.
	uint32_t			fIntervalPnPMessage;			// secs.
	
	com_ximeta_driver_NDASDeviceController	*fController;
	
	// Flow control.
	uint64_t			fReadSpeed[FLOW_NUMBER_OF_REQUEST_SIZE];
	uint64_t			fWriteSpeed[FLOW_NUMBER_OF_REQUEST_SIZE];
	SInt8					fCurrentReadRequestSizeIndex;
	SInt8					fCurrentWriteRequestSizeIndex;
	SInt8					fReadStableCount;
	SInt8					fWriteStableCount;	

	//
	// Interface.
	//
	UInt32				fInterfaceArrayIndex;
	UInt64				fLinkSpeed;
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
	
	void	setController(com_ximeta_driver_NDASDeviceController *controller);
	
	void	setSlotNumber(UInt32 SlotNumber);
	UInt32	slotNumber();

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
	
	void	setConfiguration(UInt32 configuration);
	UInt32	configuration();
	
	void	setStatus(UInt32 status);
	UInt32	status();
			
	void	setMaxRequestBlocks(UInt32 MRB);
	UInt32	MaxRequestBlocks();
	
	void	setVID(UInt8 vid);
	UInt8	VID();
	
	int	targetType(UInt32 TargetNo);
	
	void setTargetNeedRefresh(UInt32 TargetNo);
				
	uint64_t lastTimeReceivingPnPMessage();
	uint64_t intervalPnPMessage();
	
	void	receivePnPMessage(uint32_t receivedTime, UInt8 type, UInt8 Version, UInt32 interfaceArrayIndex, char* address, UInt64 Speed);
	void	completePnPMessage(com_ximeta_driver_NDASCommand *command);
	
	// Utility Command Part.
	int getTargetData(TARGET_DATA *Targets, com_ximeta_driver_NDASPhysicalUnitDevice *unitDevice);
	int discovery(TARGET_DATA *PerTarget);
	int getUnitDiskInfo(TARGET_DATA *Targets);

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
								UInt64 LogicalBlockAddress, 
								UInt32 NumberOfBlocks,
								UInt32 BufferSize
								);
		
	bool	processWriteCommand(
						PSCSICommand pScsiCommand, 
						UInt64 LogicalBlockAddress, 
						UInt32 NumberOfBlocks,
						UInt32 BufferSize
								);
	
	bool	sendIDECommand(PSCSICommand pCommand, com_ximeta_driver_NDASPhysicalUnitDevice *physicalDevice);
	bool	processSrbCommandPacket(PSCSICommand pScsiCommand);
	
	bool	processManagingIOCommand(PManagingIOCommand pManagingIOCommand);
};
