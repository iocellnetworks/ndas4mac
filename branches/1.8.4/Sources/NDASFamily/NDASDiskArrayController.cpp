/***************************************************************************
*
*  NDASDiskArrayContorller.cpp
*
*    NDAS Disk array Controller
*
*    Copyright (c) 2004 XiMeta, Inc. All rights reserved.
*
**************************************************************************/

#include <IOKit/IOLib.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOMessage.h>
#include <IOKit/scsi/SCSICommandOperationCodes.h>

#include "NDASDiskArrayController.h"

#include "NDASLogicalDevice.h"

#include "NDASDevice.h"

#include "NDASCommand.h"

#include "NDASBusEnumeratorUserClientTypes.h"

#include "Utilities.h"

extern "C" {
#include <pexpert/pexpert.h>//This is for debugging purposes ONLY
}

enum {
	kNDASPowerOffState = 0,
	kNDASPowerOnState = 1,
	kNumberNDASPowerStates = 2
};

#if 0
static IOPMPowerState powerStates[ kNumberNDASPowerStates ] = {
	{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 1, kIOPMDeviceUsable, kIOPMPowerOn, kIOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 }
};
#endif

// Define my superclass
#define super IOService

// REQUIRED! This macro defines the class's constructors, destructors,
// and several other methods I/O Kit requires. Do NOT use super as the
// second parameter. You must use the literal name of the superclass.

OSDefineMetaClassAndStructors(com_ximeta_driver_NDASDiskArrayController, IOService)

bool com_ximeta_driver_NDASDiskArrayController::init(OSDictionary *dict)
{
	bool res = super::init(dict);
	
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Initializing\n"));

	//fCommandQueueHead = fCommandQueueTail = fCurrentCommand = NULL;
//	fCommandQueueHead = fCommandQueueTail = NULL;
	
	for(int count = 0; count < MAX_NR_OF_TARGETS_PER_DEVICE; count++) {
		fTargetDevices[count] = NULL;
	}
		
	fInSleepMode = false;
	fWakeup = false;

	NDAS_clock_get_system_value(&lastBroadcastTime);
	
	fCommandArray = OSArray::withCapacity(0);
	
    return res;
}

void com_ximeta_driver_NDASDiskArrayController::free(void)
{
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Freeing\n"));
	
	fCommandArray->release();
	
	super::free();
}

IOService *com_ximeta_driver_NDASDiskArrayController::probe(IOService *provider, SInt32 *score)
{
    IOService *res = super::probe(provider, score);
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Probing\n"));
    return res;
}

bool com_ximeta_driver_NDASDiskArrayController::start(IOService *provider)
{
    bool res = super::start(provider);
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Starting\n"));
	
	fProvider = OSDynamicCast ( com_ximeta_driver_NDASDevice, provider );
	if(!fProvider) {
		goto exit;
	}
	
	//
	// Add a retain here so we can keep LanScsiDevice from doing garbage
	// collection on us when we are in the middle of our finalize method.
	//
	fProvider->retain ( );
	
	bool openSucceeded;
	
	openSucceeded = fProvider->open (this);
	if(openSucceeded == false) {
		goto exit;
	}
	
	fProvider->setController(this);
	
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
	
	// Create worker Thread.
	fThreadTerminate = false;
	
	if(KERN_SUCCESS != semaphore_create(kernel_task,
                                        &fCommandSema,
                                        SYNC_POLICY_FIFO, 0)) {
        
		DbgIOLog(DEBUG_MASK_PNP_ERROR, ("start::semaphore_create(), failed to Create Command Sema\n"));
		
        return false;
    }

	if(KERN_SUCCESS != semaphore_create(kernel_task,
                                        &fExitSema,
                                        SYNC_POLICY_FIFO, 0)) {
		
		DbgIOLog(DEBUG_MASK_PNP_ERROR, ("start::semaphore_create(), failed to Create Exit Sema\n"));
		
		semaphore_destroy(kernel_task, fCommandSema);

        return false;
    }
    
    fWorkerThread = IOCreateThread(workerThreadWrapper, this);
	if(!fWorkerThread) {
		DbgIOLog(DEBUG_MASK_PNP_ERROR, ("start::IOCreateThread(), failed to Create Thread\n"));

		semaphore_destroy(kernel_task, fExitSema);
		semaphore_destroy(kernel_task, fCommandSema);
		
		return false;
	}
	
    return res;
	
exit:
	return false;
}

