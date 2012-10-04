/***************************************************************************
*
*  NDASBusEnumerator.h
*
*    NDAS Bus Enumerator
*
*    Copyright (c) 2004 XiMeta, Inc. All rights reserved.
*
**************************************************************************/

#ifndef _NDAS_BUS_ENUMERATOR__H_
#define _NDAS_BUS_ENUMERATOR__H_
/*
// Set up a minimal set of availability macros.
// Use Availability.h for Mac OX 10.6
#ifndef MAC_OS_X_VERSION_MIN_REQUIRED

#ifndef MAC_OS_X_VERSION_10_4
#define MAC_OS_X_VERSION_10_4 1040
#endif

#ifdef __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__
#define MAC_OS_X_VERSION_MIN_REQUIRED __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__
#else
#define MAC_OS_X_VERSION_MIN_REQUIRED 1030
#endif

#endif
*/

#ifndef MAC_OS_X_VERSION_10_4
#define MAC_OS_X_VERSION_10_4 1040
#endif

#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
//#include <kern/lock.h>

#include "NDASBusEnumeratorUserClientTypes.h"
#include "LanScsiProtocol.h"

#include "LanScsiSocket.h"
#include "Utilities.h"

#define	INITIAL_NUMBER_OF_DEVICES	16
#define	NUMBER_OF_INTERFACES		16

#define NDASFamilyClassName			com_ximeta_driver_NDASBusEnumerator

class com_ximeta_driver_NDASDevice;
class com_ximeta_driver_NDASUnitDevice;
class com_ximeta_driver_NDASCommand;
class com_ximeta_driver_NDASPnpCommand;
class com_ximeta_driver_NDASDeviceNotificationCommand;
class com_ximeta_driver_NDASProtocolTransportNub;
class com_ximeta_driver_NDASLogicalDevice;
class com_ximeta_driver_NDASRAIDUnitDevice;
class com_ximeta_driver_NDASUnitDeviceList;

typedef struct _EthernetInterface {
	
	IOService	*fService;
	
	char			fAddress[6];
	uint64_t			fSpeed;
	uint32_t			fStatus;
	
	xi_socket_t		fBroadcastListenSocket;
	
	bool			fRemoved;
	
	bool			fBadNIC;
	
	THREAD_T		fThread;
	
} EthernetInterface, *PEthernetInterface;

class com_ximeta_driver_NDASBusEnumerator : public IOService
{
	OSDeclareDefaultStructors(com_ximeta_driver_NDASBusEnumerator)

private:

	THREAD_T				fWorkerThread;
	bool					fThreadTerminate;
	bool					fWillTerminate;
	semaphore_port_t		fCommandSema;
	
	// Mutex between thread and UserClient.
#if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= 1040
	lck_grp_attr_t			*fCriticalSection_slock_grp_attr;
	lck_grp_t				*fCriticalSection_slock_grp;
	lck_attr_t				*fCriticalSection_slock_attr;
	lck_mtx_t				*fCriticalSectionMutex;
#else 
	mutex_t					*fCriticalSectionMutex;
#endif
	
	IOWorkLoop			*fWorkLoop;
	IOCommandGate		*fCommandGate;
	OSArray				*fCommandArray;

	IOService			*fProvider;
	
	OSOrderedSet		*fDeviceListPtr;
	
	com_ximeta_driver_NDASUnitDeviceList		*fRAIDDeviceList;
	
	com_ximeta_driver_NDASUnitDeviceList		*fLogicalDeviceList;

	IOTimerEventSource	*fInterfaceCheckTimerSourcePtr;
	
//	IOTimerEventSource	*fpowerTimerSourcePtr;
		
//	bool				fIsLpxUp;
	unsigned long		fCurrentPowerState;
	
	EthernetInterface	fInterfaces[NUMBER_OF_INTERFACES];
	
	com_ximeta_driver_NDASDevice* FindNDASDeviceBySlotNumber(uint32_t slotNo);

	// Debugging 
	int					fNumberOfLogicalDevices;
	
protected:
	IOWorkLoop			*cntrlSync;
	
public:

#pragma mark -
#pragma mark == Basic Methods ==
#pragma mark -
	
	virtual bool init(OSDictionary *dictionary = 0);
	virtual void free(void);
	virtual IOService *probe(IOService *provider, SInt32 *score);
	virtual bool start(IOService *provider);
	virtual bool finalize ( IOOptionBits options );
	virtual void stop(IOService *provider);
	virtual IOReturn message(UInt32 type, IOService * provider, void * argument = 0);	
	IOWorkLoop *getWorkLoop();

#pragma mark -
#pragma mark == Message(Command) Management ==
#pragma mark -
	
	IOReturn receiveMsg(void *newRequest, void *, void *, void * );
	bool enqueueMessage(com_ximeta_driver_NDASCommand *command);
	void dequeueMessage(com_ximeta_driver_NDASCommand *command);

#pragma mark -
#pragma mark == User Clinet Requests ==
#pragma mark -
	
#pragma mark === For NDAS Device ===

	IOReturn Register(PNDAS_ENUM_REGISTER			registerData,
					  PNDAS_ENUM_REGISTER_RESULT	result,
					  IOByteCount					inStructSize,
					  IOByteCount					*outStructSize);
	
