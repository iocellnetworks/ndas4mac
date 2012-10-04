/***************************************************************************
*
*  NDASRAIDUnitDeviceContorller.cpp
*
*    NDAS RAID Unit Device Controller
*
*    Copyright (c) 2004 XiMeta, Inc. All rights reserved.
*
**************************************************************************/

#include <IOKit/IOLib.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOMultiMemoryDescriptor.h>
#include <IOKit/scsi/SCSICommandOperationCodes.h>
#include <IOKit/scsi/SCSICmds_INQUIRY_Definitions.h>
#include <IOKit/IOTimerEventSource.h>

#include "NDASRAIDUnitDeviceController.h"

#include "NDASCommand.h"
#include "NDASChangeSettingCommand.h"
#include "NDASManagingIOCommand.h"
#include "NDASSCSICommand.h"
#include "NDASDeviceNotificationCommand.h"

#include "NDASRAIDUnitDevice.h"
#include "NDASBusEnumerator.h"

#include "NDASBusEnumeratorUserClientTypes.h"
#include "hash.h"
#include "scrc32.h"
#include "NDASDIB.h"

#include "Utilities.h"

extern "C" {
#include <pexpert/pexpert.h>//This is for debugging purposes ONLY
}

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

static void RAIDTimerWrapper(OSObject *owner, IOTimerEventSource *sender);

// Define my superclass
#define super IOService

// REQUIRED! This macro defines the class's constructors, destructors,
// and several other methods I/O Kit requires. Do NOT use super as the
// second parameter. You must use the literal name of the superclass.

OSDefineMetaClassAndStructors(com_ximeta_driver_NDASRAIDUnitDeviceController, IOService)

bool com_ximeta_driver_NDASRAIDUnitDeviceController::init(OSDictionary *dict)
{
	bool res = super::init(dict);
	
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Initializing\n"));
			
	fCommandArray = OSArray::withCapacity(0);
	fNumberOfNewBuildCommands = 0;
	
	fWorkerThread = NULL;
	
	fWrittenSectorsList = NULL;
	fWrittenLocationsList = NULL;
	fSizeOfWrittenSectors = 0;
	
    return res;
}

void com_ximeta_driver_NDASRAIDUnitDeviceController::free(void)
{
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Freeing\n"));
	
	fCommandArray->flushCollection();
	fCommandArray->release();

	if (cntrlSync && cntrlSync != (IOWorkLoop *)1) {
		cntrlSync->release();
	}
	
	super::free();
}

IOService *com_ximeta_driver_NDASRAIDUnitDeviceController::probe(IOService *provider, SInt32 *score)
{
    IOService *res = super::probe(provider, score);
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Probing\n"));
    return res;
}

