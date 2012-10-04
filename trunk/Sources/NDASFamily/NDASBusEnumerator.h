/***************************************************************************
*
*  NDASBusEnumerator.h
*
*    NDAS Bus Enumerator
*
*    Copyright (c) 2004 XiMeta, Inc. All rights reserved.
*
**************************************************************************/

#include <IOKit/IOService.h>

#include "NDASBusEnumeratorUserClientTypes.h"
#include "LanScsiProtocol.h"

#include "LanScsiSocket.h"

#define	INITIAL_NUMBER_OF_DEVICES	16
#define	NUMBER_OF_INTERFACES		4

class com_ximeta_driver_NDASDevice;
class com_ximeta_driver_NDASUnitDevice;
class com_ximeta_driver_NDASCommand;
class com_ximeta_driver_NDASProtocolTransportNub;
class com_ximeta_driver_NDASLogicalDevice;
class com_ximeta_driver_NDASRAIDUnitDevice;
class com_ximeta_driver_NDASUnitDeviceList;

typedef struct _EthernetInterface {
	
	IOService	*fService;
	
	char			fAddress[6];
	UInt64			fSpeed;
	UInt32			fStatus;
	
	xi_socket_t		fBroadcastListenSocket;
	
	bool			fRemoved;
	
	bool			fBadNIC;
	
	IOThread		fThread;
	
} EthernetInterface, *PEthernetInterface;

class com_ximeta_driver_NDASBusEnumerator : public IOService
{
	OSDeclareDefaultStructors(com_ximeta_driver_NDASBusEnumerator)

private:

	IOThread				fWorkerThread;
	bool					fThreadTerminate;
	bool					fWillTerminate;
	semaphore_port_t		fCommandSema;
	
	IOWorkLoop			*fWorkLoop;
	IOCommandGate		*fCommandGate;
	OSArray				*fCommandArray;

	IOService			*fProvider;
	
	OSOrderedSet		*fDeviceListPtr;
	
	com_ximeta_driver_NDASUnitDeviceList		*fRAIDDeviceList;
	
	com_ximeta_driver_NDASUnitDeviceList		*fLogicalDeviceList;

	IOTimerEventSource	*fInterfaceCheckTimerSourcePtr;
	
	IOTimerEventSource	*fpowerTimerSourcePtr;
		
//	bool				fIsLpxUp;
	unsigned long		fCurrentPowerState;
	
	EthernetInterface	fInterfaces[NUMBER_OF_INTERFACES];
	
	com_ximeta_driver_NDASDevice* FindDeviceBySlotNumber(UInt32 slotNo);

	// Debugging 
	int					fNumberOfLogicalDevices;
	
protected:
	IOWorkLoop			*cntrlSync;
	
public:
	virtual bool init(OSDictionary *dictionary = 0);
	virtual void free(void);
	virtual IOService *probe(IOService *provider, SInt32 *score);
	virtual bool start(IOService *provider);
	virtual bool finalize ( IOOptionBits options );
	virtual void stop(IOService *provider);
	virtual IOReturn message(UInt32 type, IOService * provider, void * argument = 0);
	
	IOWorkLoop *getWorkLoop();
		
	virtual IOReturn setPowerState (unsigned long powerStateOrdinal, IOService* whatDevice);
	
	virtual IOReturn powerStateWillChangeTo (
											 IOPMPowerFlags  capabilities,
											 unsigned long   stateNumber,
											 IOService*		whatDevice
											 );
	
	virtual IOReturn powerStateDidChangeTo (
											 IOPMPowerFlags  capabilities,
											 unsigned long   stateNumber,
											 IOService*		whatDevice
											 );
	
	//
	// For Broadcast reciver.
	//
	void InterfaceCheckTimeOutOccurred(
							OSObject *owner,
							IOTimerEventSource *sender);
	
	int	initBroadcastReceiver(int interfaceArrayIndex);
	int releaseBroadcastReceiver(int interfaceArrayIndex);
	
	void broadcastReceiverThread(UInt32 intefaceArrayIndex);
	
	void PowerDownOccurred(
							OSObject *owner,
							IOTimerEventSource *sender);
		
	//
	// For User client.
	//
	IOReturn Register(PNDAS_ENUM_REGISTER			registerData,
					  PNDAS_ENUM_REGISTER_RESULT	result,
					  IOByteCount					inStructSize,
					  IOByteCount					*outStructSize);
	