bool com_ximeta_driver_NDASDiskArrayController::finalize (
														  IOOptionBits options 
														  )
{
	
	DbgIOLog(DEBUG_MASK_PNP_TRACE, ("finalizing.\n"));
	
	if ( fProvider ) {
		fProvider->release ( );
		fProvider = NULL;
	}
	
	return super::finalize ( options );
	
}

void com_ximeta_driver_NDASDiskArrayController::stop(IOService *provider)
{
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Stopping\n"));
			
	if (fCommandGate) {     
		fWorkLoop->removeEventSource(fCommandGate);     
		fCommandGate->release();     
		fCommandGate = 0; 
	} 
	
	// Destroy Semaphores.
	semaphore_destroy(kernel_task, fExitSema);
	semaphore_destroy(kernel_task, fCommandSema);

	fWorkerThread = NULL;
		
    super::stop(provider);
}

bool com_ximeta_driver_NDASDiskArrayController::enqueueCommand(com_ximeta_driver_NDASCommand *command)
{
	DbgIOLog(DEBUG_MASK_NDAS_TRACE, ("enqueueCommand: Entered\n"));
	
	if(fCommandGate) {
		fCommandGate->runCommand((void*)kNDASCommandQueueAppend, (void*)command);
		
		return true;
	} else {
		return false;
	}
}

IOReturn com_ximeta_driver_NDASDiskArrayController::receiveMsg ( void * newRequest, void *command, void *, void * )
{
	DbgIOLog(DEBUG_MASK_NDAS_TRACE, ("receiveMsg: Entered\n"));
	
	switch((int)newRequest) {
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
			
//			com_ximeta_driver_NDASCommand	*tempCommand = (com_ximeta_driver_NDASCommand *)fCommandArray->getObject(0);
						
			fCommandArray->removeObject(0);

			if( 0 < fCommandArray->getCount() ) {
			
				// Notify to the worker thread.
				semaphore_signal(fCommandSema);
			}			
		}
			break;
		default:
			DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("Invalid Command!!! %d\n", (int)newRequest));
			
			return kIOReturnInvalid;
	}
			
	return kIOReturnSuccess;
}

IOReturn receiveMsgWrapper( OSObject * theDriver, void * first, void * second, void * third, void * fourth )
{
	com_ximeta_driver_NDASDiskArrayController	*controller = (com_ximeta_driver_NDASDiskArrayController*)theDriver;
	
	return controller->receiveMsg(first, second, third, fourth);
}


void com_ximeta_driver_NDASDiskArrayController::workerThread(void* argument)
{		
	DbgIOLog(DEBUG_MASK_NDAS_TRACE, ("workerThread: Entered.\n"));
	
    do {
//		com_ximeta_driver_NDASCommand	*command;
		kern_return_t					sema_result;
		mach_timespec_t					wait_time;
		UInt32							currentTime;
		
		// Set timeout and wait.
		wait_time.tv_sec = NDAS_MAX_WAIT_TIME_FOR_PNP_MESSAGE;
        sema_result = semaphore_timedwait(fCommandSema, wait_time);
				
		switch(sema_result) {
			case KERN_SUCCESS:
			{
				// Check Command Queue.
				if(0 == fCommandArray->getCount()) {
					// Maybe Terminate signal.
					DbgIOLog(DEBUG_MASK_NDAS_TRACE, ("workerThread: No Command.\n"));
					
					continue;
				}
				
				com_ximeta_driver_NDASCommand *command = (com_ximeta_driver_NDASCommand *)fCommandArray->getObject(0);
				if( NULL == command ) {
					DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("workerThread: getObject return NULL.\n"));
					
					continue;
				}
				
				switch(command->command()) {
					case kNDASCommandChangedSetting:
					{
						// Update Last BroadCast Time.
						NDAS_clock_get_system_value(&lastBroadcastTime);
						processSettingCommand(command);
					}
						break;
					case kNDASCommandSRB:
					{												
						processSrbCommand(command);
					}
						break;
					default:
						DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("workerThread: Unknowen Command. %d\n", command->command()));
						
				}
				
				fCommandGate->runCommand((void *)kNDASCommandQueueCompleteCommand);
				
				// Check Broadcast Time.
				NDAS_clock_get_system_value(&currentTime);
				
				if(currentTime - lastBroadcastTime > NDAS_MAX_WAIT_TIME_FOR_PNP_MESSAGE) {
				
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
			}
				break;
			case KERN_OPERATION_TIMED_OUT:
			{
				DbgIOLog(DEBUG_MASK_NDAS_INFO, ("workerThread: Timed Out.\n"));
			
				if ( fWakeup ) {
					// Timer value is not correct. 
					fWakeup = false;
				} else {
					if(!fInSleepMode) {
						// No PnP Message and User command.
						processNoPnPMessageFromNDAS();
					}
				}
			}
				break;
			default:
				DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("workerThread: Invalid return value. %d\n", sema_result));
				break;
		}
		
		
	} while(fThreadTerminate == false);
		
	for(int count = 0; count < MAX_NR_OF_TARGETS_PER_DEVICE; count++) {
		
		if ( fTargetDevices[count] ) {			
			deleteLogicalDevice(count);
			
			fProvider->unmount(count);
		}
	}
	
	semaphore_signal(fExitSema);
	
	IOExitThread();
	
	// Never return.
}