bool com_ximeta_driver_NDASRAIDUnitDeviceController::start(IOService *provider)
{
    bool res = super::start(provider);
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Starting\n"));
	
	fProvider = OSDynamicCast ( com_ximeta_driver_NDASRAIDUnitDevice, provider );
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
		
	// Allocate Wrtten Loc/Sec Lists.
	fWrittenLocationsList = OSArray::withCapacity(0);
	if(fWrittenLocationsList == NULL) {		
		DbgIOLog(DEBUG_MASK_PNP_ERROR, ("Start: Can't Allocate Device Array with Capacity.\n"));
		
		goto exit;
	}

	fWrittenSectorsList = OSArray::withCapacity(0);
	if(fWrittenSectorsList == NULL) {		
		DbgIOLog(DEBUG_MASK_PNP_ERROR, ("Start: Can't Allocate Device Array with Capacity.\n"));
		
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
	
	// Create PnP Timer.
	fRAIDTimerSourcePtr = IOTimerEventSource::timerEventSource(this, RAIDTimerWrapper);
	
	if(!fRAIDTimerSourcePtr) {
		DbgIOLog(DEBUG_MASK_PNP_ERROR, ("Failed to create timer event source!\n"));
	} else {
		if(fWorkLoop->addEventSource(fRAIDTimerSourcePtr) != kIOReturnSuccess) {
			DbgIOLog(DEBUG_MASK_PNP_ERROR, ("Failed to ad timer event source to work loop!\n"));
		}
	}	
		
	// Power Management.
	PMinit();
	provider->joinPMtree(this);
	registerPowerDriver(this, powerStates, kNumberNDASPowerStates);
	
    return res;
	
exit:
	
	if (fWrittenLocationsList) {
		fWrittenLocationsList->release();
	}
		
	return false;
}

bool com_ximeta_driver_NDASRAIDUnitDeviceController::finalize (
														  IOOptionBits options 
														  )
{
	
	DbgIOLog(DEBUG_MASK_PNP_ERROR, ("finalizing.\n"));
			
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
			DbgIOLog(DEBUG_MASK_USER_ERROR, ("stop: Thread is still running!\n"));
			
			waitMore = true;
			semaphore_signal(fCommandSema);
		}	
		
		DbgIOLog(DEBUG_MASK_USER_ERROR, ("stop: Wait.\n"));
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

void com_ximeta_driver_NDASRAIDUnitDeviceController::stop(IOService *provider)
{
    DbgIOLog(DEBUG_MASK_PNP_ERROR, ("Stopping\n"));
		
	if (fCommandGate) {  
		
		// Free all command.
		while (fCommandArray->getCount() > 0) {			
			fCommandArray->removeObject(0);			
		}
		
		fWorkLoop->removeEventSource(fCommandGate);     
		fCommandGate->release();     
		fCommandGate = 0; 
	} 
	
	// Destroy Semaphores.
	semaphore_destroy(current_task(), fCommandSema);
	
	// Cleanup Timer
	if(fRAIDTimerSourcePtr) {
		fRAIDTimerSourcePtr->cancelTimeout();
		fWorkLoop->removeEventSource(fRAIDTimerSourcePtr);
		fRAIDTimerSourcePtr->release();
	}
	
	if (fWrittenSectorsList) {
		fWrittenSectorsList->flushCollection();
		fWrittenSectorsList->release();
	}

	if (fWrittenLocationsList) {
		fWrittenLocationsList->flushCollection();
		fWrittenLocationsList->release();
	}
	
	PMstop();

    super::stop(provider);
}

IOReturn 
com_ximeta_driver_NDASRAIDUnitDeviceController::setPowerState (
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

IOWorkLoop *com_ximeta_driver_NDASRAIDUnitDeviceController::getWorkLoop()
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

bool com_ximeta_driver_NDASRAIDUnitDeviceController::enqueueCommand(com_ximeta_driver_NDASCommand *command)
{
	DbgIOLog(DEBUG_MASK_NDAS_TRACE, ("enqueueCommand: Entered\n"));
	
	if (fThreadTerminate) {
		DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("enqueueCommand: Thread Terminated.\n"));
		
		return false;
	}
		
	if(fCommandGate) {
		fCommandGate->runCommand((void*)kNDASCommandQueueAppend, (void*)command);
		
		return true;
	} else {
		return false;
	}
}

bool com_ximeta_driver_NDASRAIDUnitDeviceController::executeAndWaitCommand(com_ximeta_driver_NDASCommand *command)
{
	DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("executeAndWaitCommand: Entered\n"));
	
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

IOReturn com_ximeta_driver_NDASRAIDUnitDeviceController::receiveMsg ( void * newRequest, void *command, void *, void * )
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
			
			//com_ximeta_driver_NDASCommand	*tempCommand = (com_ximeta_driver_NDASCommand *)fCommandArray->getObject(0);
		
			fCommandArray->removeObject(0);

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

static IOReturn receiveMsgWrapper( OSObject * theDriver, void * first, void * second, void * third, void * fourth )
{
	com_ximeta_driver_NDASRAIDUnitDeviceController	*controller = (com_ximeta_driver_NDASRAIDUnitDeviceController*)theDriver;
	
	return controller->receiveMsg(first, second, third, fourth);
}

void com_ximeta_driver_NDASRAIDUnitDeviceController::RAIDTimeOutOccurred(
																OSObject *owner,
																IOTimerEventSource *sender)
{
	// Wake-up thread.
	semaphore_signal(fCommandSema);
}

void RAIDTimerWrapper(OSObject *owner, IOTimerEventSource *sender)
{
	com_ximeta_driver_NDASRAIDUnitDeviceController *controller = (com_ximeta_driver_NDASRAIDUnitDeviceController*)owner;
	
	controller->RAIDTimeOutOccurred(owner, sender);
	
	return;
}

void com_ximeta_driver_NDASRAIDUnitDeviceController::workerThread(void* argument)
{	
//	bool	bTerminate = false;
	uint32_t	secondFailedTime = 0;	// Secs.

	DbgIOLog(DEBUG_MASK_NDAS_TRACE, ("workerThread: Entered.\n"));

	this->retain();
	fRAIDTimerSourcePtr->retain();
	
	bool	bIdle = false;

    do {
		kern_return_t					sema_result;
		UInt32							timeoutMS;
		
		if(kNDASRAIDStatusRecovering == fProvider->RAIDStatus()
		   && 0 == fCommandArray->getCount()
		   && bIdle) {
			fProvider->recovery();
		} else {
			
			DbgIOLog(DEBUG_MASK_NDAS_TRACE, ("workerThread: Wait.\n"));
			
			// Set timeout and wait.
			if (kNDASRAIDStatusRecovering == fProvider->RAIDStatus()) {
				timeoutMS = NDAS_MAX_WAIT_TIME_FOR_RAID_THREAD_RECOVERING_MS;
			} else {
				timeoutMS = NDAS_MAX_WAIT_TIME_FOR_RAID_THREAD_NORMAL_SEC * 1000;
			}
			
			fRAIDTimerSourcePtr->setTimeoutMS(timeoutMS);
			sema_result = semaphore_wait(fCommandSema);
			
			fRAIDTimerSourcePtr->cancelTimeout();

			switch(sema_result) {
				case KERN_SUCCESS:
				{

					// Check Command Queue.
					if(0 == fCommandArray->getCount()) {
						// Maybe Terminate signal.
						DbgIOLog(DEBUG_MASK_NDAS_INFO, ("workerThread: No Command. By Time out.\n"));
						
						continue;
					}
					
					com_ximeta_driver_NDASCommand *command = OSDynamicCast(com_ximeta_driver_NDASCommand, fCommandArray->getObject(0));
					if( NULL == command ) {
						DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("workerThread: getObject return NULL.\n"));
						
						continue;
					}
					
					switch(command->command()) {
						case kNDASCommandChangedSetting:
						{
							
							com_ximeta_driver_NDASChangeSettingCommand *pnpCommand = OSDynamicCast(com_ximeta_driver_NDASChangeSettingCommand, command);
							processSettingCommand(pnpCommand);
						}
							break;
						case kNDASCommandSRB:
						{										
							bIdle = false;								

							com_ximeta_driver_NDASSCSICommand *scsiCommand = OSDynamicCast(com_ximeta_driver_NDASSCSICommand, command);

							processSrbCommand(scsiCommand);
						}
							break;
						case kNDASCommandSRBCompleted:
						{
							com_ximeta_driver_NDASSCSICommand *scsiCommand = OSDynamicCast(com_ximeta_driver_NDASSCSICommand, command);

							processSrbCompletedCommand(scsiCommand);							
						}
							break;
						default:
							DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("workerThread: Unknowen Command. %d\n", command->command()));
							
					}

					if (kNDASRAIDStatusGoodButPartiallyFailed == fProvider->RAIDStatus()) {
						InitAndSetBitmap();
					}

					command->setToComplete(kIOReturnSuccess);
					
					fCommandGate->runCommand((void *)kNDASCommandQueueCompleteCommand);
				}
					break;
				case KERN_OPERATION_TIMED_OUT:
				{
					DbgIOLog(DEBUG_MASK_NDAS_INFO, ("workerThread: Timed Out.\n"));							
					bIdle = true;
				}
					break;
				default:
					DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("workerThread: Invalid return value. %d\n", sema_result));
					break;
			}
			
			// Check RAID breaking when use.
			if (kNDASRAIDStatusBroken == fProvider->RAIDStatus()
				&& fProvider->NDASCommand()) {
				DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("workerThread: Broken RAID with Command.\n"));
				
				fProvider->completeRecievedCommand();
			}
			
			// Check RAID 1 status.
			if (NMT_RAID1R3 == fProvider->Type()) {
				
				switch(fProvider->RAIDStatus()) {
					case kNDASRAIDStatusGoodButPartiallyFailed:
					case kNDASRAIDStatusBrokenButWorkable:
					{
						if (0 == secondFailedTime) {
							NDAS_clock_get_system_value(&secondFailedTime);
							
							DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("workerThread: secondFailed time reoceded.\n"));

						} else {
							uint32_t currentTime;
							
							NDAS_clock_get_system_value(&currentTime);
							
							if (NDAS_MAX_WAIT_TIME_FOR_RAID_REPLACE < currentTime - secondFailedTime) {
								
								if (0 < fProvider->CountOfSpareDevices()) {
									fProvider->ReplaceToSpare();
									fProvider->updateRAIDStatus();
									
									secondFailedTime = 0;
								}
							}
						}
					}
						break;
					default:
					{
						if (0 != secondFailedTime) {
							
							DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("workerThread: secondFailed time reseted.n"));

							secondFailedTime = 0;
						}	
					}
				}
			}
		}
		
	} while(fThreadTerminate == false);
		
	fRAIDTimerSourcePtr->release();
	this->release();

	myExitThread(&fWorkerThread);

	// Never return.
}

void com_ximeta_driver_NDASRAIDUnitDeviceController::processSettingCommand(com_ximeta_driver_NDASChangeSettingCommand *command)
{

	PChangeSettingCommand		pnpCommand = command->changeSettingCommand();
	bool						bNeedSendCommand = false;
	kNDASDeviceNotificationType	notificationType;
	
	if (pnpCommand->fInterfaceChanged) {
		
		fProvider->updateStatus();
		
		// Update NRRWHost
		fProvider->setNRRWHosts(fProvider->getNRRWHosts());
		
		// Update NRROHost
		fProvider->setNRROHosts(fProvider->getNRROHosts());
		
		if (fProvider->IsWorkable()) {
			// Recovery.
			if ( (kNDASRAIDStatusReadyToRecovery == fProvider->RAIDStatus() || kNDASRAIDStatusReadyToRecoveryWithReplacedUnit == fProvider->RAIDStatus())
				 && (kNDASUnitStatusConnectedRW == fProvider->Status() || kNDASUnitStatusReconnectRW == fProvider->Status())) {
				
				fProvider->recovery();
			}	
			
			if (kNDASRAIDStatusNotReady != fProvider->RAIDStatus()) {
				bNeedSendCommand = true;
				notificationType = kNDASDeviceNotificationPresent;
			}
		}
		
	} else {
		
		fProvider->updateStatus();
		
		switch(fProvider->RAIDStatus()) {
			case kNDASRAIDStatusNotReady:
			{
				bNeedSendCommand = true;
				notificationType = kNDASDeviceNotificationNotPresent;
			}
				break;				
/*
			case kNDASRAIDStatusBroken:
			{
				bNeedSendCommand = true;
				notificationType = kNDASDeviceNotificationDisconnect;
			}
				break;
 */
			default:
				break;
		}		
	}
	
	// Send Command to Enumerator.
	if (bNeedSendCommand) {
		
		com_ximeta_driver_NDASBusEnumerator	*bus;
		
		bus = OSDynamicCast(com_ximeta_driver_NDASBusEnumerator, 
							fProvider->getParentEntry(gIOServicePlane));
		
		if (NULL == bus) {
			DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("processSettingCommand: Can't Find bus.\n"));
		} else {
			
			// Create NDAS Command.
			com_ximeta_driver_NDASDeviceNotificationCommand *command = OSTypeAlloc(com_ximeta_driver_NDASDeviceNotificationCommand);
			
			if(command == NULL || !command->init()) {
				DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("processSettingCommand: failed to alloc command class\n"));
				if (command) {
					command->release();
				}
				
				goto out;
			}
			
			command->setCommand(notificationType, fProvider);
			
			bus->enqueueMessage(command);
			
			command->release();
		}
	}
	
