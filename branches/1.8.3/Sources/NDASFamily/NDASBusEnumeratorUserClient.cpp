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
#include <IOKit/IOKitKeys.h>

#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>

#include "NDASBusEnumeratorUserClient.h"

#include "Utilities.h"

#define super IOUserClient
OSDefineMetaClassAndStructors(com_ximeta_driver_NDASBusEnumeratorUserClient, IOUserClient)

#if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ <= 1040
//#if MAC_OS_X_VERSION_MIN_REQUIRED <= MAC_OS_X_VERSION_10_4
// User client method dispatch table.
//
// The user client mechanism is designed to allow calls from a user process to be dispatched to
// any IOService-based object in the kernel. Almost always this mechanism is used to dispatch calls to
// either member functions of the user client itself or of the user client's provider. The provider is
//  the driver which the user client is connecting to the user process.
//
const IOExternalMethod NDASFamilyUserClientClassName::sMethods[kNDASNumberOfBusEnumeratorMethods] = {
	
	{   // kNDASBusEnumeratorUserClientOpen
		(IOService*) NULL,	 // The IOService * will be determined at runtime below.
		(IOMethod) &NDASFamilyUserClientClassName::openUserClient,	 // Method pointer.
		kIOUCScalarIScalarO,		       		 // Scalar Input, Scalar Output.
		0,							 // No scalar input values.
		0							 // No scalar output values.
	},
	{   // kNDASBusEnumeratorUserClientClose
		(IOService*) NULL,	 // The IOService * will be determined at runtime below.
		(IOMethod) &NDASFamilyUserClientClassName::closeUserClient, // Method pointer.
		kIOUCScalarIScalarO,				 // Scalar Input, Scalar Output.
		0,							 // No scalar input values.
		0							 // No scalar output values.
	},
	{   // kNDASBusEnumeratorRegister
		(IOService*) NULL,	 // The IOService * will be determined at runtime below.
		(IOMethod) &NDASFamilyUserClientClassName::Register,	// Method pointer.
		kIOUCStructIStructO,				// Struct Input, Struct Output.
		sizeof(NDAS_ENUM_REGISTER),			// The size of the input struct.
		sizeof(NDAS_ENUM_REGISTER_RESULT)	// The size of the output struct.
	},
	{   // kNDASBusEnumeratorUnregister
		(IOService*) NULL,	 // The IOService * will be determined at runtime below.
		(IOMethod) &NDASFamilyUserClientClassName::Unregister,	// Method pointer.
		kIOUCStructIStructO,				// Scalar Input, Struct Input.
		sizeof(NDAS_ENUM_UNREGISTER),		// One scalar input value.
		sizeof(uint32_t)				// The size of the input struct.
	},
	{   // kNDASBusEnumeratorEnable
		(IOService*) NULL,	 // The IOService * will be determined at runtime below.
		(IOMethod) &NDASFamilyUserClientClassName::Enable,	// Method pointer.
		kIOUCStructIStructO,				// Scalar Input, Struct Input.
		sizeof(NDAS_ENUM_ENABLE),		// One scalar input value.
		sizeof(uint32_t)				// The size of the input struct.
	},
	{   // kNDASBusEnumeratorDisable
		(IOService*) NULL,	 // The IOService * will be determined at runtime below.
		(IOMethod) &NDASFamilyUserClientClassName::Disable,	// Method pointer.
		kIOUCStructIStructO,				// Scalar Input, Struct Input.
		sizeof(NDAS_ENUM_DISABLE),		// One scalar input value.
		sizeof(uint32_t)				// The size of the input struct.
	},
	{   // kNDASBusEnumeratorMount
		(IOService*) NULL,	 // The IOService * will be determined at runtime below.
		(IOMethod) &NDASFamilyUserClientClassName::Mount,	// Method pointer.
		kIOUCStructIStructO,				// Scalar Input, Struct Input.
		sizeof(NDAS_ENUM_MOUNT),		// One scalar input value.
		sizeof(uint32_t)				// The size of the input struct.
	},
	{   // kNDASBusEnumeratorUnmount
		(IOService*) NULL,	 // The IOService * will be determined at runtime below.
		(IOMethod) &NDASFamilyUserClientClassName::Unmount,	// Method pointer.
		kIOUCStructIStructO,				// Scalar Input, Struct Input.
		sizeof(NDAS_ENUM_UNMOUNT),		// One scalar input value.
		sizeof(uint32_t)				// The size of the input struct.
	},
	{   // kNDASBusEnumeratorEdit
		(IOService*) NULL,	 // The IOService * will be determined at runtime below.
		(IOMethod) &NDASFamilyUserClientClassName::Edit,	// Method pointer.
		kIOUCStructIStructO,				// Scalar Input, Struct Input.
		sizeof(NDAS_ENUM_EDIT),		// One scalar input value.
		sizeof(uint32_t)				// The size of the input struct.
	},
	{   // kNDASBusEnumeratorRefresh
		(IOService*) NULL,	 // The IOService * will be determined at runtime below.
		(IOMethod) &NDASFamilyUserClientClassName::Refresh,	// Method pointer.
		kIOUCStructIStructO,				// Scalar Input, Struct Input.
		sizeof(NDAS_ENUM_REFRESH),		// One scalar input value.
		sizeof(uint32_t)				// The size of the input struct.
	},
	{   // kNDASBusEnumeratorBind
		(IOService*) NULL,	 // The IOService * will be determined at runtime below.
		(IOMethod) &NDASFamilyUserClientClassName::Bind,	// Method pointer.
		kIOUCStructIStructO,				// Scalar Input, Struct Input.
		sizeof(NDAS_ENUM_BIND),		// One scalar input value.
		sizeof(GUID)				// The size of the input struct.
	},
	{   // kNDASBusEnumeratorUnbind
		(IOService*) NULL,	 // The IOService * will be determined at runtime below.
		(IOMethod) &NDASFamilyUserClientClassName::Unbind,	// Method pointer.
		kIOUCStructIStructO,				// Scalar Input, Struct Input.
		sizeof(NDAS_ENUM_UNBIND),		// One scalar input value.
		sizeof(uint32_t)				// The size of the input struct.
	},
	{   // kNDASBusEnumeratorBindEdit
		(IOService*) NULL,	 // The IOService * will be determined at runtime below.
		(IOMethod) &NDASFamilyUserClientClassName::BindEdit,	// Method pointer.
		kIOUCStructIStructO,				// Scalar Input, Struct Input.
		sizeof(NDAS_ENUM_BIND_EDIT),		// One scalar input value.
		sizeof(uint32_t)				// The size of the input struct.
	},
	{   // kNDASBusEnumeratorRebind
		(IOService*) NULL,	 // The IOService * will be determined at runtime below.
		(IOMethod) &NDASFamilyUserClientClassName::Rebind,	// Method pointer.
		kIOUCStructIStructO,				// Scalar Input, Struct Input.
		sizeof(NDAS_ENUM_REBIND),		// One scalar input value.
		sizeof(uint32_t)				// The size of the input struct.
	}
		
};