	IOReturn Unregister(PNDAS_ENUM_UNREGISTER	unregisterData,
						uint32_t					*result,
						IOByteCount				inStructSize,
						IOByteCount				*outStructSize);
	
	IOReturn Enable(PNDAS_ENUM_ENABLE	enableData,
					uint32_t				*result,
					IOByteCount			inStructSize,
					IOByteCount			*outStructSize);
	
	IOReturn Disable(PNDAS_ENUM_DISABLE	disableData,
					 uint32_t				*result,
					 IOByteCount		inStructSize,
					 IOByteCount		*outStructSize);
	
	IOReturn Edit(PNDAS_ENUM_EDIT	editData,
				  uint32_t			*result,
				  IOByteCount		inStructSize,
				  IOByteCount		*outStructSize);
	
	IOReturn Refresh(PNDAS_ENUM_REFRESH			refreshData,
					 uint32_t					*result,
					 IOByteCount				inStructSize,
					 IOByteCount				*outStructSize);


#pragma mark === For Logical Device ===

	IOReturn Mount(PNDAS_ENUM_MOUNT	mountData,
				   uint32_t			*result,
				   IOByteCount		inStructSize,
				   IOByteCount		*outStructSize);
	
	IOReturn Unmount(PNDAS_ENUM_UNMOUNT	unmountData,
					 uint32_t				*result,
					 IOByteCount		inStructSize,
					 IOByteCount		*outStructSize);
	
	
	IOReturn Bind(PNDAS_ENUM_BIND	bindData,
				  PGUID				result,
				  IOByteCount		inStructSize,
				  IOByteCount		*outStructSize);
	
	IOReturn Unbind(PNDAS_ENUM_UNBIND	unbindData,
					uint32_t				*result,
					IOByteCount		inStructSize,
					IOByteCount		*outStructSize);
	
	IOReturn BindEdit(PNDAS_ENUM_BIND_EDIT	editData,
					  uint32_t				*result,
					  IOByteCount			inStructSize,
					  IOByteCount			*outStructSize);
	
	IOReturn Rebind(PNDAS_ENUM_REBIND	rebindData,
					uint32_t				*result,
					IOByteCount		inStructSize,
					IOByteCount		*outStructSize);

#pragma mark -
#pragma mark == Interface checker and Braodcast Reciever ==
#pragma mark -

	void InterfaceCheckTimeOutOccurred(
									   OSObject *owner,
									   IOTimerEventSource *sender);
	
	int	initBroadcastReceiver(int interfaceArrayIndex);
	int releaseBroadcastReceiver(int interfaceArrayIndex);
	
	void broadcastReceiverThread(uint32_t intefaceArrayIndex);

#pragma mark -
#pragma mark == Power Management ==
#pragma mark -	
	
	virtual IOReturn setPowerState (unsigned long powerStateOrdinal, IOService* whatDevice);
/*	
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
		
	void PowerDownOccurred(
							OSObject *owner,
							IOTimerEventSource *sender);
*/
#pragma mark -
#pragma mark == NDAS Device Management ==
#pragma mark -

	com_ximeta_driver_NDASDevice* FindNDASDeviceByAddress(struct sockaddr_lpx *fAddress);
	int32_t FindEmptySlotNumberOfNDASDevice(bool bRegistered);
	bool createNDASDevice(PNDAS_ENUM_REGISTER	registerData, uint32_t status);
	bool deleteNDASDevice(com_ximeta_driver_NDASDevice	*NDASDevicePtr);
	bool enableNDASDevice(com_ximeta_driver_NDASDevice *NDASDevicePtr);
	bool disableNDASDevice(com_ximeta_driver_NDASDevice *NDASDevicePtr);

#pragma mark -
#pragma mark == Logical Device Management ==
#pragma mark -

	com_ximeta_driver_NDASLogicalDevice* FindLogicalDeviceByIndex(uint32_t index);
	bool createLogicalDevice(com_ximeta_driver_NDASUnitDevice *device);
	bool deleteLogicalDevice(com_ximeta_driver_NDASLogicalDevice *logicalDevice);
	
#pragma mark -
#pragma mark == RAID Device Management ==
#pragma mark -
	
	com_ximeta_driver_NDASRAIDUnitDevice* FindRAIDDeviceByIndex(uint32_t index);
	com_ximeta_driver_NDASRAIDUnitDevice* FindRAIDDeviceByGUID(PGUID guid);
	bool createRAIDDevice(com_ximeta_driver_NDASUnitDevice *device);
	int deleteRAIDDevice(com_ximeta_driver_NDASUnitDevice *device);		
	
#pragma mark -
#pragma mark == Device Management ==
#pragma mark -
	
	void deviceManagementThread(void	*argument);
	void processDeviceNotification(com_ximeta_driver_NDASDeviceNotificationCommand *command);
	void processPnpMessage(com_ximeta_driver_NDASPnpCommand *command);

};

typedef struct _BroadcastRecieverArgs {
	com_ximeta_driver_NDASBusEnumerator	*bus;
	int									interfaceArrayIndex;
} BroadcastRecieverArgs, *PBroadcastRecieverArgs;

#endif