	IOReturn Unregister(PNDAS_ENUM_UNREGISTER	unregisterData,
						UInt32					*result,
						IOByteCount				inStructSize,
						IOByteCount				*outStructSize);
	
	IOReturn Enable(PNDAS_ENUM_ENABLE	enableData,
					UInt32				*result,
					IOByteCount			inStructSize,
					IOByteCount			*outStructSize);
	
	IOReturn Disable(PNDAS_ENUM_DISABLE	disableData,
					 UInt32				*result,
					 IOByteCount		inStructSize,
					 IOByteCount		*outStructSize);
	
	IOReturn Mount(PNDAS_ENUM_MOUNT	mountData,
				   UInt32			*result,
				   IOByteCount		inStructSize,
				   IOByteCount		*outStructSize);
	
	IOReturn Unmount(PNDAS_ENUM_UNMOUNT	unmountData,
					 UInt32				*result,
					 IOByteCount		inStructSize,
					 IOByteCount		*outStructSize);
	
	IOReturn Edit(PNDAS_ENUM_EDIT	editData,
				  UInt32			*result,
				  IOByteCount		inStructSize,
				  IOByteCount		*outStructSize);
	
	IOReturn Refresh(PNDAS_ENUM_REFRESH			refreshData,
					 UInt32					*result,
					 IOByteCount				inStructSize,
					 IOByteCount				*outStructSize);
	
	IOReturn Bind(PNDAS_ENUM_BIND	bindData,
				  PGUID				result,
				  IOByteCount		inStructSize,
				  IOByteCount		*outStructSize);
	
	IOReturn Unbind(PNDAS_ENUM_UNBIND	unbindData,
					UInt32				*result,
					IOByteCount		inStructSize,
					IOByteCount		*outStructSize);
	
	IOReturn BindEdit(PNDAS_ENUM_BIND_EDIT	editData,
					  UInt32				*result,
					  IOByteCount			inStructSize,
					  IOByteCount			*outStructSize);
	
	IOReturn Rebind(PNDAS_ENUM_REBIND	rebindData,
					UInt32				*result,
					IOByteCount		inStructSize,
					IOByteCount		*outStructSize);

	IOReturn receiveMsg(void *newRequest, void *, void *, void * );
	bool enqueueMessage(com_ximeta_driver_NDASCommand *command);
	void dequeueMessage(com_ximeta_driver_NDASCommand *command);
	
	void deviceManagementThread(void	*argument);
	
	void processDeviceNotification(com_ximeta_driver_NDASCommand *command);
	
	com_ximeta_driver_NDASLogicalDevice* FindLogicalDeviceByIndex(UInt32 index);
	bool createLogicalDevice(com_ximeta_driver_NDASUnitDevice *device);
	bool deleteLogicalDevice(com_ximeta_driver_NDASLogicalDevice *logicalDevice);
	
	bool attachProtocolTransport(com_ximeta_driver_NDASLogicalDevice *logicalDevice);
	bool detachProtocolTransport(com_ximeta_driver_NDASProtocolTransportNub *ProtocolTransportNub);
	
	com_ximeta_driver_NDASRAIDUnitDevice* FindRAIDDeviceByIndex(UInt32 index);
	com_ximeta_driver_NDASRAIDUnitDevice* FindRAIDDeviceByGUID(PGUID guid);
	bool createRAIDDevice(com_ximeta_driver_NDASUnitDevice *device);
	int deleteRAIDDevice(com_ximeta_driver_NDASUnitDevice *device);		

	void processPnpMessage(com_ximeta_driver_NDASCommand *command);

};

typedef struct _BroadcastRecieverArgs {
	com_ximeta_driver_NDASBusEnumerator	*bus;
	int									interfaceArrayIndex;
} BroadcastRecieverArgs, *PBroadcastRecieverArgs;

void InterfaceCheckTimerWrapper(OSObject *owner, IOTimerEventSource *sender);
void PowerTimerWrapper(OSObject *owner, IOTimerEventSource *sender);
static IOReturn receiveMsgWrapper( OSObject * theDriver, void *first, void *second, void *third, void *fourth );

void deviceManagementThreadWrapper(void* argument);