IOExternalMethod *NDASFamilyUserClientClassName::getTargetAndMethodForIndex(IOService ** target, UInt32 index)
{
    DbgIOLog(DEBUG_MASK_USER_TRACE, ("Entered. (%p, %d)\n", target, (uint32_t)index));
	
	// Make sure that the index of the function we're calling actually exists in the function table.
	if (index < (UInt32)kNDASNumberOfBusEnumeratorMethods) {
		if (index == kNDASBusEnumeratorUserClientOpen || index == kNDASBusEnumeratorUserClientClose) {
			*target = this;
		} else {
			*target = this; //fProvider;
		}
		
		return (IOExternalMethod*) &sMethods[index];
	} else {
		*target = NULL;
		return NULL;
	}
}

#else
// This is the technique which supports both 32-bit and 64-bit user processes starting with Mac OS X 10.5.
//
// User client method dispatch table.
//
// The user client mechanism is designed to allow calls from a user process to be dispatched to
// any IOService-based object in the kernel. Almost always this mechanism is used to dispatch calls to
// either member functions of the user client itself or of the user client's provider. The provider is
// the driver which the user client is connecting to the user process.
//
// It is recommended that calls be dispatched to the user client and not directly to the provider driver.
// This allows the user client to perform error checking on the parameters before passing them to the driver.
// It also allows the user client to do any endian-swapping of parameters in the cross-endian case.
// (See ScalarIStructI below for further discussion of this subject.)