out:
			
	fProvider->completeChangeSettingMessage(command);
	
	return;
}

void com_ximeta_driver_NDASRAIDUnitDeviceController::processSrbCommand(com_ximeta_driver_NDASSCSICommand *command)
{
	PSCSICommand	pScsiCommand;
	
	DbgIOLog(DEBUG_MASK_RAID_TRACE, ("processSrbCommand: Entered.\n"));
			
	pScsiCommand = command->scsiCommand();
	
	if (kSCSICmd_START_STOP_UNIT != pScsiCommand->cdb[0]) {
		
		if( !fProvider->IsInRunningStatus() ) {
			
			DbgIOLog(DEBUG_MASK_RAID_WARNING, ("processSrbCommand: Not Mounted. Command 0x%x\n", pScsiCommand->cdb[0]));
			
			pScsiCommand->CmdData.results = kIOReturnNotAttached;
			pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_DeliveryFailure;
			
			goto Out;
		}
		
		if( 0 != pScsiCommand->LogicalUnitNumber) {
			
			DbgIOLog(DEBUG_MASK_RAID_WARNING, ("processSrbCommand: LU Number %d 0x%x\n", pScsiCommand->LogicalUnitNumber, pScsiCommand->cdb[0]));
			
			pScsiCommand->CmdData.results = kIOReturnNotAttached;
			pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_DeviceNotPresent;
			
			goto Out;
		}
	}

	DbgIOLog(DEBUG_MASK_RAID_INFO, ("processSrbCommand: Command 0x%x\n", pScsiCommand->cdb[0]));

	switch ( pScsiCommand->cdb[0] ) {
		case kSCSICmd_INQUIRY:
		{
			SCSICmd_INQUIRY_StandardData 	inqData;
			IOMemoryDescriptor 	*inqDataDesc;
			uint32_t          	transferCount;
			uint32_t				xferBlocks;
			char				buffer[kINQUIRY_PRODUCT_IDENTIFICATION_Length];

			bzero(&inqData, sizeof(inqData));
			
			inqData.PERIPHERAL_DEVICE_TYPE = kINQUIRY_PERIPHERAL_TYPE_DirectAccessSBCDevice;
			
			if(fProvider->Configuration() == kNDASUnitConfigurationMountRO) {
				inqData.RMB = kINQUIRY_PERIPHERAL_RMB_MediumRemovable;
			}
			memcpy(inqData.VENDOR_IDENTIFICATION, VENDOR_ID, kINQUIRY_VENDOR_IDENTIFICATION_Length);
			
			switch(fProvider->Type()) {
				case NMT_AGGREGATE:
					mySnprintf(buffer, sizeof(buffer), "AGGREGATE_%d", fProvider->Index());
					break;
				case NMT_RAID0R2:
					mySnprintf(buffer, sizeof(buffer), "RAID0R2_%d", fProvider->Index());
					break;
				case NMT_RAID1R3:
					mySnprintf(buffer, sizeof(buffer), "RAID1R3_%d", fProvider->Index());
					break;
				default:
					mySnprintf(buffer, sizeof(buffer), "UNSUPPRAID_%d", fProvider->Index());
					break;
			}
			
			memcpy(inqData.PRODUCT_IDENTIFICATION, buffer, kINQUIRY_PRODUCT_IDENTIFICATION_Length);

			memcpy(inqData.PRODUCT_REVISION_LEVEL, ND_PRODUCT_REVISION_LEVEL, kINQUIRY_PRODUCT_REVISION_LEVEL_Length);
			
			//LanScsiAdapter->Cmd->getPointers(&inqDataDesc, &transferCount, NULL);
			inqDataDesc = pScsiCommand->MemoryDescriptor_ptr;
			transferCount = pScsiCommand->RequestedDataTransferCount;
			
			xferBlocks = transferCount < sizeof(inqData) ? transferCount : sizeof(inqData);
			//            PreIOLog("transfrCount = %d, inqData = %d, xferBlocks  = %d\n", transferCount, sizeof(inqData), xferBlocks);
			inqDataDesc->writeBytes(0, &inqData, xferBlocks);
			
			pScsiCommand->CmdData.xferCount = xferBlocks;
			pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_GOOD;
			
			pScsiCommand->CmdData.results = kIOReturnSuccess;        
			break;
		}
			
		case kSCSICmd_TEST_UNIT_READY:
		{
			pScsiCommand->CmdData.xferCount = 0; 
			pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_GOOD;
			
			pScsiCommand->CmdData.results = kIOReturnSuccess;        
			break;
		} 
// BUG BUG BUG!!!
/*			
		case kSCSICmd_READ_CAPACITY:
		{
			ReadCapacityData    	readCapacityData;
			IOMemoryDescriptor  	*readCapacityDesc;
			uint32_t          	transferCount;
			uint32_t		xferBlocks;
			
			bzero(&readCapacityData, sizeof(readCapacityData));
			readCapacityData.logicalBlockAddress = NDASSwap32HostToBig(fProvider->Capacity() - 1);
			readCapacityData.bytesPerBlock = NDASSwap32HostToBig(fProvider->blocksize());
			
			//LanScsiAdapter->Cmd->getPointers(&readCapacityDesc, &transferCount, NULL);
			readCapacityDesc = pScsiCommand->MemoryDescriptor_ptr;
			transferCount = pScsiCommand->RequestedDataTransferCount;
			
			xferBlocks = transferCount < sizeof(readCapacityData) ? transferCount : sizeof(readCapacityData);
			
			readCapacityDesc->writeBytes(0, &readCapacityData, xferBlocks);
			pScsiCommand->CmdData.xferCount = xferBlocks;
			
			pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_GOOD;
			
			pScsiCommand->CmdData.results = kIOReturnSuccess;        
			
			break; 
		}
			
		case kSCSICmd_MODE_SENSE_6:
		{
			uint32_t						transferCount; 
			uint32_t						xferBlocks;
			UInt8						controlModePage[32];
			IOMemoryDescriptor			*controlModePageDesc;
			PMODE_PARAMETER_HEADER_6	header;
			PMODE_PARAMETER_BLOCK		block;
			
			
			bzero ( controlModePage, 32 );
			
			header = (PMODE_PARAMETER_HEADER_6)controlModePage;
			block = (PMODE_PARAMETER_BLOCK)(controlModePage + sizeof( MODE_PARAMETER_HEADER_6 ));
			header->ModeDataLength = sizeof( MODE_PARAMETER_HEADER_6 ) + sizeof( MODE_PARAMETER_BLOCK );
			
			if(fProvider->Configuration() == kNDASUnitConfigurationMountRO) {
				header->DeviceSpecificParameter |= kWriteProtectMask; // Write Protect;
				DbgIOLog(DEBUG_MASK_DISK_INFO, ("processSrbCommandNormal: Write Protected: modeBuffer[2] = %d\n", controlModePage[2]));
				//controlModePage[15] |= 0x04;	// Write Protect for Reduced Block Dev.
			}
			
			header->BlockDescriptorLength = sizeof ( MODE_PARAMETER_BLOCK ); //sizeof(*block);
			
			DbgIOLog(DEBUG_MASK_DISK_INFO, ("processSrbCommandNormal: cdb[1] = %d\n", pScsiCommand->cdb[1]));
			//            if(!(cdb[1] & 0x08))            
			//            header->ModeDataLength = sizeof(*header) + sizeof(*block) - sizeof(header->ModeDataLen);
			
			controlModePageDesc = pScsiCommand->MemoryDescriptor_ptr;
			transferCount = pScsiCommand->RequestedDataTransferCount;
			
			xferBlocks = transferCount < 32 ? transferCount : 32;
			controlModePageDesc->writeBytes(0, controlModePage, xferBlocks);
			pScsiCommand->CmdData.xferCount = xferBlocks;
			pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_GOOD;
			
			pScsiCommand->CmdData.results = kIOReturnSuccess;        
			
			break;
		}
			
		case kSCSICmd_MODE_SENSE_10:
		{
			uint32_t          	transferCount; 
			uint32_t		xferBlocks;
			UInt8		controlModePage[32];
			IOMemoryDescriptor  *controlModePageDesc;
			PMODE_PARAMETER_HEADER_10 header;
			PMODE_PARAMETER_BLOCK block;
			
			
			DbgIOLog(DEBUG_MASK_DISK_INFO, ("processSrbCommandNormal: cdb[1] = %d\n", pScsiCommand->cdb[1]));
			
			bzero(controlModePage, 32);
			
			header = (PMODE_PARAMETER_HEADER_10)controlModePage;
			block = (PMODE_PARAMETER_BLOCK)(controlModePage + sizeof( MODE_PARAMETER_HEADER_10 ));
			header->ModeDataLength = sizeof( MODE_PARAMETER_HEADER_10 ) + sizeof( MODE_PARAMETER_BLOCK );
			
			if(fProvider->Configuration() == kNDASUnitConfigurationMountRO) {
				header->DeviceSpecificParameter |= kWriteProtectMask; // Write Protect;
			}
			
			header->BlockDescriptorLength = sizeof ( MODE_PARAMETER_BLOCK ); //sizeof(*block);
			
			//            if(!(cdb[1] & 0x08))            
			//            header->ModeDataLength = sizeof(*header) + sizeof(*block) - sizeof(header->ModeDataLen);
			
			
			
			//LanScsiAdapter->Cmd->getPointers(&controlModePageDesc, &transferCount, NULL);
			controlModePageDesc = pScsiCommand->MemoryDescriptor_ptr;
			transferCount = pScsiCommand->RequestedDataTransferCount;
			
			xferBlocks = transferCount < 32 ? transferCount : 32;
			controlModePageDesc->writeBytes(0, controlModePage, xferBlocks);
			pScsiCommand->CmdData.xferCount = xferBlocks;
			pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_GOOD;
			
			pScsiCommand->CmdData.results = kIOReturnSuccess;        
			
			break;
		}
*/			
		case kSCSICmd_MODE_SELECT_6:
		{
			pScsiCommand->CmdData.xferCount = 0;
			pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_GOOD;
			
			pScsiCommand->CmdData.results = kIOReturnSuccess;        
			break;
		}
			
		case kSCSICmd_START_STOP_UNIT:
		{
			pScsiCommand->CmdData.xferCount = 0;
			pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_GOOD;
			
			pScsiCommand->CmdData.results = kIOReturnSuccess;        
			break;
		}      
			
		case kSCSICmd_REQUEST_SENSE:
		{
			SCSI_Sense_Data		scsiSenseData;
			IOMemoryDescriptor 	*scsiSenseDataDesc;
			uint32_t          	transferCount;
			uint32_t		xferBlocks;
			
			bzero(&scsiSenseData, sizeof(scsiSenseData));
			
			scsiSenseData.VALID_RESPONSE_CODE = kSENSE_RESPONSE_CODE_Current_Errors | kSENSE_DATA_VALID;
			scsiSenseData.SENSE_KEY = fProvider->senseKey();
			scsiSenseData.ADDITIONAL_SENSE_LENGTH = 0x0B;
			scsiSenseData.ADDITIONAL_SENSE_CODE = 0;
			scsiSenseData.ADDITIONAL_SENSE_CODE_QUALIFIER = 0;
			
			DbgIOLog(DEBUG_MASK_DISK_INFO, ("processSrbCommandNormal: scsiSenseData.senseKey = %x\n", scsiSenseData.SENSE_KEY));
			fProvider->setSenseKey(kSENSE_KEY_NO_SENSE);           
			
			scsiSenseDataDesc = pScsiCommand->MemoryDescriptor_ptr;
			transferCount = pScsiCommand->RequestedDataTransferCount;
			
			xferBlocks = transferCount < sizeof(scsiSenseData) ? transferCount : sizeof(scsiSenseData);
			scsiSenseDataDesc->writeBytes(0, &scsiSenseData, xferBlocks);
			
			pScsiCommand->CmdData.xferCount = xferBlocks;
			pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_GOOD;
			
			break;
		}
		case kSCSICmd_READ_10:
		case kSCSICmd_WRITE_10:
		{
			processReadWriteCommand(command);
			
			return;
		}
			break;
		case kSCSICmd_PREVENT_ALLOW_MEDIUM_REMOVAL:
		{
			// We don't support this command.
			// INVALID FIELD IN CDB 5/24/0
			fProvider->setSenseKey(kSENSE_KEY_ILLEGAL_REQUEST);
			//physicalDevice->ConnectionData()->fTargetASC = 0x24;	
			//physicalDevice->ConnectionData()->fTargetASCQ = 0;
			
			pScsiCommand->CmdData.xferCount = 0;
			pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_CHECK_CONDITION;
			pScsiCommand->CmdData.results = kIOReturnSuccess;
		}
			break;	
		default: 
		{
			DbgIOLog(DEBUG_MASK_DISK_WARNING, ("processSrbCommandNormal: Unsupported Operaion = %d\n", pScsiCommand->cdb[0]));
			
			//            SCSISenseData	scsiSendseData;
			fProvider->setSenseKey(kSENSE_KEY_ILLEGAL_REQUEST);
			
			pScsiCommand->CmdData.xferCount = 0;
			pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_CHECK_CONDITION;
			pScsiCommand->CmdData.results = kIOReturnSuccess;
			
			break;
		}
	}

Out:
		
	// Complete Recieved Command.
	fProvider->completeRecievedCommand();
	
	return;
}

