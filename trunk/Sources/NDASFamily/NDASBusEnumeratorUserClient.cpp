/***************************************************************************
 *
 *  NDASBusEnumeratorUserClient.cpp
 *
 *    
 *
 *    Copyright (c) 2004 XiMeta, Inc. All rights reserved.
 *
 **************************************************************************/


#include <IOKit/IOLib.h>

#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>

#include "NDASBusEnumeratorUserClient.h"

#include "NDASBusEnumerator.h"

#include "Utilities.h"

#define super IOUserClient
OSDefineMetaClassAndStructors(com_ximeta_driver_NDASBusEnumeratorUserClient, IOUserClient)

/***************************************************************************
 *
 *  Public functions
 *
 **************************************************************************/

bool com_ximeta_driver_NDASBusEnumeratorUserClient::start(IOService * provider)
{
    DbgIOLog(DEBUG_MASK_USER_TRACE, ("start()\n"));

    if(!super::start(provider))
        return false;

    fProvider = OSDynamicCast(com_ximeta_driver_NDASBusEnumerator, provider);

    if (!fProvider)
        return false;

    return true;
}


void com_ximeta_driver_NDASBusEnumeratorUserClient::stop(IOService * provider)
{
    DbgIOLog(DEBUG_MASK_USER_TRACE, ("stop()\n"));

	super::stop(provider);
}


IOReturn com_ximeta_driver_NDASBusEnumeratorUserClient::open(void)
{
    DbgIOLog(DEBUG_MASK_USER_TRACE, ("open()\n"));

    // If we don't have an fProvider, this user client isn't going to do much, so return kIOReturnNotAttached.
    if (!fProvider)
        return kIOReturnNotAttached;

    // Call fProvider->open, and if it fails, it means someone else has already opened the device.
    if (!fProvider->open(this))
        return kIOReturnExclusiveAccess;

    return kIOReturnSuccess;
}


IOReturn com_ximeta_driver_NDASBusEnumeratorUserClient::close(void)
{
    DbgIOLog(DEBUG_MASK_USER_TRACE, ("close()\n"));

    // If we don't have an fProvider, then we can't really call the fProvider's close() function, so just return.
    if (!fProvider)
        return kIOReturnNotAttached;

    // Make sure the fProvider is open before we tell it to close.
    if (fProvider->isOpen(this))
        fProvider->close(this);

    fProvider = NULL;

    return kIOReturnSuccess;
}


IOReturn com_ximeta_driver_NDASBusEnumeratorUserClient::message(UInt32 type, IOService * provider,  void * argument)
{
	DbgIOLog(DEBUG_MASK_PNP_TRACE, ("message: Entered.\n"));

    switch ( type )
    {
        default:
            break;
    }

    return super::message(type, provider, argument);
}


bool com_ximeta_driver_NDASBusEnumeratorUserClient::initWithTask(task_t owningTask, void * securityToken, UInt32 type)
{
    DbgIOLog(DEBUG_MASK_USER_TRACE, ("initWithTask()\n"));

    if (!super::initWithTask(owningTask, securityToken , type))
        return false;

    if (!owningTask)
        return false;

    fTask = owningTask;
    fProvider = NULL;
    fDead = false;

    return true;
}

bool com_ximeta_driver_NDASBusEnumeratorUserClient::finalize(IOOptionBits options)
{
    bool ret;

    DbgIOLog(DEBUG_MASK_USER_TRACE, ("finalize()\n"));

    ret = super::finalize(options);

    return ret;
}