const IOExternalMethodDispatch NDASFamilyUserClientClassName::sMethods[kNDASNumberOfBusEnumeratorMethods] = {
	{   // kNDASBusEnumeratorUserClientOpen
		(IOExternalMethodAction) &NDASFamilyUserClientClassName::sOpenUserClient,	// Method pointer.
		0,																		// No scalar input values.
		0,																		// No struct input value.
		0,																		// No scalar output values.
		0																		// No struct output value.
	},
	{   // kNDASBusEnumeratorUserClientOpen
		(IOExternalMethodAction) &NDASFamilyUserClientClassName::sCloseUserClient,	// Method pointer.
		0,																		// No scalar input values.
		0,																		// No struct input value.
		0,																		// No scalar output values.
		0																		// No struct output value.
	},
	{   // kNDASBusEnumeratorRegister
		(IOExternalMethodAction) &NDASFamilyUserClientClassName::sRegister,	// Method pointer.
		0,																		// No scalar input values.
		sizeof(NDAS_ENUM_REGISTER),													// The size of the input struct.
		0,																		// No scalar output values.
		sizeof(NDAS_ENUM_REGISTER_RESULT)													// The size of the output struct.
	},
	{   // kNDASBusEnumeratorUnregister
		(IOExternalMethodAction) &NDASFamilyUserClientClassName::sUnregister,	// Method pointer.
		0,																		// No scalar input values.
		sizeof(NDAS_ENUM_UNREGISTER),													// The size of the input struct.
		0,																		// No scalar output values.
		sizeof(uint32_t)													// The size of the output struct.
	},
	{   // kNDASBusEnumeratorEnable
		(IOExternalMethodAction) &NDASFamilyUserClientClassName::sEnable,	// Method pointer.
		0,																		// No scalar input values.
		sizeof(NDAS_ENUM_ENABLE),													// The size of the input struct.
		0,																		// No scalar output values.
		sizeof(uint32_t)													// The size of the output struct.
	},
	{   // kNDASBusEnumeratorDisable
		(IOExternalMethodAction) &NDASFamilyUserClientClassName::sDisable,	// Method pointer.
		0,																		// No scalar input values.
		sizeof(NDAS_ENUM_DISABLE),													// The size of the input struct.
		0,																		// No scalar output values.
		sizeof(uint32_t)													// The size of the output struct.
	},
	{   // kNDASBusEnumeratorMount
		(IOExternalMethodAction) &NDASFamilyUserClientClassName::sMount,	// Method pointer.
		0,																		// No scalar input values.
		sizeof(NDAS_ENUM_MOUNT),													// The size of the input struct.
		0,																		// No scalar output values.
		sizeof(uint32_t)													// The size of the output struct.
	},
	{   // kNDASBusEnumeratorUnmount
		(IOExternalMethodAction) &NDASFamilyUserClientClassName::sUnmount,	// Method pointer.
		0,																		// No scalar input values.
		sizeof(NDAS_ENUM_UNMOUNT),													// The size of the input struct.
		0,																		// No scalar output values.
		sizeof(uint32_t)													// The size of the output struct.
	},
	{   // kNDASBusEnumeratorEdit
		(IOExternalMethodAction) &NDASFamilyUserClientClassName::sEdit,	// Method pointer.
		0,																		// No scalar input values.
		sizeof(NDAS_ENUM_EDIT),													// The size of the input struct.
		0,																		// No scalar output values.
		sizeof(uint32_t)													// The size of the output struct.
	},
	{   // kNDASBusEnumeratorRefresh
		(IOExternalMethodAction) &NDASFamilyUserClientClassName::sRefresh,	// Method pointer.
		0,																		// No scalar input values.
		sizeof(NDAS_ENUM_REFRESH),													// The size of the input struct.
		0,																		// No scalar output values.
		sizeof(uint32_t)													// The size of the output struct.
	},
	{   // kNDASBusEnumeratorRegister
		(IOExternalMethodAction) &NDASFamilyUserClientClassName::sBind,	// Method pointer.
		0,																		// No scalar input values.
		sizeof(NDAS_ENUM_BIND),													// The size of the input struct.
		0,																		// No scalar output values.
		sizeof(GUID)													// The size of the output struct.
	},
	{   // kNDASBusEnumeratorUnbind
		(IOExternalMethodAction) &NDASFamilyUserClientClassName::sUnbind,	// Method pointer.
		0,																		// No scalar input values.
		sizeof(NDAS_ENUM_UNBIND),													// The size of the input struct.
		0,																		// No scalar output values.
		sizeof(uint32_t)													// The size of the output struct.
	},
	{   // kNDASBusEnumeratorBindEdit
		(IOExternalMethodAction) &NDASFamilyUserClientClassName::sBindEdit,	// Method pointer.
		0,																		// No scalar input values.
		sizeof(NDAS_ENUM_BIND_EDIT),													// The size of the input struct.
		0,																		// No scalar output values.
		sizeof(uint32_t)													// The size of the output struct.
	},
	{   // kNDASBusEnumeratorRebind
		(IOExternalMethodAction) &NDASFamilyUserClientClassName::sRebind,	// Method pointer.
		0,																		// No scalar input values.
		sizeof(NDAS_ENUM_REBIND),													// The size of the input struct.
		0,																		// No scalar output values.
		sizeof(uint32_t)													// The size of the output struct.
	}
	
};

IOReturn NDASFamilyUserClientClassName::externalMethod(uint32_t selector, IOExternalMethodArguments* arguments,
													   IOExternalMethodDispatch* dispatch, OSObject* target, void* reference)