void com_ximeta_driver_NDASDiskArrayController::processSettingCommand(com_ximeta_driver_NDASCommand *command)
{
	
	PPNPCommand	pnpCommand = command->pnpCommand();
	int result;
	
	// Discovery.
	result = fProvider->discovery();
	/*
	if ( kNDASStatusFailure != fProvider->status() ) {
		result = fProvider->discovery();
	} else {
		return;
	}
	*/

	// Check Data connection 
	for (int count = 0; count < MAX_NR_OF_TARGETS_PER_DEVICE; count++) {
		fProvider->checkDataConnection(count);		
	}

	if ( kNDASStatusOffline == result ) {
		for (int count = 0; count < MAX_NR_OF_TARGETS_PER_DEVICE; count++) {
						
			if ( kNDASUnitStatusMountRO == fProvider->targetStatus(count)
				 || kNDASUnitStatusMountRW == fProvider->targetStatus(count) ) {
				
				result = kNDASStatusOnline;
				
				goto out;
			}
		}
	}
	
	switch(fProvider->status()) {
		case kNDASStatusOffline:
		{
			// Get Unit disk info.
			if( kNDASStatusOnline == result ) {
				result = fProvider->getUnitDiskInfo();
			}
			
			DbgIOLog(DEBUG_MASK_NDAS_INFO, ("processSettingCommand: The Status of this NDAS changed to %d", fProvider->getState()));
			
		}
			break;
		case kNDASStatusOnline:
		{
			// Check every Targets.
			for(int count = 0; count < MAX_NR_OF_TARGETS_PER_DEVICE; count++) {
				
				bool	bNeedMount = false;
				bool	bNeedUnmount = false;
				bool	bNeedCreateDevice = false;
				bool	bNeedDeleteDevice = false;
	
				// Check BSD Name.
				if(fTargetDevices[count] != NULL) {
					fProvider->setTargetBSDName(count, fTargetDevices[count]->bsdName());
				}
								
				if ( fProvider->targetNeedRefresh(count) ) {
					result = fProvider->getUnitDiskInfo();
					
					fProvider->setTargetNeedRefresh(count, false);	
				}
				
				switch(fProvider->targetStatus(count)) {
					case kNDASUnitStatusNotPresent:
						break;
					case kNDASUnitStatusUnmount:
					{
//						UInt32	status;
						
						// Need Mount?
						switch(fProvider->targetConfiguration(count)) {
							case kNDASUnitConfigurationMountRW:
							{
								bNeedMount = true;
								
								if(fTargetDevices[count] == NULL) {
									bNeedCreateDevice = true;
								}
							}
								break;
							case kNDASUnitConfigurationMountRO:
							{
								bNeedMount = true;
								if(fTargetDevices[count] == NULL) {
									bNeedCreateDevice = true;
								}
							}
								break;
							default:
								break;
						}
					}
						break;
					case kNDASUnitStatusMountRO:
					{
//						UInt32	status;
						
						switch(fProvider->targetConfiguration(count)) {
							case kNDASUnitConfigurationMountRW:
							case kNDASUnitConfigurationUnmount:
							{
								// Need Unmount.
								bNeedUnmount = true;
								bNeedDeleteDevice = true;
							}
								break;
							case kNDASUnitConfigurationMountRO:
							{
								// Interface Change.
								if(pnpCommand->fInterfaceChanged) {
								
									DbgIOLog(DEBUG_MASK_NDAS_INFO, ("processSettingCommand: Interface Changed.\n"));
								
									bNeedUnmount = true;
									//fProvider->unmount(count);
								} else {
								
									// Check Connection.
									if(!fProvider->targetConnected(count)) {
										
										DbgIOLog(DEBUG_MASK_NDAS_WARING, ("processSettingCommand: Connection Disconnected!!!\n"));
										
										// Need Reconnect.									
										bNeedMount = true;
									}
								}
							}
								break;
							default:
								break;
						}
					}
						break;
					case kNDASUnitStatusMountRW:
					{
//						UInt32	status;
						
						// Need Unmount?
						switch(fProvider->targetConfiguration(count)) {
							case kNDASUnitConfigurationMountRO:
							case kNDASUnitConfigurationUnmount:
							{
								bNeedUnmount = true;
								bNeedDeleteDevice = true;
							}
								break;
							case kNDASUnitConfigurationMountRW:
							{
								// Interface Change.
								if(pnpCommand->fInterfaceChanged) {
									
									DbgIOLog(DEBUG_MASK_NDAS_WARING, ("processSettingCommand: Interface Changed.\n"));
									
									bNeedUnmount = true;
									//fProvider->unmount(count);
								} else {
								
									// Check Connection.
									if(!fProvider->targetConnected(count)) {
									
										DbgIOLog(DEBUG_MASK_NDAS_WARING, ("processSettingCommand: Connection Disconnected!!!\n"));
										
										// Need Reconnect.
										bNeedMount = true;
									}
								}
							}
								break;
							default:
								break;
						}
						
					}
					default:
						break;
				}

				//
				// Mount, Unmount...
				// 
				if(bNeedMount) {
					UInt32 status, prevStatus;
					
					prevStatus = fProvider->targetStatus(count);
					status = fProvider->mount(count, pnpCommand);
					
					if((status == kNDASUnitStatusMountRW
						|| status == kNDASUnitStatusMountRO))
					{
						//
						// Create Logical Device.
						//
						if(bNeedCreateDevice)
						{
							// Create Logical Device.
							if(createLogicalDevice(count) != kIOReturnSuccess) {
								fProvider->unmount(count);
							}
						}
						
						//
						// Process pending command caused by disconnection.
						//
						com_ximeta_driver_NDASCommand* command = fProvider->targetPendingCommand(count);
						if(NULL != command) {

							DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("processSettingCommand: Processing Pending Command\n"));
							
							fProvider->setTargetPendingCommand(count, NULL);
							
							if(fTargetDevices[count] != NULL) {
								
								enqueueCommand(command);
								/*
								PSCSICommand	pScsiCommand;
								
								pScsiCommand = command->scsiCommand();
								
								if(kIOReturnSuccess != pScsiCommand->CmdData.results) {
									pScsiCommand->CmdData.results = kIOReturnBusy;
									pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_ProtocolTimeoutOccurred;
								}
								
								fTargetDevices[count]->completeCommand(command);
								 */
							} else {
								DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("processSettingCommand: fTargetDevice is NULL!!!\n"));
							}
						}
					} else {
						if(kNDASUnitStatusMountRW == prevStatus) {
							bNeedUnmount = true;
							bNeedDeleteDevice = true;
						}
					}
				}
				
				if(bNeedUnmount) {
					
					if(bNeedDeleteDevice) {
						if(deleteLogicalDevice(count) == kIOReturnSuccess
						   && bNeedUnmount) {
							fProvider->unmount(count);
						} 
					} else {
						fProvider->unmount(count);
					}
				}
			}
		}
			break;
		case kNDASStatusNoPermission:
		case kNDASStatusFailure:
		{
			if( kNDASStatusOnline == result ) {
				result = fProvider->getUnitDiskInfo();
				
				for(int count = 0; count < MAX_NR_OF_TARGETS_PER_DEVICE; count++) {
					fProvider->setTargetNeedRefresh(count, false);
				}
			}
		}
			break;
		default:
			DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("processSettingCommand: Bad Status %d", fProvider->status()));
			break;
	}

