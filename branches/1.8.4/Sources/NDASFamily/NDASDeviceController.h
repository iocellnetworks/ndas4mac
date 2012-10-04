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
#include "Utilities.h"

class com_ximeta_driver_NDASDevice;
class com_ximeta_driver_NDASCommand;
class com_ximeta_driver_NDASChangeSettingCommand;
class com_ximeta_driver_NDASSCSICommand;
class com_ximeta_driver_NDASManagingIOCommand;
class com_ximeta_driver_NDASLogicalDevice;
class com_ximeta_driver_NDASBusEnumerator;
class com_ximeta_driver_NDASPhysicalUnitDevice;
class com_ximeta_driver_NDASUnitDeviceList;
class com_ximeta_driver_NDASServiceCommand;

class com_ximeta_driver_NDASDeviceController : public IOService
{
	OSDeclareDefaultStructors(com_ximeta_driver_NDASDeviceController)
	
private:
	
	IOWorkLoop				*fWorkLoop;
	IOCommandGate			*fCommandGate;
	
	bool					fThreadTerminate;
	semaphore_port_t		fCommandSema;
	THREAD_T				fWorkerThread;
	IOTimerEventSource		*fPnpTimerSourcePtr;
	
	com_ximeta_driver_NDASDevice	*fProvider;
	
	OSArray							*fCommandArray;
	
	com_ximeta_driver_NDASUnitDeviceList	*fUnits;
		
	bool							fInSleepMode;
	bool							fWakeup;
	bool							fNeedCompareTargetData;
	
	uint32_t						lastBroadcastTime;	// Secs.
	
	unsigned long		fCurrentPowerState;
	
	void processSettingCommand(com_ximeta_driver_NDASChangeSettingCommand *command);
	void processSrbCommand(com_ximeta_driver_NDASSCSICommand *command);
	void processManagingIOCommand(com_ximeta_driver_NDASManagingIOCommand *command);
	void processNoPnPMessageFromNDAS();
	void processServiceRequest(com_ximeta_driver_NDASServiceCommand *command);

	int createUnitDevice(uint32_t targetNo, TARGET_DATA *Target);
	int updateUnitDevice(uint32_t targetNo, TARGET_DATA *Target, bool interfaceChanged, bool bPresent);
	int	deleteUnitDevice(uint32_t targetNo);
	com_ximeta_driver_NDASPhysicalUnitDevice *getUnitDeviceByTargetNumber(uint32_t targetNo);
	
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
	
	virtual bool willTerminate(
							   IOService * provider,
							   IOOptionBits options 
							   );
	
	virtual IOReturn setPowerState (
									unsigned long   powerStateOrdinal,
									IOService*		whatDevice
									);
	IOWorkLoop *getWorkLoop();
	
	bool enqueueCommand(com_ximeta_driver_NDASCommand *command);
	bool executeAndWaitCommand(com_ximeta_driver_NDASCommand *command);
	
	IOReturn receiveMsg(void *newRequest, void *, void *, void * );
	void workerThread(void	*argument);
		
	com_ximeta_driver_NDASBusEnumerator *getBusEnumerator();
	
	void PnpTimeOutOccurred(
							OSObject *owner,
							IOTimerEventSource *sender);
	
};