{
	DbgIOLog(DEBUG_MASK_USER_TRACE, ("Entered. (%p, %d, %p). this %p, fProvider %p\n", target, selector, dispatch, this, fProvider));
	
    if (selector < (uint32_t) kNDASNumberOfBusEnumeratorMethods) {
        dispatch = (IOExternalMethodDispatch *) &sMethods[selector];
        
        if (!target) {
            if (selector == kNDASBusEnumeratorUserClientOpen || selector == kNDASBusEnumeratorUserClientClose) {
				target = this;
			}
			else {
				target = this; //fProvider;
			}
		}
    }

	DbgIOLog(DEBUG_MASK_USER_TRACE, ("Changed. (%p, %d, %p)\n", target, selector, dispatch));

	return super::externalMethod(selector, arguments, dispatch, target, reference);
}

#endif



/***************************************************************************
 *
 *  Public functions
 *
 **************************************************************************/

bool NDASFamilyUserClientClassName::start(IOService * provider)
{
    DbgIOLog(DEBUG_MASK_USER_TRACE, ("Entered.\n"));

    if(!super::start(provider))
        return false;

    fProvider = OSDynamicCast(com_ximeta_driver_NDASBusEnumerator, provider);

    if (!fProvider)
        return false;

    return true;
}


void NDASFamilyUserClientClassName::stop(IOService * provider)
{
    DbgIOLog(DEBUG_MASK_USER_TRACE, ("Entered.\n"));

	super::stop(provider);
}


bool NDASFamilyUserClientClassName::initWithTask(task_t owningTask, void * securityToken, UInt32 type)
{
    DbgIOLog(DEBUG_MASK_USER_TRACE, ("Entered.\n"));

	bool success;
	
    success = super::initWithTask(owningTask, securityToken , type);
/*	
	if (success) {
		// This code will do the right thing on both PowerPC- and Intel-based systems because the cross-endian
		// property will never be set on PowerPC-based Macs.
		fCrossEndian = false;
		
		if (properties != NULL && properties->getObject(kIOUserClientCrossEndianKey)) {
			// A connection to this user client is being opend by a user process running using Rosetta.
			
			// Indicate that this user client can handle being called from cross-endianuser processes by
			// setting its IOUserClientCrossEndianCompatible property in the I/O Registry.
			if (setProperty(kIOUserClientCrossEndianCompatibleKey, kOSBooleanTrue)) {
				fCrossEndian = true;
			}
		}
	}
*/	
    fTask = owningTask;
    fProvider = NULL;

    return true;
}

bool NDASFamilyUserClientClassName::finalize(IOOptionBits options)
{
    bool ret;

    DbgIOLog(DEBUG_MASK_USER_TRACE, ("Entered.\n"));

    ret = super::finalize(options);

    return ret;
}


IOReturn NDASFamilyUserClientClassName::clientClose(void)
{
    DbgIOLog(DEBUG_MASK_USER_TRACE, ("Entered.\n"));

    // release my hold on my parent (if I have one).
    closeUserClient();

    bool success = terminate();
	if (!success) {
		DbgIOLog(DEBUG_MASK_USER_ERROR, ("teminate() failed.\n"));
	}

    // DONT call super::clientClose, which just returns notSupported

    return kIOReturnSuccess;
}


IOReturn NDASFamilyUserClientClassName::clientDied(void)
{
    IOReturn ret = kIOReturnSuccess;

    DbgIOLog(DEBUG_MASK_USER_TRACE, ("Entered.\n"));

    // this just calls clientClose
    ret = super::clientDied();

    return ret;
}

bool NDASFamilyUserClientClassName::willTerminate(IOService* provider, IOOptionBits options)
{
	DbgIOLog(DEBUG_MASK_USER_TRACE, ("Entered.\n"));

	return super::willTerminate(provider, options);
}

bool NDASFamilyUserClientClassName::didTerminate(IOService* provider, IOOptionBits options, bool* defer)
{
    DbgIOLog(DEBUG_MASK_USER_TRACE, ("Entered.\n"));

	closeUserClient();
	*defer = false;
	
	return super::didTerminate(provider, options, defer);
}

bool NDASFamilyUserClientClassName::terminate(IOOptionBits options)
{
    bool ret;

    DbgIOLog(DEBUG_MASK_USER_TRACE, ("Entered.\n"));

    ret = super::terminate(options);

    return ret;
}

#if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ > MAC_OS_X_VERSION_10_4
IOReturn NDASFamilyUserClientClassName::sOpenUserClient(NDASFamilyUserClientClassName* target, void* reference, IOExternalMethodArguments* arguments)
{
    return target->openUserClient();
}
#endif

