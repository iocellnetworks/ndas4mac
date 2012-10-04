/*
 *  NDASLogicalUnitDeviceController.cpp
 *  NDASFamily
 *
 *  Created by Jung-gyun Ahn on 09. 07. 08.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */
#include <IOKit/IOLib.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOMessage.h>

#include "NDASBusEnumeratorUserClientTypes.h"

#include "NDASBusEnumerator.h"

#include "NDASLogicalDeviceController.h"
#include "NDASLogicalDevice.h"
#include "NDASProtocolTransportNub.h"

#include "NDASCommand.h"
#include "NDASSCSICommand.h"
#include "NDASDeviceNotificationCommand.h"

#include "Utilities.h"


static IOReturn receiveMsgWrapper( OSObject * theDriver, void *first, void *second, void *third, void *fourth );
static void workerThreadWrapper(void* argument
#if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= MAC_OS_X_VERSION_10_4
								, __unused wait_result_t wr);
#else
								);
#endif

enum {
	kNDASPowerOffState = 0,
	kNDASPowerOnState = 1,
	kNumberNDASPowerStates = 2
};

static IOPMPowerState powerStates[ kNumberNDASPowerStates ] = {
{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
{ 1, kIOPMDeviceUsable, kIOPMPowerOn, kIOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 }
};

// Define my superclass
#define super IOService

// REQUIRED! This macro defines the class's constructors, destructors,
// and several other methods I/O Kit requires. Do NOT use super as the
// second parameter. You must use the literal name of the superclass.

OSDefineMetaClassAndStructors(com_ximeta_driver_NDASLogicalDeviceController, IOService)

bool com_ximeta_driver_NDASLogicalDeviceController::init(OSDictionary *dict)
{
	bool res = super::init(dict);
	
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Initializing\n"));
	
    return res;
}

void com_ximeta_driver_NDASLogicalDeviceController::free(void)
{
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Freeing\n"));
	
	super::free();
}

IOService *com_ximeta_driver_NDASLogicalDeviceController::probe(IOService *provider, SInt32 *score)
{
    IOService *res = super::probe(provider, score);
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Probing\n"));
    
	return res;
}

bool com_ximeta_driver_NDASLogicalDeviceController::start(IOService *provider)
{
    bool res = super::start(provider);
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Starting\n"));
	
	fProvider = OSDynamicCast ( com_ximeta_driver_NDASLogicalDevice, provider );
	if(!fProvider) {
		goto exit;
	}
	
	//
	// Add a retain here so we can keep NDASPhysicalUnitDevice from doing garbage
	// collection on us when we are in the middle of our finalize method.
	//
	fProvider->retain();
	
	bool openSucceeded;
	
	openSucceeded = fProvider->open (this);
	if(openSucceeded == false) {
		goto exit;
	}
	
	// Create command Gate.
	fCommandGate = IOCommandGate::commandGate(this, receiveMsgWrapper);
	
	if(!fCommandGate) {
		DbgIOLog(DEBUG_MASK_PNP_ERROR, ("can't create commandGate\n"));
		return false;
	}
	
	fWorkLoop = (IOWorkLoop *)getWorkLoop();
	
	if (fWorkLoop->addEventSource(fCommandGate) != kIOReturnSuccess)  {
		DbgIOLog(DEBUG_MASK_PNP_ERROR, ("can't add commandGate\n"));
		return false;
	}
	
	fCommandArray = OSArray::withCapacity(0);

	// Create worker Thread.
	fThreadTerminate = false;
	
	if(KERN_SUCCESS != semaphore_create(current_task(),
                                        &fCommandSema,
                                        SYNC_POLICY_FIFO, 0)) {
        
		DbgIOLog(DEBUG_MASK_PNP_ERROR, ("start::semaphore_create(), failed to Create Command Sema\n"));
		
        return false;
    }
    
	if(KERN_SUCCESS != myCreateThread(workerThreadWrapper, this, &fWorkerThread)) {
		DbgIOLog(DEBUG_MASK_PNP_ERROR, ("start::kernel_thread_start(), failed to Create Thread\n"));
		
		semaphore_destroy(kernel_task, fCommandSema);
		
		return false;
	}
		
	// Power Management.
	PMinit();
	provider->joinPMtree(this);
	registerPowerDriver(this, powerStates, kNumberNDASPowerStates);
	
    return res;
	
exit:
	return false;
}

bool com_ximeta_driver_NDASLogicalDeviceController::finalize (
																   IOOptionBits options 
																   )
{
	
	DbgIOLog(DEBUG_MASK_PNP_TRACE, ("finalizing.\n"));
	
	// Terminate Thread.
	if(false == fThreadTerminate) {
		// Terminate Thread.
		fThreadTerminate = true;
		
		semaphore_signal(fCommandSema);
	}			
	
	// Wait Thread termination.
	bool waitMore = true;
	
	while (waitMore) {
		waitMore = false;
		
		IOSleep(1000);
		
		if (fWorkerThread) {
			DbgIOLog(DEBUG_MASK_USER_ERROR, ("Thread is still running! %p\n", fWorkerThread));
			
			waitMore = true;
			
			semaphore_signal(fCommandSema);
		}		
	}
	
	// Detach Protocol Transport Nub.
	if(fProtocolTransportNub) {
		detachProtocolTransport();
	}
	
	if ( fProvider != NULL ) {		
		
		if ( fProvider->isOpen ( this ) ) {
			fProvider->close ( this );
		}
	}
	
	if ( fProvider ) {
		fProvider->release ( );
		fProvider = NULL;
	}
	
	return super::finalize ( options );
	
}

void com_ximeta_driver_NDASLogicalDeviceController::stop(IOService *provider)
{
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Stopping\n"));

	if (fCommandGate) {
		
		// Free all command.
		while (fCommandArray->getCount() > 0) {			
			fCommandArray->removeObject(0);
			DbgIOLog(DEBUG_MASK_PNP_ERROR, ("remove Command!!!\n"));
		}
		
		fWorkLoop->removeEventSource(fCommandGate);     
		fCommandGate->release();     
		fCommandGate = NULL; 
	} 
	
	// Destroy Semaphores.
	semaphore_destroy(current_task(), fCommandSema);
		
	// Release Command Array.
	fCommandArray->release();

	PMstop();
	
    super::stop(provider);
}

IOReturn 
com_ximeta_driver_NDASLogicalDeviceController::setPowerState (
													   unsigned long   powerStateOrdinal,
													   IOService*		whatDevice
													   )
{
	IOReturn	result = IOPMAckImplied;
	
	DbgIOLog(DEBUG_MASK_POWER_TRACE, ("state %lu\n", powerStateOrdinal ));
	
	if( fCurrentPowerState == powerStateOrdinal ) {
		return result;
	}
	
	switch( powerStateOrdinal ) {
		case kNDASPowerOffState:
		{
			result = IOPMAckImplied;
		}
			break;
		case kNDASPowerOnState:
		{
			result = IOPMAckImplied;
		}
			break;
	}      
	
	return result;
	
}

IOReturn com_ximeta_driver_NDASLogicalDeviceController::receiveMsg ( void * newRequest, void *command, void *, void * )
{
	DbgIOLog(DEBUG_MASK_NDAS_TRACE, ("receiveMsg: Entered\n"));
	
	uint32_t request;
	
	memcpy(&request, &newRequest, sizeof(uint32_t));
		   
	switch(request) {
		case kNDASCommandQueueAppend:
		{
			DbgIOLog(DEBUG_MASK_NDAS_INFO, ("Append Command!!!\n"));
			
			if(!command) {
				DbgIOLog(DEBUG_MASK_NDAS_INFO, ("No Command!!!\n"));
				
				return kIOReturnBadArgument;
			}
			
			com_ximeta_driver_NDASCommand	*tempCommand = (com_ximeta_driver_NDASCommand *)command;	
			
			fCommandArray->setObject(tempCommand);
			
			if( 1 == fCommandArray->getCount() ) {
				// Notify to the worker thread.
				semaphore_signal(fCommandSema);
			}			
		}
			break;
		case kNDASCommandQueueCompleteCommand:
		{
			DbgIOLog(DEBUG_MASK_NDAS_INFO, ("Complete Command!!!\n"));
			
			com_ximeta_driver_NDASCommand	*firstCommand = (com_ximeta_driver_NDASCommand *)fCommandArray->getObject(0);
			fCommandArray->removeObject(0);
			
			// Call Complete Callback.
			switch(firstCommand->command()) {
				case kNDASCommandDeviceNotification:
				{
					com_ximeta_driver_NDASDeviceNotificationCommand *notiCommand = OSDynamicCast(com_ximeta_driver_NDASDeviceNotificationCommand, firstCommand);
					DbgIOASSERT(notiCommand);
					
					fProvider->completeDeviceNotificationMessage(notiCommand);
				}
					break;
			}
			
			if( 0 < fCommandArray->getCount() ) {
				
				// Notify to the worker thread.
				semaphore_signal(fCommandSema);
			}			
		}
			break;
		default:
			DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("Invalid Command!!! %d\n", request));
			
			return kIOReturnInvalid;
	}
	
	return kIOReturnSuccess;
}

