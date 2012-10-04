/***************************************************************************
*
*  NDASDeviceContorller.h
*
*    NDAS Device Controller
*
*    Copyright (c) 2004 XiMeta, Inc. All rights reserved.
*
**************************************************************************/

#include <IOKit/IOService.h>

#include "LanScsiProtocol.h"

class com_ximeta_driver_NDASDevice;
class com_ximeta_driver_NDASCommand;
class com_ximeta_driver_NDASLogicalDevice;
class com_ximeta_driver_NDASBusEnumerator;

class com_ximeta_driver_NDASDeviceController : public IOService
{
	OSDeclareDefaultStructors(com_ximeta_driver_NDASDeviceController)
	
private:
	
	IOWorkLoop				*fWorkLoop;
	IOCommandGate			*fCommandGate;
	
	bool					fThreadTerminate;
	semaphore_port_t		fCommandSema;
	IOThread				fWorkerThread;
	
	com_ximeta_driver_NDASDevice	*fProvider;
	
	OSArray							*fCommandArray;
	
	OSArray							*fUnits;
	
//	com_ximeta_driver_NDASLogicalDevice	*fTargetDevices[MAX_NR_OF_TARGETS_PER_DEVICE];
	
	bool							fInSleepMode;
	bool							fWakeup;
	bool							fNeedCompareTargetData;
	
	UInt32						lastBroadcastTime;	// Secs.
	
	void processSettingCommand(com_ximeta_driver_NDASCommand *command);
	void processSrbCommand(com_ximeta_driver_NDASCommand *command);
	void processManagingIOCommand(com_ximeta_driver_NDASCommand *command);
	void processNoPnPMessageFromNDAS();
	
	int createUnitDevice(UInt32 targetNo, TARGET_DATA *Target);
	int updateUnitDevice(UInt32 targetNo, TARGET_DATA *Target, bool interfaceChanged, bool bPresent);
	int	deleteUnitDevice(UInt32 targetNo);

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
	
	IOWorkLoop *getWorkLoop();

	bool enqueueCommand(com_ximeta_driver_NDASCommand *command);
	bool executeAndWaitCommand(com_ximeta_driver_NDASCommand *command);
	
	IOReturn receiveMsg(void *newRequest, void *, void *, void * );
	void workerThread(void	*argument);
	
	UInt32	MaxRequestBlocksOfProvider();
};

static IOReturn receiveMsgWrapper( OSObject * theDriver, void *first, void *second, void *third, void *fourth );
static void workerThreadWrapper(void* argument);