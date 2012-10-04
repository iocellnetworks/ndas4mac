/***************************************************************************
*
*  NDASDeviceContorller.cpp
*
*    NDAS Device Controller
*
*    Copyright (c) 2004 XiMeta, Inc. All rights reserved.
*
**************************************************************************/

#include <IOKit/IOLib.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOMessage.h>
#include <IOKit/scsi/SCSICommandOperationCodes.h>

#include "NDASDeviceController.h"

#include "NDASBusEnumerator.h"
#include "NDASPhysicalUnitDevice.h"

#include "NDASDevice.h"

#include "NDASCommand.h"

#include "NDASBusEnumeratorUserClientTypes.h"
#include "NDASFamilyIOMessage.h"

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

OSDefineMetaClassAndStructors(com_ximeta_driver_NDASDeviceController, IOService)

bool com_ximeta_driver_NDASDeviceController::init(OSDictionary *dict)
{
	bool res = super::init(dict);
	
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Initializing\n"));

	//fCommandQueueHead = fCommandQueueTail = fCurrentCommand = NULL;
//	fCommandQueueHead = fCommandQueueTail = NULL;
	
	fUnits = NULL;
	
//	for(int count = 0; count < MAX_NR_OF_TARGETS_PER_DEVICE; count++) {
//		fTargetDevices[count] = NULL;
//	}
		
	fInSleepMode = false;
	fWakeup = false;
	fNeedCompareTargetData = false;
	fWorkerThread = NULL;
	
	NDAS_clock_get_system_value(&lastBroadcastTime);
	
	fCommandArray = OSArray::withCapacity(0);
		
    return res;
}

void com_ximeta_driver_NDASDeviceController::free(void)
{
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Freeing\n"));
	
	fCommandArray->release();
	
	if (cntrlSync && cntrlSync != (IOWorkLoop *)1) {
		cntrlSync->release();
	}
	
	super::free();
}

IOService *com_ximeta_driver_NDASDeviceController::probe(IOService *provider, SInt32 *score)
{
    IOService *res = super::probe(provider, score);
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Probing\n"));
    return res;
}

bool com_ximeta_driver_NDASDeviceController::start(IOService *provider)
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
	
	// Allocate Unit set.
	fUnits = OSArray::withCapacity(INITIAL_NUMBER_OF_DEVICES);
	if(fUnits == NULL) {
		 
		DbgIOLog(DEBUG_MASK_PNP_ERROR, ("Start: Can't Allocate Device Array with Capacity %d\n", INITIAL_NUMBER_OF_DEVICES));
		
		return false;
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
	
	if(KERN_SUCCESS != semaphore_create(current_task(),
                                        &fCommandSema,
                                        SYNC_POLICY_FIFO, 0)) {
        
		DbgIOLog(DEBUG_MASK_PNP_ERROR, ("start::semaphore_create(), failed to Create Command Sema\n"));
		
        return false;
    }
    
    fWorkerThread = IOCreateThread(workerThreadWrapper, this);
	if(!fWorkerThread) {
		DbgIOLog(DEBUG_MASK_PNP_ERROR, ("start::IOCreateThread(), failed to Create Thread\n"));

		semaphore_destroy(kernel_task, fCommandSema);
		
		return false;
	}
	
    return res;
	
exit:
	return false;
}