IOReturn NDASFamilyUserClientClassName::openUserClient(void)
{
	IOReturn	result = kIOReturnSuccess;
	
    DbgIOLog(DEBUG_MASK_USER_ERROR, ("Entered. %d %d\n", sizeof(NDAS_ENUM_REGISTER), sizeof(struct sockaddr_lpx)));
	
    if (!fProvider || isInactive()) {
		// Return an error if we don't have a provider. This could happen if the user process
		// called openUserClient without calling IOServiceOpen first. Or, the user client could be
		// in the process of being terminated and is thus inactive.
		result = kIOReturnNotAttached;
	} else if (!fProvider->open(this)) {
		// The most common reason this open call will fail is because the provider is already open
		// and it doesn't support being opend by more than one client at a time
        result = kIOReturnExclusiveAccess;
	}

	DbgIOLog(DEBUG_MASK_USER_ERROR, ("Exited. result %d\n", result));

    return result;
}

#if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ > MAC_OS_X_VERSION_10_4
IOReturn NDASFamilyUserClientClassName::sCloseUserClient(NDASFamilyUserClientClassName* target, void* reference, IOExternalMethodArguments* arguments)
{
    return target->closeUserClient();
}
#endif

IOReturn NDASFamilyUserClientClassName::closeUserClient(void)
{
	IOReturn	result = kIOReturnSuccess;
	
    DbgIOLog(DEBUG_MASK_USER_TRACE, ("Entered.\n"));
	
    // If we don't have an fProvider, then we can't really call the fProvider's close() function, so just return.
    if (!fProvider) {
        result = kIOReturnNotAttached;
		DbgIOLog(DEBUG_MASK_USER_ERROR, ("returning kIOReturnNotAttached.\n"));
	} else if (fProvider->isOpen(this)) {
		// Make sure we're the one who opend our provider before we tell it to close.
        fProvider->close(this);
	} else {
		result = kIOReturnNotOpen;
		DbgIOLog(DEBUG_MASK_USER_ERROR, ("returning kIOReturnNotOpen.\n"));
	}
	
	return result;
}

// kNDASEnumRegister
#if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ > MAC_OS_X_VERSION_10_4

IOReturn NDASFamilyUserClientClassName::sRegister(NDASFamilyUserClientClassName* target, void* reference, IOExternalMethodArguments* arguments)
{
	DbgIOLogNC(DEBUG_MASK_USER_TRACE, ("sRegister: Entered. target %p\n", target));
		
	return target->Register((PNDAS_ENUM_REGISTER) arguments->structureInput,
							(PNDAS_ENUM_REGISTER_RESULT) arguments->structureOutput,
							(IOByteCount) arguments->structureInputSize,
							(IOByteCount*) &arguments->structureOutputSize);
}

// kNDASEnumUnregister
IOReturn NDASFamilyUserClientClassName::sUnregister(NDASFamilyUserClientClassName* target, void* reference, IOExternalMethodArguments* arguments)
{
	return target->Unregister((PNDAS_ENUM_UNREGISTER) arguments->structureInput,
							  (uint32_t*) arguments->structureOutput,
							  (IOByteCount) arguments->structureInputSize,
							  (IOByteCount*) &arguments->structureOutputSize);
}

// kNDASEnumEnable
IOReturn NDASFamilyUserClientClassName::sEnable(NDASFamilyUserClientClassName* target, void* reference, IOExternalMethodArguments* arguments)
{
	return target->Enable((PNDAS_ENUM_ENABLE) arguments->structureInput,
						  (uint32_t*) arguments->structureOutput,
						  (IOByteCount) arguments->structureInputSize,
						  (IOByteCount*) &arguments->structureOutputSize);
}

// kNDASEnumDisable
IOReturn NDASFamilyUserClientClassName::sDisable(NDASFamilyUserClientClassName* target, void* reference, IOExternalMethodArguments* arguments)
{
	return target->Disable((PNDAS_ENUM_DISABLE) arguments->structureInput,
						   (uint32_t*) arguments->structureOutput,
						   (IOByteCount) arguments->structureInputSize,
						   (IOByteCount*) &arguments->structureOutputSize);
}

// kNDASEnumEdit
IOReturn NDASFamilyUserClientClassName::sEdit(NDASFamilyUserClientClassName* target, void* reference, IOExternalMethodArguments* arguments)
{
	return target->Edit((PNDAS_ENUM_EDIT) arguments->structureInput,
						(uint32_t*) arguments->structureOutput,
						(IOByteCount) arguments->structureInputSize,
						(IOByteCount*) &arguments->structureOutputSize);
}