out:
		
	// Change Status.
	fProvider->setStatus(result);
	
	fProvider->completePnPMessage(command);
		
	return;
}

void com_ximeta_driver_NDASDiskArrayController::processSrbCommand(com_ximeta_driver_NDASCommand *command)
{
	PSCSICommand	pScsiCommand;
	
	DbgIOLog(DEBUG_MASK_DISK_TRACE, ("processSrbCommand: Entered.\n"));
			
	pScsiCommand = command->scsiCommand();
	
	if(fTargetDevices[pScsiCommand->targetNo] == NULL) {
		DbgIOLog(DEBUG_MASK_DISK_ERROR, ("processSrbCommand: target Device is NULL!!!\n"));
		
		return;
	}
	
	if( 0 != pScsiCommand->LogicalUnitNumber) {
		
		DbgIOLog(DEBUG_MASK_DISK_WARING, ("processSrbCommand: LU Number %d\n", pScsiCommand->LogicalUnitNumber));
		
		pScsiCommand->CmdData.results = kIOReturnNotAttached;
				
		fTargetDevices[pScsiCommand->targetNo]->completeCommand(command);
		
		return;
	}

	if(fInSleepMode) {
		
		DbgIOLog(DEBUG_MASK_DISK_INFO, ("processSrbCommand: In Sleep Mode\n"));
		
		switch ( pScsiCommand->cdb[0] ) {
			case kSCSICmd_READ_10:
			case kSCSICmd_WRITE_10:
			{
				pScsiCommand->CmdData.results = kIOReturnBusy;
				pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_No_Status;
				
				fTargetDevices[pScsiCommand->targetNo]->completeCommand(command);
				
				return;
			}				
			default:
				break;
		}
	} else {
		// Check Connection.
		if(!fProvider->targetConnected(pScsiCommand->targetNo)) {
			switch ( pScsiCommand->cdb[0] ) {
				case kSCSICmd_READ_10:
				case kSCSICmd_WRITE_10:
				{
					// Pendig Command.
					fProvider->setTargetPendingCommand(pScsiCommand->targetNo, command);
					
					return;
				}
					break;
				default:
					break;
			}
		}
	}
	
	// Send SRB.
	fProvider->processSrbCommand(pScsiCommand);
	
	// Drop Command If terminated.	
	if(fTargetDevices[pScsiCommand->targetNo] == NULL) {
		DbgIOLog(DEBUG_MASK_DISK_ERROR, ("processSrbCommand: target Device is NULL!!!\n"));
		
		return;
	} else {
		
		// Check Sleep mode
		if(fInSleepMode) {
			
			if(kIOReturnSuccess == pScsiCommand->CmdData.results) {
				// Send completion.	
				fTargetDevices[pScsiCommand->targetNo]->completeCommand(command);
			} else {
				pScsiCommand->CmdData.results = kIOReturnBusy;
				pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_No_Status;
				fTargetDevices[pScsiCommand->targetNo]->completeCommand(command);
				
				// Pendig Command.
				//fProvider->setTargetPendingCommand(pScsiCommand->targetNo, command);
			}
		} else {
			// Check Disconnection.
			if(fProvider->targetConnected(pScsiCommand->targetNo)) {
				// Send completion.	
				fTargetDevices[pScsiCommand->targetNo]->completeCommand(command);
			} else {	
				if(kIOReturnSuccess == pScsiCommand->CmdData.results) {
					// Send completion.	
					fTargetDevices[pScsiCommand->targetNo]->completeCommand(command);
				} else {
					// Pendig Command.
					fProvider->setTargetPendingCommand(pScsiCommand->targetNo, command);
				}
			}
		}
	}
	return;
}

