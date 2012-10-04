/***************************************************************************
*
*  NDASBusEnumerator.cpp
*
*    NDAS Bus Enumerator
*
*    Copyright (c) 2004 XiMeta, Inc. All rights reserved.
*
**************************************************************************/

extern "C" {
#include <sys/systm.h>		// For thread_funnel_switch()
#include <sys/socketvar.h>
#include <sys/socket.h>
#include <sys/malloc.h>
#include <pexpert/pexpert.h>//This is for debugging purposes ONLY
#include "LanScsiDiskInformationBlock.h"
#include "NDASDIB.h"
#include "NDASFamilyIOMessage.h"
}

#include <IOKit/IOLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/network/IONetworkController.h>
#include <IOKit/network/IOEthernetInterface.h>

#include "NDASBusEnumerator.h"
#include "NDASDevice.h"
#include "NDASCommand.h"
#include "NDASLogicalDevice.h"
#include "NDASProtocolTransportNub.h"
#include "NDASPhysicalUnitDevice.h"
#include "NDASRAIDUnitDevice.h"
#include "NDASUnitDeviceList.h"

#include "Utilities.h"
#include "LanScsiSocket.h"

#define kOneSecond	(1 * 1000 * 1000)

// Define my superclass
#define super IOService

// REQUIRED! This macro defines the class's constructors, destructors,
// and several other methods I/O Kit requires. Do NOT use super as the
// second parameter. You must use the literal name of the superclass.

OSDefineMetaClassAndStructors(com_ximeta_driver_NDASBusEnumerator, IOService)

/*
static void terminateDeviceThread(void* argument)
{		
	com_ximeta_driver_NDASDevice	*NDASDevicePtr = NULL;
	
	NDASDevicePtr = (com_ximeta_driver_NDASDevice *)argument;
	
	while(!NDASDevicePtr->isInactive()) {
		IOSleep(100);	// 100 ms
	};
	
	// Release Device.
	NDASDevicePtr->release();
	
	IOExitThread();
	
	// Never return.
}
*/

#pragma mark -
#pragma mark == Basic Methods ==
#pragma mark -

bool com_ximeta_driver_NDASBusEnumerator::init(OSDictionary *dict)
{
	bool res = super::init(dict);
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Initializing\n"));
	
	// Init. member variables.
	fDeviceListPtr = NULL;
	fLogicalDeviceList = NULL;
	fRAIDDeviceList = NULL;
	
	fWillTerminate = false;
	fThreadTerminate = false;
	
	fWorkerThread = NULL;
	fCommandSema = NULL;
	
	for(int count = 0; count < NUMBER_OF_INTERFACES; count++) {
		memset(&fInterfaces[count], 0, sizeof(EthernetInterface));
	}
	
	fCommandArray = OSArray::withCapacity(0);
	
	fNumberOfLogicalDevices = 0;
	
    return res;
}

void com_ximeta_driver_NDASBusEnumerator::free(void)
{
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Freeing\n"));
	
	DbgIOLog(DEBUG_MASK_USER_ERROR, ("stop: Before Wait sema.\n"));
	
	// Wait Thread termination.
	bool waitMore = true;
	
	while (waitMore) {
		waitMore = false;
		
		IOSleep(1000);
		
		if (fWorkerThread) {
			DbgIOLog(DEBUG_MASK_USER_ERROR, ("stop: Worker Thread is still running!\n"));
			
			waitMore = true;
			
			semaphore_signal(fCommandSema);
		}		
	}
	
	DbgIOLog(DEBUG_MASK_USER_ERROR, ("stop: After Wait sema.\n"));
	
	if (fCommandGate) { 
		
		// Free all command.
		while (fCommandArray->getCount() > 0) {
			com_ximeta_driver_NDASCommand *tempCommand = (com_ximeta_driver_NDASCommand *)fCommandArray->getObject(0);
			
			fCommandArray->removeObject(0);
			
			DbgIOLog(DEBUG_MASK_PNP_ERROR, ("Stop: Free command!!!\n"));
			
			if (NULL != tempCommand) {
				dequeueMessage(tempCommand);
			}
		}
		
		fWorkLoop->removeEventSource(fCommandGate);     
		fCommandGate->release();     
		fCommandGate = 0; 
	} 
	
	// Destroy Semaphores.
	semaphore_destroy(current_task(), fCommandSema);
	
	fCommandArray->release();
	
	if (cntrlSync && cntrlSync != (IOWorkLoop *)1) {
		cntrlSync->release();
	}
	
	super::free();
}

IOService *com_ximeta_driver_NDASBusEnumerator::probe(IOService *provider, SInt32 *score)
{
    IOService *res = super::probe(provider, score);
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Probing\n"));
    return res;
}

enum {
	kNDASPowerOffState = 0,
	kNDASPowerOnState = 1,
	kNumberNDASPowerStates = 2
};

static IOPMPowerState powerStates[ kNumberNDASPowerStates ] = {
	{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 1, kIOPMDeviceUsable,		kIOPMPowerOn,	kIOPMPowerOn,	0, 0, 0, 0, 0, 0, 0, 0 }
};

bool com_ximeta_driver_NDASBusEnumerator::start(IOService *provider)
{
	fProvider = provider;
			
    bool res = super::start(provider);
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Starting\n"));
		
	// Allocate Device Array.
	fDeviceListPtr = OSOrderedSet::withCapacity(INITIAL_NUMBER_OF_DEVICES);
	if(fDeviceListPtr == NULL) {
	
		DbgIOLog(DEBUG_MASK_PNP_ERROR, ("Start: Can't Allocate Device Array with Capacity %d\n", INITIAL_NUMBER_OF_DEVICES));
		
		return false;
	}

	// Allocate Unit Device Array.
	fLogicalDeviceList = OSDynamicCast(com_ximeta_driver_NDASUnitDeviceList,
									   OSMetaClass::allocClassWithName(NDAS_DEVICE_LIST_CLASS));
	
	if(fLogicalDeviceList == NULL || !fLogicalDeviceList->init()) {
		
		DbgIOLog(DEBUG_MASK_PNP_ERROR, ("Start: Can't Allocate Unit Device Array with Capacity %d\n", INITIAL_NUMBER_OF_DEVICES));
		
		return false;
	}
	
	// Allocate RAID Device Array.
	fRAIDDeviceList = OSDynamicCast(com_ximeta_driver_NDASUnitDeviceList,
									   OSMetaClass::allocClassWithName(NDAS_DEVICE_LIST_CLASS));
	
	if(fRAIDDeviceList == NULL || !fRAIDDeviceList->init()) {
		
		DbgIOLog(DEBUG_MASK_PNP_ERROR, ("Start: Can't Allocate RAID Device Array with Capacity %d\n", INITIAL_NUMBER_OF_DEVICES));
		
		return false;
	}
		
	// Power Management.
	PMinit();
	provider->joinPMtree(this);
	registerPowerDriver(this, powerStates, kNumberNDASPowerStates);
	
	fWorkLoop = (IOWorkLoop *)getWorkLoop();
	
	// Create PnP Timer.
	fInterfaceCheckTimerSourcePtr = IOTimerEventSource::timerEventSource(this, InterfaceCheckTimerWrapper);
		
	if(!fInterfaceCheckTimerSourcePtr) {
		DbgIOLog(DEBUG_MASK_PNP_ERROR, ("Failed to create timer event source!\n"));
	} else {
		if(fWorkLoop->addEventSource(fInterfaceCheckTimerSourcePtr) != kIOReturnSuccess) {
			DbgIOLog(DEBUG_MASK_PNP_ERROR, ("Failed to ad timer event source to work loop!\n"));
		}
	}
		
	fInterfaceCheckTimerSourcePtr->setTimeoutMS(100);
	
	// Create command Gate.
	fCommandGate = IOCommandGate::commandGate(this, receiveMsgWrapper);
	
	if(!fCommandGate) {
		DbgIOLog(DEBUG_MASK_PNP_ERROR, ("can't create commandGate\n"));
		return false;
	}
		
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
	    
    fWorkerThread = IOCreateThread(deviceManagementThreadWrapper, this);
	if(!fWorkerThread) {
		DbgIOLog(DEBUG_MASK_PNP_ERROR, ("start::IOCreateThread(), failed to Create Thread\n"));
		
		semaphore_destroy(kernel_task, fCommandSema);
		
		return false;
	}
	
	// Create Power Down Timer.
	fpowerTimerSourcePtr = IOTimerEventSource::timerEventSource(this, PowerTimerWrapper);
	
	if(!fpowerTimerSourcePtr) {
		DbgIOLog(DEBUG_MASK_POWER_ERROR, ("Failed to create timer event source!\n"));
	} else {
		if(fWorkLoop->addEventSource(fpowerTimerSourcePtr) != kIOReturnSuccess) {
			DbgIOLog(DEBUG_MASK_POWER_ERROR, ("Failed to ad timer event source to work loop!\n"));
		}
	}
	
	//DbgIOLog(DEBUG_MASK_PNP_ERROR, ("Power Timer 0x%x!\n", fpowerTimerSourcePtr));
	
	// No idle sleep
	changePowerStateTo(kNDASPowerOnState);
	
	// publish ourselves so clients can find us 
	registerService();
	
    return res;
	
bad:	
	return false;	
}

bool com_ximeta_driver_NDASBusEnumerator::finalize (
													   IOOptionBits options 
													   )
{

	DbgIOLog(DEBUG_MASK_PNP_TRACE, ("finalizing.\n"));
	
	// set Will Terminate flag
	fWillTerminate = true;
	
	// Cancel Timer.
	if(fInterfaceCheckTimerSourcePtr) {
		fInterfaceCheckTimerSourcePtr->cancelTimeout();
		fWorkLoop->removeEventSource(fInterfaceCheckTimerSourcePtr);
		fInterfaceCheckTimerSourcePtr->release();
		fInterfaceCheckTimerSourcePtr = NULL;
	}
	
	if(fpowerTimerSourcePtr) {
		fpowerTimerSourcePtr->cancelTimeout();
		fWorkLoop->removeEventSource(fpowerTimerSourcePtr);
		fpowerTimerSourcePtr->release();
		fpowerTimerSourcePtr = NULL;		
	}
	
	// close socket.
	for (int count = 0; count < NUMBER_OF_INTERFACES; count++) {
		releaseBroadcastReceiver(count);
	}
/*	
	// Wait Thread termination.
	bool waitMore = true;
	
	while (waitMore) {
		waitMore = false;

		IOSleep(1000);

		for (int count = 0; count < NUMBER_OF_INTERFACES; count++) {
			if (fInterfaces[count].fThread) {
				DbgIOLog(DEBUG_MASK_USER_ERROR, ("finalize: Broadcast Thread %d is still running!\n", count, fInterfaces[count].fThread));

				waitMore = true;
			}
		}
	}
*/		
	
	//
	// Unregister All Logical Devices.
	//
	OSMetaClassBase							*metaObject = NULL;
/*
	com_ximeta_driver_NDASLogicalDevice	*NDASLogicalDevicePtr = NULL;
	
	while((metaObject = fLogicalDeviceList->getFirstObject()) != NULL) { 
		
		NDASLogicalDevicePtr = OSDynamicCast(com_ximeta_driver_NDASLogicalDevice, metaObject);
		if(NDASLogicalDevicePtr == NULL) {
			DbgIOLog(DEBUG_MASK_USER_ERROR, ("stop: Can't Cast to Device Class.\n"));
			continue;
		}
		
		// Delete from Device List.
		fLogicalDeviceList->removeObject(NDASLogicalDevicePtr);
		
		// Terminate Device.
		NDASLogicalDevicePtr->terminate();
		
		//while(!NDASDevicePtr->isInactive()) {
		//	IOSleep(100);	// 100 ms
		//};
		
		// Release Device.
		NDASLogicalDevicePtr->release();
	}
*/	
	/*
	//
	// Unregister All RAID Devices.
	//		
	com_ximeta_driver_NDASRAIDUnitDevice	*raidDevice = NULL;
	
	while((metaObject = fRAIDDeviceList->getLastObject()) != NULL) { 
		
		raidDevice = OSDynamicCast(com_ximeta_driver_NDASRAIDUnitDevice, metaObject);
		if(raidDevice == NULL) {
			DbgIOLog(DEBUG_MASK_USER_ERROR, ("finalize: Can't Cast to Device Class.\n"));
			continue;
		}
		
		// Unset Unit Device.
		for (int count = 0; count <  raidDevice->CountOfRegistetedUnitDevices(); count++) {
			
			com_ximeta_driver_NDASUnitDevice	*unitDevice = NULL;
			
			if (NULL != (unitDevice = raidDevice->UnitDeviceAtIndex(count))) {
				unitDevice->setUpperUnitDevice(NULL);
				
				raidDevice->setUnitDevice(count, NULL, kNDASRAIDSubUnitStatusNotPresent);
			}			
		}
		
		// Delete from List.
		fRAIDDeviceList->removeObject(raidDevice);
		
		// Send kIOMessageServiceIsRequestingClose message.
		//messageClient ( kIOMessageServiceIsRequestingClose, raidDevice );
		
		// Terminate Device.
		raidDevice->terminate();
		
		//raidDevice->willTerminate(this, 0);
		
		// Release Device.
		raidDevice->release();		
	}
*/
	//
	// Unregister All NDAS Devices.
	//
	com_ximeta_driver_NDASDevice	*NDASDevicePtr = NULL;
	
	while((metaObject = fDeviceListPtr->getFirstObject()) != NULL) { 
		
		NDASDevicePtr = OSDynamicCast(com_ximeta_driver_NDASDevice, metaObject);
		if(NDASDevicePtr == NULL) {
			DbgIOLog(DEBUG_MASK_USER_ERROR, ("finalize: Can't Cast to Device Class.\n"));
			continue;
		}
				
		
		// Delete from Device List.
		fDeviceListPtr->removeObject(NDASDevicePtr);
		
		// Terminate Device.
		NDASDevicePtr->terminate();
				
		while(!NDASDevicePtr->isInactive()) {
			DbgIOLog(DEBUG_MASK_USER_ERROR, ("finalize: Wait for termination.\n"));
			IOSleep(1000);	// 1000 ms
		};
		
		// Release Device.
		NDASDevicePtr->release();
	}
	
	// Terminate Thread.
	if(false == fThreadTerminate) {
		// Terminate Thread.
		fThreadTerminate = true;
		
		semaphore_signal(fCommandSema);
	}			
	/*
	// Create NDAS Command.
	com_ximeta_driver_NDASCommand *command = OSDynamicCast(com_ximeta_driver_NDASCommand, 
							OSMetaClass::allocClassWithName(NDAS_COMMAND_CLASS));
	
	if(command != NULL || command->init()) {
	
		command->setToTerminateCommand();
		
		enqueueMessage(command);
		
	} else {
		DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("receivePnPMessage: failed to alloc command class\n"));	
	}
	*/			
		
	return super::finalize ( options );
	
}

