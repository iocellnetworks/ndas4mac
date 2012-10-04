/***************************************************************************
*
*  NDASLogicalDevice.h
*
*    NDAS Logical Device
*
*    Copyright (c) 2004 XiMeta, Inc. All rights reserved.
*
**************************************************************************/

#include <IOKit/IOService.h>

#include "NDASUnitDevice.h"
#include "NDASDeviceNotificationCommand.h"

#define NDAS_LOGICAL_DEVICE_CLASS	"com_ximeta_driver_NDASLogicalDevice"

class com_ximeta_driver_NDASSCSICommand;

typedef struct _SCSICommand *PSCSICommand;

class com_ximeta_driver_NDASLogicalDevice : public com_ximeta_driver_NDASUnitDevice
{
	OSDeclareDefaultStructors(com_ximeta_driver_NDASLogicalDevice)
	
	uint32_t									fIndex;		// Index of Logical Device Array.
	com_ximeta_driver_NDASUnitDevice			*fUnitDevice;	
	OSString									*fBSDName;
	
public:
	
	// IOObject.
	virtual bool init(OSDictionary *dictionary = 0);
	virtual void free(void);
	virtual IOService *probe(IOService *provider, SInt32 *score);
	virtual bool start(IOService *provider);
	virtual void stop(IOService *provider);
	virtual IOReturn message( 
							  UInt32	mess, 
							  IOService	* provider,
							  void		* argument 
							  );

	// Virtual Unit Device.
	virtual uint32_t	Type();
	virtual bool	IsWritable();

	virtual bool	readSectorsOnce(uint64_t location, uint32_t sectors, char *buffer);
	virtual bool	writeSectorsOnce(uint64_t location, uint32_t sectors, char *buffer);
	virtual bool	processSRBCommand(com_ximeta_driver_NDASSCSICommand *command);
	virtual void	completeCommand(com_ximeta_driver_NDASSCSICommand *command);

	virtual uint32_t	updateStatus();	
	virtual bool	sendNDASFamilyIOMessage(UInt32 type, void * argument = 0, vm_size_t argSize = 0 );
	
	// Logical Device Only.
	uint32_t	Index();
	void	setIndex(uint32_t index);
		
	uint32_t	maxRequestSectors();
		
	com_ximeta_driver_NDASUnitDevice	*UnitDevice();
	void	setUnitDevice(com_ximeta_driver_NDASUnitDevice	*device);
		
	bool	isBusy();
	
	OSString *BSDName();
	void	setBSDName(OSString *BSDname);
			
	void setConfiguration(uint32_t configuration);
	
	bool IsMounted();	
	
	bool	bind(uint32_t	type, uint32_t sequence, uint32_t	NumberOfUnitDevices, com_ximeta_driver_NDASLogicalDevice *LogicalDevices[], NDAS_uuid_t raidID, NDAS_uuid_t configID, bool Sync);
	
	uint32_t	NRRWHosts();
	uint32_t	NRROHosts();
	
	// Message From Bus.
	bool	receiveDeviceNotification(kNDASDeviceNotificationType type);
	void	completeDeviceNotificationMessage(com_ximeta_driver_NDASDeviceNotificationCommand *command);
		
};