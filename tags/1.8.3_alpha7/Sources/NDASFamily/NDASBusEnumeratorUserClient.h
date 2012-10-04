/***************************************************************************
*
*  NDASBusEnumeratorUserClient.h
*
*    NDASBusEnumeratorUserClient class definitions
*
*    Copyright (c) 2004 XiMeta, Inc. All rights reserved.
*
**************************************************************************/

#include "NDASBusEnumerator.h"

#define NDASFamilyUserClientClassName com_ximeta_driver_NDASBusEnumeratorUserClient

class NDASFamilyUserClientClassName : public IOUserClient
{
    OSDeclareDefaultStructors(com_ximeta_driver_NDASBusEnumeratorUserClient)
	
protected:
    com_ximeta_driver_NDASBusEnumerator*	fProvider;
    task_t									fTask;
	bool									fCrossEndian;

#if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ <= 1040
//#if MAC_OS_X_VERSION_MIN_REQUIRED <= MAC_OS_X_VERSION_10_4
	static const IOExternalMethod			sMethods[kNDASNumberOfBusEnumeratorMethods];
#else
	static const IOExternalMethodDispatch	sMethods[kNDASNumberOfBusEnumeratorMethods];
#endif
	
public:
	
	// IOUserClient methods
	virtual bool start(IOService * provider);
    virtual void stop(IOService * provider);

	virtual bool initWithTask(task_t owningTask, void * securityToken, UInt32 type);

	virtual IOReturn clientClose(void);
    virtual IOReturn clientDied(void);

	virtual	bool willTerminate(IOService* provider, IOOptionBits options);
	virtual	bool didTerminate(IOService* provider, IOOptionBits options, bool* defer);

	virtual bool terminate(IOOptionBits options = 0);
    virtual bool finalize(IOOptionBits options);

protected:
	
#if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ <= MAC_OS_X_VERSION_10_4
	// Legacy KPI - only supports access from 32-bit user process.
    virtual IOExternalMethod * getTargetAndMethodForIndex(IOService ** target, UInt32 index);
#else
	// KPI for supporting access from both 32-bit and 64bit user process beginning with Mac OS X 10.5
	virtual IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments* arguments,
									IOExternalMethodDispatch* dispatch, OSObject* target, void* reference);
#endif
	
#if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ > MAC_OS_X_VERSION_10_4
	static IOReturn sOpenUserClient(NDASFamilyUserClientClassName* target, void* reference, IOExternalMethodArguments* arguments);
#endif
	virtual IOReturn  openUserClient(void);

#if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ > MAC_OS_X_VERSION_10_4
	static IOReturn sCloseUserClient(NDASFamilyUserClientClassName* target, void* reference, IOExternalMethodArguments* arguments);
#endif	
    virtual IOReturn  closeUserClient(void);

#if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ > MAC_OS_X_VERSION_10_4
	static IOReturn sRegister(NDASFamilyUserClientClassName* target, void* reference, IOExternalMethodArguments* arguments);
	static IOReturn sUnregister(NDASFamilyUserClientClassName* target, void* reference, IOExternalMethodArguments* arguments);
	static IOReturn sEnable(NDASFamilyUserClientClassName* target, void* reference, IOExternalMethodArguments* arguments);
	static IOReturn sDisable(NDASFamilyUserClientClassName* target, void* reference, IOExternalMethodArguments* arguments);
	static IOReturn sEdit(NDASFamilyUserClientClassName* target, void* reference, IOExternalMethodArguments* arguments);
	static IOReturn sRefresh(NDASFamilyUserClientClassName* target, void* reference, IOExternalMethodArguments* arguments);
	static IOReturn sMount(NDASFamilyUserClientClassName* target, void* reference, IOExternalMethodArguments* arguments);
	static IOReturn sUnmount(NDASFamilyUserClientClassName* target, void* reference, IOExternalMethodArguments* arguments);
	static IOReturn sBind(NDASFamilyUserClientClassName* target, void* reference, IOExternalMethodArguments* arguments);
	static IOReturn sUnbind(NDASFamilyUserClientClassName* target, void* reference, IOExternalMethodArguments* arguments);
	static IOReturn sBindEdit(NDASFamilyUserClientClassName* target, void* reference, IOExternalMethodArguments* arguments);
	static IOReturn sRebind(NDASFamilyUserClientClassName* target, void* reference, IOExternalMethodArguments* arguments);
#endif	
	
	virtual IOReturn Register(PNDAS_ENUM_REGISTER			registerData,
							  PNDAS_ENUM_REGISTER_RESULT	result,
							  IOByteCount					inStructSize,
							  IOByteCount					*outStructSize);
	
	virtual IOReturn Unregister(PNDAS_ENUM_UNREGISTER	unregisterData,
								uint32_t					*result,
								IOByteCount				inStructSize,
								IOByteCount				*outStructSize);
	
	virtual IOReturn Enable(PNDAS_ENUM_ENABLE	enableData,
							uint32_t				*result,
							IOByteCount			inStructSize,
							IOByteCount			*outStructSize);
	
	virtual IOReturn Disable(PNDAS_ENUM_DISABLE	disableData,
							 uint32_t				*result,
							 IOByteCount		inStructSize,
							 IOByteCount		*outStructSize);
	
	virtual IOReturn Edit(PNDAS_ENUM_EDIT	editData,
						  uint32_t			*result,
						  IOByteCount		inStructSize,
						  IOByteCount		*outStructSize);
	
	virtual IOReturn Refresh(PNDAS_ENUM_REFRESH			refreshData,
							 uint32_t					*result,
							 IOByteCount				inStructSize,
							 IOByteCount				*outStructSize);
	
	
	virtual IOReturn Mount(PNDAS_ENUM_MOUNT	mountData,
						   uint32_t			*result,
						   IOByteCount		inStructSize,
						   IOByteCount		*outStructSize);
	
	virtual IOReturn Unmount(PNDAS_ENUM_UNMOUNT	unmountData,
							 uint32_t				*result,
							 IOByteCount		inStructSize,
							 IOByteCount		*outStructSize);
	
	
	virtual IOReturn Bind(PNDAS_ENUM_BIND	bindData,
						  PGUID				result,
						  IOByteCount		inStructSize,
						  IOByteCount		*outStructSize);
	
	virtual IOReturn Unbind(PNDAS_ENUM_UNBIND	unbindData,
							uint32_t				*result,
							IOByteCount		inStructSize,
							IOByteCount		*outStructSize);
	
	virtual IOReturn BindEdit(PNDAS_ENUM_BIND_EDIT	editData,
							  uint32_t				*result,
							  IOByteCount			inStructSize,
							  IOByteCount			*outStructSize);
	
	virtual IOReturn Rebind(PNDAS_ENUM_REBIND	rebindData,
							uint32_t				*result,
							IOByteCount		inStructSize,
							IOByteCount		*outStructSize);
	
	
};