void com_ximeta_driver_NDASRAIDUnitDeviceController::processSrbCompletedCommand(com_ximeta_driver_NDASSCSICommand *command)
{
	com_ximeta_driver_NDASSCSICommand	*receivedCommand = fProvider->NDASCommand();
	bool							NeedCompleteReceivedCommand = false;

	DbgIOLog(DEBUG_MASK_DISK_TRACE, ("processSrbCompletedCommand: Entered. Number of Commands %d\n", fNumberOfNewBuildCommands));
	
	
	fNumberOfNewBuildCommands--;
	
	// Already Completed?
	if (NULL == fProvider->NDASCommand()) {
		DbgIOLog(DEBUG_MASK_DISK_WARNING, ("processSrbCompletedCommand: Already Completed...\n"));
						
		command->release();
		
		return;
	}
	
	switch(fProvider->Type()) {
		case NMT_AGGREGATE:
		{
			PSCSICommand	scsiCommand;
			
			scsiCommand = command->scsiCommand();
						
			// Copy Error Code.
			receivedCommand->scsiCommand()->CmdData.results = scsiCommand->CmdData.results;
			receivedCommand->scsiCommand()->CmdData.serviceResponse = scsiCommand->CmdData.serviceResponse;
			receivedCommand->scsiCommand()->CmdData.taskStatus = scsiCommand->CmdData.taskStatus;
			
			// Update xfer Count;
			receivedCommand->scsiCommand()->CmdData.xferCount += scsiCommand->CmdData.xferCount;
						
			// Check Error.
			if (0 == fNumberOfNewBuildCommands) {
				NeedCompleteReceivedCommand = true;
			}			
		}
			break;
		case NMT_RAID0R2:
		{
			PSCSICommand	scsiCommand;
			
			scsiCommand = command->scsiCommand();
			
			// Copy Error Code.
			receivedCommand->scsiCommand()->CmdData.results = scsiCommand->CmdData.results;
			receivedCommand->scsiCommand()->CmdData.serviceResponse = scsiCommand->CmdData.serviceResponse;
			receivedCommand->scsiCommand()->CmdData.taskStatus = scsiCommand->CmdData.taskStatus;
			
			// Update xfer Count;
			receivedCommand->scsiCommand()->CmdData.xferCount += scsiCommand->CmdData.xferCount;
			
			// Check Error.
			if (0 == fNumberOfNewBuildCommands) {
				NeedCompleteReceivedCommand = true;
			}
			
			// Free MultiMemoryDescriptor
			scsiCommand->MemoryDescriptor_ptr->complete();

			scsiCommand->MemoryDescriptor_ptr->release();
		}
			break;
		case NMT_RAID1R3:
		{			
			PSCSICommand	scsiCommand;

			scsiCommand = command->scsiCommand();

			switch (scsiCommand->cdb[0]) {
				case kSCSICmd_READ_10:
				{
					// Copy Error Code.
					receivedCommand->scsiCommand()->CmdData.results = scsiCommand->CmdData.results;
					receivedCommand->scsiCommand()->CmdData.serviceResponse = scsiCommand->CmdData.serviceResponse;
					receivedCommand->scsiCommand()->CmdData.taskStatus = scsiCommand->CmdData.taskStatus;
					
					// Check Error.
					if (scsiCommand->CmdData.taskStatus == kSCSITaskStatus_GOOD) {
						
						// Update xfer Count;
						receivedCommand->scsiCommand()->CmdData.xferCount += scsiCommand->CmdData.xferCount;
						
						if (0 == fNumberOfNewBuildCommands) {
							NeedCompleteReceivedCommand = true;
						}						

					} else {
						
						// Check Sub Unit Status.
						fProvider->updateRAIDStatus();

						if ( (fProvider->UnitDeviceAtIndex(0) && fProvider->UnitDeviceAtIndex(0)->IsInRunningStatus() && fProvider->UnitDeviceAtIndex(0)->IsConnected() ) 
							 || (fProvider->UnitDeviceAtIndex(1) && fProvider->UnitDeviceAtIndex(1)->IsInRunningStatus()) && fProvider->UnitDeviceAtIndex(1)->IsConnected() ) {
							
							DbgIOLog(DEBUG_MASK_DISK_WARNING, ("processSrbCompletedCommand: Resent Command...\n"));
							
							processSrbCommand(receivedCommand);
						} else {
							NeedCompleteReceivedCommand = true;
						}
					}
				}
					break;
				case kSCSICmd_WRITE_10:
				{
					// Check Error.
					if (scsiCommand->CmdData.taskStatus == kSCSITaskStatus_GOOD) {
						
						// Update xfer Count;
						receivedCommand->scsiCommand()->CmdData.xferCount = scsiCommand->CmdData.xferCount;
						
						// Copy Error Code.
						receivedCommand->scsiCommand()->CmdData.results = scsiCommand->CmdData.results;
						receivedCommand->scsiCommand()->CmdData.serviceResponse = scsiCommand->CmdData.serviceResponse;
						receivedCommand->scsiCommand()->CmdData.taskStatus = scsiCommand->CmdData.taskStatus;
						
						if (0 == fNumberOfNewBuildCommands) {
							NeedCompleteReceivedCommand = true;
						}
						
					} else {

						//
						// Process For Broken RAID.
						//
						fProvider->updateRAIDStatus();

						//
						// Update command result.
						//
						if (0 == fNumberOfNewBuildCommands) {
							if (receivedCommand->scsiCommand()->CmdData.xferCount == scsiCommand->CmdData.xferCount) {
								// Another Command success.
								NeedCompleteReceivedCommand = true;
							} else {
								
								// Copy Error Code.
								receivedCommand->scsiCommand()->CmdData.results = scsiCommand->CmdData.results;
								receivedCommand->scsiCommand()->CmdData.serviceResponse = scsiCommand->CmdData.serviceResponse;
								receivedCommand->scsiCommand()->CmdData.taskStatus = scsiCommand->CmdData.taskStatus;
								
								NeedCompleteReceivedCommand = true;
							}
						}
					}					
				}
					break;
				default:
					break;
			} //switch (scsiCommand->cdb[0])
		}
			break;
		default:
			break;
	}
	
	// Release Command.
	command->release();

	// Update RAID Status.
	fProvider->updateRAIDStatus();

	if(NeedCompleteReceivedCommand) {		
		fProvider->completeRecievedCommand();
	}	
	
	return;
}