IOExternalMethod *com_ximeta_driver_NDASBusEnumeratorUserClient::getTargetAndMethodForIndex(IOService ** target, UInt32 index)
{
    DbgIOLog(DEBUG_MASK_USER_TRACE, ("getTargetAndMethodForIndex(index = %d)\n", (int)index));
	
    static const IOExternalMethod sMethods[kNDASNumberOfBusEnumeratorMethods] =
    {
    {   // kMyUserClientOpen
        NULL,						 // The IOService * will be determined at runtime below.
        (IOMethod) &com_ximeta_driver_NDASBusEnumeratorUserClient::open,	 // Method pointer.
        kIOUCScalarIScalarO,		       		 // Scalar Input, Scalar Output.
        0,							 // No scalar input values.
        0							 // No scalar output values.
    },
    {   // kMyUserClientClose
        NULL,						 // The IOService * will be determined at runtime below.
        (IOMethod) &com_ximeta_driver_NDASBusEnumeratorUserClient::close, // Method pointer.
        kIOUCScalarIScalarO,				 // Scalar Input, Scalar Output.
        0,							 // No scalar input values.
        0							 // No scalar output values.
    },
	{   // kMyScalarIStructImethod
        NULL,						// The IOService * will be determined at runtime below.
        (IOMethod) &com_ximeta_driver_NDASBusEnumerator::Register,	// Method pointer.
        kIOUCStructIStructO,				// Struct Input, Struct Output.
        sizeof(NDAS_ENUM_REGISTER),			// The size of the input struct.
        sizeof(NDAS_ENUM_REGISTER_RESULT)	// The size of the output struct.
    },
    {   // kMyScalarIStructImethod
        NULL,						// The IOService * will be determined at runtime below.
        (IOMethod) &com_ximeta_driver_NDASBusEnumerator::Unregister,	// Method pointer.
        kIOUCStructIStructO,				// Scalar Input, Struct Input.
        sizeof(NDAS_ENUM_UNREGISTER),		// One scalar input value.
        sizeof(UInt32)				// The size of the input struct.
    },
	{   // kMyScalarIStructImethod
        NULL,						// The IOService * will be determined at runtime below.
        (IOMethod) &com_ximeta_driver_NDASBusEnumerator::Enable,	// Method pointer.
        kIOUCStructIStructO,				// Scalar Input, Struct Input.
        sizeof(NDAS_ENUM_ENABLE),		// One scalar input value.
        sizeof(UInt32)				// The size of the input struct.
    },
    {   // kMyScalarIStructImethod
        NULL,						// The IOService * will be determined at runtime below.
        (IOMethod) &com_ximeta_driver_NDASBusEnumerator::Disable,	// Method pointer.
        kIOUCStructIStructO,				// Scalar Input, Struct Input.
        sizeof(NDAS_ENUM_DISABLE),		// One scalar input value.
        sizeof(int)				// The size of the input struct.
    },
    {   // kMyScalarIStructImethod
        NULL,						// The IOService * will be determined at runtime below.
        (IOMethod) &com_ximeta_driver_NDASBusEnumerator::Mount,	// Method pointer.
        kIOUCStructIStructO,				// Scalar Input, Struct Input.
        sizeof(NDAS_ENUM_MOUNT),		// One scalar input value.
        sizeof(UInt32)				// The size of the input struct.
    },
    {   // kMyScalarIStructImethod
        NULL,						// The IOService * will be determined at runtime below.
        (IOMethod) &com_ximeta_driver_NDASBusEnumerator::Unmount,	// Method pointer.
        kIOUCStructIStructO,				// Scalar Input, Struct Input.
        sizeof(NDAS_ENUM_UNMOUNT),		// One scalar input value.
        sizeof(UInt32)				// The size of the input struct.
    },
    {   // kMyScalarIStructImethod
        NULL,						// The IOService * will be determined at runtime below.
        (IOMethod) &com_ximeta_driver_NDASBusEnumerator::Edit,	// Method pointer.
        kIOUCStructIStructO,				// Scalar Input, Struct Input.
        sizeof(NDAS_ENUM_EDIT),		// One scalar input value.
        sizeof(UInt32)				// The size of the input struct.
    },
	{   // kMyScalarIStructImethod
        NULL,						// The IOService * will be determined at runtime below.
        (IOMethod) &com_ximeta_driver_NDASBusEnumerator::Refresh,	// Method pointer.
        kIOUCStructIStructO,				// Scalar Input, Struct Input.
        sizeof(NDAS_ENUM_REFRESH),		// One scalar input value.
        sizeof(UInt32)				// The size of the input struct.
    },
	{   // kMyScalarIStructImethod
		NULL,						// The IOService * will be determined at runtime below.
		(IOMethod) &com_ximeta_driver_NDASBusEnumerator::Bind,	// Method pointer.
		kIOUCStructIStructO,				// Scalar Input, Struct Input.
		sizeof(NDAS_ENUM_BIND),		// One scalar input value.
		sizeof(GUID)				// The size of the input struct.
	},
	{   // kMyScalarIStructImethod
        NULL,						// The IOService * will be determined at runtime below.
        (IOMethod) &com_ximeta_driver_NDASBusEnumerator::Unbind,	// Method pointer.
        kIOUCStructIStructO,				// Scalar Input, Struct Input.
        sizeof(NDAS_ENUM_UNBIND),		// One scalar input value.
        sizeof(UInt32)				// The size of the input struct.
    },
	{   // kMyScalarIStructImethod
        NULL,						// The IOService * will be determined at runtime below.
        (IOMethod) &com_ximeta_driver_NDASBusEnumerator::BindEdit,	// Method pointer.
        kIOUCStructIStructO,				// Scalar Input, Struct Input.
        sizeof(NDAS_ENUM_BIND_EDIT),		// One scalar input value.
        sizeof(UInt32)				// The size of the input struct.
    },
	{   // kMyScalarIStructImethod
		NULL,						// The IOService * will be determined at runtime below.
		(IOMethod) &com_ximeta_driver_NDASBusEnumerator::Rebind,	// Method pointer.
		kIOUCStructIStructO,				// Scalar Input, Struct Input.
		sizeof(NDAS_ENUM_REBIND),		// One scalar input value.
		sizeof(UInt32)				// The size of the input struct.
	}
		
		
    };


    // Make sure that the index of the function we're calling actually exists in the function table.
    if (index < (UInt32)kNDASNumberOfBusEnumeratorMethods)
    {
        if (index == kNDASBusEnumeratorUserClientOpen || index == kNDASBusEnumeratorUserClientClose)
        {
            // These methods exist in LanScsiUserClient, so return a pointer to LanScsiUserClient.
            *target = this;
        }
        else
        {
            // These methods exist in SimpleDriver, so return a pointer to LanScsiBus.
            *target = fProvider;
        }

        return (IOExternalMethod *) &sMethods[index];
    }

    return NULL;
}


IOReturn com_ximeta_driver_NDASBusEnumeratorUserClient::clientClose(void)
{

    DbgIOLog(DEBUG_MASK_USER_TRACE, ("clientClose()\n"));

    // release my hold on my parent (if I have one).
    close();

    if (fTask)
        fTask = NULL;

    fProvider = NULL;
    terminate();

    // DONT call super::clientClose, which just returns notSupported

    return kIOReturnSuccess;
}


IOReturn com_ximeta_driver_NDASBusEnumeratorUserClient::clientDied(void)
{
    IOReturn ret = kIOReturnSuccess;

    DbgIOLog(DEBUG_MASK_USER_TRACE, ("clientDied()\n"));

    fDead = true;

    // this just calls clientClose
    ret = super::clientDied();

    return ret;
}


bool com_ximeta_driver_NDASBusEnumeratorUserClient::terminate(IOOptionBits options)
{
    bool ret;

    DbgIOLog(DEBUG_MASK_USER_TRACE, ("terminate()\n"));

    ret = super::terminate(options);

    return ret;
}