// kNDASEnumRefresh
IOReturn NDASFamilyUserClientClassName::sRefresh(NDASFamilyUserClientClassName* target, void* reference, IOExternalMethodArguments* arguments)
{
	return target->Refresh((PNDAS_ENUM_REFRESH) arguments->structureInput,
						   (uint32_t*) arguments->structureOutput,
						   (IOByteCount) arguments->structureInputSize,
						   (IOByteCount*) &arguments->structureOutputSize);
}

// kNDASEnumMount
IOReturn NDASFamilyUserClientClassName::sMount(NDASFamilyUserClientClassName* target, void* reference, IOExternalMethodArguments* arguments)
{
	return target->Mount((PNDAS_ENUM_MOUNT) arguments->structureInput,
						 (uint32_t*) arguments->structureOutput,
						 (IOByteCount) arguments->structureInputSize,
						 (IOByteCount*) &arguments->structureOutputSize);
}

// kNDASEnumUnmount
IOReturn NDASFamilyUserClientClassName::sUnmount(NDASFamilyUserClientClassName* target, void* reference, IOExternalMethodArguments* arguments)
{
	return target->Unmount((PNDAS_ENUM_UNMOUNT) arguments->structureInput,
						   (uint32_t*) arguments->structureOutput,
						   (IOByteCount) arguments->structureInputSize,
						   (IOByteCount*) &arguments->structureOutputSize);
}

// kNDASEnumBind
IOReturn NDASFamilyUserClientClassName::sBind(NDASFamilyUserClientClassName* target, void* reference, IOExternalMethodArguments* arguments)
{
	return target->Bind((PNDAS_ENUM_BIND) arguments->structureInput,
						(PGUID) arguments->structureOutput,
						(IOByteCount) arguments->structureInputSize,
						(IOByteCount*) &arguments->structureOutputSize);
}

// kNDASEnumUnbind
IOReturn NDASFamilyUserClientClassName::sUnbind(NDASFamilyUserClientClassName* target, void* reference, IOExternalMethodArguments* arguments)
{
	return target->Unbind((PNDAS_ENUM_UNBIND) arguments->structureInput,
						  (uint32_t*) arguments->structureOutput,
						  (IOByteCount) arguments->structureInputSize,
						  (IOByteCount*) &arguments->structureOutputSize);
}

// kNDASEnumBindEdit
IOReturn NDASFamilyUserClientClassName::sBindEdit(NDASFamilyUserClientClassName* target, void* reference, IOExternalMethodArguments* arguments)
{
	return target->BindEdit((PNDAS_ENUM_BIND_EDIT) arguments->structureInput,
							(uint32_t*) arguments->structureOutput,
							(IOByteCount) arguments->structureInputSize,
							(IOByteCount*) &arguments->structureOutputSize);
}

// kNDASEnumUnbind
IOReturn NDASFamilyUserClientClassName::sRebind(NDASFamilyUserClientClassName* target, void* reference, IOExternalMethodArguments* arguments)
{
	return target->Rebind((PNDAS_ENUM_REBIND) arguments->structureInput,
						  (uint32_t*) arguments->structureOutput,
						  (IOByteCount) arguments->structureInputSize,
						  (IOByteCount*) &arguments->structureOutputSize);
}

#endif

IOReturn NDASFamilyUserClientClassName::Register(PNDAS_ENUM_REGISTER			registerData,
						  PNDAS_ENUM_REGISTER_RESULT	registerResult,
						  IOByteCount					inStructSize,
						  IOByteCount					*outStructSize)
{
	IOReturn result;
	
	DbgIOLog(DEBUG_MASK_USER_TRACE, ("Entered.\n"));
		
	if (fProvider == NULL || isInactive()) {
		// Return an error if we don't have a provider. This could happen if the user process
		// called StructIStructO without calling IOServiceOpen first. Or, the user client could be
		// in the process of being terminated and is thus inactive.
		result = kIOReturnNotAttached;
	}
	else if (!fProvider->isOpen(this)) {
		// Return an error if we do not have the driver open. This could happen if the user process
		// did not call openUserClient before calling this function.
		result = kIOReturnNotOpen;
	}
	else {
		result = fProvider->Register(registerData, registerResult, inStructSize, outStructSize);
	}

	return result;
}