void com_ximeta_driver_NDASBusEnumerator::stop(IOService *provider)
{
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Stopping\n"));
			
	// Power management.
	PMstop();
	
	// Free RAID Device Array.
	fRAIDDeviceList->release();
	
	// Free Unit Device Array.
	fLogicalDeviceList->release();
	
	// Free Device Array.
	fDeviceListPtr->flushCollection();
	fDeviceListPtr->release();
	
    super::stop(provider);
}

IOReturn com_ximeta_driver_NDASBusEnumerator::message (
															 UInt32 		type,
															 IOService *	nub,
															 void * 		arg 
															 )
{
	DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("message Entered. 0x%x\n", type));
	
	IOReturn				status		= kIOReturnSuccess;
	
	switch ( type )
	{
		
		case kIOMessageServiceIsSuspended:
		{
			DbgIOLog(DEBUG_MASK_POWER_INFO, ("message: kIOMessageServiceIsSuspended\n"));
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
		}
			break;
		case kIOMessageServiceIsTerminated:
		{
			DbgIOLog(DEBUG_MASK_NDAS_INFO, ("kIOMessageServiceIsTerminated\n"));			
		}
			break;
		case  kIOMessageServicePropertyChange:
		{
			// Do nothing.
			DbgIOLog(DEBUG_MASK_NDAS_INFO, ("kIOMessageServicePropertyChange\n"));
		}
		default:
			status = IOService::message (type, nub, arg);
			break;
			
	}
	
	return status;
	
}

IOWorkLoop *com_ximeta_driver_NDASBusEnumerator::getWorkLoop()
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

#pragma mark -
#pragma mark == Message(Command) Management ==
#pragma mark -

bool com_ximeta_driver_NDASBusEnumerator::enqueueMessage(com_ximeta_driver_NDASCommand *command)
{
	DbgIOLog(DEBUG_MASK_NDAS_TRACE, ("enqueueMessage: Entered\n"));
		
	if (fWillTerminate) {
		return false;
	}
	
//	command->retain();
	
	if(fCommandGate) {
		if (kIOReturnSuccess == fCommandGate->runCommand((void *)kNDASCommandQueueAppend, (void*)command)) {
			return true;
		} else {
//			command->release();
			return false;
		}
	} else {
//		command->release();
		return false;
	}
}

void com_ximeta_driver_NDASBusEnumerator::dequeueMessage(com_ximeta_driver_NDASCommand *command)
{
	DbgIOLog(DEBUG_MASK_NDAS_TRACE, ("dequeueMessage: Entered\n"));
	
//	command->release();
}

IOReturn com_ximeta_driver_NDASBusEnumerator::receiveMsg ( void * newRequest, void *command, void *arg, void * )
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
			
			int arrayIndex;
			bool found = false;
			
			for (arrayIndex = 0; arrayIndex < fCommandArray->getCount(); arrayIndex++) {
				com_ximeta_driver_NDASCommand	*tempCommand = OSDynamicCast(com_ximeta_driver_NDASCommand, fCommandArray->getObject(arrayIndex));

				if (tempCommand == command) {
					found = true;
					break;
				}
			}
			
			DbgIOASSERT(found);
			
			fCommandArray->removeObject(arrayIndex);
			
			dequeueMessage((com_ximeta_driver_NDASCommand*)command);
			
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
	com_ximeta_driver_NDASBusEnumerator	*bus = (com_ximeta_driver_NDASBusEnumerator*)theDriver;
/*	
	if (NULL == bus ||
		bus->isInactive()) {
		
		DbgIOLogNC(DEBUG_MASK_NDAS_ERROR, ("[com_ximeta_driver_NDASBusEnumerator]receiveMsgWrapper: No Bus Enumerator. bus = 0x%x first = 0x%x\n", bus, first));
		
//		((com_ximeta_driver_NDASCommand *)second)->release();
		
		return kIOReturnError;
	}
*/	
	return bus->receiveMsg(first, second, third, fourth);
}

#pragma mark -
#pragma mark == User Clinet Requests ==
#pragma mark -

// kNDASEnumRegister
IOReturn com_ximeta_driver_NDASBusEnumerator::Register(PNDAS_ENUM_REGISTER			registerData,
													   PNDAS_ENUM_REGISTER_RESULT	result,
													   IOByteCount					inStructSize,
													   IOByteCount					*outStructSize)
{
    DbgIOLog(DEBUG_MASK_USER_TRACE, ("--------------------Register---------------\n"));
	
    // If the user client's 'open' method was never called, return kIOReturnNotOpen.
    if (!isOpen()) {
        DbgIOLog(DEBUG_MASK_USER_ERROR, ("Not Opened\n"));
        return kIOReturnNotOpen;
    }    
	
    bool								bret;
    OSDictionary						*prop = NULL;
    com_ximeta_driver_NDASDevice		*NDASDevicePtr = NULL;
	
	// Search Device List.
	OSCollectionIterator *DeviceListIterator = OSCollectionIterator::withCollection(fDeviceListPtr);
	if(DeviceListIterator == NULL) {
		DbgIOLog(DEBUG_MASK_USER_ERROR, ("Register: Can't Create Iterator.\n"));
		return kIOReturnDeviceError;
	}

	OSMetaClassBase	*metaObject;
	
	// Check Address.
	
	NDASDevicePtr = NULL;

	while((metaObject = DeviceListIterator->getNextObject()) != NULL) {
		NDASDevicePtr = OSDynamicCast(com_ximeta_driver_NDASDevice, metaObject);
		if(NDASDevicePtr == NULL) {
			DbgIOLog(DEBUG_MASK_USER_INFO, ("Register: Can't Cast to Device Class.\n"));
			continue;
		}
		
		if(bcmp(NDASDevicePtr->address()->slpx_node, registerData->DevAddr.slpx_node, 6) == 0
		   && NDASDevicePtr->VID() == registerData->VID) {
			
			registerData->SlotNo = NDASDevicePtr->slotNumber();
			
			DbgIOLog(DEBUG_MASK_USER_ERROR, ("Register: Already Registed NDAS. Slot Number %d.\n", registerData->SlotNo));
			
			DeviceListIterator->release();
			
			// Pass Slot Number.
			result->bDuplicated = true;
			result->SlotNo = registerData->SlotNo;
			*outStructSize = (IOByteCount)(sizeof(NDAS_ENUM_REGISTER_RESULT));			

			return kIOReturnSuccess;
		}
		
		NDASDevicePtr = NULL;
	}
	
	DeviceListIterator->reset();
	
	if(registerData->SlotNo == 0) {
		
		// Assign slot number and check.		
		do {			
			registerData->SlotNo++;
			
			NDASDevicePtr = NULL;
						
			while((metaObject = DeviceListIterator->getNextObject()) != NULL) {
				NDASDevicePtr = OSDynamicCast(com_ximeta_driver_NDASDevice, metaObject);
				if(NDASDevicePtr == NULL) {
					DbgIOLog(DEBUG_MASK_USER_INFO, ("Register: Can't Cast to Device Class.\n"));
					continue;
				}
				
				if(NDASDevicePtr->slotNumber() == registerData->SlotNo) {
					DbgIOLog(DEBUG_MASK_USER_INFO, ("Register: Already Registed Slot Number %d.\n", registerData->SlotNo));
					break;
				}
				
				NDASDevicePtr = NULL;
			}
			
			DeviceListIterator->reset();

		} while(NDASDevicePtr != NULL);
		
	} else {
		
		// Check Slot Number.
		while((metaObject = DeviceListIterator->getNextObject()) != NULL) {
			NDASDevicePtr = OSDynamicCast(com_ximeta_driver_NDASDevice, metaObject);
			if(NDASDevicePtr == NULL) {
				DbgIOLog(DEBUG_MASK_USER_INFO, ("Register: Can't Cast to Device Class.\n"));
				continue;
			}
			
			if(NDASDevicePtr->slotNumber() == registerData->SlotNo) {
				DbgIOLog(DEBUG_MASK_USER_INFO, ("Register: Already Registed Slot Number %d.\n", registerData->SlotNo));
				break;
			}
			
			NDASDevicePtr = NULL;
		}
	}
		
	DeviceListIterator->release();

	// Pass Slot Number.
	result->bDuplicated = false;
	result->SlotNo = registerData->SlotNo;
	*outStructSize = (IOByteCount)(sizeof(NDAS_ENUM_REGISTER_RESULT));			
	
	if(NDASDevicePtr != NULL) {
		DbgIOLog(DEBUG_MASK_USER_ERROR, ("Register: NDASDevicePtr is NULL.\n"));

		return kIOReturnError;
	}
		
	// Create NDASDevice.	  
    prop = OSDictionary::withCapacity(4);
    if(prop == NULL)
    {
        DbgIOLog(DEBUG_MASK_USER_ERROR, ("Register: failed to alloc property\n"));

        return kIOReturnError;
    }

    prop->setCapacityIncrement(4);
    
    NDASDevicePtr = OSDynamicCast(com_ximeta_driver_NDASDevice, 
                               OSMetaClass::allocClassWithName(NDAS_DEVICE_CLASS));
    
    if(NDASDevicePtr == NULL) {
        DbgIOLog(DEBUG_MASK_USER_ERROR, ("Register: failed to alloc Device class\n"));

		prop->release();

        return kIOReturnError;
    }
	
    DbgIOLog(DEBUG_MASK_USER_INFO, ("Register: LanScsiDevice = %p\n", NDASDevicePtr));
    
	// Init Device.
    bret = NDASDevicePtr->init(prop);
    prop->release();
    if(bret == false)
    {
        NDASDevicePtr->release();
        DbgIOLog(DEBUG_MASK_USER_ERROR, ("Register: failed to init Device\n"));

        return kIOReturnError;
    }

	// Assign attributes.
	NDASDevicePtr->setSlotNumber(registerData->SlotNo);
	NDASDevicePtr->setAddress(&(registerData->DevAddr));
	NDASDevicePtr->setWriteRight(registerData->hasWriteRight);
	NDASDevicePtr->setConfiguration(registerData->Configuration);
	NDASDevicePtr->setUserPassword(registerData->UserPassword);
	NDASDevicePtr->setSuperPassword(registerData->SuperPassword);
	NDASDevicePtr->setName(registerData->Name);
	NDASDevicePtr->setDeviceIDString(registerData->DeviceIDString);
	NDASDevicePtr->setVID(registerData->VID);
	
	// Insert Device Array.
	if(fDeviceListPtr->setObject(NDASDevicePtr) == false) {
		DbgIOLog(DEBUG_MASK_USER_ERROR, ("Register: failed to add Device to Device Array\n"));
        
		NDASDevicePtr->release();
		
		return kIOReturnError;
	}

	// Attach Device.
	bret = NDASDevicePtr->attach(this);
	if(bret == true) {
		// Register Service.
		NDASDevicePtr->registerService();
	}
	
	UInt32 slotNo = NDASDevicePtr->slotNumber();
	
	messageClients(kNDASFamilyIOMessageNDASDeviceWasCreated, &slotNo, sizeof(UInt32));

    return kIOReturnSuccess;
}

com_ximeta_driver_NDASDevice* 
com_ximeta_driver_NDASBusEnumerator::FindDeviceBySlotNumber(UInt32 slotNo)
{
    com_ximeta_driver_NDASDevice		*NDASDevicePtr = NULL;
	
	// Search Device List.
	OSCollectionIterator *DeviceListIterator = OSCollectionIterator::withCollection(fDeviceListPtr);
	if(DeviceListIterator == NULL) {
		DbgIOLog(DEBUG_MASK_USER_ERROR, ("FindDeviceBySlotNumber: Can't Create Iterator.\n"));
		return NULL;
	}
	
	OSMetaClassBase	*metaObject;
	
	while((metaObject = DeviceListIterator->getNextObject()) != NULL) { 
		
		NDASDevicePtr = OSDynamicCast(com_ximeta_driver_NDASDevice, metaObject);
		if(NDASDevicePtr == NULL) {
			DbgIOLog(DEBUG_MASK_USER_INFO, ("FindDeviceBySlotNumber: Can't Cast to Device Class.\n"));
			continue;
		}
		
		if(NDASDevicePtr->slotNumber() == slotNo) {
			DbgIOLog(DEBUG_MASK_USER_INFO, ("FindDeviceBySlotNumber: Find Registed Slot Number %d.\n", slotNo));
			break;
		}
		
		NDASDevicePtr = NULL;
	}
	
	DeviceListIterator->release();

	return NDASDevicePtr;
}

// kNDASEnumUnregister
IOReturn com_ximeta_driver_NDASBusEnumerator::Unregister(PNDAS_ENUM_UNREGISTER	unregisterData,
														 UInt32					*result,
														 IOByteCount			inStructSize,
														 IOByteCount			*outStructSize)
{
	DbgIOLog(DEBUG_MASK_USER_TRACE, ("--------------------Unregister---------------\n"));
	
	// If the user client's 'open' method was never called, return kIOReturnNotOpen.
    if (!isOpen()) {
        DbgIOLog(DEBUG_MASK_USER_ERROR, ("Unregister: Not Opened\n"));
        return kIOReturnNotOpen;
    }    

	com_ximeta_driver_NDASDevice		*NDASDevicePtr = NULL;
	
	NDASDevicePtr = FindDeviceBySlotNumber(unregisterData->SlotNo);

	if(NDASDevicePtr == NULL) {
		DbgIOLog(DEBUG_MASK_USER_ERROR, ("Unregister: Can't Find Registed Slot Number %d.\n", unregisterData->SlotNo));
		
		return kIOReturnError;
	}
	
	// Check Configuration.
	if(NDASDevicePtr->configuration() != kNDASConfigurationDisable) {
		DbgIOLog(DEBUG_MASK_USER_ERROR, ("Unregister: Configuration is not Disabled Slot Number %d.\n", unregisterData->SlotNo));
		
		return kIOReturnError;		
	}
	
	UInt32 slotNo = NDASDevicePtr->slotNumber();

	// Delete from Device List.
	fDeviceListPtr->removeObject(NDASDevicePtr);
	
	// Terminate Device.
	NDASDevicePtr->terminate();
			
	//IOCreateThread(terminateDeviceThread, NDASDevicePtr);	
	
	// Release Device.
	NDASDevicePtr->release();
	
	*result = unregisterData->SlotNo;
	*outStructSize = (IOByteCount)(sizeof(UInt32));
		
	messageClients(kNDASFamilyIOMessageNDASDeviceWasDeleted, &slotNo, sizeof(UInt32));

    return kIOReturnSuccess;	
}

// kNDASEnumUnregister
IOReturn com_ximeta_driver_NDASBusEnumerator::Enable(PNDAS_ENUM_ENABLE	enableData,
													 UInt32					*result,
													 IOByteCount			inStructSize,
													 IOByteCount			*outStructSize)
{
	DbgIOLog(DEBUG_MASK_USER_TRACE, ("--------------------Enable---------------\n"));
	
	// If the user client's 'open' method was never called, return kIOReturnNotOpen.
    if (!isOpen()) {
        DbgIOLog(DEBUG_MASK_USER_ERROR, ("Enable: Not Opened\n"));
        return kIOReturnNotOpen;
    }    
	
	com_ximeta_driver_NDASDevice		*NDASDevicePtr = NULL;
	
	NDASDevicePtr = FindDeviceBySlotNumber(enableData->SlotNo);
	
	if(NDASDevicePtr == NULL) {
		DbgIOLog(DEBUG_MASK_USER_ERROR, ("Enable: Can't Find Registed Slot Number %d.\n", enableData->SlotNo));
		
		return kIOReturnError;
	}

	// Check NDAS status.
	switch(NDASDevicePtr->configuration()) {
		case kNDASConfigurationDisable:
		{
			// Change configuration to kNDASConfigurationEnable
			NDASDevicePtr->setConfiguration(kNDASConfigurationEnable);
			DbgIOLog(DEBUG_MASK_USER_INFO, ("Enable: Enabled.\n"));
		}
			break;
		case kNDASConfigurationEnable:
			DbgIOLog(DEBUG_MASK_USER_INFO, ("Enable: Already Enabled configuration.\n"));
			break;
		default:
			return kIOReturnBadArgument;
	}
	
	*result = enableData->SlotNo;
	*outStructSize = (IOByteCount)(sizeof(UInt32));
	
    return kIOReturnSuccess;	
}

// kNDASEnumDisable
IOReturn com_ximeta_driver_NDASBusEnumerator::Disable(PNDAS_ENUM_DISABLE	disableData,
													  UInt32				*result,
													  IOByteCount			inStructSize,
													  IOByteCount			*outStructSize)
{
	DbgIOLog(DEBUG_MASK_USER_TRACE, ("--------------------Disable---------------\n"));
	
	// If the user client's 'open' method was never called, return kIOReturnNotOpen.
    if (!isOpen()) {
        DbgIOLog(DEBUG_MASK_USER_ERROR, ("Disable: Not Opened\n"));
        return kIOReturnNotOpen;
    }    
	
	com_ximeta_driver_NDASDevice		*NDASDevicePtr = NULL;
	
	NDASDevicePtr = FindDeviceBySlotNumber(disableData->SlotNo);
		
	if(NDASDevicePtr == NULL) {
		DbgIOLog(DEBUG_MASK_USER_ERROR, ("Disable: Can't Find Registed Slot Number %d.\n", disableData->SlotNo));
		
		return kIOReturnError;
	}

	// Check NDAS status.
	switch(NDASDevicePtr->configuration()) {
		case kNDASConfigurationDisable:
			DbgIOLog(DEBUG_MASK_USER_INFO, ("Disable: Already disabled configuration.\n"));
			break;
		case kNDASConfigurationEnable:
		{
			int mountedUnits = 0;
			
			// Check Units.
			// BUG BUG BUG
			/*
			for(int count = 0; count < MAX_NR_OF_TARGETS_PER_DEVICE; count++) {
				if(kNDASUnitStatusMountRW == NDASDevicePtr->targetStatus(count) ||
				   kNDASUnitStatusMountRO == NDASDevicePtr->targetStatus(count) ) {
					mountedUnits++;
				}
			}
			*/
			
			if(mountedUnits == 0) {
				// Change configuration to kNDASConfigurationDisable
				NDASDevicePtr->setConfiguration(kNDASConfigurationDisable);
				DbgIOLog(DEBUG_MASK_USER_INFO, ("Disable: Disabled.\n"));
			} else {
				return kIOReturnError;
			}
		}
			break;
		default:
			return kIOReturnBadArgument;
	}
	
	*result = disableData->SlotNo;
	*outStructSize = (IOByteCount)(sizeof(UInt32));

    return kIOReturnSuccess;	
}

// kNDASEnumMount
IOReturn com_ximeta_driver_NDASBusEnumerator::Mount(PNDAS_ENUM_MOUNT	mountData,
													UInt32				*result,
													IOByteCount			inStructSize,
													IOByteCount			*outStructSize)
{
	DbgIOLog(DEBUG_MASK_USER_TRACE, ("--------------------Mount---------------\n"));
	
	// If the user client's 'open' method was never called, return kIOReturnNotOpen.
    if (!isOpen()) {
        DbgIOLog(DEBUG_MASK_USER_ERROR, ("Mount: Not Opened\n"));
        return kIOReturnNotOpen;
    }    
	
	com_ximeta_driver_NDASLogicalDevice		*NDASLogicalDevicePtr = NULL;
		
	NDASLogicalDevicePtr = FindLogicalDeviceByIndex(mountData->LogicalDeviceIndex);
	
	if(NDASLogicalDevicePtr == NULL) {
		DbgIOLog(DEBUG_MASK_USER_ERROR, ("Mount: Can't Find Logical Device of Index %d.\n", mountData->LogicalDeviceIndex));
		
		return kIOReturnError;
	}
	
	// Check request.
	if(mountData->Writeable == true && NDASLogicalDevicePtr->IsWritable() == false) {
		DbgIOLog(DEBUG_MASK_USER_ERROR, ("Mount: No write access right. Index %d.\n", mountData->LogicalDeviceIndex));
		
		return kIOReturnNotPermitted;		
	}

	// Check Status.
	if (kNDASUnitStatusNotPresent == NDASLogicalDevicePtr->Status()) {
		return kIOReturnSuccess;		
	}
	
	// Change configuration to kNDASConfigurationDisable
	if(mountData->Writeable == true) {
		NDASLogicalDevicePtr->setConfiguration(kNDASUnitConfigurationMountRW);
		DbgIOLog(DEBUG_MASK_USER_INFO, ("Mount: Ready to mount with write access.\n"));
	} else {
		NDASLogicalDevicePtr->setConfiguration(kNDASUnitConfigurationMountRO);
		DbgIOLog(DEBUG_MASK_USER_INFO, ("Mount: Ready to mount without write access.\n"));
	}
	
	*result = mountData->LogicalDeviceIndex;
	*outStructSize = (IOByteCount)(sizeof(UInt32));
	
    return kIOReturnSuccess;	
}

// kNDASEnumUnmount
IOReturn com_ximeta_driver_NDASBusEnumerator::Unmount(PNDAS_ENUM_UNMOUNT	unmountData,
													  UInt32				*result,
													  IOByteCount		inStructSize,
													  IOByteCount		*outStructSize)
{
	DbgIOLog(DEBUG_MASK_USER_TRACE, ("--------------------Unmount---------------\n"));
	
	// If the user client's 'open' method was never called, return kIOReturnNotOpen.
    if (!isOpen()) {
        DbgIOLog(DEBUG_MASK_USER_ERROR, ("Unmount: Not Opened\n"));
        return kIOReturnNotOpen;
    }    
	
	com_ximeta_driver_NDASLogicalDevice	*NDASLogicalDevicePtr = NULL;
	
	char buffer[256];
	
	sprintf(buffer, "%d", unmountData->LogicalDeviceIndex);
	
	// Check Index.
	NDASLogicalDevicePtr = FindLogicalDeviceByIndex(unmountData->LogicalDeviceIndex);

	if(NDASLogicalDevicePtr == NULL) {
		DbgIOLog(DEBUG_MASK_USER_ERROR, ("Unmount: Can't Find Logical Device of Index %d.\n", unmountData->LogicalDeviceIndex));
		
		return kIOReturnBadArgument;
	}
	
	// Check NDAS status.
	NDASLogicalDevicePtr->setConfiguration(kNDASUnitConfigurationUnmount);
	DbgIOLog(DEBUG_MASK_USER_INFO, ("Unmount: set to Unmount.\n"));
	
	*result = unmountData->LogicalDeviceIndex;
	*outStructSize = (IOByteCount)(sizeof(UInt32));
	
    return kIOReturnSuccess;	
}

// kNDASEnumEdit
IOReturn com_ximeta_driver_NDASBusEnumerator::Edit(PNDAS_ENUM_EDIT	editData,
												   UInt32			*result,
												   IOByteCount		inStructSize,
												   IOByteCount		*outStructSize)
{
	DbgIOLog(DEBUG_MASK_USER_TRACE, ("--------------------Edit---------------\n"));
	
	// If the user client's 'open' method was never called, return kIOReturnNotOpen.
    if (!isOpen()) {
        DbgIOLog(DEBUG_MASK_USER_ERROR, ("Edit: Not Opened\n"));
        return kIOReturnNotOpen;
    }    
	
	com_ximeta_driver_NDASDevice		*NDASDevicePtr = NULL;
	
	NDASDevicePtr = FindDeviceBySlotNumber(editData->SlotNo);
	
	if(NDASDevicePtr == NULL) {
		DbgIOLog(DEBUG_MASK_USER_ERROR, ("Edit: Can't Find Registed Slot Number %d.\n", editData->SlotNo));
		
		return kIOReturnError;
	}
	
	// Check NDAS status.
	switch(NDASDevicePtr->configuration()) {
		case kNDASConfigurationDisable:
		case kNDASConfigurationEnable:
		{
			switch(editData->EditType) {
				case kNDASEditTypeWriteAccessRight:
				{
					NDASDevicePtr->setWriteRight(editData->Writable);
					DbgIOLog(DEBUG_MASK_USER_INFO, ("Edit: Edit Write access right. %d\n", editData->Writable));
				}
					break;
				case kNDASEditTypeName:
				{
					NDASDevicePtr->setName(editData->Name);
					DbgIOLog(DEBUG_MASK_USER_INFO, ("Edit: Edit name.\n"));
				}
					break;						
				default:
					return kIOReturnBadArgument;
			}	
		}
			break;
		default:
			return kIOReturnBadArgument;
	}
	
	*result = editData->SlotNo;
	*outStructSize = (IOByteCount)(sizeof(UInt32));
	
    return kIOReturnSuccess;	
}

// kNDASEnumRefresh
IOReturn com_ximeta_driver_NDASBusEnumerator::Refresh(PNDAS_ENUM_REFRESH	refreshData,
													  UInt32				*result,
													  IOByteCount			inStructSize,
													  IOByteCount			*outStructSize)
{
	DbgIOLog(DEBUG_MASK_USER_TRACE, ("--------------------Refresh---------------\n"));
	
	// If the user client's 'open' method was never called, return kIOReturnNotOpen.
    if (!isOpen()) {
        DbgIOLog(DEBUG_MASK_USER_ERROR, ("Refresh: Not Opened\n"));
        return kIOReturnNotOpen;
    }    
	
	com_ximeta_driver_NDASDevice		*NDASDevicePtr = NULL;
	
	// Check Slot Number.
	NDASDevicePtr = FindDeviceBySlotNumber(refreshData->SlotNo);
	if(NDASDevicePtr == NULL) {
		DbgIOLog(DEBUG_MASK_USER_ERROR, ("Refresh: Can't Find Registed Slot Number %d.\n", refreshData->SlotNo));
		
		return kIOReturnBadArgument;
	}
	
	// Check Unit Number.
	if(refreshData->UnitNo >= MAX_NR_OF_TARGETS_PER_DEVICE) {
		DbgIOLog(DEBUG_MASK_USER_ERROR, ("Refresh: Bad Unit Number. %d.\n", refreshData->UnitNo));
		
		return kIOReturnBadArgument;				
	}
	
	// Check NDAS status.
	NDASDevicePtr->setTargetNeedRefresh(refreshData->UnitNo);
	
	*result = refreshData->SlotNo;
	*outStructSize = (IOByteCount)(sizeof(UInt32));
	
    return kIOReturnSuccess;	
}

// kNDASEnumBind
IOReturn com_ximeta_driver_NDASBusEnumerator::Bind(PNDAS_ENUM_BIND	bindData,
												   PGUID			result,
												   IOByteCount		inStructSize,
												   IOByteCount		*outStructSize)
{
	DbgIOLog(DEBUG_MASK_USER_TRACE, ("--------------------Bind---------------\n"));
	
	// If the user client's 'open' method was never called, return kIOReturnNotOpen.
    if (!isOpen()) {
        DbgIOLog(DEBUG_MASK_USER_ERROR, ("Bind: Not Opened\n"));
        return kIOReturnNotOpen;
    }    
	
	// Check Parameter.
	switch (bindData->RAIDType) {
		case NMT_INVALID:
		case NMT_SINGLE:
		{
			// Invalid Type.
			DbgIOLog(DEBUG_MASK_USER_ERROR, ("Bind: Invalid RAID Type %d\n", bindData->RAIDType));			
			return kIOReturnBadArgument;
		}
			break;
		case NMT_AGGREGATE:
		{
			if (2 > bindData->NumberofSubUnitDevices &&
				bindData->NumberofSubUnitDevices > NDAS_MAX_UNITS_IN_V2 ) {
				// Invalid Number Of Sub Unit Devices.
				DbgIOLog(DEBUG_MASK_USER_ERROR, ("Bind: Invalid Number of Sub Unit Devices. %d\n", bindData->NumberofSubUnitDevices));			
				return kIOReturnBadArgument;				
			}
		}
			break;
		case NMT_RAID0R2:
		{
			if (2 > bindData->NumberofSubUnitDevices &&
				bindData->NumberofSubUnitDevices > NDAS_MAX_UNITS_IN_V2 ) {
				// Invalid Number Of Sub Unit Devices.
				DbgIOLog(DEBUG_MASK_USER_ERROR, ("Bind: Invalid Number of Sub Unit Devices. %d\n", bindData->NumberofSubUnitDevices));			
				return kIOReturnBadArgument;				
			}			
		}
			break;
		case NMT_RAID1R3:
		{
			if (2 > bindData->NumberofSubUnitDevices
				|| 3 < bindData->NumberofSubUnitDevices) {
				// Invalid Number Of Sub Unit Devices.
				DbgIOLog(DEBUG_MASK_USER_ERROR, ("Bind: Invalid Number of Sub Unit Devices. %d\n", bindData->NumberofSubUnitDevices));			
				return kIOReturnBadArgument;				
			}			
		}
			break;
		default:
		{
			// Unsupported Type.
			DbgIOLog(DEBUG_MASK_USER_ERROR, ("Bind: Unsupported RAID Type %d\n", bindData->RAIDType));			
			return kIOReturnUnsupported;			
		}
			break;
	}

	com_ximeta_driver_NDASLogicalDevice	*logicalDeviceArray[NDAS_MAX_UNITS_IN_V2] = { NULL };
	int count;
	
	// Check Logical Device Index.
	for (count = 0; count < bindData->NumberofSubUnitDevices - 1; count++) {
		int innerCount;
		
		for (innerCount = count + 1; innerCount < bindData->NumberofSubUnitDevices; innerCount++) {
			if (bindData->LogicalDeviceIndexOfSubUnitDevices[count] == bindData->LogicalDeviceIndexOfSubUnitDevices[innerCount]) {
				DbgIOLog(DEBUG_MASK_USER_ERROR, ("Bind: Duplicated Logical Devices. at Index %d, %d\n", count, innerCount));			
				return kIOReturnBadArgument;
			}
		}
	}
	
	// Check Logical Devices.
	for (count = 0; count < bindData->NumberofSubUnitDevices; count++) {
		
		// Search 
		logicalDeviceArray[count] = FindLogicalDeviceByIndex(bindData->LogicalDeviceIndexOfSubUnitDevices[count]);
		
		if(logicalDeviceArray[count] == NULL) {
			DbgIOLog(DEBUG_MASK_USER_ERROR, ("Bind: Can't Find Logical Device of Index %d.\n", count));
			
			return kIOReturnBadArgument;
		}
		
		// Check Duplicate Logical Device.
		for (int innerCount = 0; innerCount < count; innerCount++) {
			if ( logicalDeviceArray[count] == logicalDeviceArray[innerCount]) {
				DbgIOLog(DEBUG_MASK_USER_ERROR, ("Bind: Duplicated Logical Device. %d %d.\n", count, innerCount));
				
				return kIOReturnBadArgument;
			}
		}
		
		if(logicalDeviceArray[count]->UnitDevice() == NULL) {
			DbgIOLog(DEBUG_MASK_USER_ERROR, ("Bind: Can't Unit Device at Logical Device of Index %d.\n", count));
			
			return kIOReturnBadArgument;
		}
		
		if (logicalDeviceArray[count]->IsInRunningStatus()) {
			DbgIOLog(DEBUG_MASK_USER_ERROR, ("Bind: Logical Device of Index %d is in running status. Conf %d, Status %d\n",
											 logicalDeviceArray[count]->Configuration(),
											 logicalDeviceArray[count]->Status()));
			
			return kIOReturnNotReady;
		}
		
		if (0 < logicalDeviceArray[count]->NRRWHosts() || 0 < logicalDeviceArray[count]->NRROHosts()) {
			DbgIOLog(DEBUG_MASK_USER_ERROR, ("Bind: Another Host mount Logical Device of Index %d.\n", count));

			return kIOReturnNotReady;
		}
	}
	
	// Create GUID.
	NDAS_uuid_t raidID;
	NDAS_uuid_t	configID;
	
	NDAS_uuid_generate(raidID);
	NDAS_uuid_generate(configID);

	// Bind.
	for (count = 0; count < bindData->NumberofSubUnitDevices; count++) {
		if (!logicalDeviceArray[count]->bind(bindData->RAIDType, 
											 count, 
											 bindData->NumberofSubUnitDevices, 
											 logicalDeviceArray,
											 raidID,
											 configID,
											 bindData->Sync))
		{
			DbgIOLog(DEBUG_MASK_USER_ERROR, ("Bind: bind Failed. Logical Device of Index %d.\n", count));

			return kIOReturnDeviceError;
		}
		
	}
	
	// Send Check Bind Message.	
	for (count = 0; count < bindData->NumberofSubUnitDevices; count++) {
		
		com_ximeta_driver_NDASUnitDevice	*UnitDevice = NULL;
		
		if (UnitDevice = logicalDeviceArray[count]->UnitDevice()) {
			
			com_ximeta_driver_NDASCommand *command = NULL;
			
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
			
			command->setCommand(kNDASDeviceNotificationCheckBind, UnitDevice);
			
			enqueueMessage(command);
			
			command->release();						
		}
	}			
	
	DbgIOLog(DEBUG_MASK_USER_INFO, ("Bind: set to Bind.\n"));
	
	memcpy(result, &raidID, sizeof(GUID));
	*outStructSize = (IOByteCount)(sizeof(GUID));
	
    return kIOReturnSuccess;	
}

// kNDASEnumUnbind
IOReturn com_ximeta_driver_NDASBusEnumerator::Unbind(PNDAS_ENUM_UNBIND	unbindData,
													  UInt32			*result,
													  IOByteCount		inStructSize,
													  IOByteCount		*outStructSize)
{
	DbgIOLog(DEBUG_MASK_USER_TRACE, ("--------------------Unbind---------------\n"));
	
	// If the user client's 'open' method was never called, return kIOReturnNotOpen.
    if (!isOpen()) {
        DbgIOLog(DEBUG_MASK_USER_ERROR, ("Unbind: Not Opened\n"));
        return kIOReturnNotOpen;
    }    
	
	com_ximeta_driver_NDASRAIDUnitDevice	*RAIDDevicePtr = NULL;
	
	// Check Index.
	RAIDDevicePtr = FindRAIDDeviceByIndex(unbindData->RAIDDeviceIndex);
	
	if(RAIDDevicePtr == NULL) {
		DbgIOLog(DEBUG_MASK_USER_ERROR, ("Unbind: Can't Find RAID Device of Index %d.\n", unbindData->RAIDDeviceIndex));
		
		return kIOReturnBadArgument;
	}
			
	if (RAIDDevicePtr->IsInRunningStatus()) {
		DbgIOLog(DEBUG_MASK_USER_ERROR, ("Bind: Logical Device of Index %d is in running status. Conf %d, Status %d\n",
										 RAIDDevicePtr->Configuration(),
										 RAIDDevicePtr->Status()));
		
		return kIOReturnNotReady;
	}
	
	
	// Check NDAS status.
	RAIDDevicePtr->unbind();
	
	// Send Check Bind Message.	
	for (int count = RAIDDevicePtr->CountOfUnitDevices() + RAIDDevicePtr->CountOfSpareDevices() - 1; count >= 0; count--) {
		
		com_ximeta_driver_NDASUnitDevice	*UnitDevice = NULL;
		
		if (UnitDevice = RAIDDevicePtr->UnitDeviceAtIndex(count)) {
			
			com_ximeta_driver_NDASCommand *command = NULL;
			
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
			
			command->setCommand(kNDASDeviceNotificationCheckBind, UnitDevice);
			
			enqueueMessage(command);
			
			command->release();						
		}
	}		
	
	DbgIOLog(DEBUG_MASK_USER_INFO, ("Unbind: set to Unmount.\n"));
	
	*result = unbindData->RAIDDeviceIndex;
	*outStructSize = (IOByteCount)(sizeof(UInt32));
	
    return kIOReturnSuccess;	
}

// kNDASEnumBindEdit
IOReturn com_ximeta_driver_NDASBusEnumerator::BindEdit(PNDAS_ENUM_BIND_EDIT	editData,
													   UInt32				*result,
													   IOByteCount			inStructSize,
													   IOByteCount			*outStructSize)
{
	DbgIOLog(DEBUG_MASK_USER_TRACE, ("--------------------BIND Edit---------------\n"));
	
	// If the user client's 'open' method was never called, return kIOReturnNotOpen.
    if (!isOpen()) {
        DbgIOLog(DEBUG_MASK_USER_ERROR, ("BindEdit: Not Opened\n"));
        return kIOReturnNotOpen;
    }    
	
	com_ximeta_driver_NDASRAIDUnitDevice	*RAIDDevicePtr = NULL;

	// Check Index.
	RAIDDevicePtr = FindRAIDDeviceByIndex(editData->RAIDDeviceIndex);
	
	if(RAIDDevicePtr == NULL) {
		DbgIOLog(DEBUG_MASK_USER_ERROR, ("BindEdit: Can't Find RAID Device of Index %d.\n", editData->RAIDDeviceIndex));
		
		return kIOReturnBadArgument;
	}
	
	switch(editData->EditType) {
		case kNDASEditTypeName:
		{
			RAIDDevicePtr->setName(editData->Name);
			DbgIOLog(DEBUG_MASK_USER_INFO, ("BindEdit: Edit name.\n"));
		}
			break;
		case kNDASEditTypeRAIDType:
		{
			RAIDDevicePtr->convertBindType(editData->RAIDType);
			DbgIOLog(DEBUG_MASK_USER_INFO, ("BindEdit: Edit RAID Type.\n"));			
			
			// Send Check Bind Message.	
			for (int count = 0; count < RAIDDevicePtr->CountOfUnitDevices(); count++) {
				
				com_ximeta_driver_NDASUnitDevice	*UnitDevice = NULL;
				
				if (UnitDevice = RAIDDevicePtr->UnitDeviceAtIndex(count)) {
					
					com_ximeta_driver_NDASCommand *command = NULL;
					
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
					
					command->setCommand(kNDASDeviceNotificationCheckBind, UnitDevice);
					
					enqueueMessage(command);
					
					command->release();						
				}
			}		
			
		}
			break;
		default:
			return kIOReturnBadArgument;
	}	

	
	*result = editData->RAIDDeviceIndex;
	*outStructSize = (IOByteCount)(sizeof(UInt32));
	
    return kIOReturnSuccess;	
}

// kNDASEnumUnbind
IOReturn com_ximeta_driver_NDASBusEnumerator::Rebind(PNDAS_ENUM_REBIND	rebindData,
													 UInt32				*result,
													 IOByteCount		inStructSize,
													 IOByteCount		*outStructSize)
{
	DbgIOLog(DEBUG_MASK_USER_TRACE, ("--------------------Rebind---------------\n"));
	
	// If the user client's 'open' method was never called, return kIOReturnNotOpen.
    if (!isOpen()) {
        DbgIOLog(DEBUG_MASK_USER_ERROR, ("Rebind: Not Opened\n"));
        return kIOReturnNotOpen;
    }    
	
	com_ximeta_driver_NDASRAIDUnitDevice	*RAIDDevicePtr = NULL;
	
	// Check Index.
	RAIDDevicePtr = FindRAIDDeviceByIndex(rebindData->RAIDDeviceIndex);
	
	if(RAIDDevicePtr == NULL) {
		DbgIOLog(DEBUG_MASK_USER_ERROR, ("Rebind: Can't Find RAID Device of Index %d.\n", rebindData->RAIDDeviceIndex));
		
		return kIOReturnBadArgument;
	}
	
	if (RAIDDevicePtr->IsInRunningStatus()) {
		DbgIOLog(DEBUG_MASK_USER_ERROR, ("Rebind: Logical Device of Index %d is in running status. Conf %d, Status %d\n",
										 RAIDDevicePtr->Configuration(),
										 RAIDDevicePtr->Status()));
		
		return kIOReturnNotReady;
	}

	if (kNDASRAIDStatusBadRAIDInformation != RAIDDevicePtr->RAIDStatus()
		&& kNDASRAIDStatusGoodWithReplacedUnit != RAIDDevicePtr->RAIDStatus()
		&& kNDASRAIDStatusReadyToRecoveryWithReplacedUnit != RAIDDevicePtr->RAIDStatus()) {
		DbgIOLog(DEBUG_MASK_USER_ERROR, ("Rebind: RAID Status is not BadRAIDInformation. RAID Status %d\n",
										 RAIDDevicePtr->RAIDStatus()));
		
		return kIOReturnNotReady;
	}
	
	// Check NDAS status.
	RAIDDevicePtr->rebind();
	
	// Send Check Bind Message.	
	for (int count = RAIDDevicePtr->CountOfUnitDevices() + RAIDDevicePtr->CountOfSpareDevices() - 1; count >= 0; count--) {
		
		com_ximeta_driver_NDASUnitDevice	*UnitDevice = NULL;
		
		if (UnitDevice = RAIDDevicePtr->UnitDeviceAtIndex(count)) {
			
			com_ximeta_driver_NDASCommand *command = NULL;
			
			// Create NDAS Command.
			command = OSDynamicCast(com_ximeta_driver_NDASCommand, 
									OSMetaClass::allocClassWithName(NDAS_COMMAND_CLASS));
			
			if(command == NULL || !command->init()) {
				DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("Rebind: failed to alloc command class\n"));
				if (command) {
					command->release();
				}
				
				return kIOReturnError;
			}
			
			command->setCommand(kNDASDeviceNotificationCheckBind, UnitDevice);
			
			enqueueMessage(command);
			
			command->release();						
		}
	}		
	
	DbgIOLog(DEBUG_MASK_USER_INFO, ("Rebind: set to Unmount.\n"));
	
	*result = rebindData->RAIDDeviceIndex;
	*outStructSize = (IOByteCount)(sizeof(UInt32));
	
    return kIOReturnSuccess;	
}

#pragma mark -
#pragma mark == Interface checker and Braodcast Reciever ==
#pragma mark -

#define BROADCAST_RECEIVER_ARG_BUS_POINTER				"BroadcastReceiverArg_BusPointer"
#define BROADCAST_RECEIVER_ARG_INDEX_OF_INTERFACE_ARRAY	"BroadcastRecieverArg_IndexOfInterfaceArray"

void com_ximeta_driver_NDASBusEnumerator::InterfaceCheckTimeOutOccurred(
															 OSObject *owner,
															 IOTimerEventSource *sender)
{
	//DbgIOLog(DEBUG_MASK_NDAS_INFO, ("InterfaceCheckTimeOutOccurred!\n"));
	
	int interfaceArrayIndex;
	
	//
	// Check Interfaces.
	//
	
	// Get Ethernet Interface class.
	OSDictionary* services = IOService::serviceMatching(kIOEthernetInterfaceClass);
	if (!services) {
		return;
	}
	
	OSIterator* iterator = IOService::getMatchingServices(services);
	if (!iterator) {
		services->release();
		return;
	}
	
	// Manage Interface Array.
	for(interfaceArrayIndex = 0; interfaceArrayIndex < NUMBER_OF_INTERFACES; interfaceArrayIndex++) {
		fInterfaces[interfaceArrayIndex].fRemoved = true;
	}
	
	int numberOfInterfaces = 0;
	
	while (numberOfInterfaces < NUMBER_OF_INTERFACES)
	{
		bool	found = false;
		char	tempAddress[6];
		
nextService:
			
		IOService* service = OSDynamicCast(IOService, iterator->getNextObject());
		if (!service) {			
			break;
		} else {
			// Check Address.
			OSData	*Address = (OSData *)service->getProperty(kIOMACAddress, gIOServicePlane);
			if(!Address || Address->getCapacity() != 6) { 
				goto nextService;
			}
			
			memcpy(tempAddress, Address->getBytesNoCopy(), 6);
			
			int addressIndex;
			int sum = 0;
			
			for ( addressIndex = 0; addressIndex < 6; addressIndex++ ) {
				sum += tempAddress[addressIndex];
			}
			
			if(sum == 0) {
				goto nextService;
			}
		}
		
		// Search Interface Array.
		found = false;
		for(interfaceArrayIndex = 0; interfaceArrayIndex < NUMBER_OF_INTERFACES; interfaceArrayIndex++) {
			
			if(fInterfaces[interfaceArrayIndex].fService == service) {
				found = true;
				fInterfaces[interfaceArrayIndex].fRemoved = false;

				break;
			}
		}
		
		// New Interface?
		if(!found) {
			
			for(interfaceArrayIndex = 0; interfaceArrayIndex < NUMBER_OF_INTERFACES; interfaceArrayIndex++) {
				
				if(fInterfaces[interfaceArrayIndex].fService == NULL) {
					
					memcpy(fInterfaces[interfaceArrayIndex].fAddress, tempAddress, 6);
					
					fInterfaces[interfaceArrayIndex].fService = service;

					fInterfaces[interfaceArrayIndex].fRemoved = false;
					
					found = true;
					
					break;
				}
			}
		}
		
		if(!found) {
			break;
		}
			
		// Get Status.
		OSNumber	*Status = (OSNumber *)service->getProperty(kIOLinkStatus, gIOServicePlane);
		fInterfaces[interfaceArrayIndex].fStatus = Status->unsigned64BitValue();
		
		// Get Link Speed.
		OSNumber	*Speed = (OSNumber *)service->getProperty(kIOLinkSpeed, gIOServicePlane);
		fInterfaces[interfaceArrayIndex].fSpeed = Speed->unsigned32BitValue();
		
		numberOfInterfaces++;
	}

	// Remove Interfaces.
	for(interfaceArrayIndex = 0; interfaceArrayIndex < NUMBER_OF_INTERFACES; interfaceArrayIndex++) {
		if(fInterfaces[interfaceArrayIndex].fRemoved == true
		   && (fInterfaces[interfaceArrayIndex].fBroadcastListenSocket || fInterfaces[interfaceArrayIndex].fService)) {

			releaseBroadcastReceiver(interfaceArrayIndex);
		}
	}	
	
	iterator->release();
	services->release();
	
	// Every NIC are bad? try again.
	bool bEveryNICisBad = true;
	
	for(interfaceArrayIndex = 0; interfaceArrayIndex < NUMBER_OF_INTERFACES; interfaceArrayIndex++) {
		if( (fInterfaces[interfaceArrayIndex].fService != NULL) ) {
	
			if (false == fInterfaces[interfaceArrayIndex].fBadNIC) {
				bEveryNICisBad = false;
				break;
			}
			
			if (fInterfaces[interfaceArrayIndex].fBroadcastListenSocket != NULL) {
				bEveryNICisBad = false;
				break;
			}
		}
	}
	if (bEveryNICisBad) {
		for(interfaceArrayIndex = 0; interfaceArrayIndex < NUMBER_OF_INTERFACES; interfaceArrayIndex++) {
			fInterfaces[interfaceArrayIndex].fBadNIC = false;
		}		
	}
	
	// Open Socket.
	for(interfaceArrayIndex = 0; interfaceArrayIndex < NUMBER_OF_INTERFACES; interfaceArrayIndex++) {
		if((fInterfaces[interfaceArrayIndex].fService != NULL) &&
		   (fInterfaces[interfaceArrayIndex].fBroadcastListenSocket == NULL) &&
		   (false == fInterfaces[interfaceArrayIndex].fBadNIC)) {
			
			initBroadcastReceiver(interfaceArrayIndex);
		}
	}

Out:
		
	fInterfaceCheckTimerSourcePtr->setTimeoutMS(10000);	// 10 sec.
}

void InterfaceCheckTimerWrapper(OSObject *owner, IOTimerEventSource *sender)
{
	com_ximeta_driver_NDASBusEnumerator *bus = (com_ximeta_driver_NDASBusEnumerator*)owner;
	
	bus->InterfaceCheckTimeOutOccurred(owner, sender);
	
	return;
}

void com_ximeta_driver_NDASBusEnumerator::broadcastReceiverThread(UInt32 interfaceArrayIndex)
{		
	if(fInterfaces[interfaceArrayIndex].fService == NULL ||
	   fInterfaces[interfaceArrayIndex].fBroadcastListenSocket == NULL) {
		
		goto Out;
	}
	
	this->retain();
	
	while (true) {
		
		size_t messageLength = sizeof(PNP_MESSAGE);
		int fromLen = sizeof(sockaddr_lpx);
		struct sockaddr_lpx		fromlpx;
		int						flags = 0; //MSG_DONTWAIT;
		PNP_MESSAGE				message;
		
		errno_t error = xi_sock_receivefrom(fInterfaces[interfaceArrayIndex].fBroadcastListenSocket, &message, &messageLength, flags,   
											(struct sockaddr *)&fromlpx, &fromLen);
		
		if(!error 
		   && fInterfaces[interfaceArrayIndex].fBroadcastListenSocket
		   && fInterfaces[interfaceArrayIndex].fService
		   && messageLength == sizeof(PNP_MESSAGE)) {
			DbgIOLog(DEBUG_MASK_NDAS_INFO, ("broadcastReceiverThread:[%d] Received PnP Packet Size %d Dest 0x%x:0x%x:0x%x:0x%x:0x%x:0x%x \n", interfaceArrayIndex, messageLength,
											 fromlpx.slpx_node[0], fromlpx.slpx_node[1], fromlpx.slpx_node[2],
											 fromlpx.slpx_node[3], fromlpx.slpx_node[4], fromlpx.slpx_node[5])
					 );
			
			UInt32	receivedTime;
			
			NDAS_clock_get_system_value(&receivedTime);

			// Create NDAS Command.
			com_ximeta_driver_NDASCommand *command = OSDynamicCast(com_ximeta_driver_NDASCommand, 
									OSMetaClass::allocClassWithName(NDAS_COMMAND_CLASS));
			
			if(command == NULL || !command->init()) {
				DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("broadcastReceiverThread: failed to alloc command class\n"));
				if (command) {
					command->release();
				}
				
				break;
			}
			
			command->setCommand(interfaceArrayIndex, (char *)fromlpx.slpx_node, receivedTime, &message);
			
			enqueueMessage(command);
			
			command->release();

		} else {
			// No Packet.
			goto Out;
		}
	}		
	
Out:
	fInterfaces[interfaceArrayIndex].fThread = NULL;
	
//	IOLog("broadcastReciever %d Terminated\n", interfaceArrayIndex);
	
	this->release();
	
	IOExitThread();
}

void broadcastReceiverThreadWrapper(void* argument)
{		
	BroadcastRecieverArgs	*args = (BroadcastRecieverArgs *)argument;
	UInt32			index;

	com_ximeta_driver_NDASBusEnumerator *bus = args->bus;
	index = args->interfaceArrayIndex;
	
	IOFree(args, sizeof(BroadcastRecieverArgs));

	bus->broadcastReceiverThread(index);
		
	IOExitThread();
}

int	com_ximeta_driver_NDASBusEnumerator::initBroadcastReceiver(int interfaceArrayIndex)
{
	uint					uiPort;
	struct sockaddr_lpx 	sLpx;
	errno_t					error;
		
	error = xi_sock_socket(AF_LPX, SOCK_DGRAM, 0, NULL, NULL, &fInterfaces[interfaceArrayIndex].fBroadcastListenSocket);
	if(error) {
		DbgIOLog(DEBUG_MASK_NDAS_INFO, ("initBroadcastReceiver: socreate error\n"));
								
		return error;
	}
	
	/* Bind */
	uiPort = LPX_PORT_PNP_LISTEN;
	
	bzero(&sLpx, sizeof(struct sockaddr_lpx));
	sLpx.slpx_family = AF_LPX;
	sLpx.slpx_len = sizeof(struct sockaddr_lpx); 
	memcpy(sLpx.slpx_node, fInterfaces[interfaceArrayIndex].fAddress, 6);
	sLpx.slpx_port = htons(uiPort);
	
	DbgIOLog(DEBUG_MASK_NDAS_INFO, ("initBroadcastReceiver: Interface:[%d] Interface 0x%x:0x%x:0x%x:0x%x:0x%x:0x%x \n", interfaceArrayIndex,
									sLpx.slpx_node[0], sLpx.slpx_node[1], sLpx.slpx_node[2],
									sLpx.slpx_node[3], sLpx.slpx_node[4], sLpx.slpx_node[5])
			 );
	
	
	error = xi_sock_bind(fInterfaces[interfaceArrayIndex].fBroadcastListenSocket, (struct sockaddr *)&sLpx);			
	if (error) {
		DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("initBroadcastReceiver: sobind error\n"));
		
		DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("Interface:[%d] Interface 0x%x:0x%x:0x%x:0x%x:0x%x:0x%x \n", interfaceArrayIndex,
										 sLpx.slpx_node[0], sLpx.slpx_node[1], sLpx.slpx_node[2],
										 sLpx.slpx_node[3], sLpx.slpx_node[4], sLpx.slpx_node[5])
				 );
		
		xi_sock_close(fInterfaces[interfaceArrayIndex].fBroadcastListenSocket);
		fInterfaces[interfaceArrayIndex].fBroadcastListenSocket = NULL;
		
		fInterfaces[interfaceArrayIndex].fBadNIC = true;
		
		return error;
	}
	
	// Create Thread.
	PBroadcastRecieverArgs	broadcastReceiverArgs;
	
	broadcastReceiverArgs = (PBroadcastRecieverArgs)IOMalloc(sizeof(BroadcastRecieverArgs));
	if (NULL == broadcastReceiverArgs) {
		DbgIOLog(DEBUG_MASK_PNP_ERROR, ("initBroadcastReceiver:: Can't Create args for broadcastReceiver.\n"));		
		return -1;
	}
	
	broadcastReceiverArgs->bus = this;
	broadcastReceiverArgs->interfaceArrayIndex = interfaceArrayIndex;

	fInterfaces[interfaceArrayIndex].fThread = IOCreateThread(broadcastReceiverThreadWrapper, broadcastReceiverArgs);
	if(!fInterfaces[interfaceArrayIndex].fThread) {
		DbgIOLog(DEBUG_MASK_PNP_ERROR, ("initBroadcastReceiver::IOCreateThread(), failed to Create Thread\n"));
				
		return -1;
	}
		
	return error;
}

int com_ximeta_driver_NDASBusEnumerator::releaseBroadcastReceiver(int interfaceArrayIndex)
{
	fInterfaces[interfaceArrayIndex].fService = NULL;
	
	// close socket.
	if (fInterfaces[interfaceArrayIndex].fBroadcastListenSocket != NULL) {
		xi_sock_close(fInterfaces[interfaceArrayIndex].fBroadcastListenSocket);
		fInterfaces[interfaceArrayIndex].fBroadcastListenSocket = NULL;
	}	
}

#pragma mark -
#pragma mark == Power Management ==
#pragma mark -

void com_ximeta_driver_NDASBusEnumerator::PowerDownOccurred(
															 OSObject *owner,
															 IOTimerEventSource *sender)
{
	DbgIOLog(DEBUG_MASK_POWER_TRACE, ("PowerDownOccurred: Entered\n"));
	
	acknowledgePowerChange(this);
}

void PowerTimerWrapper(OSObject *owner, IOTimerEventSource *sender)
{
	com_ximeta_driver_NDASBusEnumerator *bus = (com_ximeta_driver_NDASBusEnumerator*)owner;
	
	bus->PowerDownOccurred(owner, sender);
	
	return;
}

IOReturn 
com_ximeta_driver_NDASBusEnumerator::setPowerState (
													unsigned long   powerStateOrdinal,
													IOService*		whatDevice
													)
{
	IOReturn	result = IOPMAckImplied;
	
	DbgIOLog(DEBUG_MASK_POWER_TRACE, ("setPowerState: state 0x%x\n", powerStateOrdinal ));
	
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

IOReturn 
com_ximeta_driver_NDASBusEnumerator::powerStateWillChangeTo (
															 IOPMPowerFlags  capabilities,
															 unsigned long   stateNumber,
															 IOService*		whatDevice
															 )
{
	DbgIOLog(DEBUG_MASK_POWER_TRACE, ("powerStateWillChangeTo: Flag 0x%x, Number 0x%x 0x%x\n", capabilities, stateNumber, fpowerTimerSourcePtr ));

	int result = IOPMAckImplied;
		
	if(kNDASPowerOffState == stateNumber) {
		
		result = kOneSecond * 2;
		
		// Send message.
		messageClients(kIOMessageServiceIsSuspended);				
		
		if ( fpowerTimerSourcePtr ) {
			fpowerTimerSourcePtr->setTimeoutMS(1 * 1000);
		}
		
		clampPowerOn(2 * 1000);
	}

	return result;
}

IOReturn 
com_ximeta_driver_NDASBusEnumerator::powerStateDidChangeTo (
															 IOPMPowerFlags  capabilities,
															 unsigned long   stateNumber,
															 IOService*		whatDevice
															 )
{
	DbgIOLog(DEBUG_MASK_POWER_TRACE, ("powerStateDidChangeTo: Flag 0x%x, Number 0x%x\n", capabilities, stateNumber ));

	if(kNDASPowerOnState == stateNumber) {
		
		// Send message.
		messageClients(kIOMessageServiceIsResumed);
		
		// Check Bind...
		com_ximeta_driver_NDASDevice	*NDASDevicePtr = NULL;
		OSMetaClassBase					*metaObject = NULL;

		// Search Device List.
		OSCollectionIterator *DeviceListIterator = OSCollectionIterator::withCollection(fDeviceListPtr);
		if(DeviceListIterator == NULL) {
			DbgIOLog(DEBUG_MASK_USER_ERROR, ("powerStateDidChangeTo: Can't Create Iterator.\n"));
			return IOPMAckImplied;
		}
				
		while((metaObject = DeviceListIterator->getNextObject()) != NULL) {

			NDASDevicePtr = OSDynamicCast(com_ximeta_driver_NDASDevice, metaObject);
			if(NDASDevicePtr == NULL) {
				DbgIOLog(DEBUG_MASK_USER_ERROR, ("powerStateDidChangeTo: Can't Cast to Device Class.\n"));
				continue;
			}
			
			for (int unitIndex = 0; unitIndex < MAX_NR_OF_TARGETS_PER_DEVICE; unitIndex++) {
				NDASDevicePtr->setTargetNeedRefresh(unitIndex);
			}
		}
		
		
	}
	
	fCurrentPowerState = stateNumber;
		
	return IOPMAckImplied;
}

#pragma mark -
#pragma mark == Logiical Device Management ==
#pragma mark -

com_ximeta_driver_NDASLogicalDevice* 
com_ximeta_driver_NDASBusEnumerator::FindLogicalDeviceByIndex(UInt32 index)
{
	
    com_ximeta_driver_NDASLogicalDevice		*NDASDevicePtr = NULL;
/*
	// Search Device List.
	OSCollectionIterator *DeviceListIterator = OSCollectionIterator::withCollection(fLogicalDeviceList);
	if(DeviceListIterator == NULL) {
		DbgIOLog(DEBUG_MASK_USER_ERROR, ("FindLogicalDeviceByIndex: Can't Create Iterator.\n"));
		return NULL;
	}
	
	OSMetaClassBase	*metaObject;
	
	while((metaObject = DeviceListIterator->getNextObject()) != NULL) { 
		
		NDASDevicePtr = OSDynamicCast(com_ximeta_driver_NDASLogicalDevice, metaObject);
		if(NDASDevicePtr == NULL) {
			DbgIOLog(DEBUG_MASK_USER_INFO, ("FindLogicalDeviceByIndex: Can't Cast to Device Class.\n"));
			continue;
		}
		
		if(NDASDevicePtr->Index() == index) {
			DbgIOLog(DEBUG_MASK_USER_INFO, ("FindLogicalDeviceByIndex: Find Registed Slot Number %d.\n", index));
			break;
		}
		
	}
	
	DeviceListIterator->release();
	
	return NDASDevicePtr;
	 */
	NDASDevicePtr = OSDynamicCast(com_ximeta_driver_NDASLogicalDevice, fLogicalDeviceList->findUnitDeviceByIndex(index));
	
	return NDASDevicePtr;
}

bool com_ximeta_driver_NDASBusEnumerator::createLogicalDevice(
															  com_ximeta_driver_NDASUnitDevice *device
															  )
{
    DbgIOLog(DEBUG_MASK_USER_TRACE, ("createLogicalDevice: Entered.\n"));

	DbgIOLog(DEBUG_MASK_USER_INFO, ("========createLogicalDevice: Before Create logical Device. Num %d list %d\n", fNumberOfLogicalDevices, fLogicalDeviceList->getCount()));

    bool								bret;
    OSDictionary						*prop = NULL;
    com_ximeta_driver_NDASLogicalDevice	*NDASLogicalDevice = NULL;
			
	// Create NDASLogicalDevice.	  
    prop = OSDictionary::withCapacity(4);
    if(prop == NULL)
    {
        DbgIOLog(DEBUG_MASK_USER_ERROR, ("createLogicalDevice: failed to alloc property\n"));

        return false;
    }

    prop->setCapacityIncrement(4);
    
    NDASLogicalDevice = OSDynamicCast(com_ximeta_driver_NDASLogicalDevice, 
                               OSMetaClass::allocClassWithName(NDAS_LOGICAL_DEVICE_CLASS));
    
    if(NDASLogicalDevice == NULL) {
        DbgIOLog(DEBUG_MASK_USER_ERROR, ("createLogicalDevice: failed to alloc Device class\n"));

		prop->release();

        return false;
    }
	
    DbgIOLog(DEBUG_MASK_USER_INFO, ("createLogicalDevice: Logical Device = %p\n", NDASLogicalDevice));
    
	// Init Device.
    bret = NDASLogicalDevice->init(prop);
    prop->release();
    if(bret == false)
    {
        NDASLogicalDevice->release();
        DbgIOLog(DEBUG_MASK_USER_ERROR, ("createLogicalDevice: failed to init Device\n"));

        return false;
    }

	// Assign attributes.
	NDASLogicalDevice->setUnitDevice(device);
	NDASLogicalDevice->setStatus(kNDASUnitStatusDisconnected);

	// Update Write Right.
	NDASLogicalDevice->IsWritable();
	
	device->setUpperUnitDevice(NDASLogicalDevice);
		
	// Attach Device.
	bret = NDASLogicalDevice->attach(this);
	if(bret == true) {
		// Register Service.
		//NDASLogicalDevice->registerService();
	}

	// Append Unit Device List.
	int index = fLogicalDeviceList->insertUnitDevice(NDASLogicalDevice);
	if (index == -1) {
		DbgIOLog(DEBUG_MASK_USER_INFO, ("createLogicalDevice: Can't insert Logical Device to List index %d.\n", index));
		
		return false;
	}
	
	NDASLogicalDevice->setIndex(index);
	
	fNumberOfLogicalDevices++;
	
	DbgIOLog(DEBUG_MASK_USER_INFO, ("========createLogicalDevice: After Create logical Device. Num %d list %d\n", fNumberOfLogicalDevices, fLogicalDeviceList->getCount()));

	messageClients(kNDASFamilyIOMessageLogicalDeviceWasCreated, NDASLogicalDevice->UnitDevice()->ID(), sizeof(GUID));
	
    return true;
}

// kNDASEnumUnregister
bool com_ximeta_driver_NDASBusEnumerator::deleteLogicalDevice(com_ximeta_driver_NDASLogicalDevice *logicalDevice)
{
	DbgIOLog(DEBUG_MASK_USER_TRACE, ("deleteLogicalDevice: Entered.\n"));
		
	DbgIOLog(DEBUG_MASK_USER_INFO, ("========deleteLogicalDevice: Before Delete logical Device. Num %d list %d\n", fNumberOfLogicalDevices, fLogicalDeviceList->getCount()));
	
	if(logicalDevice == NULL) {
		DbgIOLog(DEBUG_MASK_USER_ERROR, ("deleteLogicalDevice: logical Device Pointer is NULL\n"));
		
		return false;
	}

	// detach Protocol Transport Nub.
	if (logicalDevice->ProtocolTransportNub()) {
		DbgIOLog(DEBUG_MASK_USER_ERROR, ("deleteLogicalDevice: Protocol transport NUB is not NULL\n"));
		
		return false;
	}
	
	logicalDevice->setStatus(kNDASUnitStatusNotPresent);

	GUID	id;
	
	memcpy(&id, logicalDevice->UnitDevice()->ID(), sizeof(GUID));

	if (logicalDevice->UnitDevice()) {		
		logicalDevice->UnitDevice()->setUpperUnitDevice(NULL);
		logicalDevice->setUnitDevice(NULL);
	}
	
	// Delete from List.	
	logicalDevice->detach(this);
	
	/*
	// Send kIOMessageServiceIsRequestingClose message.
	messageClient ( kIOMessageServiceIsRequestingClose, logicalDevice );
	
	// Terminate Device.
	logicalDevice->terminate();
	
	logicalDevice->willTerminate(this, 0);
*/
	// remove from List.
	if (!fLogicalDeviceList->removeUnitDevice(logicalDevice->Index())) {
		DbgIOLog(DEBUG_MASK_USER_ERROR, ("deleteLogicalDevice: Can't remote logical Device from list!!! \n"));	
	}

	// Release Device.
	logicalDevice->release();
	
	fNumberOfLogicalDevices--;

	DbgIOLog(DEBUG_MASK_USER_INFO, ("========deleteLogicalDevice: After Delete logical Device. Num %d. list %d\n", fNumberOfLogicalDevices, fLogicalDeviceList->getCount()));

	messageClients(kNDASFamilyIOMessageLogicalDeviceWasDeleted, &id, sizeof(GUID));

    return true;	
}

#pragma mark -
#pragma mark == Protocol Transport Management ==
#pragma mark -

bool com_ximeta_driver_NDASBusEnumerator::attachProtocolTransport(com_ximeta_driver_NDASLogicalDevice *logicalDevice)
{
    DbgIOLog(DEBUG_MASK_DISK_TRACE, ("attachProtocolTransport: Entered\n"));
	
    bool										bret;
    OSDictionary								*prop = NULL;
    com_ximeta_driver_NDASProtocolTransportNub	*ProtocolTransportNub = NULL;
	
	// Check 
	if(NULL != logicalDevice->ProtocolTransportNub()) {
		DbgIOLog(DEBUG_MASK_DISK_INFO, ("attachProtocolTransport: Already Created.\n"));
	
		return true;
	}
	
	// Create NDASLogicalDevice.	  
    prop = OSDictionary::withCapacity(4);
    if(prop == NULL)
    {
        DbgIOLog(DEBUG_MASK_USER_ERROR, ("attachProtocolTransport: failed to alloc property\n"));
		
        return false;
    }
	
	// Icon.
	switch(logicalDevice->UnitDevice()->Type()) {
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
    
    ProtocolTransportNub = OSDynamicCast(com_ximeta_driver_NDASProtocolTransportNub, 
									  OSMetaClass::allocClassWithName(NDAS_PROTOCOL_TRANSPORT_NUB_CLASS));
    
    if(ProtocolTransportNub == NULL) {
        DbgIOLog(DEBUG_MASK_USER_ERROR, ("attachProtocolTransport: failed to alloc Device class\n"));
		
		prop->release();
		
        return false;
    }
	
    DbgIOLog(DEBUG_MASK_USER_INFO, ("attachProtocolTransport: Protocol Transport NUB Device = %p\n", ProtocolTransportNub));
    
	// Init Device.
    bret = ProtocolTransportNub->init(prop);
    prop->release();
    if(bret == false)
    {
        ProtocolTransportNub->release();
        DbgIOLog(DEBUG_MASK_USER_ERROR, ("attachProtocolTransport: failed to init Device\n"));
		
        return false;
    }
		
	// Attach Device.
	bret = ProtocolTransportNub->attach(this);
	if(bret == false)
    {
        ProtocolTransportNub->release();
        DbgIOLog(DEBUG_MASK_USER_ERROR, ("attachProtocolTransport: failed to attach Device\n"));
		
        return false;
    }
	
	// Register Service.
	ProtocolTransportNub->registerService();

	// Assign attributes.
	ProtocolTransportNub->setLogicalDevice(logicalDevice);

	// Append Unit Device List.
	logicalDevice->setProtocolTransportNub(ProtocolTransportNub);

    return true;
}

bool com_ximeta_driver_NDASBusEnumerator::detachProtocolTransport(com_ximeta_driver_NDASProtocolTransportNub *ProtocolTransportNub)
{
	DbgIOLog(DEBUG_MASK_DISK_TRACE, ("detachProtocolTransport: Entered.\n"));
			
	if (ProtocolTransportNub == NULL) {
		DbgIOLog(DEBUG_MASK_USER_ERROR, ("detachProtocolTransport: ProtocolTransportNub Device Pointer is NULL\n"));
		
		return false;
	}
	
	if (ProtocolTransportNub->LogicalDevice()) {
		ProtocolTransportNub->LogicalDevice()->setProtocolTransportNub(NULL);
	}
	
	ProtocolTransportNub->setLogicalDevice(NULL);
			
	// Send kIOMessageServiceIsRequestingClose message.
	messageClient ( kIOMessageServiceIsRequestingClose, ProtocolTransportNub );

	// Send will Terminate
	ProtocolTransportNub->willTerminate(this, 0);

	// Terminate Device.
	ProtocolTransportNub->terminate();
			
	// Release Device.
	ProtocolTransportNub->release();
	
    return true;	
}

#pragma mark -
#pragma mark == RAID Device Management ==
#pragma mark -

com_ximeta_driver_NDASRAIDUnitDevice* 
com_ximeta_driver_NDASBusEnumerator::FindRAIDDeviceByIndex(UInt32 index)
{
    com_ximeta_driver_NDASRAIDUnitDevice		*NDASDevicePtr = NULL;
/*	
	// Search Device List.
	OSCollectionIterator *DeviceListIterator = OSCollectionIterator::withCollection(fRAIDDeviceList);
	if(DeviceListIterator == NULL) {
		DbgIOLog(DEBUG_MASK_USER_ERROR, ("FindRAIDDeviceByIndex: Can't Create Iterator.\n"));
		return NULL;
	}
	
	OSMetaClassBase	*metaObject;
	
	while((metaObject = DeviceListIterator->getNextObject()) != NULL) { 
		
		NDASDevicePtr = OSDynamicCast(com_ximeta_driver_NDASRAIDUnitDevice, metaObject);
		if(NDASDevicePtr == NULL) {
			DbgIOLog(DEBUG_MASK_USER_INFO, ("FindRAIDDeviceByIndex: Can't Cast to Device Class.\n"));
			continue;
		}
		
		if(NDASDevicePtr->Index() == index) {
			DbgIOLog(DEBUG_MASK_USER_INFO, ("FindRAIDDeviceByIndex: Find Registed Slot Number %d.\n", index));
			break;
		}
		
		NDASDevicePtr = NULL;
	}
	
	DeviceListIterator->release();
*/
	NDASDevicePtr = OSDynamicCast(com_ximeta_driver_NDASRAIDUnitDevice, fRAIDDeviceList->findUnitDeviceByIndex(index));
	
	return NDASDevicePtr;
}

com_ximeta_driver_NDASRAIDUnitDevice* 
com_ximeta_driver_NDASBusEnumerator::FindRAIDDeviceByGUID(PGUID guid)
{
    com_ximeta_driver_NDASRAIDUnitDevice		*NDASDevicePtr = NULL;
	/*
	// Search Device List.
	OSCollectionIterator *DeviceListIterator = OSCollectionIterator::withCollection(fRAIDDeviceList);
	if(DeviceListIterator == NULL) {
		DbgIOLog(DEBUG_MASK_USER_ERROR, ("FindRAIDDeviceByGUID: Can't Create Iterator.\n"));
		return NULL;
	}
	
	OSMetaClassBase	*metaObject;
	
	while((metaObject = DeviceListIterator->getNextObject()) != NULL) { 
		
		NDASDevicePtr = OSDynamicCast(com_ximeta_driver_NDASRAIDUnitDevice, metaObject);
		if(NDASDevicePtr == NULL) {
			DbgIOLog(DEBUG_MASK_USER_INFO, ("FindRAIDDeviceByGUID: Can't Cast to Device Class.\n"));
			continue;
		}
		
		if(0 == memcmp(NDASDevicePtr->ID(), guid, sizeof(GUID))) {
			DbgIOLog(DEBUG_MASK_USER_INFO, ("FindRAIDDeviceByGUID: Find Registed Slot Number %d.\n", NDASDevicePtr->Index()));
			break;
		}
		
		NDASDevicePtr = NULL;
	}
	
	DeviceListIterator->release();
	*/
	NDASDevicePtr = OSDynamicCast(com_ximeta_driver_NDASRAIDUnitDevice, fRAIDDeviceList->findUnitDeviceByGUID(guid));
	
	return NDASDevicePtr;
}

bool com_ximeta_driver_NDASBusEnumerator::createRAIDDevice(
														   com_ximeta_driver_NDASUnitDevice *device
														   )
{
    DbgIOLog(DEBUG_MASK_USER_TRACE, ("createRAIDDevice: Entered\n"));
	
    bool									bret;
    OSDictionary							*prop = NULL;
    com_ximeta_driver_NDASRAIDUnitDevice	*NDASRAIDDevice = NULL;
	
	// Check Parameter.
	if (NULL == device) {

		DbgIOLog(DEBUG_MASK_USER_ERROR, ("createRAIDDevice: Invalid Parameter\n"));

		return false;
	}

	NDASRAIDDevice = FindRAIDDeviceByGUID(getRMDGUID(device->RMD()));
	
	if (NULL == NDASRAIDDevice) {
		// Create NDASRAIDUnitDevice.	  
		prop = OSDictionary::withCapacity(4);
		if(prop == NULL)
		{
			DbgIOLog(DEBUG_MASK_USER_ERROR, ("createRAIDDevice: failed to alloc property\n"));
			
			return false;
		}
		
		prop->setCapacityIncrement(4);
		
		NDASRAIDDevice = OSDynamicCast(com_ximeta_driver_NDASRAIDUnitDevice, 
									  OSMetaClass::allocClassWithName(NDAS_RAID_UNIT_DEVICE_CLASS));
		
		if(NDASRAIDDevice == NULL) {
			DbgIOLog(DEBUG_MASK_USER_ERROR, ("createRAIDDevice: failed to alloc Device class\n"));
			
			prop->release();
			
			return false;
		}
		
		DbgIOLog(DEBUG_MASK_USER_INFO, ("createRAIDDevice: RAID Unit Device = %p\n", NDASRAIDDevice));
		
		// Init Device.
		bret = NDASRAIDDevice->init(prop);
		prop->release();
		if(bret == false)
		{
			NDASRAIDDevice->release();
			DbgIOLog(DEBUG_MASK_USER_ERROR, ("createRAIDDevice: failed to init Device\n"));
			
			return false;
		}				
		
		// Attach Device.
		bret = NDASRAIDDevice->attach(this);
		if(bret == true) {
			// Register Service.
			NDASRAIDDevice->registerService();
		}	
		
		// Append Unit Device List.
		int index = fRAIDDeviceList->insertUnitDevice(NDASRAIDDevice);
		if (index == -1) {
			DbgIOLog(DEBUG_MASK_USER_INFO, ("createRAIDDevice: Can't insert RAID Device to List index %d.\n", index));
			
			return false;
		}
				
		NDASRAIDDevice->setIndex(index);
		
		NDASRAIDDevice->setID(getRMDGUID(device->RMD()));
		NDASRAIDDevice->setUnitDiskLocation(getDIBUnitDiskLocation(device->DIB2(), 0));
		
		// Set Type.
		NDASRAIDDevice->setType(device->DiskArrayType());
				
		NDASRAIDDevice->setCountOfUnitDevices(getDIBCountOfUnitDisks(device->DIB2()));
		NDASRAIDDevice->setCountOfSpareDevices(getDIBCountOfSpareDevices(device->DIB2()));
		
		// Set Unit Devices Info.
		NDASRAIDDevice->setUnitDevices(device->DIB2(), device->RMD());
		
		NDASRAIDDevice->setRAIDStatus(kNDASRAIDStatusNotReady);
		
		NDASRAIDDevice->setConfigID(getRMDConfigSetID(device->RMD()));
		NDASRAIDDevice->setRMDUSN(getRMDUSN(device->RMD()));
		
	} else {
		// Check Config ID and USN.
		if (0 != memcmp(NDASRAIDDevice->ConfigID(), getRMDConfigSetID(device->RMD()), sizeof(GUID)) ) {
			if (NDASRAIDDevice->RMDUSN() > getRMDUSN(device->RMD())) {
				
				int index;
								
				// Copy Good RMD.
				for (index = 0; index < NDASRAIDDevice->CountOfUnitDevices(); index++) {
					com_ximeta_driver_NDASUnitDevice *subUnit;
					
					if (subUnit = NDASRAIDDevice->UnitDeviceAtIndex(index)) {
						
						memcpy(device->RMD(), subUnit->RMD(), sizeof(NDAS_RAID_META_DATA));
						break;
					}
				}
				
				if (index == NDASRAIDDevice->CountOfUnitDevices()) {
					
					// Attached Unit is bad device.
					device->setDiskArrayType(NMT_CONFLICT);

					return false;
				}
	
			} else {
				// We got a problem. ^^;
				if (NDASRAIDDevice->IsInRunningStatus()) {
					// BUG BUG BUG.
					
				} else {
					int index;
					
					for (index = 0; index < NDASRAIDDevice->CountOfUnitDevices(); index++) {
						com_ximeta_driver_NDASUnitDevice *subUnit;
						
						if (subUnit = NDASRAIDDevice->UnitDeviceAtIndex(index)) {
																					
							NDASRAIDDevice->setUnitDevice(index, NULL);
														
							subUnit->setDiskArrayType(NMT_INVALID);
							subUnit->setUpperUnitDevice(NULL);

						}
					}					
					
					// Set Unit Devices Info.
					NDASRAIDDevice->setUnitDevices(device->DIB2(), device->RMD());
					
					NDASRAIDDevice->setRAIDStatus(kNDASRAIDStatusNotReady);
					
					NDASRAIDDevice->setConfigID(getRMDConfigSetID(device->RMD()));
					NDASRAIDDevice->setRMDUSN(getRMDUSN(device->RMD()));					
				}	
			}
		}
	}
	
	// Assign attributes.
	device->setSequence(getDIBSequence(device->DIB2()));
	
	if (NMT_RAID1R3 == device->DiskArrayType()) {

		SInt32	temp = getRMDIndexOfUnit(device->RMD(), device->Sequence());
		
		if (-1 == temp) {
			device->setSubUnitIndex(device->Sequence());	
		} else {
			device->setSubUnitIndex(temp);
		}

	} else {
		device->setSubUnitIndex(device->Sequence());	
	}
	
	NDASRAIDDevice->setUnitDevice(device->SubUnitIndex(), device);
	
	device->setUpperUnitDevice(NDASRAIDDevice);
			
	messageClients(kNDASFamilyIOMessageRAIDDeviceWasCreated, NDASRAIDDevice->ID(), sizeof(GUID));

    return true;
}

// kNDASEnumUnregister
int com_ximeta_driver_NDASBusEnumerator::deleteRAIDDevice(com_ximeta_driver_NDASUnitDevice *device)
{
	DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("deleteRAIDDevice: Entered.\n"));
	
	com_ximeta_driver_NDASRAIDUnitDevice	*raidDevice;
	
	if(device == NULL) {
		DbgIOLog(DEBUG_MASK_USER_ERROR, ("deleteRAIDDevice: Unit Device Pointer is NULL\n"));
		
		return -1;
	}
	
	raidDevice = OSDynamicCast(com_ximeta_driver_NDASRAIDUnitDevice, device->UpperUnitDevice());
	if (raidDevice == NULL) {
		DbgIOLog(DEBUG_MASK_USER_ERROR, ("deleteRAIDDevice: RAID Unit Device Pointer is NULL\n"));
		
		device->setUpperUnitDevice(NULL);
		
		return -1;
	}

	// Unset Unit Device.
	raidDevice->setUnitDevice(device->SubUnitIndex(), NULL);
	
	device->setUpperUnitDevice(NULL);
	
	int countOfRegistedUnitDevices = raidDevice->CountOfRegistetedUnitDevices();
	
	if (0 == countOfRegistedUnitDevices) {
		
		DbgIOLog(DEBUG_MASK_USER_INFO, ("deleteRAIDDevice: No Unit Device. Release RAID Unit Device\n"));
		
		// Create NDAS Command.
		com_ximeta_driver_NDASCommand *command = OSDynamicCast(com_ximeta_driver_NDASCommand, 
															   OSMetaClass::allocClassWithName(NDAS_COMMAND_CLASS));
		
		if(command == NULL || !command->init()) {
			DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("processDeviceNotification: failed to alloc command class\n"));
			if (command) {
				command->release();
			}
			
			return false;
		}
		
		GUID id;
		
		memcpy(&id, raidDevice->ID(), sizeof(GUID));
		
		command->setCommand(kNDASDeviceNotificationNotPresent, raidDevice);
		
		enqueueMessage(command);
		
		command->release();
		
		fRAIDDeviceList->removeUnitDevice(raidDevice->Index());
			
		// Send kIOMessageServiceIsRequestingClose message.
		//messageClient ( kIOMessageServiceIsRequestingClose, raidDevice );
		
		// Terminate Device.
		raidDevice->terminate();
		
		//raidDevice->willTerminate(this, 0);
		
		// Release Device.
		raidDevice->release();		
		
		messageClients(kNDASFamilyIOMessageRAIDDeviceWasDeleted, &id, sizeof(GUID));
	}	
	
    return countOfRegistedUnitDevices;	
}

#pragma mark -
#pragma mark == Device Management ==
#pragma mark -

void com_ximeta_driver_NDASBusEnumerator::deviceManagementThread(void* argument)
{	
//	bool bTerminate = false;
	
	DbgIOLog(DEBUG_MASK_NDAS_TRACE, ("deviceManagementThread: Entered.\n"));
	
	this->retain();
	
    do {
		kern_return_t					sema_result;
		mach_timespec_t					wait_time = { 0 };

		DbgIOLog(DEBUG_MASK_NDAS_INFO, ("deviceManagementThread: Before wait sema\n"));				

		// Set timeout and wait.
		wait_time.tv_sec = NDAS_MAX_WAIT_TIME_FOR_BUS_THREAD;
        sema_result = semaphore_timedwait(fCommandSema, wait_time);
		
		DbgIOLog(DEBUG_MASK_NDAS_INFO, ("deviceManagementThread: sema Result %d\n", sema_result));				

		switch(sema_result) {
			case KERN_SUCCESS:
			{
				// Check Command Queue.
				if(0 == fCommandArray->getCount()) {
					// Maybe Terminate signal.
					DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("deviceManagementThread: No Command.\n"));
					
					continue;
				}
				
				com_ximeta_driver_NDASCommand *command = OSDynamicCast(com_ximeta_driver_NDASCommand, fCommandArray->getObject(0));
				if( NULL == command ) {
					DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("deviceManagementThread: getObject return NULL.\n"));
					
					continue;
				}
				
				command->retain();
				
				switch(command->command()) {
					case kNDASCommandTerminateThread:
					{
						//bTerminate = TRUE;
						command->release();
						continue;
					}
						break;
					case kNDASCommandDeviceNotification:
					{
						processDeviceNotification(command);						
					}
						break;
					case kNDASCommandReceivePnpMessage:
					{
						if (!fWillTerminate) {
							processPnpMessage(command);
						}
					}
						break;
					default:
						DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("deviceManagementThread: Unknowen Command. %d\n", command->command()));
						
				}
				
				command->setToComplete();

				command->release();

				fCommandGate->runCommand((void *)kNDASCommandQueueCompleteCommand, (void*)command);
			}
				break;
			case KERN_OPERATION_TIMED_OUT:
			{
				DbgIOLog(DEBUG_MASK_NDAS_INFO, ("deviceManagementThread: Timed Out.\n"));				
			}
				break;
			default:
				DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("deviceManagementThread: Invalid return value. %d\n", sema_result));
				break;
		}
		
		
	} while(fThreadTerminate == false);

	fWorkerThread = NULL;
	
	DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("deviceManagementThread: End.\n"));

	this->release();

	IOExitThread();

	// Never return.
}

void deviceManagementThreadWrapper(void* argument)
{		
	com_ximeta_driver_NDASBusEnumerator *bus = (com_ximeta_driver_NDASBusEnumerator*)argument;
	
	bus->deviceManagementThread(NULL);
	
	return;
}

void com_ximeta_driver_NDASBusEnumerator::processPnpMessage(com_ximeta_driver_NDASCommand *command)
{
	PPNPCommand pnpCommand = command->pnpCommand();
	
	DbgIOLog(DEBUG_MASK_NDAS_TRACE, ("processPnpMessage:  Entered.\n"));

	// Search Device List.
	OSCollectionIterator *DeviceListIterator = OSCollectionIterator::withCollection(fDeviceListPtr);
	if(DeviceListIterator == NULL) {
		DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("broadcastReceiverThread: Can't Create Iterator.\n"));
		return;
	}
	
	OSMetaClassBase					*metaObject;
	com_ximeta_driver_NDASDevice	*NDASDevicePtr;
	
	while((metaObject = DeviceListIterator->getNextObject()) != NULL) {
		NDASDevicePtr = OSDynamicCast(com_ximeta_driver_NDASDevice, metaObject);
		if(NDASDevicePtr == NULL) {
			DbgIOLog(DEBUG_MASK_NDAS_INFO, ("broadcastReceiverThread: Can't Cast to Device Class.\n"));
			continue;
		}
		
		if(bcmp(pnpCommand->fAddress, NDASDevicePtr->address()->slpx_node, 6) == 0) {
						
			// Check configuration.
			if(kNDASConfigurationDisable != NDASDevicePtr->configuration()
			   && !NDASDevicePtr->isInactive()) {
				NDASDevicePtr->receivePnPMessage(
												 pnpCommand->fReceivedTime, 
												 pnpCommand->fMessage.ucType, 
												 pnpCommand->fMessage.ucVersion,
												 pnpCommand->fInterfaceArrayIndex,
												 fInterfaces[pnpCommand->fInterfaceArrayIndex].fAddress,
												 fInterfaces[pnpCommand->fInterfaceArrayIndex].fSpeed
												 );
				
			} else {
				DbgIOLog(DEBUG_MASK_NDAS_INFO, ("broadcastReceiverThread: Not Enabled. %d\n", NDASDevicePtr->configuration()));
			}
			
			break;
		} else {
			DbgIOLog(DEBUG_MASK_NDAS_INFO, ("broadcastReceiverThread: Not Matched. slotNumber %d\n", NDASDevicePtr->slotNumber()));			
		}
	}
	
	DeviceListIterator->release();
	
	DbgIOLog(DEBUG_MASK_NDAS_TRACE, ("processPnpMessage: Exited.\n"));

}

void com_ximeta_driver_NDASBusEnumerator::processDeviceNotification(com_ximeta_driver_NDASCommand *command)
{
	PDeviceNotificationCommand		deviceCommand;
	com_ximeta_driver_NDASUnitDevice *unitDevice;
	
	DbgIOLog(DEBUG_MASK_NDAS_TRACE, ("processDeviceNotification:  Entered.\n"));
	
	deviceCommand = command->DeviceNotificationCommand();
	unitDevice = deviceCommand->device;
	
	switch (deviceCommand->notificationType) {
		case kNDASDeviceNotificationPresent:
		{
			DbgIOLog(DEBUG_MASK_NDAS_INFO, ("processDeviceNotification:  Receive kNDASDeviceNotificationPresent.\n"));
			
			// Check Disk type.
			if (kNDASUnitStatusNotPresent != unitDevice->Status()
//				&& kNDASUnitStatusFailure != unitDevice->Status()
				&& NMT_INVALID == unitDevice->DiskArrayType()) {
								
				DbgIOLog(DEBUG_MASK_NDAS_INFO, ("processDeviceNotification:  No Disk Array Type.\n"));
				
				if (!unitDevice->checkDiskArrayType()) {
					DbgIOLog(DEBUG_MASK_NDAS_TRACE, ("processDeviceNotification: checkDiskArrayType Failed\n"));
					
					break;
				}
			}
			
			switch (unitDevice->DiskArrayType()) {
				case NMT_SINGLE:
				case NMT_CDROM:
				{
					DbgIOLog(DEBUG_MASK_NDAS_INFO, ("processDeviceNotification: Single Disk\n"));

					com_ximeta_driver_NDASLogicalDevice *LogicalDevice = NULL;
					
					// Search Logical Device for this Unit Device If not present Create newe Logical Device.
					if (NULL == unitDevice->UpperUnitDevice()) {
						if(false == createLogicalDevice(unitDevice)) {
							DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("processDeviceNotification: Can't Create Logical device.\n"));
							
							break;
						}
												
						//messageClient(kIOMessageServicePropertyChange, this, NULL, 0);
					}
					
					LogicalDevice = OSDynamicCast(com_ximeta_driver_NDASLogicalDevice, unitDevice->UpperUnitDevice());
					if (NULL == LogicalDevice) {
						DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("processDeviceNotification: No Logical Device.\n"));
						
						break;						
					}
					
					// Update Configuration.
					LogicalDevice->setConfiguration(LogicalDevice->Configuration());
					LogicalDevice->setNRRWHosts(LogicalDevice->NRRWHosts());
					LogicalDevice->setNRROHosts(LogicalDevice->NRROHosts());
					LogicalDevice->IsWritable();
					
					// Update Status.
					LogicalDevice->updateStatus();
					
					switch(LogicalDevice->Status()) {
						case kNDASUnitStatusTryConnectRO:
						case kNDASUnitStatusTryConnectRW:
						{
						}
							break;
						case kNDASUnitStatusTryDisconnectRO:
						case kNDASUnitStatusTryDisconnectRW:
						{
							// Notify to Transport Driver.
							if (LogicalDevice->ProtocolTransportNub()) {
								detachProtocolTransport(LogicalDevice->ProtocolTransportNub());
							}
						}
							break;
						case kNDASUnitStatusConnectedRO:
						case kNDASUnitStatusConnectedRW:
						{
							// Notify to Transport Driver.					
							if (!LogicalDevice->ProtocolTransportNub()) {
								attachProtocolTransport(LogicalDevice);
							}
							
							LogicalDevice->setBSDName();
						}
							break;
					}
												
					// Update Status.
					LogicalDevice->updateStatus();
				}
					break;
				case NMT_INVALID:
				{
					break;
				}
				default:
				{
					DbgIOLog(DEBUG_MASK_NDAS_INFO, ("processDeviceNotification: RAID Disk\n"));

					com_ximeta_driver_NDASRAIDUnitDevice	*raidDevice;
					
					// Search RAID Device for this Unit Device If not present Create newe RAID Device.
					if (NULL == unitDevice->UpperUnitDevice()) {
						if(false == createRAIDDevice(unitDevice)) {
							DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("processDeviceNotification: Can't Create RAID device.\n"));
							
							break;
						}
						
						// Check writable.
						raidDevice = OSDynamicCast(com_ximeta_driver_NDASRAIDUnitDevice, unitDevice->UpperUnitDevice());
						
						if (raidDevice) {
							raidDevice->IsWritable();
						}
					}
					
					raidDevice = OSDynamicCast(com_ximeta_driver_NDASRAIDUnitDevice, unitDevice->UpperUnitDevice());
					
					if (raidDevice) {
						
						raidDevice->receivePnPMessage();
					
					}
				}
					break;
			}			
		}
			break;
		case kNDASDeviceNotificationNotPresent:
		{
			UInt32	DiskArrayType;
			
			DbgIOLog(DEBUG_MASK_NDAS_INFO, ("processDeviceNotification: Receive kNDASDeviceNotificationNotPresent \n"));

			//unitDevice->setStatus(kNDASUnitStatusNotPresent);
			DiskArrayType = unitDevice->DiskArrayType();
			
			// 
			// Clear Disk type.
			//
			unitDevice->setDiskArrayType(NMT_INVALID);
			
			switch (DiskArrayType) {
				case NMT_SINGLE:
				case NMT_CDROM:
				{
					DbgIOLog(DEBUG_MASK_NDAS_INFO, ("processDeviceNotification: Single Disk\n"));

					com_ximeta_driver_NDASLogicalDevice	*logicalDevice = NULL;
					
					logicalDevice = OSDynamicCast(com_ximeta_driver_NDASLogicalDevice, unitDevice->UpperUnitDevice());
					
					if (NULL == logicalDevice) {
						DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("processDeviceNotification: No Logical Device!!!\n"));
						
						break;
					} else {
						
						if (logicalDevice->ProtocolTransportNub()) {
							DbgIOLog(DEBUG_MASK_NDAS_INFO, ("processDeviceNotification: Logical device has Protocol transport NUB.\n"));

							DbgIOASSERT(detachProtocolTransport(logicalDevice->ProtocolTransportNub()));
						}
						
						if(false == deleteLogicalDevice(logicalDevice)) {
							DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("processDeviceNotification: Can't delete Logical device.\n"));
							
							break;
						}						
					}
					
				}
					break;
				case NMT_INVALID:
				{
				}
					break;
				default:
				{
					DbgIOLog(DEBUG_MASK_NDAS_INFO, ("processDeviceNotification: [NotPresent] to RAID Device\n"));

					com_ximeta_driver_NDASRAIDUnitDevice	*raidDevice = NULL;
					
					raidDevice = OSDynamicCast(com_ximeta_driver_NDASRAIDUnitDevice, unitDevice->UpperUnitDevice());
					
					if (NULL == raidDevice) {
						DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("processDeviceNotification: No RAID Device!!!\n"));
						
						break;
					}
					
					raidDevice->noPnPMessage();

					int countOfSubUnits;
					
					// Delete RAID device.
					if(-1 == (countOfSubUnits = deleteRAIDDevice(unitDevice))) {
						DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("processDeviceNotification: Can't delete RAID device.\n"));
						
						break;
					}					
				}
					break;
			}	
		}
			break;
		case kNDASDeviceNotificationCheckBind:
		{
			DbgIOLog(DEBUG_MASK_NDAS_INFO, ("processDeviceNotification: kNDSASDeviceNotificationCheckBind\n"));

			UInt32	previousType; 
			GUID	prevRAIDID;
			GUID	prevConfigID;

			if (kNDASUnitStatusNotPresent == unitDevice->Status()) {
				DbgIOLog(DEBUG_MASK_NDAS_WARNING, ("processDeviceNotification: kNDSASDeviceNotificationCheckBind:: No Unit Device.\n"));

				break;
			}
				
			previousType = unitDevice->DiskArrayType();
			
			switch(previousType) {
				case NMT_INVALID:
				case NMT_SINGLE:
				case NMT_CDROM:
				{
					break;
				}
				default:
				{
					memcpy(&prevRAIDID, getRMDGUID(unitDevice->RMD()), sizeof(GUID));
					memcpy(&prevConfigID, getRMDConfigSetID(unitDevice->RMD()), sizeof(GUID));
				}
					break;
			}
			
			// Check Disk Array Type.
			if (false == unitDevice->checkDiskArrayType()) {
				DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("processDeviceNotification: checkDiskArrayType Failed\n"));
				
				break;
				
			}

			if (previousType == unitDevice->DiskArrayType()) {
				if (NMT_SINGLE == previousType
					|| NMT_CDROM == previousType) {
					
					DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("processDeviceNotification: [kNDASDeviceNotificationCheckBind] Single to Single.\n"));
					
					return;
				} else if (0 == memcmp(&prevRAIDID, getRMDGUID(unitDevice->RMD()), sizeof(GUID))
						   && 0 == memcmp(&prevConfigID, getRMDConfigSetID(unitDevice->RMD()), sizeof(GUID)))  {
					
					DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("processDeviceNotification: [kNDASDeviceNotificationCheckBind] Same RAID.\n"));

					return;
				}
			}
						
			switch (previousType) {
				case NMT_SINGLE:
				case NMT_CDROM:
				{
					DbgIOLog(DEBUG_MASK_NDAS_INFO, ("processDeviceNotification: [kNDASDeviceNotificationCheckBind] Single Disk\n"));
					
					com_ximeta_driver_NDASLogicalDevice	*logicalDevice = NULL;
					
					logicalDevice = OSDynamicCast(com_ximeta_driver_NDASLogicalDevice, unitDevice->UpperUnitDevice());
					
					if (NULL == logicalDevice) {
						DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("processDeviceNotification: No Logical Device!!!\n"));
						
						break;
					} else {
						
						if (logicalDevice->ProtocolTransportNub()) {
							DbgIOLog(DEBUG_MASK_NDAS_INFO, ("processDeviceNotification: Logical device has Protocol transport NUB.\n"));
							
							DbgIOASSERT(detachProtocolTransport(logicalDevice->ProtocolTransportNub()));
						}
						
						if(false == deleteLogicalDevice(logicalDevice)) {
							DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("processDeviceNotification: Can't delete Logical device.\n"));
							
							break;
						}						
					}					
				}
					break;
				case NMT_INVALID:
				{
					break;
				}
				default:
				{
					DbgIOLog(DEBUG_MASK_NDAS_INFO, ("processDeviceNotification: [kNDASDeviceNotificationCheckBind] RAID Disk\n"));
					
					com_ximeta_driver_NDASRAIDUnitDevice	*raidDevice = NULL;
					
					raidDevice = OSDynamicCast(com_ximeta_driver_NDASRAIDUnitDevice, unitDevice->UpperUnitDevice());
					
					if (NULL == raidDevice) {
						DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("processDeviceNotification: No RAID Device!!!\n"));
						
						break;
					}
					
					//
					// Send Command to me ^^
					//
					
					// Create NDAS Command.
					com_ximeta_driver_NDASCommand *command = OSDynamicCast(com_ximeta_driver_NDASCommand, 
																		   OSMetaClass::allocClassWithName(NDAS_COMMAND_CLASS));
					
					if(command == NULL || !command->init()) {
						DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("processDeviceNotification: failed to alloc command class\n"));
						if (command) {
							command->release();
						}
						
						break;
					}
					
					if (NMT_SINGLE == raidDevice->Type()) {
						command->setCommand(kNDASDeviceNotificationNotPresent, raidDevice);
					} else {
						command->setCommand(kNDASDeviceNotificationCheckBind, raidDevice);
					}
					
					enqueueMessage(command);
					
					command->release();						
					
					int countOfSubUnits;
					
					// Delete RAID device.
					if(-1 == (countOfSubUnits = deleteRAIDDevice(unitDevice))) {
						DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("processDeviceNotification: Can't delete RAID device.\n"));
						
						break;
					}
				}
					break;
			}			
		}
			break;
		case kNDASDeviceNotificationDisconnect:
		{
			UInt32	DiskArrayType; 
			
			DiskArrayType = unitDevice->DiskArrayType();
			
			switch (DiskArrayType) {
				case NMT_SINGLE:
				case NMT_CDROM:
				{
					DbgIOLog(DEBUG_MASK_NDAS_INFO, ("processDeviceNotification: Single Disk\n"));
					
					com_ximeta_driver_NDASLogicalDevice	*logicalDevice = NULL;
					
					logicalDevice = OSDynamicCast(com_ximeta_driver_NDASLogicalDevice, unitDevice->UpperUnitDevice());
					
					if (NULL == logicalDevice) {
						DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("processDeviceNotification: No Logical Device!!!\n"));
						
						break;
					} else if (logicalDevice->ProtocolTransportNub()) {
						if(false == detachProtocolTransport(logicalDevice->ProtocolTransportNub())) {
							DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("processDeviceNotification: Can't detach Protocol Transport.\n"));
							
							break;
						}								
					}
					
				}
					break;
				case NMT_INVALID:
				{
					break;
				}
				default:
				{	
					
					com_ximeta_driver_NDASRAIDUnitDevice	*raidDevice = NULL;
					
					raidDevice = OSDynamicCast(com_ximeta_driver_NDASRAIDUnitDevice, unitDevice->UpperUnitDevice());
					
					if (NULL == raidDevice) {
						DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("processDeviceNotification: No RAID Device!!!\n"));
						
						break;
					}
					
					//
					// Send Command to me ^^
					//
					
					// Create NDAS Command.
					com_ximeta_driver_NDASCommand *command = OSDynamicCast(com_ximeta_driver_NDASCommand, 
																		   OSMetaClass::allocClassWithName(NDAS_COMMAND_CLASS));
					
					if(command == NULL || !command->init()) {
						DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("processDeviceNotification: failed to alloc command class\n"));
						if (command) {
							command->release();
						}
						
						break;
					}
					
					command->setCommand(kNDASDeviceNotificationDisconnect, raidDevice);
					
					enqueueMessage(command);
					
					command->release();
				}
					break;
			}	
		}
			break;
		default:
			break;
	}
	
	DbgIOLog(DEBUG_MASK_NDAS_TRACE, ("processDeviceNotification:  Exited.\n"));
	
}