void com_ximeta_driver_NDASDiskArrayController::processNoPnPMessageFromNDAS()
{
	DbgIOLog(DEBUG_MASK_NDAS_TRACE, ("processNoPnPMessageFromNDAS: Entered\n"));
	
	switch(fProvider->status()) {
		case kNDASStatusOffline:
		case kNDASStatusNoPermission:
		case kNDASStatusFailure:
		{
			// Set to offline.
			fProvider->setStatus(kNDASStatusOffline);
		}
			break;
		case kNDASStatusOnline:
		{
			UInt32	mountedTargets = 0;
			
			// Check every Targets.
			for(int count = 0; count < MAX_NR_OF_TARGETS_PER_DEVICE; count++) {
				
				// Check Data Connection.
				fProvider->checkDataConnection(count);
				
				switch(fProvider->targetStatus(count)) {
					case kNDASUnitStatusMountRO:
					case kNDASUnitStatusMountRW:
					{
						if(!fProvider->targetConnected(count)
						    &&fTargetDevices[count] != NULL) {
							// No PnP Message from NDAS. Remove it!
							DbgIOLog(DEBUG_MASK_NDAS_WARING, ("processNoPnPMessageFromNDAS: NDAS %d is really disconnected.\n", fProvider->slotNumber()));
							
							if(deleteLogicalDevice(count) == kIOReturnSuccess) {
								fProvider->unmount(count);
							}
						} else {
							mountedTargets++;
						}
					}
						break;
					case kNDASUnitStatusUnmount:
					default:
						break;
				}
			}
			
			if(mountedTargets == 0) {
				// Set to offline.
				fProvider->setStatus(kNDASStatusOffline);
			}
		}
			break;
		default:
			DbgIOLog(DEBUG_MASK_NDAS_INFO, ("processSettingCommand: Bad Status %d", fProvider->getState()));
			break;
	}
	
	return;
}