IOReturn NDASFamilyUserClientClassName::Unregister(PNDAS_ENUM_UNREGISTER	unregisterData,
							uint32_t					*unregisterResult,
							IOByteCount				inStructSize,
							IOByteCount				*outStructSize)
{
	IOReturn result;
	
	if (fProvider == NULL || isInactive()) {
		// Return an error if we don't have a provider. This could happen if the user process
		// called StructIStructO without calling IOServiceOpen first. Or, the user client could be
		// in the process of being terminated and is thus inactive.
		result = kIOReturnNotAttached;
	}
	else if (!fProvider->isOpen(this)) {
		// Return an error if we do not have the driver open. This could happen if the user process
		// did not call openUserClient before calling this function.
		result = kIOReturnNotOpen;
	}
	else {
		result = fProvider->Unregister(unregisterData, unregisterResult, inStructSize, outStructSize);
	}
	
	return result;
}

IOReturn NDASFamilyUserClientClassName::Enable(PNDAS_ENUM_ENABLE	enableData,
						uint32_t				*enableResult,
						IOByteCount			inStructSize,
						IOByteCount			*outStructSize)
{
	IOReturn result;
	
	if (fProvider == NULL || isInactive()) {
		// Return an error if we don't have a provider. This could happen if the user process
		// called StructIStructO without calling IOServiceOpen first. Or, the user client could be
		// in the process of being terminated and is thus inactive.
		result = kIOReturnNotAttached;
	}
	else if (!fProvider->isOpen(this)) {
		// Return an error if we do not have the driver open. This could happen if the user process
		// did not call openUserClient before calling this function.
		result = kIOReturnNotOpen;
	}
	else {
		result = fProvider->Enable(enableData, enableResult, inStructSize, outStructSize);
	}
	
	return result;
}

IOReturn NDASFamilyUserClientClassName::Disable(PNDAS_ENUM_DISABLE	disableData,
						 uint32_t				*disableResult,
						 IOByteCount		inStructSize,
						 IOByteCount		*outStructSize)
{
	IOReturn result;
	
	if (fProvider == NULL || isInactive()) {
		// Return an error if we don't have a provider. This could happen if the user process
		// called StructIStructO without calling IOServiceOpen first. Or, the user client could be
		// in the process of being terminated and is thus inactive.
		result = kIOReturnNotAttached;
	}
	else if (!fProvider->isOpen(this)) {
		// Return an error if we do not have the driver open. This could happen if the user process
		// did not call openUserClient before calling this function.
		result = kIOReturnNotOpen;
	}
	else {
		result = fProvider->Disable(disableData, disableResult, inStructSize, outStructSize);
	}
	
	return result;
}

IOReturn NDASFamilyUserClientClassName::Edit(PNDAS_ENUM_EDIT	editData,
					  uint32_t			*editResult,
					  IOByteCount		inStructSize,
					  IOByteCount		*outStructSize)
{
	IOReturn result;
	
	if (fProvider == NULL || isInactive()) {
		// Return an error if we don't have a provider. This could happen if the user process
		// called StructIStructO without calling IOServiceOpen first. Or, the user client could be
		// in the process of being terminated and is thus inactive.
		result = kIOReturnNotAttached;
	}
	else if (!fProvider->isOpen(this)) {
		// Return an error if we do not have the driver open. This could happen if the user process
		// did not call openUserClient before calling this function.
		result = kIOReturnNotOpen;
	}
	else {
		result = fProvider->Edit(editData, editResult, inStructSize, outStructSize);
	}
	
	return result;
}

IOReturn NDASFamilyUserClientClassName::Refresh(PNDAS_ENUM_REFRESH			refreshData,
						 uint32_t					*refreshResult,
						 IOByteCount				inStructSize,
						 IOByteCount				*outStructSize)
{
	IOReturn result;
	
	if (fProvider == NULL || isInactive()) {
		// Return an error if we don't have a provider. This could happen if the user process
		// called StructIStructO without calling IOServiceOpen first. Or, the user client could be
		// in the process of being terminated and is thus inactive.
		result = kIOReturnNotAttached;
	}
	else if (!fProvider->isOpen(this)) {
		// Return an error if we do not have the driver open. This could happen if the user process
		// did not call openUserClient before calling this function.
		result = kIOReturnNotOpen;
	}
	else {
		result = fProvider->Refresh(refreshData, refreshResult, inStructSize, outStructSize);
	}
	
	return result;
}	

IOReturn NDASFamilyUserClientClassName::Mount(PNDAS_ENUM_MOUNT	mountData,
					   uint32_t			*mountResult,
					   IOByteCount		inStructSize,
					   IOByteCount		*outStructSize)
{
	IOReturn result;
	
	if (fProvider == NULL || isInactive()) {
		// Return an error if we don't have a provider. This could happen if the user process
		// called StructIStructO without calling IOServiceOpen first. Or, the user client could be
		// in the process of being terminated and is thus inactive.
		result = kIOReturnNotAttached;
	}
	else if (!fProvider->isOpen(this)) {
		// Return an error if we do not have the driver open. This could happen if the user process
		// did not call openUserClient before calling this function.
		result = kIOReturnNotOpen;
	}
	else {
		result = fProvider->Mount(mountData, mountResult, inStructSize, outStructSize);
	}
	
	return result;

}

