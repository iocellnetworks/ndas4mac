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

class com_ximeta_driver_NDASCommand;
class com_ximeta_driver_NDASRAIDUnitDevice;

class com_ximeta_driver_NDASRAIDUnitDeviceController : public IOService
{
	OSDeclareDefaultStructors(com_ximeta_driver_NDASRAIDUnitDeviceController)
	
private:
	
	IOWorkLoop				*fWorkLoop;
	IOCommandGate			*fCommandGate;
	
	bool					fThreadTerminate;
	semaphore_port_t		fCommandSema;
	IOThread				fWorkerThread;
	
	com_ximeta_driver_NDASRAIDUnitDevice	*fProvider;
	
	OSArray					*fCommandArray;
			
	UInt32					fNumberOfNewBuildCommands;
		
	IOMemoryDescriptor		*fMemoryDescriptorArray[1024];
	
	// Written Sector List.
	OSArray								*fWrittenSectorsList;
	OSArray								*fWrittenLocationsList;
	UInt32								fSizeOfWrittenSectors;
	
	void processSettingCommand(com_ximeta_driver_NDASCommand *command);
	void processSrbCommand(com_ximeta_driver_NDASCommand *command);
	void processSrbCompletedCommand(com_ximeta_driver_NDASCommand *command);
	void processReadWriteCommand(com_ximeta_driver_NDASCommand *command);
	
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
	void InitAndSetBitmap();

};

static IOReturn receiveMsgWrapper( OSObject * theDriver, void *first, void *second, void *third, void *fourth );
static void workerThreadWrapper(void* argument);