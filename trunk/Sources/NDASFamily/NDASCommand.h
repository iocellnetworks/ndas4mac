/***************************************************************************
*
*  NDASCommand.h
*
*    NDAS Command
*
*    Copyright (c) 2004 XiMeta, Inc. All rights reserved.
*
**************************************************************************/

#ifndef _NDAS_COMMAND_H_
#define _NDAS_CIMMAND_H_

#include <IOKit/IOService.h>
#include <IOKit/IOCommand.h>
#include <IOKit/scsi/IOSCSIProtocolServices.h>

#include "LanScsiProtocol.h"

#define NDAS_COMMAND_CLASS	"com_ximeta_driver_NDASCommand"

class	com_ximeta_driver_NDASUnitDevice;
class	com_ximeta_driver_NDASPhysicalUnitDevice;

enum {
	kNDASCommandTerminateThread,
	kNDASCommandChangedSetting,
	kNDASCommandRefresh,
	kNDASCommandSRB,
	kNDASCommandManagingIO,
	kNDASCommandDeviceNotification,
	kNDASCommandSRBCompleted,
	kNDASCommandReceivePnpMessage,
	kNDASNumberOfCommands
};

enum {
	kNDASCommandQueueAppend,
	kNDASCommandQueueCompleteCommand
};


enum {
	kNDASManagingIOReadSectors,
	kNDASManagingIOWriteSectors,
	kNDASNumberOfManagingIOs,
};

enum {
	kNDASDeviceNotificationPresent,
	kNDASDeviceNotificationNotPresent,
	kNDASDeviceNotificationCheckBind,
	kNDASDeviceNotificationDisconnect,
	kNDASNumberOfDeviceNotifications
};

typedef struct PrivCmdData
{
    UInt32					xferCount;
	IOReturn				results;
	SCSIServiceResponse 	serviceResponse;
	SCSITaskStatus			taskStatus;
} PrivCmdData;

typedef struct _SCSICommand {

	com_ximeta_driver_NDASUnitDevice	*device;

	SCSITaskIdentifier			scsiTask;
	
	UInt32						targetNo;
	
	SInt8						LogicalUnitNumber;
	
	SCSICommandDescriptorBlock  cdb;
	UInt8						cdbSize;
	
	// Buffer and Data Transfer size.
	IOMemoryDescriptor		*MemoryDescriptor_ptr;
	UInt64					BufferOffset;
	UInt64					RequestedDataTransferCount;
	UInt8					DataTransferDirection;
	
//	UInt32					BlockSize;
	PrivCmdData				CmdData;

	bool					completeWhenFailed;
	
	UInt32					SubUnitIndex;
		
} SCSICommand, *PSCSICommand;

typedef struct _ManagingIOCommand {
	
	com_ximeta_driver_NDASUnitDevice	*device;
	
	UInt32					command;
	
	UInt64					Location;
	UInt32					sectors;
	
	char					*buffer;
	
	bool					IsSuccess;
	
} ManagingIOCommand, *PManagingIOCommand;

typedef struct _PNPCommand {
	
	UInt32			fReceivedTime;	
	PNP_MESSAGE		fMessage;	
	UInt32			fInterfaceArrayIndex;
	char			fAddress[6];
	UInt64			fLinkSpeed;
	bool			fInterfaceChanged;		// For RAID device it means that device is presented.
	
} PNPCommand, *PPNPCommand;

typedef struct _RefreshCommand {
	
	UInt32	fTargetNumber;
	
} RefreshCommand, *PRefreshCommand;

typedef struct _DeviceNotificationCommand {
	
	UInt32								notificationType;
	com_ximeta_driver_NDASUnitDevice	*device;
	
} DeviceNotificationCommand, *PDeviceNotificationCommand;

class com_ximeta_driver_NDASCommand : public IOCommand
{
	OSDeclareDefaultStructors(com_ximeta_driver_NDASCommand)
	
private:
	
	UInt32	fCommand;
	
	union {
		PNPCommand					fPnpCommand;
		RefreshCommand				fRefreshCommand;
		SCSICommand					fScsiCommand;
		ManagingIOCommand			fManagingIOCommand;
		DeviceNotificationCommand	fDeviceNotificationCommand;
	} u;
	
	semaphore_t				fSema;
	
public:
	
	com_ximeta_driver_NDASCommand	*fPrev;
	com_ximeta_driver_NDASCommand	*fNext;
	
	virtual bool init();
	virtual void free(void);
	void setCommand(UInt32 interfaceArrayIndex, char *address, UInt32 receivedTime, PPNP_MESSAGE message);
	void setCommand(UInt32 interfaceArrayIndex, char *address, UInt64 linkSpeed, bool interfaceChanged);
	void setCommand(UInt32 TargetNumber);
	void setCommand(PSCSICommand command);
	void setCommand(PManagingIOCommand command);
	void setCommand(UInt32 notificationType, com_ximeta_driver_NDASUnitDevice *device);
	void setToTerminateCommand();
				
	UInt32 command();
	PSCSICommand scsiCommand();
	PPNPCommand pnpCommand();
	PRefreshCommand refreshCommand();
	PManagingIOCommand managingIOCommand();
	PDeviceNotificationCommand DeviceNotificationCommand();
	
	void	convertSRBToSRBCompletedCommand();
	void	convertSRBCompletedToSRBCommand();

	bool	setToWaitForCompletion();
	void	waitForCompletion();
	void	setToComplete();
	
//	void setReceivedTime(mach_timespec_t * time);
//	mach_timespec_t *receivedTime();
};

#endif