void BuildCDB(
			  SCSICommandDescriptorBlock  *cdb,
			  UInt8 scsiCommandOperationCodes,
			  uint64_t	location,
			  uint32_t	sectors
			  )
{
	if(!cdb) {
		return;
	}
	
	memset(cdb, 0, sizeof(SCSICommandDescriptorBlock));

	(*cdb)[0] = scsiCommandOperationCodes;
	
	switch (scsiCommandOperationCodes) {
	case kSCSICmd_READ_10:
	case kSCSICmd_WRITE_10:
	{
		(*cdb)[2] = (UInt8)(location >> 24);
		(*cdb)[3] = (UInt8)(location >> 16);
		(*cdb)[4] = (UInt8)(location >> 8);
		(*cdb)[5] = (UInt8)(location);
		
		(*cdb)[7] = (UInt8)(sectors >> 8);;
		(*cdb)[8] = (UInt8)(sectors);
	}
		break;
	default:
		break;
	}
}
									  
com_ximeta_driver_NDASSCSICommand	*BuildSCSICommand(
												  UInt8					scsiCommandOperationCodes,
												  uint64_t				location,
												  uint32_t				sectors,
												  uint32_t				blocksize,
												  IOMemoryDescriptor	*dataBuffer,
												  uint32_t				offset,
												  bool					completeWhenFailed,
												  uint32_t				subUnitIndex
												  )
{
	SCSICommand						scsiCommand;
	com_ximeta_driver_NDASSCSICommand	*command;

	// Allocate Command.
	command = OSTypeAlloc(com_ximeta_driver_NDASSCSICommand);
    
    if(command == NULL || !command->init()) {
		DbgIOLogNC(DEBUG_MASK_DISK_ERROR, ("MakeSCSICommand: Can't Allocate Command.\n"));
		if (command) {
			command->release();
		}
		
		return NULL;
	}
	
	memset(&scsiCommand, 0, sizeof(SCSICommand));
	
	scsiCommand.scsiTask = NULL;
	// scsiCommand.targetNo = fProvider->targetNo();
	scsiCommand.LogicalUnitNumber = 0;
	BuildCDB(&scsiCommand.cdb, scsiCommandOperationCodes, location, sectors);
	scsiCommand.MemoryDescriptor_ptr = dataBuffer;
	scsiCommand.BufferOffset = offset;
	scsiCommand.RequestedDataTransferCount = sectors * blocksize;
//	scsiCommand.BlockSize = blocksize;
	scsiCommand.completeWhenFailed = completeWhenFailed;
	scsiCommand.SubUnitIndex = subUnitIndex;
	
	switch(scsiCommandOperationCodes) {
		case kSCSICmd_READ_10:
		{
			scsiCommand.cdbSize = 10;
			scsiCommand.DataTransferDirection = kSCSIDataTransfer_FromTargetToInitiator;
		}
			break;
		case kSCSICmd_WRITE_10:
		{
			scsiCommand.cdbSize = 10;
			scsiCommand.DataTransferDirection = kSCSIDataTransfer_FromInitiatorToTarget;
		}
			break;
		default:
			break;
	}
	
	command->setCommand(&scsiCommand);	
	
	return command;
}

