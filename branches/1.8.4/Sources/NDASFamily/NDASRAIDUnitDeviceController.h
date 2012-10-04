/***************************************************************************
*
*  NDASRAIDUnitDeviceContorller.h
*
*    NDAS RAID Unit Device Controller
*
*    Copyright (c) 2004 XiMeta, Inc. All rights reserved.
*
**************************************************************************/

#include <IOKit/IOService.h>
#include <IOKit/IOLib.h>

#include "NDASBusEnumerator.h"

#include "Utilities.h"

class com_ximeta_driver_NDASCommand;
class com_ximeta_driver_NDASChangeSettingCommand;
class com_ximeta_driver_NDASRAIDUnitDevice;
class com_ximeta_driver_NDASSCSICommand;

class com_ximeta_driver_NDASRAIDUnitDeviceController : public IOService
{
	OSDeclareDefaultStructors(com_ximeta_driver_NDASRAIDUnitDeviceController)
	
private:
	
	IOWorkLoop				*fWorkLoop;
	IOCommandGate			*fCommandGate;
	
	bool					fThreadTerminate;
	semaphore_port_t		fCommandSema;
	THREAD_T				fWorkerThread;
	IOTimerEventSource		*fRAIDTimerSourcePtr;

	com_ximeta_driver_NDASRAIDUnitDevice	*fProvider;
	
	OSArray					*fCommandArray;
			
	uint32_t					fNumberOfNewBuildCommands;
		
#if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= 1060

	IOSubMemoryDescriptor		*fMemoryDescriptorArray[1024];

#else

	IOMemoryDescriptor		*fMemoryDescriptorArray[1024];

#endif
	
	// Written Sector List.
	OSArray								*fWrittenSectorsList;
	OSArray								*fWrittenLocationsList;
	uint32_t								fSizeOfWrittenSectors;
	
	unsigned long		fCurrentPowerState;
	
	void processSettingCommand(com_ximeta_driver_NDASChangeSettingCommand *command);
	void processSrbCommand(com_ximeta_driver_NDASSCSICommand *command);
	void processSrbCompletedCommand(com_ximeta_driver_NDASSCSICommand *command);
	void processReadWriteCommand(com_ximeta_driver_NDASSCSICommand *command);
	
protected:
		IOWorkLoop			*cntrlSync;

public:
	virtual bool init(OSDictionary *dictionary = 0);
	virtual void free(void);
	virtual IOService *probe(IOService *provider, SInt32 *score);
	virtual bool start(IOService *provider);
	virtual bool finalize ( IOOptionBits options );
	virtual void stop(IOService *provider);
	virtual IOReturn message ( UInt32 type, IOService * provider, void * argument = 0 );
	virtual IOReturn setPowerState (
									unsigned long   powerStateOrdinal,
									IOService*		whatDevice
									);
	
	IOWorkLoop *getWorkLoop();

	bool enqueueCommand(com_ximeta_driver_NDASCommand *command);
	bool executeAndWaitCommand(com_ximeta_driver_NDASCommand *command);
	
	IOReturn receiveMsg(void *newRequest, void *, void *, void * );
	void workerThread(void	*argument);
	void InitAndSetBitmap();
	
	void RAIDTimeOutOccurred(
							 OSObject *owner,
							 IOTimerEventSource *sender);

};