void com_ximeta_driver_NDASLogicalDeviceController::workerThread(void* argument)
{		
	//	bool bTerminate = false;
	
	DbgIOLog(DEBUG_MASK_NDAS_TRACE, ("Entered.\n"));
	
	this->retain();
	
    do {
		kern_return_t					sema_result;
//		mach_timespec_t					wait_time = { 0 };
		//		uint32_t							currentTime;
		
		// Set timeout and wait.
//		wait_time.tv_sec = NDAS_MAX_WAIT_TIME_FOR_PNP_MESSAGE;
        sema_result = semaphore_wait(fCommandSema);
		
		switch(sema_result) {
			case KERN_SUCCESS:
			{
				// Check Command Queue.
				if(0 == fCommandArray->getCount()) {
					// Maybe Terminate signal.
					DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("No Command.\n"));
										
					continue;
				}
				
				com_ximeta_driver_NDASCommand *command = OSDynamicCast(com_ximeta_driver_NDASCommand, fCommandArray->getObject(0));
				if( NULL == command ) {
					DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("getObject return NULL.\n"));
					
					continue;
				}
				
				if (kNDASUnitStatusNotPresent == fProvider->Status()) {
					DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("Status is kNDASUnitStatusNotPresent!!!!!!!!!! command %d\n", command->command()));
				} else {
					
					switch(command->command()) {
						case kNDASCommandDeviceNotification:
						{						
							com_ximeta_driver_NDASDeviceNotificationCommand *notiCommand = OSDynamicCast(com_ximeta_driver_NDASDeviceNotificationCommand, command);
							
							processDeviceNotificationCommand(notiCommand);
						}
							break;
						case kNDASCommandSRB:
						{
							com_ximeta_driver_NDASSCSICommand *scsiCommand = OSDynamicCast(com_ximeta_driver_NDASSCSICommand, command);
							
							processSRBCommand(scsiCommand);						
						}
							break;
						default:
							DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("Unknowen Command. %d\n", command->command()));
							
					}
				}
				
				command->setToComplete(kIOReturnSuccess);
			
				fCommandGate->runCommand((void *)kNDASCommandQueueCompleteCommand);

				/*
				 // Check Broadcast Time.
				 NDAS_clock_get_system_value(&currentTime);
				 
				 if(currentTime - lastBroadcastTime > NDAS_MAX_WAIT_TIME_FOR_TRY_MOUNT) {
				 
				 DbgIOLog(DEBUG_MASK_NDAS_INFO, ("workerThread: Timed Out with SRB commands.\n"));
				 
				 if ( fWakeup ) {
				 // Timer value is not correct. 
				 fWakeup = false;
				 NDAS_clock_get_system_value(&lastBroadcastTime);
				 
				 } else {
				 if(!fInSleepMode) {
				 // No PnP Message and User command.
				 processNoPnPMessageFromNDAS();
				 }
				 }
				 }
				 */
			}
				break;
			default:
				DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("Invalid return value. %d\n", sema_result));
				break;
		}
		
		
	} while(fThreadTerminate == false);

	DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("Terminated. fWorkerThread %p, current task %p\n", fWorkerThread, current_thread()));

	this->release();

	myExitThread(&fWorkerThread);

	// Never return.
}