void	com_ximeta_driver_NDASRAIDUnitDeviceController::processReadWriteCommand(com_ximeta_driver_NDASSCSICommand *command)
{
	PSCSICommand pScsiCommand = NULL;
		
	if (!command) {
		DbgIOLog(DEBUG_MASK_RAID_ERROR, ("processReadWriteCommand: No Command!!!\n"));
	}
	
	pScsiCommand = command->scsiCommand();
	if (!pScsiCommand) {
		DbgIOLog(DEBUG_MASK_RAID_ERROR, ("processReadWriteCommand: No SCSI Command!!!\n"));
	}

	uint64_t	endSector = 0;
	uint64_t	offset = pScsiCommand->BufferOffset;
	uint32_t	reqSectors = 0;
	uint64_t	reqLocation = 0;
	
	// Get Location and Sectors.
	switch(pScsiCommand->cdb[0]) {
		case kSCSICmd_READ_10:
		case kSCSICmd_WRITE_10:
		{
			// Caclurate location and number of blocks.
			reqLocation = pScsiCommand->cdb[2] << 24
			| pScsiCommand->cdb[3] << 16 
			| pScsiCommand->cdb[4] << 8
			| pScsiCommand->cdb[5];
			
			reqSectors = pScsiCommand->cdb[7] << 8
				| pScsiCommand->cdb[8];			
		}
			break;
		default:
			break;
	}
	
	DbgIOLog(DEBUG_MASK_RAID_INFO, ("processReadWriteCommand: Location %lld, Sectors %d\n", reqLocation, reqSectors));
	
	switch(fProvider->Type()) {
		case NMT_AGGREGATE:
		{			
			for (int count = 0; count < fProvider->CountOfUnitDevices(); count++) {
				if (fProvider->UnitDeviceAtIndex(count)) {
					
					endSector = fProvider->UnitDeviceAtIndex(count)->NumberOfSectors() - 1;
					
					DbgIOLog(DEBUG_MASK_RAID_INFO, ("processReadWriteCommand: Count %d Location %lld, Sectors %d, EndSector %lld\n", count, reqLocation, reqSectors, endSector));

					if (reqLocation <= endSector) {
						
						uint32_t	actualSectors;
						com_ximeta_driver_NDASSCSICommand	*newCommand;
						
						if (endSector < reqLocation + reqSectors - 1) {
							actualSectors = endSector - reqLocation + 1;
						} else {
							actualSectors = reqSectors;
						}
						
						reqSectors -= actualSectors;
						
						// Make New Command and Sent to Lower Unit Device.
						newCommand = BuildSCSICommand(
													  pScsiCommand->cdb[0],
													  reqLocation,
													  actualSectors * (fProvider->blockSize() / fProvider->UnitDeviceAtIndex(count)->blockSize() ),
													  fProvider->UnitDeviceAtIndex(count)->blockSize(),
													  pScsiCommand->MemoryDescriptor_ptr,
													  offset,
													  false,
													  count
													  );
						if(NULL == newCommand) {
							DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("processReadWriteCommand: Can't alloc Command!!!\n"));
						
							goto ErrorOut;
							
						} else {
							
							fNumberOfNewBuildCommands++;
							
							fProvider->UnitDeviceAtIndex(count)->processSRBCommand(newCommand);
						
							DbgIOLog(DEBUG_MASK_RAID_INFO, ("processReadWriteCommand: Sent Command. Count %d Location %lld, ASectors %d, offset %lld\n", count, reqLocation, actualSectors, offset));

						}		
						
						if (0 == reqSectors) {
							// Done.
							break;
						}
						
						offset += actualSectors * fProvider->blockSize();
						
						reqLocation += actualSectors;
					}
					
				} else {
					DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("processReadWriteCommand: No Unit Device!!!\n"));
					
					goto ErrorOut;
				}
				
				reqLocation -= (endSector + 1);
			}			
		}
			break;
		case NMT_RAID0R2:
		{					
			// 
			// Allcate Buffer.
			//
			IOMemoryDescriptor	**memoryDescriptorArray;
			
			memoryDescriptorArray = (IOMemoryDescriptor **)IOMalloc(sizeof(IOMemoryDescriptor *) * (reqSectors / fProvider->RAIDBlocksPerSector()));
			if(NULL == memoryDescriptorArray) {
				DbgIOLog(DEBUG_MASK_RAID_ERROR, ("processReadWriteCommand: Can't Alloc Meory descpritor array.\n"));
			
				break;
			}
			
			for (int count = 0; count < fProvider->CountOfUnitDevices(); count++) {
				if (fProvider->UnitDeviceAtIndex(count)) {
			
					// Make IOMemoryDescripter.
					uint32_t	diskSequence;
					uint64_t	newLocation;
					uint64_t	locationPointer;
					uint32_t	actualSectors = 0;
					uint32_t	MDIndex;
					uint32_t	subOffset;
					
					MDIndex = 0;
					
					diskSequence = (reqLocation / fProvider->RAIDBlocksPerSector()) % fProvider->CountOfUnitDevices();
					
					if (diskSequence == count) {
						locationPointer = reqLocation;						
					} else if (diskSequence > count) {
						locationPointer = (reqLocation / fProvider->CountOfUnitDevices() / fProvider->RAIDBlocksPerSector()
										   * fProvider->CountOfUnitDevices() * fProvider->RAIDBlocksPerSector())
											+ fProvider->RAIDBlocksPerSector() * fProvider->CountOfUnitDevices()
											+ fProvider->RAIDBlocksPerSector() * count;
					} else {
						locationPointer = (reqLocation / fProvider->CountOfUnitDevices() / fProvider->RAIDBlocksPerSector()
										   * fProvider->CountOfUnitDevices() * fProvider->RAIDBlocksPerSector())
											+ fProvider->RAIDBlocksPerSector() * count;
					}
						
					// Calc offset.
					subOffset = locationPointer - reqLocation + offset;
					
					while(locationPointer <= reqLocation + reqSectors - 1) {
												
						uint32_t	subSectors;
						
						subSectors = ((reqLocation + reqSectors) - locationPointer > (fProvider->RAIDBlocksPerSector() - (locationPointer % fProvider->RAIDBlocksPerSector()))) ? 
							(fProvider->RAIDBlocksPerSector()  - (locationPointer % fProvider->RAIDBlocksPerSector())): 
							(reqLocation + reqSectors) - locationPointer;		
						
						//DbgIOLog(DEBUG_MASK_RAID_ERROR, ("processReadWriteCommand: locationPointer %lld sectors %d offset %d\n", locationPointer, subSectors, offset));

#if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= 1060

						fMemoryDescriptorArray[MDIndex] = IOSubMemoryDescriptor::withSubRange(
																						  pScsiCommand->MemoryDescriptor_ptr,
																						  subOffset * fProvider->blockSize(),
																						  subSectors * fProvider->blockSize(),
																						  pScsiCommand->MemoryDescriptor_ptr->getDirection()
																						  ); 
#else

						fMemoryDescriptorArray[MDIndex] = IOMemoryDescriptor::withSubRange(
																						   pScsiCommand->MemoryDescriptor_ptr,
																						   subOffset * fProvider->blockSize(),
																						   subSectors * fProvider->blockSize(),
																						   pScsiCommand->MemoryDescriptor_ptr->getDirection()
																						   ); 
						
#endif
						if(NULL == fMemoryDescriptorArray[MDIndex]) {
							DbgIOLog(DEBUG_MASK_RAID_ERROR, ("processReadWriteCommand: Can't Make MD Index %d\n", MDIndex));
							
							goto ErrorOut;
						} 
 						
						fMemoryDescriptorArray[MDIndex]->prepare();
						
						MDIndex++;
						
						subOffset += subSectors;
						subOffset += fProvider->RAIDBlocksPerSector() * (fProvider->CountOfUnitDevices() - 1);
						
						actualSectors += subSectors;
						
						locationPointer += subSectors;
						locationPointer += fProvider->RAIDBlocksPerSector() * (fProvider->CountOfUnitDevices() - 1);
					}
					
					if (MDIndex > 0) {
						
						IOMultiMemoryDescriptor	*multiMemoryDescriptor = NULL;

						multiMemoryDescriptor = IOMultiMemoryDescriptor::withDescriptors(
																						 (IOMemoryDescriptor**)fMemoryDescriptorArray,
																						 MDIndex,
																						 pScsiCommand->MemoryDescriptor_ptr->getDirection(),
																						 false
																						 );
						if(NULL == multiMemoryDescriptor) {
							DbgIOLog(DEBUG_MASK_RAID_ERROR, ("processReadWriteCommand: Can't Make MMD\n"));
							
							goto ErrorOut;
						}
												
						// Free MDs
						for (int index = 0; index < MDIndex ; index++) {
							fMemoryDescriptorArray[index]->release();
						}
						
						// Build New Command and Send to Unit Device.
						com_ximeta_driver_NDASSCSICommand	*newCommand;
						
						newLocation = ((reqLocation / fProvider->CountOfUnitDevices() / fProvider->RAIDBlocksPerSector()) * fProvider->RAIDBlocksPerSector());
						diskSequence = (reqLocation / fProvider->RAIDBlocksPerSector()) % fProvider->CountOfUnitDevices();
						
						if (count >= diskSequence) {
							newLocation += (reqLocation % fProvider->RAIDBlocksPerSector());
						} else {
							newLocation += fProvider->RAIDBlocksPerSector();
						}
												
						// Make New Command and Sent to Lower Unit Device.
						newCommand = BuildSCSICommand(
													  pScsiCommand->cdb[0],
													  newLocation,
													  actualSectors * (fProvider->blockSize() / fProvider->UnitDeviceAtIndex(count)->blockSize() ),
													  fProvider->UnitDeviceAtIndex(count)->blockSize(),
													  multiMemoryDescriptor,
													  0,
													  false,
													  count
													  );
						if(NULL == newCommand) {
							DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("processReadWriteCommand: Can't alloc Command!!!\n"));
							
							multiMemoryDescriptor->release();
							
							goto ErrorOut;
							
						} else {
							
							fNumberOfNewBuildCommands++;
							
							fProvider->UnitDeviceAtIndex(count)->processSRBCommand(newCommand);
							
							DbgIOLog(DEBUG_MASK_RAID_INFO, ("processReadWriteCommand: Sent Command. Count %d Location %lld, ASectors %d\n", count, newLocation, actualSectors));
						}
					} else {
						DbgIOLog(DEBUG_MASK_NDAS_INFO, ("processReadWriteCommand: Noting to do. index %d\n", count));
					}
				} else {
					DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("processReadWriteCommand: No Unit Device!!!\n"));
					
					goto ErrorOut;
				}
			}  // For loop.		
			
			// Release memory Descriptor Array.
			IOFree(memoryDescriptorArray, sizeof(IOMemoryDescriptor *) * (reqSectors / fProvider->RAIDBlocksPerSector()));
			
		}
			break;			
		case NMT_RAID1R3:
		{
			com_ximeta_driver_NDASSCSICommand	*newCommand = NULL;
			
			switch(pScsiCommand->cdb[0]) {
				case kSCSICmd_READ_10:
				{

					if ( fProvider->UnitDeviceAtIndex(0) && fProvider->UnitDeviceAtIndex(0)->IsInRunningStatus()
						 && fProvider->UnitDeviceAtIndex(0)->IsConnected() 
						 && kNDASRAIDSubUnitStatusGood == fProvider->StatusOfSubUnit(0) ) {
						
						if ( reqSectors > 1
							 && fProvider->UnitDeviceAtIndex(1) && fProvider->UnitDeviceAtIndex(1)->IsInRunningStatus() 
							 && fProvider->UnitDeviceAtIndex(1)->IsConnected() 
							 && kNDASRAIDSubUnitStatusGood == fProvider->StatusOfSubUnit(1) ) {
							
							uint32_t	reqSectorsForUnit0;
							
							DbgIOLog(DEBUG_MASK_NDAS_INFO, ("processReadWriteCommand: Sent 0\n"));
							
							reqSectorsForUnit0 = (reqSectors / 2);
							
							// Make New Command and Sent to Lower Unit Device 0.
							newCommand = BuildSCSICommand(
														  pScsiCommand->cdb[0],
														  reqLocation,
														  reqSectorsForUnit0 * (fProvider->blockSize() / fProvider->UnitDeviceAtIndex(0)->blockSize() ),
														  fProvider->UnitDeviceAtIndex(0)->blockSize(),
														  pScsiCommand->MemoryDescriptor_ptr,
														  offset,
														  true,
														  0
														  );
							
							if (NULL != newCommand) {
								fNumberOfNewBuildCommands++;
								
								fProvider->UnitDeviceAtIndex(0)->processSRBCommand(newCommand);
							} else {
								DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("processReadWriteCommand: Can't make new command!!!\n"));
								
								goto ErrorOut;
							}
							
							DbgIOLog(DEBUG_MASK_NDAS_INFO, ("processReadWriteCommand: Sent 1\n"));
							
							// Make New Command and Sent to Lower Unit Device 1.
							newCommand = BuildSCSICommand(
														  pScsiCommand->cdb[0],
														  reqLocation + reqSectorsForUnit0,
														  (reqSectors - reqSectorsForUnit0) * (fProvider->blockSize() / fProvider->UnitDeviceAtIndex(1)->blockSize() ),
														  fProvider->UnitDeviceAtIndex(1)->blockSize(),
														  pScsiCommand->MemoryDescriptor_ptr,
														  offset + (reqSectorsForUnit0 * fProvider->blockSize()),
														  true,
														  1
														  );
							
							if (NULL != newCommand) {
								fNumberOfNewBuildCommands++;
								
								fProvider->UnitDeviceAtIndex(1)->processSRBCommand(newCommand);
							} else {
								DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("processReadWriteCommand: Can't make new command!!!\n"));
								
								goto ErrorOut;
							}
							
						} else {
							
							DbgIOLog(DEBUG_MASK_NDAS_INFO, ("processReadWriteCommand: Sent 0\n"));
							
							// Make New Command and Sent to Lower Unit Device.
							newCommand = BuildSCSICommand(
														  pScsiCommand->cdb[0],
														  reqLocation,
														  reqSectors * (fProvider->blockSize() / fProvider->UnitDeviceAtIndex(0)->blockSize() ),
														  fProvider->UnitDeviceAtIndex(0)->blockSize(),
														  pScsiCommand->MemoryDescriptor_ptr,
														  offset,
														  true,
														  0
														  );
							
							if (NULL != newCommand) {
								fNumberOfNewBuildCommands++;
								
								fProvider->UnitDeviceAtIndex(0)->processSRBCommand(newCommand);
							} else {
								DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("processReadWriteCommand: Can't make new command!!!\n"));
								
								goto ErrorOut;
							}
						}
						
					} else if ( fProvider->UnitDeviceAtIndex(1) && fProvider->UnitDeviceAtIndex(1)->IsInRunningStatus() 
								&& fProvider->UnitDeviceAtIndex(1)->IsConnected() 
								&& kNDASRAIDSubUnitStatusGood == fProvider->StatusOfSubUnit(1) ) {
						
						DbgIOLog(DEBUG_MASK_NDAS_INFO, ("processReadWriteCommand: Sent 1\n"));
						
						// Make New Command and Sent to Lower Unit Device.
						newCommand = BuildSCSICommand(
													  pScsiCommand->cdb[0],
													  reqLocation,
													  reqSectors * (fProvider->blockSize() / fProvider->UnitDeviceAtIndex(1)->blockSize() ),
													  fProvider->UnitDeviceAtIndex(1)->blockSize(),
													  pScsiCommand->MemoryDescriptor_ptr,
													  offset,
													  true,
													  1
													  );
						if (NULL != newCommand) {
							fNumberOfNewBuildCommands++;
						
							fProvider->UnitDeviceAtIndex(1)->processSRBCommand(newCommand);
						} else {
							DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("processReadWriteCommand: Can't make new command!!!\n"));
							
							goto ErrorOut;

						}
					} else {
						// ERROR.
						DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("processReadWriteCommand: Both has error!!!\n"));
						
						goto ErrorOut;
					}
				}
					break;
				case kSCSICmd_WRITE_10:
				{
					// Mark Bitmap.
					switch (fProvider->RAIDStatus()) {
						case kNDASRAIDStatusGood:
						case kNDASRAIDStatusGoodWithReplacedUnit:
						case kNDASRAIDStatusRecovering:
						{
							// Manage written history queue.
							fSizeOfWrittenSectors += reqSectors;
							
							// Append to List.
							fWrittenLocationsList->setObject(OSNumber::withNumber(reqLocation, 64));
							fWrittenSectorsList->setObject(OSNumber::withNumber(reqSectors, 32));
							
							OSNumber *firstWrittenSectors = OSDynamicCast(OSNumber, fWrittenSectorsList->getObject(0));
							
							while (firstWrittenSectors 
								   && (fSizeOfWrittenSectors - firstWrittenSectors->unsigned32BitValue()) > DISK_CACHE_SIZE_BY_BLOCK_SIZE) {
								
								fSizeOfWrittenSectors -= firstWrittenSectors->unsigned32BitValue();
								
								// Remove first(old) written history.
								OSObject	*temp;
								
								if (temp = fWrittenLocationsList->getObject(0)) {
									temp->release();
									fWrittenLocationsList->removeObject(0);
								}
								
								if (temp = fWrittenSectorsList->getObject(0)) {
									temp->release();
									fWrittenSectorsList->removeObject(0);
								}
								
								firstWrittenSectors = OSDynamicCast(OSNumber, fWrittenSectorsList->getObject(0));
								
								
							}
															
							DbgIOLog(DEBUG_MASK_NDAS_INFO, ("processReadWriteCommand: fSizeOfWrittenSectors %d, entries %d\n", fSizeOfWrittenSectors, fWrittenSectorsList->getCount()));

						} 
							break;
						case kNDASRAIDStatusGoodButPartiallyFailed:
						case kNDASRAIDStatusBrokenButWorkable:
						{														
							// Set dirty bit.
							fProvider->SetBitmap( reqLocation, reqSectors, true);
						}
							break;
						default:
							break;
					}							
					
					for (int count = 0; count < fProvider->CountOfUnitDevices(); count++) {
						if (fProvider->UnitDeviceAtIndex(count) && fProvider->UnitDeviceAtIndex(count)->IsInRunningStatus()
							&& fProvider->UnitDeviceAtIndex(count)->IsConnected() 
							&& (kNDASRAIDSubUnitStatusGood == fProvider->StatusOfSubUnit(count) 
								|| kNDASRAIDSubUnitStatusNeedRecovery == fProvider->StatusOfSubUnit(count)
								|| kNDASRAIDSubUnitStatusRecovering == fProvider->StatusOfSubUnit(count)) ) {

							fNumberOfNewBuildCommands++;
							
							// Make New Command and Sent to Lower Unit Device.
							newCommand = BuildSCSICommand(
														  pScsiCommand->cdb[0],
														  reqLocation,
														  reqSectors * (fProvider->blockSize() / fProvider->UnitDeviceAtIndex(count)->blockSize() ),
														  fProvider->UnitDeviceAtIndex(count)->blockSize(),
														  pScsiCommand->MemoryDescriptor_ptr,
														  offset, 
														  false,
														  count
														  );
							
							fProvider->UnitDeviceAtIndex(count)->processSRBCommand(newCommand);							
						}
					}
				}
					break;
			}
		}
			break;
		default:
			break;
	}
	
	return;
	