bool com_ximeta_driver_NDASDeviceController::finalize (
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
	
	DbgIOLog(DEBUG_MASK_PNP_INFO, ("Stop: Before wait Sema\n"));
	
	// Wait Thread termination.
	bool waitMore = true;
	
	while (waitMore) {
		waitMore = false;
		
		IOSleep(1000);
		
		if (fWorkerThread) {
			DbgIOLog(DEBUG_MASK_USER_ERROR, ("Stop: Thread is still running!\n"));
			
			waitMore = true;
			
			semaphore_signal(fCommandSema);
		}		
	}
	
	DbgIOLog(DEBUG_MASK_PNP_INFO, ("Stop: After Wait Sema\n"));
	
	if ( fProvider != NULL ) {		
		
		fProvider->setController(NULL);
		
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

void com_ximeta_driver_NDASDeviceController::stop(IOService *provider)
{
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Stopping\n"));
		
	if (fCommandGate) {
		
		// Free all command.
		while (fCommandArray->getCount() > 0) {			
			fCommandArray->removeObject(0);
			DbgIOLog(DEBUG_MASK_PNP_ERROR, ("stop: remove Command!!!\n"));
		}
		
		fWorkLoop->removeEventSource(fCommandGate);     
		fCommandGate->release();     
		fCommandGate = 0; 
	} 
	
	// Destroy Semaphores.
	semaphore_destroy(current_task(), fCommandSema);
		
	// Free Device Array.
	fUnits->flushCollection();
	fUnits->release();
	fUnits = NULL;
	
    super::stop(provider);
}

IOWorkLoop *com_ximeta_driver_NDASDeviceController::getWorkLoop()
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

bool com_ximeta_driver_NDASDeviceController::enqueueCommand(com_ximeta_driver_NDASCommand *command)
{
	DbgIOLog(DEBUG_MASK_NDAS_TRACE, ("enqueueCommand: Entered\n"));
	
	if (fThreadTerminate) {
		return false;
	}
	
	if (kNDASCommandTerminateThread == command->command()) {
		fThreadTerminate = true;
	}
	
	if(fCommandGate) {
		fCommandGate->runCommand((void*)kNDASCommandQueueAppend, (void*)command);
		
		return true;
	} else {
		return false;
	}
}

bool com_ximeta_driver_NDASDeviceController::executeAndWaitCommand(com_ximeta_driver_NDASCommand *command)
{
	DbgIOLog(DEBUG_MASK_NDAS_TRACE, ("executeCommand: Entered\n"));
	
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

IOReturn com_ximeta_driver_NDASDeviceController::receiveMsg ( void * newRequest, void *command, void *, void * )
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
			
			//com_ximeta_driver_NDASCommand	*tempCommand = (com_ximeta_driver_NDASCommand *)fCommandArray->getObject(0);
		
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

static IOReturn receiveMsgWrapper( OSObject * theDriver, void * first, void * second, void * third, void * fourth )
{
	com_ximeta_driver_NDASDeviceController	*controller = (com_ximeta_driver_NDASDeviceController*)theDriver;
	
	return controller->receiveMsg(first, second, third, fourth);
}


void com_ximeta_driver_NDASDeviceController::workerThread(void* argument)
{		
//	bool bTerminate = false;
	
	DbgIOLog(DEBUG_MASK_NDAS_TRACE, ("workerThread: Entered.\n"));
	
	this->retain();
	
    do {
		kern_return_t					sema_result;
		mach_timespec_t					wait_time = { 0 };
//		UInt32							currentTime;
		
		// Set timeout and wait.
		wait_time.tv_sec = NDAS_MAX_WAIT_TIME_FOR_PNP_MESSAGE;
        sema_result = semaphore_timedwait(fCommandSema, wait_time);
				
		switch(sema_result) {
			case KERN_SUCCESS:
			{
				// Check Command Queue.
				if(0 == fCommandArray->getCount()) {
					// Maybe Terminate signal.
					DbgIOLog(DEBUG_MASK_NDAS_INFO, ("workerThread: No Command.\n"));
					
					fThreadTerminate = true;
					
					continue;
				}
				
				com_ximeta_driver_NDASCommand *command = OSDynamicCast(com_ximeta_driver_NDASCommand, fCommandArray->getObject(0));
				if( NULL == command ) {
					DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("workerThread: getObject return NULL.\n"));
					
					continue;
				}
				
				switch(command->command()) {
					case kNDASCommandTerminateThread:
					{
//						bTerminate = true;
						continue;
					}
						break;
					case kNDASCommandChangedSetting:
					{
						// Update Last BroadCast Time.
						NDAS_clock_get_system_value(&lastBroadcastTime);
						processSettingCommand(command);
					}
						break;
					case kNDASCommandRefresh:
					{
						DbgIOLog(DEBUG_MASK_NDAS_INFO, ("workerThread: Refresh Command.\n"));
						
						com_ximeta_driver_NDASPhysicalUnitDevice *device = NULL;
						
						device = OSDynamicCast(com_ximeta_driver_NDASPhysicalUnitDevice,
											   fUnits->getObject(command->refreshCommand()->fTargetNumber));
						
						if(device) {
							device->setNeedRefresh(true);
						}
											   
					}
						break;
					case kNDASCommandSRB:
					{												
						processSrbCommand(command);
					}
						break;
					case kNDASCommandManagingIO:
					{
						//if ( kNDASUnitStatusUnmount == status() ) {
							
						processManagingIOCommand(command);
							
						//} else {
						//	DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("workerThread: Can't Doing Managing IO, status %d.\n", status()));
						//}
					}
						break;
					default:
						DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("workerThread: Unknowen Command. %d\n", command->command()));
						
				}
				
				command->setToComplete();
				
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
		
	fWorkerThread = NULL;
	
	this->release();

	IOExitThread();
	
	// Never return.
}

void com_ximeta_driver_NDASDeviceController::processSettingCommand(com_ximeta_driver_NDASCommand *command)
{
	
	PPNPCommand	pnpCommand = command->pnpCommand();
	int result = fProvider->status();
	TARGET_DATA	Targets[MAX_NR_OF_TARGETS_PER_DEVICE] = { 0 };
//	int count;
	
	// Check Data connection 
	for (int count = 0; count < MAX_NR_OF_TARGETS_PER_DEVICE; count++) {
		com_ximeta_driver_NDASPhysicalUnitDevice *device;
		
		device = OSDynamicCast(com_ximeta_driver_NDASPhysicalUnitDevice, fUnits->getObject(count));
		
		if (device) {
			fProvider->checkDataConnection(device);		
		}
	}
	
	// Check Target Data.
	if (fNeedCompareTargetData 
		&& kNDASStatusOnline == result) {
		
		result = fProvider->getUnitDiskInfo(Targets);
		
		if (kNDASStatusOnline ==  result) {
			
			fNeedCompareTargetData = false;
			
			for(int count = 0; count < MAX_NR_OF_TARGETS_PER_DEVICE; count++) {
				
				com_ximeta_driver_NDASPhysicalUnitDevice *device;
				
				if ( true == Targets[count].bPresent ) {
					
					if ( device = OSDynamicCast(com_ximeta_driver_NDASPhysicalUnitDevice, fUnits->getObject(count)) ) {
						
						if ( 0 != memcmp(Targets[count].model, device->TargetData()->model, 42)
							 || 0 != memcmp(Targets[count].firmware, device->TargetData()->firmware, 10)
							 || 0 != memcmp(Targets[count].serialNumber, device->TargetData()->serialNumber, 22)
							 || Targets[count].SectorCount != device->TargetData()->SectorCount) {
							
							// Unit was replaced.
							DbgIOLog(DEBUG_MASK_NDAS_WARNING, ("processSettingCommand: Unit was replaced.\n"));
							
							device->setStatus(kNDASUnitStatusNotPresent);
							
							fProvider->setStatus(kNDASStatusOffline);
							
							updateUnitDevice(count, NULL, false, false);					
							
						} else {
							DbgIOLog(DEBUG_MASK_NDAS_WARNING, ("processSettingCommand: Same Unit.\n"));
						}
					}					
				} else {
					if ( device = OSDynamicCast(com_ximeta_driver_NDASPhysicalUnitDevice, fUnits->getObject(count)) ) {
						updateUnitDevice(count, NULL, false, false);					
					}
					
				}
			}
		}
	}
	
	switch(fProvider->status()) {
		case kNDASStatusOffline:
		case kNDASStatusNoPermission:
		case kNDASStatusFailure:
		{
			// Discovery.
			result = fProvider->discovery(Targets);

			// Get Unit disk info.
			if( kNDASStatusOnline == result ) {
				result = fProvider->getUnitDiskInfo(Targets);
				
				if (kNDASStatusOnline == result) {
					//
					// Create Unit Devices.
					//
					for(int count = 0; count < MAX_NR_OF_TARGETS_PER_DEVICE; count++) {

						if ( true == Targets[count].bPresent ) {
							createUnitDevice(count, Targets);
						}
					}
					
				}
			} else {
				// We need cleanup.
				for(int count = 0; count < MAX_NR_OF_TARGETS_PER_DEVICE; count++) {			
					
					if( NULL != fUnits->getObject(count) ) {
						updateUnitDevice(count, NULL, false, false);
					}
				}
			}
			
			DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("processSettingCommand: The Status of this NDAS changed to %d", fProvider->getState()));
			
		}
			break;
		case kNDASStatusOnline:
		{
			bool validTargetData = false;
			bool Connected = false;
			
			// Get Target Data.
			for(int count = 0; count < MAX_NR_OF_TARGETS_PER_DEVICE; count++) {

				com_ximeta_driver_NDASPhysicalUnitDevice *device;
				
				if ( device = OSDynamicCast(com_ximeta_driver_NDASPhysicalUnitDevice, fUnits->getObject(count)) ) {
				
					if (device->IsConnected()) {

						Connected = true;
						
						if (kNDASStatusOnline == fProvider->getTargetData(Targets, device)) {
							validTargetData = true;
							break;
						} else {
							DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("processSettingCommand: Can't get Target Data %d\n", count));
							
							fProvider->cleanupDataConnection(device, true);
						}
					}
				}
			}

			//
			// Update UnitDevices.
			//
			if (!validTargetData
				&& !Connected) {
				result = fProvider->discovery(Targets);
			}
			
			//
			// Defer delete Unit device after unmounting.
			//
			if (kNDASStatusOnline != result) {
				for(int count = 0; count < MAX_NR_OF_TARGETS_PER_DEVICE; count++) {
					
					com_ximeta_driver_NDASPhysicalUnitDevice *device;
					
					if ( (device = OSDynamicCast(com_ximeta_driver_NDASPhysicalUnitDevice, fUnits->getObject(count))) 
						 && device->IsInRunningStatus() ) {
					
						result = kNDASStatusOnline;
						
						Targets[count].bPresent = true;
						Targets[count].NRROHost = device->NRROHosts();
						Targets[count].NRRWHost = device->NRRWHosts();
					}
				}
			}
			
			switch(result) {
				case kNDASStatusOnline:
				{
					
					//
					// Update UnitDevices.
					//
					for(int count = 0; count < MAX_NR_OF_TARGETS_PER_DEVICE; count++) {
						
						if ( true == Targets[count].bPresent ) {
							
							updateUnitDevice(count, &Targets[count], pnpCommand->fInterfaceChanged, true);
						}
					}
				} 
					break;
				case kNDASStatusFailure:
				{
					// Delete Unit Devies.
					for(int count = 0; count < MAX_NR_OF_TARGETS_PER_DEVICE; count++) {
						
						com_ximeta_driver_NDASPhysicalUnitDevice *device;
						
						if ( device = OSDynamicCast(com_ximeta_driver_NDASPhysicalUnitDevice, fUnits->getObject(count)) ) {							
							
							updateUnitDevice(count, NULL, false, false);
							
							deleteUnitDevice(count);
						}
					}
				}
					break;
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

void com_ximeta_driver_NDASDeviceController::processSrbCommand(com_ximeta_driver_NDASCommand *command)
{
	PSCSICommand	pScsiCommand;
	
	DbgIOLog(DEBUG_MASK_DISK_TRACE, ("processSrbCommand: Entered.\n"));

	pScsiCommand = command->scsiCommand();

	com_ximeta_driver_NDASPhysicalUnitDevice	*physicalDevice = NULL;		
	
	physicalDevice = OSDynamicCast(com_ximeta_driver_NDASPhysicalUnitDevice, pScsiCommand->device);
	if(NULL == physicalDevice) {
		
		DbgIOLog(DEBUG_MASK_DISK_ERROR, ("processSrbCommand: No Physical Device!!!\n"));
		
		return;
	}
	
	
//	if(fTargetDevices[pScsiCommand->targetNo] == NULL) {
//		DbgIOLog(DEBUG_MASK_DISK_ERROR, ("processSrbCommand: target Device is NULL!!!\n"));
		
//		return;
//	}

	if( !physicalDevice->IsInRunningStatus() ) {
		
		DbgIOLog(DEBUG_MASK_DISK_WARNING, ("processSrbCommand: Not Mounted\n"));
		
		pScsiCommand->CmdData.results = kIOReturnNotAttached;
		pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_DeliveryFailure;
		
		physicalDevice->completeCommand(command);
		
		return;
	}
	
	if( 0 != pScsiCommand->LogicalUnitNumber) {
		
		DbgIOLog(DEBUG_MASK_DISK_WARNING, ("processSrbCommand: LU Number %d\n", pScsiCommand->LogicalUnitNumber));
		
		pScsiCommand->CmdData.results = kIOReturnNotAttached;
		pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_DeviceNotPresent;
		
		physicalDevice->completeCommand(command);
		
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
				
				physicalDevice->completeCommand(command);
				
				return;
			}				
			default:
				break;
		}
	} else {
		// Check Connection.
		if(!physicalDevice->IsConnected()) {
			switch ( pScsiCommand->cdb[0] ) {
				case kSCSICmd_READ_10:
				case kSCSICmd_WRITE_10:
				{
					// Pendig Command.
					physicalDevice->setPendingCommand(command);
					
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
		
	// Check Sleep mode
	if(fInSleepMode) {
		
		if(kIOReturnSuccess == pScsiCommand->CmdData.results) {
			// Send completion.	
			physicalDevice->completeCommand(command);
		} else {
			pScsiCommand->CmdData.results = kIOReturnBusy;
			pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_No_Status;
			physicalDevice->completeCommand(command);
			
			// Pendig Command.
			//physicalDevice->setPendingCommand(command);
		}
	} else {
		// Check Disconnection.
		if(physicalDevice->IsConnected()) {
			// Send completion.
			physicalDevice->completeCommand(command);
		} else {	
			if(kIOReturnSuccess == pScsiCommand->CmdData.results) {
				// Send completion.	
				physicalDevice->completeCommand(command);
			} else {
				// Pendig Command.
				physicalDevice->setPendingCommand(command);
			}
		}
	}
	
	return;
}

void com_ximeta_driver_NDASDeviceController::processNoPnPMessageFromNDAS()
{
	DbgIOLog(DEBUG_MASK_NDAS_TRACE, ("processNoPnPMessageFromNDAS: Entered\n"));
	
	TARGET_DATA	Targets[MAX_NR_OF_TARGETS_PER_DEVICE] = { 0 };
	
	if(kNDASStatusOnline == fProvider->status()
	   && kNDASStatusOnline == fProvider->discovery(Targets)) {
		// Still Online.
		DbgIOLog(DEBUG_MASK_NDAS_WARNING, ("processNoPnPMessageFromNDAS: discovery success.\n"));

		NDAS_clock_get_system_value(&lastBroadcastTime);

		return;
	}
	
	switch(fProvider->status()) {
		case kNDASStatusOnline:
		{
			for(int count = 0; count < MAX_NR_OF_TARGETS_PER_DEVICE; count++) {
				
				com_ximeta_driver_NDASPhysicalUnitDevice *device;
				
				if ( device = OSDynamicCast(com_ximeta_driver_NDASPhysicalUnitDevice, fUnits->getObject(count)) ) {
					
					fProvider->checkDataConnection(device);	
					
					if (device->IsConnected()) {
						// We just miss few pnp messages. so let it keep going.
						DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("processNoPnPMessageFromNDAS: Connected.\n"));

						return;
					}
				}
			}
		}
		case kNDASStatusOffline:
		case kNDASStatusNoPermission:
		case kNDASStatusFailure:
		{
			fProvider->setStatus(kNDASStatusOffline);
		
			// Delete Unit Devies.
			for(int count = 0; count < MAX_NR_OF_TARGETS_PER_DEVICE; count++) {
				
				com_ximeta_driver_NDASPhysicalUnitDevice *device;
				
				if ( device = OSDynamicCast(com_ximeta_driver_NDASPhysicalUnitDevice, fUnits->getObject(count)) ) {
					
					updateUnitDevice(count, NULL, false, false);
					
					deleteUnitDevice(count);
				}
			}
		}
			break;
		default:
			DbgIOLog(DEBUG_MASK_NDAS_INFO, ("processSettingCommand: Bad Status %d", fProvider->getState()));
			break;
	}
	
	return;
}

void com_ximeta_driver_NDASDeviceController::processManagingIOCommand(com_ximeta_driver_NDASCommand *command)
{
	PManagingIOCommand	pManagingIOCommand;
	
	DbgIOLog(DEBUG_MASK_DISK_TRACE, ("processManagingIOCommand: Entered.\n"));
	
	pManagingIOCommand = command->managingIOCommand();

	// Check Device status.
	/*
	if ( kNDASUnitStatusUnmount != pManagingIOCommand->device->Status() ) {

		DbgIOLog(DEBUG_MASK_DISK_WARNING, ("processManagingIOCommand: Status is not Unmount.\n"));

		pManagingIOCommand->IsSuccess = false;
		
		return;
	}
	*/
	if(fInSleepMode) {
		
		DbgIOLog(DEBUG_MASK_DISK_INFO, ("processSrbCommand: In Sleep Mode\n"));
		
		pManagingIOCommand->IsSuccess = false;
		
		return;
	}
		
	// Send SRB.
	pManagingIOCommand->IsSuccess = fProvider->processManagingIOCommand(pManagingIOCommand);
		
	return;
}

int com_ximeta_driver_NDASDeviceController::createUnitDevice(UInt32 targetNo, TARGET_DATA *Target)
{
	bool										bret;
	OSDictionary								*prop = NULL;
    com_ximeta_driver_NDASPhysicalUnitDevice	*NDASPhysicalUnitDevicePtr = NULL;

	DbgIOLog(DEBUG_MASK_NDAS_TRACE, ("createUnitDevice: Entered. Number %d\n", targetNo));
		
	if( NULL != (NDASPhysicalUnitDevicePtr = OSDynamicCast(com_ximeta_driver_NDASPhysicalUnitDevice, fUnits->getObject(targetNo)) )) {
		DbgIOLog(DEBUG_MASK_NDAS_INFO, ("createUnitDevice: Already Created.\n"));
	} else {
		
		prop = OSDictionary::withCapacity(4);
		if(prop == NULL)
		{
			DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("createUnitDevice: failed to alloc property\n"));
			
			return kIOReturnError;
		}
		
		NDASPhysicalUnitDevicePtr = OSDynamicCast(com_ximeta_driver_NDASPhysicalUnitDevice, 
												  OSMetaClass::allocClassWithName(NDAS_PHYSICAL_UNIT_DEVICE_CLASS));
		
		if(NDASPhysicalUnitDevicePtr == NULL) {
			DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("createUnitDevice: failed to alloc Device class\n"));
			
			prop->release();
			
			return kIOReturnError;
		}
		
		// Init Device.
		bret = NDASPhysicalUnitDevicePtr->init(prop);
		prop->release();
		if(bret == false)
		{
			NDASPhysicalUnitDevicePtr->release();
			DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("createUnitDevice: failed to init Device\n"));
			
			return kIOReturnError;
		}
	
		if (!NDASPhysicalUnitDevicePtr->createDataBuffer(fProvider->MaxRequestBlocks() * SECTOR_SIZE)) {
			NDASPhysicalUnitDevicePtr->release();
			DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("createUnitDevice: Can't allocate buffer\n"));
			
			return kIOReturnError;			
		}

		// Attach Device.
		bret = NDASPhysicalUnitDevicePtr->attach(this);
		if(bret == true) {
			// Register Service.
			//NDASPhysicalUnitDevicePtr->registerService();
		}	
		
		NDASPhysicalUnitDevicePtr->setDevice(fProvider);
		
		fUnits->setObject(targetNo, NDASPhysicalUnitDevicePtr);		
	}
    
	DbgIOLog(DEBUG_MASK_NDAS_INFO, ("createUnitDevice: LanScsiDevice = %p\n", NDASPhysicalUnitDevicePtr));

	// Create ID.
	GUID				tempGUID;
	PUNIT_DISK_LOCATION	tempUnitDiskLocationPtr;
	
	memset(&tempGUID, 0, sizeof(GUID));
	tempUnitDiskLocationPtr = (PUNIT_DISK_LOCATION)&(tempGUID.guid[8]);
	
	memcpy(tempUnitDiskLocationPtr->MACAddr,  fProvider->address()->slpx_node, 6);
	tempUnitDiskLocationPtr->UnitNumber = targetNo;
	tempUnitDiskLocationPtr->VID = fProvider->VID();
	
	NDASPhysicalUnitDevicePtr->setID(&tempGUID);
	NDASPhysicalUnitDevicePtr->setUnitDiskLocation(tempUnitDiskLocationPtr);

	// Assign attributes.
	NDASPhysicalUnitDevicePtr->setSlotNumber(fProvider->slotNumber());
	NDASPhysicalUnitDevicePtr->setUnitNumber(targetNo);
	NDASPhysicalUnitDevicePtr->setNRRWHosts(Target[targetNo].NRRWHost);
	NDASPhysicalUnitDevicePtr->setNRROHosts(Target[targetNo].NRROHost);
	NDASPhysicalUnitDevicePtr->setTargetData(&Target[targetNo]);
	NDASPhysicalUnitDevicePtr->setStatus(kNDASUnitStatusNotPresent);

	NDASPhysicalUnitDevicePtr->IsWritable();
	NDASPhysicalUnitDevicePtr->setConfiguration(kNDASUnitConfigurationUnmount);
	NDASPhysicalUnitDevicePtr->setNumberOfSectors(Target[targetNo].SectorCount);
	
	// Send Message to BunEnumerator.
	com_ximeta_driver_NDASBusEnumerator	*bus;
	
	bus = OSDynamicCast(com_ximeta_driver_NDASBusEnumerator, 
							fProvider->getParentEntry(gIOServicePlane));
	
	if (NULL == bus) {
		DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("createUnitDevice: Can't Find bus.\n"));
	} else {
		
		// Create NDAS Command.
		com_ximeta_driver_NDASCommand *command = OSDynamicCast(com_ximeta_driver_NDASCommand, 
								OSMetaClass::allocClassWithName(NDAS_COMMAND_CLASS));
		
		if(command == NULL || !command->init()) {
			DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("createUnitDevice: failed to alloc command class\n"));
			if (command) {
				command->release();
			}
			
			return kIOReturnError;
		}
		
		command->setCommand(kNDASDeviceNotificationPresent, NDASPhysicalUnitDevicePtr);
				
		bus->enqueueMessage(command);
		
		command->release();
		
		bus->messageClients(kNDASFamilyIOMessagePhysicalUnitDeviceWasCreated, NDASPhysicalUnitDevicePtr->ID(), sizeof(GUID));
	}
	
	return kIOReturnSuccess;
}

int com_ximeta_driver_NDASDeviceController::updateUnitDevice(UInt32 targetNo, TARGET_DATA *Target, bool interfaceChanged, bool bPresent)
{
	bool										bClearPendingCommand = false;
	//	OSDictionary								*prop = NULL;
    com_ximeta_driver_NDASPhysicalUnitDevice	*NDASPhysicalUnitDevicePtr = NULL;

	DbgIOLog(DEBUG_MASK_NDAS_TRACE, ("updateUnitDevice: Entered. %d\n", targetNo));

	if( NULL == (NDASPhysicalUnitDevicePtr = OSDynamicCast(com_ximeta_driver_NDASPhysicalUnitDevice, fUnits->getObject(targetNo)) )) {
		DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("updateUnitDevice: Not exist.\n"));
		
        return kIOReturnError;
	}
	
	DbgIOLog(DEBUG_MASK_NDAS_INFO, ("updateUnitDevice: Physical Unit Device 0x%x\n", NDASPhysicalUnitDevicePtr));
	
	//
	// Update Status.
	//
	NDASPhysicalUnitDevicePtr->setIsPresent(bPresent);
	NDASPhysicalUnitDevicePtr->updateStatus();
	
	if (kNDASUnitStatusNotPresent == NDASPhysicalUnitDevicePtr->Status()) {

		bClearPendingCommand = true;

	} else {
		
		if (NULL != Target) {
			// Update Target Data.
			NDASPhysicalUnitDevicePtr->setNRRWHosts(Target[targetNo].NRRWHost);
			NDASPhysicalUnitDevicePtr->setNRROHosts(Target[targetNo].NRROHost);
		}
		
		DbgIOLog(DEBUG_MASK_NDAS_INFO, ("updateUnitDevice: configuration %d, status %d\n", NDASPhysicalUnitDevicePtr->Configuration(), NDASPhysicalUnitDevicePtr->Status()));
		
		switch(NDASPhysicalUnitDevicePtr->Status()) {
			case kNDASUnitStatusTryConnectRO:
			case kNDASUnitStatusTryConnectRW:
			{ 
				// Mount.
				fProvider->mount(NDASPhysicalUnitDevicePtr);
			}
				break;
			case kNDASUnitStatusTryDisconnectRO:
			case kNDASUnitStatusTryDisconnectRW:
			{
				// Unmount.
				fProvider->unmount(NDASPhysicalUnitDevicePtr);
			}
				break;
			case kNDASUnitStatusReconnectRO:
			case kNDASUnitStatusReconnectRW:
			{
				// Mount.
				fProvider->mount(NDASPhysicalUnitDevicePtr);
			}
				break;
			case kNDASUnitStatusConnectedRO:
			case kNDASUnitStatusConnectedRW:
			{
				// Process inferface Change.
				if (interfaceChanged) {
					fProvider->reconnect(NDASPhysicalUnitDevicePtr);
				}
			}
				break;
		}
		
		// Update Status.
		NDASPhysicalUnitDevicePtr->updateStatus();
		
		switch (NDASPhysicalUnitDevicePtr->Status()) {
			case kNDASUnitStatusConnectedRO:
			case kNDASUnitStatusConnectedRW:
			case kNDASUnitStatusDisconnected:
				bClearPendingCommand = true;
				break;
			default:
				bClearPendingCommand = false;
				break;
		}		
	}
	
	//
	// Process pending command caused by disconnection.
	//
	com_ximeta_driver_NDASCommand	*command = NDASPhysicalUnitDevicePtr->PendingCommand();
		
	if(bClearPendingCommand && NULL != command) {
		
		DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("updateUnitDevice: Processing Pending Command\n"));
		
		NDASPhysicalUnitDevicePtr->setPendingCommand(NULL);
		
		if(NDASPhysicalUnitDevicePtr != NULL) {
			
			// enqueueCommand(command);
			
			PSCSICommand	pScsiCommand;
			
			pScsiCommand = command->scsiCommand();
			
			if(kIOReturnSuccess != pScsiCommand->CmdData.results) {
				
				pScsiCommand->CmdData.results = kIOReturnBusy;
				pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_ProtocolTimeoutOccurred;
			}
			
			NDASPhysicalUnitDevicePtr->completeCommand(command);
			
		} else {
			DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("updateUnitDevice: fTargetDevice is NULL!!!\n"));
		}
	}
		
	// Send Message to BunEnumerator.
	com_ximeta_driver_NDASBusEnumerator	*bus;
	
	bus = OSDynamicCast(com_ximeta_driver_NDASBusEnumerator, 
						fProvider->getParentEntry(gIOServicePlane));
	
	if (NULL == bus) {
		DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("updateUnitDevice: Can't Find bus.\n"));
	} else {
		
		com_ximeta_driver_NDASCommand *command = NULL;
		
		if (kNDASUnitStatusNotPresent != NDASPhysicalUnitDevicePtr->Status() 
			&& NDASPhysicalUnitDevicePtr->IsNeedRefresh()) {
						
			NDASPhysicalUnitDevicePtr->setNeedRefresh(false);

			// Create NDAS Command.
			command = OSDynamicCast(com_ximeta_driver_NDASCommand, 
									OSMetaClass::allocClassWithName(NDAS_COMMAND_CLASS));
			
			if(command == NULL || !command->init()) {
				DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("updateUnitDevice: failed to alloc command class\n"));
				if (command) {
					command->release();
				}
				
				return kIOReturnError;
			}
			
			command->setCommand(kNDASDeviceNotificationCheckBind, NDASPhysicalUnitDevicePtr);
			
			bus->enqueueMessage(command);
			
			command->release();						
		}
		
		// Create NDAS Command.
		command = OSDynamicCast(com_ximeta_driver_NDASCommand, 
								OSMetaClass::allocClassWithName(NDAS_COMMAND_CLASS));
		
		if(command == NULL || !command->init()) {
			DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("updateUnitDevice: failed to alloc command class\n"));
			if (command) {
				command->release();
			}
			
			return kIOReturnError;
		}
		
		if(kNDASUnitStatusNotPresent == NDASPhysicalUnitDevicePtr->Status()) {
			command->setCommand(kNDASDeviceNotificationNotPresent, NDASPhysicalUnitDevicePtr);
		} else {
			command->setCommand(kNDASDeviceNotificationPresent, NDASPhysicalUnitDevicePtr);
		}
		
		bus->enqueueMessage(command);
		
		command->release();
	}
	
	return kIOReturnSuccess;
}