IOWorkLoop *com_ximeta_driver_NDASLogicalDeviceController::getWorkLoop()
{
	// Do we have a work loop already? if so return it NOW.
	if ( (vm_address_t) cntrlSync >> 1 ) {
		return cntrlSync;
	}
	
	if ( OSCompareAndSwap(0, 1, (UInt32 *)&cntrlSync) ) {
		// Construct the workloop and set the cntrlSync variable
		// to wharever the result is and return
		cntrlSync = IOWorkLoop::workLoop();
	} else {
		while( cntrlSync == (IOWorkLoop *) 1 ) {
			// Spin around the cntrlSync variable unitl the initialization finishes.
			thread_block(0);
		}
	}
	
	return cntrlSync;
}

bool com_ximeta_driver_NDASLogicalDeviceController::enqueueCommand(com_ximeta_driver_NDASCommand *command)
{
	DbgIOLog(DEBUG_MASK_NDAS_TRACE, ("Entered\n"));
	
	if (fThreadTerminate) {
		return false;
	}
	
	if(fCommandGate) {
		fCommandGate->runCommand((void*)kNDASCommandQueueAppend, (void*)command);
		
		return true;
	} else {
		return false;
	}
}

bool com_ximeta_driver_NDASLogicalDeviceController::executeAndWaitCommand(com_ximeta_driver_NDASCommand *command)
{
	DbgIOLog(DEBUG_MASK_NDAS_TRACE, ("Entered\n"));
	
	if(fCommandGate) {
		
		if (command->setToWaitForCompletion()) {
			
			fCommandGate->runCommand((void*)kNDASCommandQueueAppend, (void*)command);
			
			// Wait semaphore.
			command->waitForCompletion();
			
		} else {
			return false;
		}
		return true;
	} else {
		return false;
	}
}

