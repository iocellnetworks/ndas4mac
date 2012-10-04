/***************************************************************************
*
*  NDASBusEnumeratorUserClient.h
*
*    NDASBusEnumeratorUserClient class definitions
*
*    Copyright (c) 2004 XiMeta, Inc. All rights reserved.
*
**************************************************************************/

class com_ximeta_driver_NDASBusEnumeratorUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(com_ximeta_driver_NDASBusEnumeratorUserClient)
	
protected:
    IOService *			fProvider;
    task_t				fTask;
    bool				fDead;
	
public:
	
	// IOUserClient methods
	virtual bool start(IOService * provider);
    virtual void stop(IOService * provider);
    virtual IOReturn  open(void);
    virtual IOReturn  close(void);
	
    virtual IOReturn message(UInt32 type, IOService * provider,  void * argument = 0);

    virtual bool initWithTask(task_t owningTask, void * securityToken, UInt32 type);
    virtual bool finalize(IOOptionBits options);
	
    virtual IOExternalMethod * getTargetAndMethodForIndex(IOService ** target, UInt32 index);
    virtual IOReturn clientClose(void);
    virtual IOReturn clientDied(void);
    virtual bool terminate(IOOptionBits options = 0);
};