/*
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
*/
int com_ximeta_driver_NDASDeviceController::deleteUnitDevice(UInt32 targetNo)
{
//	bool										bret;
//	OSDictionary								*prop = NULL;
    com_ximeta_driver_NDASPhysicalUnitDevice	*NDASPhysicalUnitDevicePtr = NULL;
	
	if( NULL == (NDASPhysicalUnitDevicePtr = OSDynamicCast(com_ximeta_driver_NDASPhysicalUnitDevice, fUnits->getObject(targetNo)) )) {
		DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("deleteUnitDevice: Not exist.\n"));
		
        return kIOReturnError;
	}
						
	DbgIOLog(DEBUG_MASK_NDAS_TRACE, ("deleteUnitDevice: Entered. 0x%x\n", NDASPhysicalUnitDevicePtr));
	
	// Clear Device pointer
	NDASPhysicalUnitDevicePtr->setDevice(NULL);
		
	// Cleanup Pending Command.
	if (NDASPhysicalUnitDevicePtr->PendingCommand()) {
		NDASPhysicalUnitDevicePtr->completeCommand(NDASPhysicalUnitDevicePtr->PendingCommand());
		
		NDASPhysicalUnitDevicePtr->setPendingCommand(NULL);
	}
	
	// Disconnect If Connected.
	if (NDASPhysicalUnitDevicePtr->IsConnected()) {
		fProvider->unmount(NDASPhysicalUnitDevicePtr);
	}
	
	// Remove from List.
	fUnits->removeObject(targetNo);
	/*
	// Send Message to BunEnumerator.
	com_ximeta_driver_NDASBusEnumerator	*bus;
	
	bus = OSDynamicCast(com_ximeta_driver_NDASBusEnumerator, 
						fProvider->getParentEntry(gIOServicePlane));
	
	if (NULL == bus) {
		DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("deleteUnitDevice: Can't Find bus.\n"));
	} else {
		
		// Create NDAS Command.
		com_ximeta_driver_NDASCommand *command = OSDynamicCast(com_ximeta_driver_NDASCommand, 
															   OSMetaClass::allocClassWithName(NDAS_COMMAND_CLASS));
		
		if(command == NULL || !command->init()) {
			DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("deleteUnitDevice: failed to alloc command class\n"));
			if (command) {
				command->release();
			}
			
			return kIOReturnError;
		}
		
		command->setCommand(kNDASDeviceNotificationNotPresent, NDASPhysicalUnitDevicePtr);
		
		bus->enqueueMessage(command);
		
		command->release();
		
		bus->messageClients(kNDASFamilyIOMessagePhysicalUnitDeviceWasDeleted, NDASPhysicalUnitDevicePtr->ID(), sizeof(GUID));
	}
	 */
	/*
	// Terminate Device.
	NDASPhysicalUnitDevicePtr->terminate();
	
	NDASPhysicalUnitDevicePtr->willTerminate(this, NULL);
		
	//IOCreateThread(terminateDeviceThread, NDASLogicalDevicePtr);	
	*/
	
	NDASPhysicalUnitDevicePtr->detach(this);
	
	NDASPhysicalUnitDevicePtr->release();

	return kIOReturnSuccess;
}