ErrorOut:
		
		pScsiCommand->CmdData.results = kIOReturnBusy;
		pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_No_Status;

		// Complete Recieved Command.
		fProvider->completeRecievedCommand();

		return;
}

static void workerThreadWrapper(void* argument
#if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= MAC_OS_X_VERSION_10_4
, __unused wait_result_t wr)
#else
)
#endif
{		
	com_ximeta_driver_NDASRAIDUnitDeviceController *controller = (com_ximeta_driver_NDASRAIDUnitDeviceController*)argument;
	
	controller->workerThread(NULL);
	
	return;
}

IOReturn com_ximeta_driver_NDASRAIDUnitDeviceController::message (
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
					
			messageClients(kIOMessageServiceIsSuspended);
			
		}
			break;
			
		case kIOMessageServiceIsResumed:
		{
			DbgIOLog(DEBUG_MASK_POWER_INFO, ("kIOMessageServiceIsResumed\n"));
									
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
				
				//executeAndWaitCommand(command);
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
/*			
			// Wait Thread Termination.
			semaphore_wait(fExitSema);
			*/						
		}
			break;
		default:
			status = IOService::message (type, nub, arg);
			break;
			
	}
	
	return status;
	
}

void com_ximeta_driver_NDASRAIDUnitDeviceController::InitAndSetBitmap()
{
	// Set Dirty.
	if ( !fProvider->IsDirty()
		 && (kNDASRAIDSubUnitStatusGood != fProvider->StatusOfSubUnit(0)
			 || kNDASRAIDSubUnitStatusGood != fProvider->StatusOfSubUnit(1)) ) {
		
		DbgIOLog(DEBUG_MASK_DISK_WARNING, ("processSrbCompletedCommand: Set Dirty!!!!!!!!!!!!!!!!\n"));
		
		fProvider->setDirty(true);
		
		uint32_t IndexOfBrokenUnit;
		
		if ( kNDASRAIDSubUnitStatusGood == fProvider->StatusOfSubUnit(0) ) {
			IndexOfBrokenUnit = 1;
		} else if ( kNDASRAIDSubUnitStatusGood == fProvider->StatusOfSubUnit(1) ) {
			IndexOfBrokenUnit = 0;
		} else {
			return;
		}
		
		//
		// Update RMD of GoodSubUnit.
		//
		NDAS_RAID_META_DATA					tempRMD;
		com_ximeta_driver_NDASUnitDevice	*GoodUnitDevice = fProvider->UnitDeviceAtIndex( (IndexOfBrokenUnit + 1) % 2);
		uint32_t							tempValue;
		
		memcpy(&tempRMD, GoodUnitDevice->RMD(), sizeof(NDAS_RAID_META_DATA));
		
		// update flag.
		tempValue = getRMDStatus(&tempRMD) | NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED;
		tempRMD.state = NDASSwap32HostToLittle(tempValue);
		
		// update USN.
		tempValue = getRMDUSN(&tempRMD) + 1;						
		tempRMD.uiUSN = NDASSwap32HostToLittle(tempValue);
		
		// update unit device info.
		tempRMD.UnitMetaData[IndexOfBrokenUnit].UnitDeviceStatus = NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED;
		
		SET_RMD_CRC(crc32_calc, tempRMD);
		
		// Update RMD.
		uint64_t location = GoodUnitDevice->NumberOfSectors() + NDAS_BLOCK_LOCATION_RMD;
		
		if(!GoodUnitDevice->writeSectorsOnce(location, 1,  (char *)&tempRMD)) {
			DbgIOLog(DEBUG_MASK_RAID_ERROR, ("processSrbCompletedCommand: Write RMD failed.\n"));
		} else {
			DbgIOLog(DEBUG_MASK_RAID_WARNING, ("processSrbCompletedCommand: Write RMD to %lld USN %d.\n", location, getRMDUSN(&tempRMD)));
			GoodUnitDevice->checkRMD();
		}
		
		//
		// Clear Bitmap.	
		//
		if (fProvider->allocDirtyBitmap()) {
			
			location = GoodUnitDevice->NumberOfSectors() + NDAS_BLOCK_LOCATION_BITMAP;
			
			DbgIOLog(DEBUG_MASK_RAID_INFO, ("processSrbCompletedCommand: Write Bitmap Location %lld\n", location));
			
			if(!GoodUnitDevice->writeSectorsOnce(location, NDAS_BLOCK_SIZE_BITMAP, fProvider->dirtyBitmap())) {
				DbgIOLog(DEBUG_MASK_RAID_ERROR, ("processSrbCompletedCommand: Write BitMap failed.\n"));							
			}
		}
		
		//
		// Mark Bitmap with written history list.
		//
		while (fWrittenLocationsList->getCount() > 0) {
			
			OSNumber	*location;
			OSNumber	*sectors;
			
			if (location = OSDynamicCast(OSNumber, fWrittenLocationsList->getObject(0))) {
				fWrittenLocationsList->removeObject(0);
			} else {
				DbgIOASSERT(location);
			}
			
			if (sectors = OSDynamicCast(OSNumber, fWrittenSectorsList->getObject(0))) {
				fWrittenSectorsList->removeObject(0);
			} else {
				DbgIOASSERT(sectors);
			}
			
			fProvider->SetBitmap(
								 location->unsigned64BitValue(),
								 sectors->unsigned32BitValue(),
								 true);
			
			fSizeOfWrittenSectors -= sectors->unsigned32BitValue();
		}
		
		fProvider->updateRAIDStatus();
	}					
}