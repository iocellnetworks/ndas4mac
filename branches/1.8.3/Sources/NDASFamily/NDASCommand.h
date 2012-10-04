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
#define _NDAS_COMMAND_H_

#include <IOKit/IOService.h>
#include <IOKit/IOCommand.h>
#include <IOKit/scsi/IOSCSIProtocolServices.h>

#include "LanScsiProtocol.h"

#define NDAS_COMMAND_CLASS	"com_ximeta_driver_NDASCommand"

class	com_ximeta_driver_NDASUnitDevice;
class	com_ximeta_driver_NDASPhysicalUnitDevice;

enum {
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

class com_ximeta_driver_NDASCommand : public IOCommand
{
	OSDeclareAbstractStructors(com_ximeta_driver_NDASCommand)
	
protected:
	
	uint32_t	fCommand;
	semaphore_t	fSema;
	IOReturn	fResult;
	
public:
	
//	com_ximeta_driver_NDASCommand	*fPrev;
//	com_ximeta_driver_NDASCommand	*fNext;
	
	virtual bool init();
	virtual void free(void);
					
	uint32_t command();
	IOReturn result();
	
	bool	setToWaitForCompletion();
	void	waitForCompletion();
	void	setToComplete(IOReturn result);
	
//	void setReceivedTime(mach_timespec_t * time);
//	mach_timespec_t *receivedTime();
};

#endif