static void workerThreadWrapper(void* argument)
{		
	com_ximeta_driver_NDASDeviceController *controller = (com_ximeta_driver_NDASDeviceController*)argument;
	
	controller->workerThread(NULL);
	
	return;
}

IOReturn com_ximeta_driver_NDASDeviceController::message (
															 UInt32 		type,
															 IOService *	nub,
															 void * 		arg 
															 )
{
	DbgIOLog(DEBUG_MASK_NDAS_TRACE, ("message Entered. Type 0x%x\n", type));
	
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
			
			fNeedCompareTargetData = true;

			// Disconnect.
			for(int count = 0; count < MAX_NR_OF_TARGETS_PER_DEVICE; count++) {
				
				com_ximeta_driver_NDASPhysicalUnitDevice	*device = NULL;
				
				device = OSDynamicCast(com_ximeta_driver_NDASPhysicalUnitDevice, fUnits->getObject(count));
				
				if (device && device->IsConnected()) {
					fProvider->cleanupDataConnection(device, true);
				}
			}
			
			messageClients(kIOMessageServiceIsResumed);
		}
			break;

		case kIOMessageServiceIsRequestingClose:
		{
			DbgIOLog(DEBUG_MASK_NDAS_INFO, ("kIOMessageServiceIsRequestingClose\n"));

			/*
			// Terminate Thread.
			// Create NDAS Command.
			com_ximeta_driver_NDASCommand *command = OSDynamicCast(com_ximeta_driver_NDASCommand, 
																   OSMetaClass::allocClassWithName(NDAS_COMMAND_CLASS));
			
			if(command != NULL || command->init()) {
				
				command->setToTerminateCommand();
				
				enqueueCommand(command);
				
			} else {
				DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("receivePnPMessage: failed to alloc command class\n"));	
			}
			 */
			
		}
			break;
		case kIOMessageServiceIsTerminated:
		{
			DbgIOLog(DEBUG_MASK_NDAS_INFO, ("kIOMessageServiceIsTerminated\n"));
			
			// Delete Unit Devices.
			OSCollectionIterator *DeviceListIterator = OSCollectionIterator::withCollection(fUnits);
			if(DeviceListIterator != NULL) {
				
				OSMetaClassBase								*metaObject;
				com_ximeta_driver_NDASPhysicalUnitDevice	*NDASDevicePtr;
				
				while((metaObject = DeviceListIterator->getNextObject()) != NULL) { 
										
					UInt32 unitNumber;
					
					NDASDevicePtr = OSDynamicCast(com_ximeta_driver_NDASPhysicalUnitDevice, metaObject);
					if(NDASDevicePtr == NULL) {
						DbgIOLog(DEBUG_MASK_USER_INFO, ("kIOMessageServiceIsTerminated: Can't Cast to Device Class.\n"));
						continue;
					}
					
					unitNumber = NDASDevicePtr->unitNumber();
					
					updateUnitDevice(unitNumber, NULL, false, false);
					
					deleteUnitDevice(unitNumber);			
				}
				
				DeviceListIterator->release();
				
			} else {
				DbgIOLog(DEBUG_MASK_USER_INFO, ("kIOMessageServiceIsTerminated: Can't Create Iterator.\n"));
			}
/*			
			if ( fProvider != NULL ) {		
				
				fProvider->setController(NULL);
				
				if ( fProvider->isOpen ( this ) ) {
					fProvider->close ( this );
				}
			}
 */		
		}		
			break;
			
		default:
			status = IOService::message (type, nub, arg);
			break;
			
	}
	
	return status;
	
}

UInt32	com_ximeta_driver_NDASDeviceController::MaxRequestBlocksOfProvider()
{
	return fProvider->MaxRequestBlocks();
}