void com_ximeta_driver_NDASLogicalDeviceController::processDeviceNotificationCommand(com_ximeta_driver_NDASDeviceNotificationCommand *command)
{
	DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Entered.\n"));
	
	switch(command->notificationType()) {
		case kNDASDeviceNotificationPresent:
		{
			DbgIOLog(DEBUG_MASK_PNP_TRACE, ("kNDASDeviceNotificationPresent.\n"));
			
			// Update Configuration.
			fProvider->setConfiguration(fProvider->Configuration());
			fProvider->setNRRWHosts(fProvider->NRRWHosts());
			fProvider->setNRROHosts(fProvider->NRROHosts());
			fProvider->IsWritable();
			
			// Update Status.
			fProvider->updateStatus();
			
			switch(fProvider->Status()) {
				case kNDASUnitStatusTryConnectRO:
				case kNDASUnitStatusTryConnectRW:
				{
				}
					break;
				case kNDASUnitStatusTryDisconnectRO:
				case kNDASUnitStatusTryDisconnectRW:
				{
					// Notify to Transport Driver.
					if (fProtocolTransportNub) {
						detachProtocolTransport();
					}
				}
					break;
				case kNDASUnitStatusConnectedRO:
				case kNDASUnitStatusConnectedRW:
				{
					// Notify to Transport Driver.					
					if (!fProtocolTransportNub) {
						attachProtocolTransport();
					}
					
					if(!fProvider->BSDName()) {						
						fProvider->setBSDName(fProtocolTransportNub->bsdName());
					}
				}
					break;
				case kNDASUnitStatusDisconnected:
				case kNDASUnitStatusFailure:
				{
					// Notify to Transport Driver.
//					if (fProtocolTransportNub) {
//						detachProtocolTransport();
//					}
				}
					break;
					
			}
			
			// Update Status.
			fProvider->updateStatus();
			
		}
			break;
		case kNDASDeviceNotificationNotPresent:
		{
			DbgIOLog(DEBUG_MASK_PNP_TRACE, ("kNDASDeviceNotificationNotPresent.\n"));
			
			if (fProtocolTransportNub) {
				DbgIOLog(DEBUG_MASK_PNP_INFO, ("Logical device has Protocol transport NUB.\n"));
				
				DbgIOASSERT(detachProtocolTransport());
			}
			
		}
			break;
		default:
		{
			DbgIOLog(DEBUG_MASK_PNP_ERROR, ("Unknown notification type : %d\n", command->notificationType()));
		}
			break;
	}

	return;
}

bool com_ximeta_driver_NDASLogicalDeviceController::isWorkable()
{
	fProvider->updateStatus();
	
	switch (fProvider->Status()) {
		case kNDASUnitStatusConnectedRO:
		case kNDASUnitStatusConnectedRW:
			return true;
		default:
			return false;
	}
}

void com_ximeta_driver_NDASLogicalDeviceController::processSRBCommand(com_ximeta_driver_NDASSCSICommand *command)
{
	DbgIOLog(DEBUG_MASK_DISK_TRACE, ("Entered.\n"));
	
	fProvider->processSRBCommand(command);
	
	return;
}

void com_ximeta_driver_NDASLogicalDeviceController::completeCommand(com_ximeta_driver_NDASSCSICommand *command)
{
	DbgIOLog(DEBUG_MASK_DISK_TRACE, ("Entered.\n"));

	com_ximeta_driver_NDASProtocolTransportNub *nub = OSDynamicCast(com_ximeta_driver_NDASProtocolTransportNub, getClient());
	
	if(nub) {
		nub->completeCommand(command);	
	}
}

