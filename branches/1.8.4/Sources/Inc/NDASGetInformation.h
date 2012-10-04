#ifndef _NDAS_GET_INFORMATION_H_
#define _NDAS_GET_INFORMATION_H_

#include <CoreServices/CoreServices.h>

#include "LanScsiProtocol.h"
#include "LanScsiDiskInformationBlock.h"

typedef struct _NDASUnitInformation {
	
	GUID	deviceID;
	UInt64	unitDiskLocation;

	UInt32	configuration;
	UInt32	status;
	
	UInt32	RWHosts;
	UInt32	ROHosts;
	
	char	model[42];
	char	firmware[10];
	char	serialNumber[22];
	
	UInt8	transferType;
	
	UInt64	capacity;
	
	UInt8	type;
	
	UInt32	diskArrayType;
	
} NDASUnitInformation, *PNDASUnitInformation;

typedef struct _NDASPhysicalDeviceInformation {

	SInt32  slotNumber;
	UInt8	address[6];
	bool	writable;
    UInt32	configuration;
    UInt32	status;
	char	name[256];
	char	deviceIDString[256];
	UInt8	vid;
	bool	autoRegister;
	
	UInt32	hwType;
	UInt32	hwVersion;
	UInt32	slots;
	UInt32	targets;
	UInt32	maxRequestBlocks;
	
	NDASUnitInformation Units[MAX_NR_OF_TARGETS_PER_DEVICE];
	
} NDASPhysicalDeviceInformation, *PNDASPhysicalDeviceInformation;

typedef struct _NDASRAIDDeviceInformation {

	UInt32  index;

	GUID	deviceID;
	UInt64	unitDiskLocation;
	bool	writable;
    UInt32	configuration;
    UInt32	status;
	
	UInt32	RWHosts;
	UInt32	ROHosts;

	UInt32	unitDeviceType;
	UInt32	diskArrayType;
	
	UInt64	capacity;
	
	UInt32	CountOfUnitDevices;
	UInt32	CountOfSpareDevices;
		
	char	name[256];
	
	UInt32	RAIDStatus;
	
	UInt64	unitDiskLocationOfSubUnits[NDAS_MAX_UNITS_IN_V2];
	UInt32	RAIDSubUnitStatus[NDAS_MAX_UNITS_IN_V2];
	UInt32	RAIDSubUnitType[NDAS_MAX_UNITS_IN_V2];
	
	UInt64	RecoveringSector;
	
} NDASRAIDDeviceInformation, *PNDASRAIDDeviceInformation;

typedef struct _NDASLogicalDeviceInformation {
    
	UInt32  index;

	bool	writable;
	
    UInt32	configuration;
    UInt32	status;
	
	GUID	LowerUnitDeviceID;
		
	UInt32	unitDeviceType;
	UInt32	physicalUnitSlotNumber;
	UInt32	physicalUnitUnitNumber;
	
	UInt32	RWHosts;
	UInt32	ROHosts;
	
	char	BSDName[256];

} NDASLogicalDeviceInformation, *PNDASLogicalDeviceInformation;

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus
	
#define NDAS_DEVICE_CLASS_NAME			"com_ximeta_driver_NDASDevice"
#define NDAS_RAID_DEVICE_CLASS_NAME		"com_ximeta_driver_NDASRAIDUnitDevice"
#define NDAS_LOGICAL_DEVICE_CLASS_NAME	"com_ximeta_driver_NDASLogicalDevice"
	
bool NDASGetInformationBySlotNumber(PNDASPhysicalDeviceInformation parameter);
bool NDASGetInformationByAddress(PNDASPhysicalDeviceInformation parameter);
bool NDASGetLogicalDeviceInformationByIndex(PNDASLogicalDeviceInformation parameter);
bool NDASGetLogicalDeviceInformationByLowerUnitDeviceID(PNDASLogicalDeviceInformation parameter);
bool NDASGetRAIDDeviceInformationByIndex(PNDASRAIDDeviceInformation parameter);
bool NDASGetRAIDDeviceInformationByDeviceID(PNDASRAIDDeviceInformation parameter);
bool NDASGetRAIDDeviceInformationByUnitDiskLocation(PNDASRAIDDeviceInformation parameter);
CFMutableArrayRef NDASGetInformationCreateIndexArray(const char *deviceClassName);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif
