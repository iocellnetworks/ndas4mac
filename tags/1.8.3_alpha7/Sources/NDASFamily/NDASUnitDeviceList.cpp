/*
 *  NDASUnitDeviceList.cpp
 *  NDASFamily
 *
 *  Created by 정균 안 on 06. 11. 28.
 *  Copyright 2006 __MyCompanyName__. All rights reserved.
 *
 */

#include <IOKit/IOLib.h>

#include "NDASUnitDeviceList.h"
#include "NDASUnitDevice.h"
#include "Utilities.h"

// REQUIRED! This macro defines the class's constructors, destructors,
// and several other methods I/O Kit requires. Do NOT use super as the
// second parameter. You must use the literal name of the superclass.

#define super   OSObject
OSDefineMetaClassAndStructors(com_ximeta_driver_NDASUnitDeviceList, OSObject)

#define	INITIAL_NUMBER_OF_DEVICES	0

bool com_ximeta_driver_NDASUnitDeviceList::init()
{
	super::init();	

	lock = IOLockAlloc();
	if (!lock) {
		DbgIOLogNC(DEBUG_MASK_NDAS_ERROR, ("init: Can't alloc lock.\n"));

		return false;
	}
	
	list = OSDictionary::withCapacity(INITIAL_NUMBER_OF_DEVICES);
	
	if (!list) {
		DbgIOLogNC(DEBUG_MASK_NDAS_ERROR, ("init: Can't alloc list.\n"));

		return false;
	}
	
	return true;
}

void com_ximeta_driver_NDASUnitDeviceList::free(void)
{	
	if (list) {
		list->flushCollection();
		list->release();
	}
	
	if (lock) {
		IOLockFree(lock);
	}
	
	super::free();
}

SInt com_ximeta_driver_NDASUnitDeviceList::insertUnitDevice(com_ximeta_driver_NDASUnitDevice *device)
{

	char	keyString[256] = { 0 };
	SInt	index = 1;
	
	if(!list) {
		DbgIOLogNC(DEBUG_MASK_NDAS_ERROR, ("insertUnitDevice: List is NULL!!!!\n"));
		
		return -1;
	}
	
	IOLockLock(lock);
	
	do {
	
		mySnprintf(keyString, sizeof(keyString), "%d", index);

		if (!list->getObject(keyString)) {

			if(!insertUnitDevice(index, device)) {
				index = -1;
			}
						
			goto Out;
		}
	
		index++;
	} while (index > 1);	// infinity Loop.
	
	index = -1; // Error
	
Out:

	IOLockUnlock(lock);

	return index;
}

bool com_ximeta_driver_NDASUnitDeviceList::insertUnitDevice(SInt	index, com_ximeta_driver_NDASUnitDevice *device)
{
	char	keyString[256] = { 0 };
	
	mySnprintf(keyString, sizeof(keyString), "%d", index);
	
	if(list->getObject(keyString)) {
		return false;
	} else {
		return list->setObject(keyString, device);
	}
}

bool com_ximeta_driver_NDASUnitDeviceList::removeUnitDevice(SInt	index)
{
	char	keyString[256] = { 0 };
	
	mySnprintf(keyString, sizeof(keyString), "%d", index);
	
	if(list->getObject(keyString)) {
		list->removeObject(keyString);
		
		return true;
	} else {
		return false;
	}
}

com_ximeta_driver_NDASUnitDevice *com_ximeta_driver_NDASUnitDeviceList::getFirstUnitDevice()
{
	com_ximeta_driver_NDASUnitDevice *unitDevice = NULL;
	
	if (list->getCount() == 0) {
		DbgIOLogNC(DEBUG_MASK_USER_INFO, ("[com_ximeta_driver_NDASUnitDeviceList::getFirstUnitDevice] list is empty.\n"));
		return NULL;	
	}
	
	// Search Device List.
	OSCollectionIterator *DeviceListIterator = OSCollectionIterator::withCollection(list);
	if(DeviceListIterator == NULL) {
		DbgIOLogNC(DEBUG_MASK_USER_ERROR, ("Can't Create Iterator.\n"));
		return NULL;
	}
	
	OSObject *metaObject = DeviceListIterator->getNextObject();
	
	DeviceListIterator->release();

	if(metaObject == NULL) {
		DbgIOLogNC(DEBUG_MASK_USER_ERROR, ("No Object.\n"));
		return NULL;
	}
	OSSymbol *key = OSDynamicCast(OSSymbol, metaObject);
	if(key == NULL) {
		DbgIOLogNC(DEBUG_MASK_USER_ERROR, ("Can't Cast to OSSymbol Class.\n"));
		return NULL;
	}
	
	unitDevice = OSDynamicCast(com_ximeta_driver_NDASUnitDevice, list->getObject(key));
	if(unitDevice == NULL) {
		DbgIOLogNC(DEBUG_MASK_USER_ERROR, ("Can't Cast to NDAS Unit Device Class.\n"));
		return NULL;
	}

	return unitDevice;
}

com_ximeta_driver_NDASUnitDevice *com_ximeta_driver_NDASUnitDeviceList::findUnitDeviceByIndex(SInt index)
{
	char	keyString[256] = { 0 };
	com_ximeta_driver_NDASUnitDevice *returnValue;
	
	mySnprintf(keyString, sizeof(keyString), "%d", index);
		
	returnValue = OSDynamicCast(com_ximeta_driver_NDASUnitDevice, list->getObject(keyString));
	
	return returnValue;
}

com_ximeta_driver_NDASUnitDevice *com_ximeta_driver_NDASUnitDeviceList::findUnitDeviceByGUID(PGUID guid)
{
	com_ximeta_driver_NDASUnitDevice	*NDASDevicePtr = NULL;
	
	// Search Device List.
	OSCollectionIterator *DeviceListIterator = OSCollectionIterator::withCollection(list);
	if(DeviceListIterator == NULL) {
		DbgIOLogNC(DEBUG_MASK_USER_ERROR, ("findUnitDeviceByGUID: Can't Create Iterator.\n"));
		return NULL;
	}
	
	OSMetaClassBase	*metaObject;
	
	while((metaObject = DeviceListIterator->getNextObject()) != NULL) { 
		
		OSSymbol *key = OSDynamicCast(OSSymbol, metaObject);
		if(key == NULL) {
			DbgIOLogNC(DEBUG_MASK_USER_ERROR, ("findUnitDeviceByGUID:  Can't Cast to OSSymbol Class.\n"));
			continue;
		}
		
		NDASDevicePtr = OSDynamicCast(com_ximeta_driver_NDASUnitDevice, list->getObject(key));
		if(NDASDevicePtr == NULL) {
			DbgIOLogNC(DEBUG_MASK_USER_ERROR, ("findUnitDeviceByGUID: Can't Cast to NDAS Unit Device Class.\n"));
			continue;
		}
		
		if(0 == memcmp(NDASDevicePtr->ID(), guid, sizeof(GUID))) {
			break;
		}
		
		NDASDevicePtr = NULL;
	}
	
	DeviceListIterator->release();

	return NDASDevicePtr;
}

unsigned int com_ximeta_driver_NDASUnitDeviceList::getCount()
{
	return list->getCount();
}

