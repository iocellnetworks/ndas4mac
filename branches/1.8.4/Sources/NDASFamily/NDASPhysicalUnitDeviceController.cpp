/*
 *  NDASPhysicalUnitDeviceController.cpp
 *  NDASFamily
 *
 *  Created by Jung-gyun Ahn on 09. 07. 02.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#include "NDASPhysicalUnitDeviceController.h"

#include "NDASPhysicalUnitDevice.h"

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

OSDefineMetaClassAndStructors(com_ximeta_driver_NDASPhysicalUnitDeviceController, IOService)

bool com_ximeta_driver_NDASPhysicalUnitDeviceController::init(OSDictionary *dict)
{
	bool res = super::init(dict);
	
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Initializing\n"));
		
    return res;
}

void com_ximeta_driver_NDASPhysicalUnitDeviceController::free(void)
{
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Freeing\n"));
		
	super::free();
}

IOService *com_ximeta_driver_NDASPhysicalUnitDeviceController::probe(IOService *provider, SInt32 *score)
{
    IOService *res = super::probe(provider, score);
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Probing\n"));
    
	return res;
}

bool com_ximeta_driver_NDASPhysicalUnitDeviceController::start(IOService *provider)
{
    bool res = super::start(provider);
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Starting\n"));
	
	fProvider = OSDynamicCast ( com_ximeta_driver_NDASPhysicalUnitDevice, provider );
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
	
	// Power Management.
	PMinit();
	provider->joinPMtree(this);
	registerPowerDriver(this, powerStates, kNumberNDASPowerStates);
	
    return res;
	
exit:
	return false;
}

bool com_ximeta_driver_NDASPhysicalUnitDeviceController::finalize (
													   IOOptionBits options 
													   )
{
	
	DbgIOLog(DEBUG_MASK_PNP_TRACE, ("finalizing.\n"));
		
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

void com_ximeta_driver_NDASPhysicalUnitDeviceController::stop(IOService *provider)
{
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Stopping\n"));

	PMstop();
		
    super::stop(provider);
}

IOReturn 
com_ximeta_driver_NDASPhysicalUnitDeviceController::setPowerState (
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
