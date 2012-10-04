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

#define NDAS_LOGICAL_DEVICE_CLASS	"com_ximeta_driver_NDASLogicalDevice"

class com_ximeta_driver_NDASCommand;
class com_ximeta_driver_NDASProtocolTransportNub;

typedef struct _SCSICommand *PSCSICommand;

class com_ximeta_driver_NDASLogicalDevice : public com_ximeta_driver_NDASUnitDevice
{
	OSDeclareDefaultStructors(com_ximeta_driver_NDASLogicalDevice)
	
	UInt32										fIndex;

	com_ximeta_driver_NDASUnitDevice			*fUnitDevice;
		
	com_ximeta_driver_NDASProtocolTransportNub	*fProtocolTransportNub;
			
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
	virtual UInt32	Type();
	virtual bool	IsWritable();
	virtual bool	readSectorsOnce(UInt64 location, UInt32 sectors, char *buffer);
	virtual bool	writeSectorsOnce(UInt64 location, UInt32 sectors, char *buffer);
	virtual bool	processSRBCommand(com_ximeta_driver_NDASCommand *command);
	virtual void	completeCommand(com_ximeta_driver_NDASCommand *command);
	virtual UInt32	updateStatus();	
	virtual bool	sendNDASFamilyIOMessage(UInt32 type, void * argument = 0, vm_size_t argSize = 0 );

	UInt32	blocksize();
	
	// Logical Device Only.
	UInt32	Index();
	void	setIndex(UInt32 index);
		
	UInt32	maxRequestSectors();
		
	com_ximeta_driver_NDASUnitDevice	*UnitDevice();
	void	setUnitDevice(com_ximeta_driver_NDASUnitDevice	*device);
	
	com_ximeta_driver_NDASProtocolTransportNub *ProtocolTransportNub();
	void	setProtocolTransportNub(com_ximeta_driver_NDASProtocolTransportNub *transport);
	
	bool	isBusy();
		
	void	setBSDName();
			
	void setConfiguration(UInt32 configuration);
	
	bool IsMounted();	
	
	bool	bind(UInt32	type, UInt32 sequence, UInt32	NumberOfUnitDevices, com_ximeta_driver_NDASLogicalDevice *LogicalDevices[], NDAS_uuid_t raidID, NDAS_uuid_t configID, bool Sync);
	
	UInt32	NRRWHosts();
	UInt32	NRROHosts();
		
};