int com_ximeta_driver_NDASDiskArrayController::createLogicalDevice(UInt32 targetNo)
{
	bool								bret;
	OSDictionary						*prop = NULL;
    com_ximeta_driver_NDASLogicalDevice	*NDASLogicalDevicePtr = NULL;

	prop = OSDictionary::withCapacity(4);
    if(prop == NULL)
    {
        DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("createLogicalDevice: failed to alloc property\n"));
		
        return kIOReturnError;
    }
	
	// Icon.
	if(kNDASUnitTypeHDD == fProvider->targetType(targetNo)) {
		
		OSDictionary *iconDict = OSDictionary::withCapacity(2);
		
		iconDict->setObject("CFBundleIdentifier", OSString::withCStringNoCopy("com.ximeta.driver.NDASFamily"));
		iconDict->setObject("IOBundleResourceFile", OSString::withCStringNoCopy("NDAS.icns"));
		
		prop->setObject("Icon", iconDict);
		
		iconDict->release();
	}
	
    NDASLogicalDevicePtr = OSDynamicCast(com_ximeta_driver_NDASLogicalDevice, 
								  OSMetaClass::allocClassWithName(NDAS_LOGICAL_DEVICE_CLASS));
    
    if(NDASLogicalDevicePtr == NULL) {
        DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("createLogicalDevice: failed to alloc Device class\n"));
		
		prop->release();
		
        return kIOReturnError;
    }
	
    DbgIOLog(DEBUG_MASK_NDAS_INFO, ("createLogicalDevice: LanScsiDevice = %p\n", NDASLogicalDevicePtr));
    
	// Init Device.
    bret = NDASLogicalDevicePtr->init(prop);
    prop->release();
    if(bret == false)
    {
        NDASLogicalDevicePtr->release();
        DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("createLogicalDevice: failed to init Device\n"));
		
        return kIOReturnError;
    }
	
	// Assign attributes.
	NDASLogicalDevicePtr->setTargetNo(targetNo);
	NDASLogicalDevicePtr->setController(this);
	NDASLogicalDevicePtr->setMaxRequestBlocks(fProvider->targetMaxRequestBlocks(targetNo));
		
	// Attach Device.
	bret = NDASLogicalDevicePtr->attach(this);
	if(bret == true) {
		// Register Service.
		NDASLogicalDevicePtr->registerService();
	}
	
	fTargetDevices[targetNo] = NDASLogicalDevicePtr;
	
	return kIOReturnSuccess;
}

static void terminateDeviceThread(void* argument)
{		
	com_ximeta_driver_NDASLogicalDevice	*NDASLogicalDevicePtr = NULL;
		
	NDASLogicalDevicePtr = (com_ximeta_driver_NDASLogicalDevice *)argument;
	
	while(!NDASLogicalDevicePtr->isInactive()) {
		IOSleep(100);	// 100 ms
	};

	// Release Device.
	NDASLogicalDevicePtr->release();
	
	IOExitThread();
	
	// Never return.
}