bool com_ximeta_driver_NDASLogicalDeviceController::isProtocolTransportNubAttached()
{
	if (fProtocolTransportNub) {
		return true;
	} else {
		return false;
	}
}

#pragma mark -
#pragma mark == Protocol Transport Management ==
#pragma mark -

bool com_ximeta_driver_NDASLogicalDeviceController::attachProtocolTransport()
{
    DbgIOLog(DEBUG_MASK_DISK_TRACE, ("Entered\n"));
	
    bool										bret;
    OSDictionary								*prop = NULL;
	
	// Check 
	if(fProtocolTransportNub) {
		DbgIOLog(DEBUG_MASK_DISK_INFO, ("Already Created.\n"));
		
		return true;
	}
	
	// Create NDASLogicalDevice.	  
    prop = OSDictionary::withCapacity(4);
    if(prop == NULL)
    {
        DbgIOLog(DEBUG_MASK_USER_ERROR, ("failed to alloc property\n"));
		
        return false;
    }
	
	// Icon.
	switch(fProvider->UnitDevice()->Type()) {
		case NMT_SINGLE:
		case NMT_AGGREGATE:
		case NMT_RAID0R2:
		case NMT_RAID1R3:
		{
			OSDictionary *iconDict = OSDictionary::withCapacity(2);
			
			iconDict->setObject("CFBundleIdentifier", OSString::withCStringNoCopy("com.ximeta.driver.NDASFamily"));
			iconDict->setObject("IOBundleResourceFile", OSString::withCStringNoCopy("NDAS.icns"));
			
			prop->setObject("Icon", iconDict);
			
			iconDict->release();
		}
			break;
		default:
			break;
	}
    
    fProtocolTransportNub = OSTypeAlloc(com_ximeta_driver_NDASProtocolTransportNub);
    
    if(fProtocolTransportNub == NULL) {
        DbgIOLog(DEBUG_MASK_USER_ERROR, ("failed to alloc Device class\n"));
		
		prop->release();
		
        return false;
    }
	
    DbgIOLog(DEBUG_MASK_USER_INFO, ("Protocol Transport NUB Device = %p\n", fProtocolTransportNub));
    
	// Init Device.
    bret = fProtocolTransportNub->init(prop);
    prop->release();
    if(bret == false)
    {
        fProtocolTransportNub->release();
        DbgIOLog(DEBUG_MASK_USER_ERROR, ("failed to init Device\n"));
		
        return false;
    }
	
	// Assign values.
	fProtocolTransportNub->setMaxRequestSectors(fProvider->maxRequestSectors());
	fProtocolTransportNub->setBlockSize(fProvider->blockSize());
	
	// Attach Device.
	bret = fProtocolTransportNub->attach(this);
	if(bret == false)
    {
        fProtocolTransportNub->release();
        DbgIOLog(DEBUG_MASK_USER_ERROR, ("failed to attach Device\n"));
		
        return false;
    }
	
	// Register Service.
	fProtocolTransportNub->registerService();
			
    return true;
}

bool com_ximeta_driver_NDASLogicalDeviceController::detachProtocolTransport()
{
	DbgIOLog(DEBUG_MASK_DISK_TRACE, ("Entered.\n"));
	
	if (fProtocolTransportNub == NULL) {
		DbgIOLog(DEBUG_MASK_USER_ERROR, ("ProtocolTransportNub Device Pointer is NULL\n"));
		
		return false;
	}
	
	// Send kIOMessageServiceIsRequestingClose message.
//	messageClient ( kIOMessageServiceIsRequestingClose, fProtocolTransportNub );
	
	// Send will Terminate
	fProtocolTransportNub->willTerminate(this, 0);
	
	// Terminate Device.
	fProtocolTransportNub->terminate();
	
	// Release Device.
	fProtocolTransportNub->release();	
	fProtocolTransportNub = NULL;
	
    return true;	
}

static IOReturn receiveMsgWrapper( OSObject * theDriver, void * first, void * second, void * third, void * fourth )
{
	com_ximeta_driver_NDASLogicalDeviceController	*controller = (com_ximeta_driver_NDASLogicalDeviceController*)theDriver;
	
	return controller->receiveMsg(first, second, third, fourth);
}

static void workerThreadWrapper(void* argument
#if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= MAC_OS_X_VERSION_10_4
, __unused wait_result_t wr)
#else
)
#endif
{		
	com_ximeta_driver_NDASLogicalDeviceController *controller = (com_ximeta_driver_NDASLogicalDeviceController*)argument;
	
	controller->workerThread(NULL);
	
	return;
}