IOReturn NDASFamilyUserClientClassName::Unmount(PNDAS_ENUM_UNMOUNT	unmountData,
						 uint32_t				*unmountResult,
						 IOByteCount		inStructSize,
						 IOByteCount		*outStructSize)
{
	IOReturn result;
	
	if (fProvider == NULL || isInactive()) {
		// Return an error if we don't have a provider. This could happen if the user process
		// called StructIStructO without calling IOServiceOpen first. Or, the user client could be
		// in the process of being terminated and is thus inactive.
		result = kIOReturnNotAttached;
	}
	else if (!fProvider->isOpen(this)) {
		// Return an error if we do not have the driver open. This could happen if the user process
		// did not call openUserClient before calling this function.
		result = kIOReturnNotOpen;
	}
	else {
		result = fProvider->Unmount(unmountData, unmountResult, inStructSize, outStructSize);
	}
	
	return result;
}

IOReturn NDASFamilyUserClientClassName::Bind(PNDAS_ENUM_BIND	bindData,
					  PGUID				bindResult,
					  IOByteCount		inStructSize,
					  IOByteCount		*outStructSize)
{
	IOReturn result;
	
	if (fProvider == NULL || isInactive()) {
		// Return an error if we don't have a provider. This could happen if the user process
		// called StructIStructO without calling IOServiceOpen first. Or, the user client could be
		// in the process of being terminated and is thus inactive.
		result = kIOReturnNotAttached;
	}
	else if (!fProvider->isOpen(this)) {
		// Return an error if we do not have the driver open. This could happen if the user process
		// did not call openUserClient before calling this function.
		result = kIOReturnNotOpen;
	}
	else {
		result = fProvider->Bind(bindData, bindResult, inStructSize, outStructSize);
	}
	
	return result;
}	

IOReturn NDASFamilyUserClientClassName::Unbind(PNDAS_ENUM_UNBIND	unbindData,
						uint32_t				*unbindResult,
						IOByteCount		inStructSize,
						IOByteCount		*outStructSize)
{
	IOReturn result;
	
	if (fProvider == NULL || isInactive()) {
		// Return an error if we don't have a provider. This could happen if the user process
		// called StructIStructO without calling IOServiceOpen first. Or, the user client could be
		// in the process of being terminated and is thus inactive.
		result = kIOReturnNotAttached;
	}
	else if (!fProvider->isOpen(this)) {
		// Return an error if we do not have the driver open. This could happen if the user process
		// did not call openUserClient before calling this function.
		result = kIOReturnNotOpen;
	}
	else {
		result = fProvider->Unbind(unbindData, unbindResult, inStructSize, outStructSize);
	}
	
	return result;
}

IOReturn NDASFamilyUserClientClassName::BindEdit(PNDAS_ENUM_BIND_EDIT	bindEditData,
						  uint32_t				*bindEditresult,
						  IOByteCount			inStructSize,
						  IOByteCount			*outStructSize)
{
	IOReturn result;
	
	if (fProvider == NULL || isInactive()) {
		// Return an error if we don't have a provider. This could happen if the user process
		// called StructIStructO without calling IOServiceOpen first. Or, the user client could be
		// in the process of being terminated and is thus inactive.
		result = kIOReturnNotAttached;
	}
	else if (!fProvider->isOpen(this)) {
		// Return an error if we do not have the driver open. This could happen if the user process
		// did not call openUserClient before calling this function.
		result = kIOReturnNotOpen;
	}
	else {
		result = fProvider->BindEdit(bindEditData, bindEditresult, inStructSize, outStructSize);
	}
	
	return result;
}

IOReturn NDASFamilyUserClientClassName::Rebind(PNDAS_ENUM_REBIND	rebindData,
						uint32_t				*rebindResult,
						IOByteCount		inStructSize,
						IOByteCount		*outStructSize)
{
	IOReturn result;
	
	if (fProvider == NULL || isInactive()) {
		// Return an error if we don't have a provider. This could happen if the user process
		// called StructIStructO without calling IOServiceOpen first. Or, the user client could be
		// in the process of being terminated and is thus inactive.
		result = kIOReturnNotAttached;
	}
	else if (!fProvider->isOpen(this)) {
		// Return an error if we do not have the driver open. This could happen if the user process
		// did not call openUserClient before calling this function.
		result = kIOReturnNotOpen;
	}
	else {
		result = fProvider->Rebind(rebindData, rebindResult, inStructSize, outStructSize);
	}
	
	return result;
}	
