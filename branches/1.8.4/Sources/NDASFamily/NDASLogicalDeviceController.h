/*
 *  NDASLogicalUnitDeviceController.h
 *  NDASFamily
 *
 *  Created by Jung-gyun Ahn on 09. 07. 08.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#include <IOKit/IOService.h>

#include "Utilities.h"

class com_ximeta_driver_NDASLogicalDevice;
class com_ximeta_driver_NDASCommand;
class com_ximeta_driver_NDASDeviceNotificationCommand;
class com_ximeta_driver_NDASSCSICommand;
class com_ximeta_driver_NDASProtocolTransportNub;

class com_ximeta_driver_NDASLogicalDeviceController : public IOService {
	
	OSDeclareDefaultStructors(com_ximeta_driver_NDASLogicalDeviceController)
	
private:
	
	com_ximeta_driver_NDASLogicalDevice			*fProvider;
	com_ximeta_driver_NDASProtocolTransportNub	*fProtocolTransportNub;

	bool								fThreadTerminate;
	semaphore_port_t					fCommandSema;
	THREAD_T							fWorkerThread;
	
	IOWorkLoop							*fWorkLoop;
	IOCommandGate						*fCommandGate;

	OSArray								*fCommandArray;

	unsigned long		fCurrentPowerState;
protected:

	IOWorkLoop							*cntrlSync;

public:
	
	virtual bool init(OSDictionary *dictionary = 0);
	virtual void free(void);
	virtual IOService *probe(IOService *provider, SInt32 *score);
	virtual bool start(IOService *provider);
	virtual bool finalize ( IOOptionBits options );
	virtual void stop(IOService *provider);
	virtual IOReturn setPowerState (
									unsigned long   powerStateOrdinal,
									IOService*		whatDevice
									);
	
	IOWorkLoop *getWorkLoop();
	
	bool enqueueCommand(com_ximeta_driver_NDASCommand *command);
	bool executeAndWaitCommand(com_ximeta_driver_NDASCommand *command);
	
	IOReturn receiveMsg(void *newRequest, void *, void *, void * );
	void workerThread(void	*argument);
	
	void processDeviceNotificationCommand(com_ximeta_driver_NDASDeviceNotificationCommand *command);
	void processSRBCommand(com_ximeta_driver_NDASSCSICommand *command);
	
	void completeCommand(com_ximeta_driver_NDASSCSICommand *command);

	bool isProtocolTransportNubAttached();
	
	bool isWorkable();
	
#pragma mark -
#pragma mark == Protocol Transport Management ==
#pragma mark -
	
	bool attachProtocolTransport();
	bool detachProtocolTransport();
		
};