int com_ximeta_driver_NDASDiskArrayController::deleteLogicalDevice(UInt32 targetNo)
{
//	bool								bret;
//	OSDictionary						*prop = NULL;
    com_ximeta_driver_NDASLogicalDevice	*NDASLogicalDevicePtr = NULL;
		
	NDASLogicalDevicePtr = fTargetDevices[targetNo];
						
	DbgIOLog(DEBUG_MASK_NDAS_TRACE, ("deleteLogicalDevice: Entered. 0x%x\n", NDASLogicalDevicePtr));
	
	if(NDASLogicalDevicePtr == NULL) {
		DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("deleteLogicalDevice: Can't Find Registed Slot Number %d.\n", targetNo));
		
		return kIOReturnError;
	} else {
		fTargetDevices[targetNo] = NULL;
	}
	
	//
	// Process pending command.
	//
	com_ximeta_driver_NDASCommand* command = fProvider->targetPendingCommand(targetNo);
	
	if(NULL != command) {
		
		DbgIOLog(DEBUG_MASK_NDAS_INFO, ("deleteLogicalDevice: Processing Pending Command\n"));
		
		fProvider->setTargetPendingCommand(targetNo, NULL);
		
		if(NDASLogicalDevicePtr != NULL) {
			
			 PSCSICommand	pScsiCommand;
			 
			 pScsiCommand = command->scsiCommand();
			 
			 if(kIOReturnSuccess != pScsiCommand->CmdData.results) {
				 pScsiCommand->CmdData.results = kIOReturnBusy;
				 pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_ProtocolTimeoutOccurred;
			 }
			 
			 NDASLogicalDevicePtr->completeCommand(command);
		}
	}
	
	// Send kIOMessageServiceIsRequestingClose message.
	messageClient ( kIOMessageServiceIsRequestingClose, NDASLogicalDevicePtr );

	// Terminate Device.
	NDASLogicalDevicePtr->terminate();
	
	NDASLogicalDevicePtr->willTerminate(this, NULL);
		
	//IOCreateThread(terminateDeviceThread, NDASLogicalDevicePtr);	
	
	NDASLogicalDevicePtr->release();
	
	// Delete BSD Name.
	fProvider->setTargetBSDName(targetNo, NULL);

	return kIOReturnSuccess;
}

void workerThreadWrapper(void* argument)
{		
	com_ximeta_driver_NDASDiskArrayController *controller = (com_ximeta_driver_NDASDiskArrayController*)argument;
	
	controller->workerThread(NULL);
	
	return;
}

IOReturn com_ximeta_driver_NDASDiskArrayController::message (
															 UInt32 		type,
															 IOService *	nub,
															 void * 		arg 
															 )
{
	DbgIOLog(DEBUG_MASK_NDAS_TRACE, ("message Entered.\n"));
	
	IOReturn				status		= kIOReturnSuccess;
	
	switch ( type )
	{
		
		case kIOMessageServiceIsSuspended:
		{
			DbgIOLog(DEBUG_MASK_POWER_INFO, ("kIOMessageServiceIsSuspended\n"));

			// Now go to sleep.
			fInSleepMode = true;
					
			messageClients(kIOMessageServiceIsSuspended);
			
		}
			break;
			
		case kIOMessageServiceIsResumed:
		{
			DbgIOLog(DEBUG_MASK_POWER_INFO, ("kIOMessageServiceIsResumed\n"));
			
			fInSleepMode = false;
			
			fWakeup = true;
						
			// Disconnect.
			for(int count = 0; count < MAX_NR_OF_TARGETS_PER_DEVICE; count++) {
				if(fProvider->targetConnected(count)) {
					
					fProvider->setTargetConnected(count, false);
					fProvider->cleanupDataConnection(count);
					//fProvider->unmount(count);
				}			
			}
			
			messageClients(kIOMessageServiceIsResumed);
		}
			break;
		case kIOMessageServiceIsRequestingClose:
		{
			DbgIOLog(DEBUG_MASK_NDAS_INFO, ("kIOMessageServiceIsRequestingClose\n"));
		
			// Terminate Thread.
			fThreadTerminate = true;
			
			semaphore_signal(fCommandSema);
			
			// Wait Until the thread terminated.
			semaphore_wait(fExitSema);
			
			if ( fProvider != NULL ) {		
				
				fProvider->setController(NULL);
				
				if ( fProvider->isOpen ( this ) ) {
					fProvider->close ( this );
				}
			}
			
		}
			break;
			
		case kIOMessageServiceIsTerminated:
			DbgIOLog(DEBUG_MASK_NDAS_INFO, ("kIOMessageServiceIsTerminated\n"));
			
			if ( fProvider != NULL ) {		
				
				fProvider->setController(NULL);
				
				if ( fProvider->isOpen ( this ) ) {
					fProvider->close ( this );
				}
			}
				
			break;
			
		default:
			status = IOService::message (type, nub, arg);
			break;
			
	}
	
	return status;
	
}
