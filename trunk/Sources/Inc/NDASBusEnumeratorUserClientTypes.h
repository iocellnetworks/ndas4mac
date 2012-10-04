/***************************************************************************
*
*  NDASBusEnumeratorUserClientTypes.h
*
*    NDASBusEnumeratorUserClient class data and structure definitions
*
*    Copyright (c) 2004 XiMeta, Inc. All rights reserved.
*
**************************************************************************/

#ifndef _NDAS_BUS_ENUMERATOR_USER_CLIENT_TYPES_H_
#define _NDAS_BUS_ENUMERATOR_USER_CLIENT_TYPES_H_

#include "lpx.h"
#include "LanScsiDiskInformationBlock.h"

#define DriversIOKitClassName 	"com_ximeta_driver_NDASBusEnumerator"

// these are the User Client method names
enum
{
    kNDASBusEnumeratorUserClientOpen,
    kNDASBusEnumeratorUserClientClose,
	kNDASBusEnumeratorRegister,
	kNDASBusEnumeratorUnregister,
    kNDASBusEnumeratorEnable,
	kNDASBusEnumeratorDisable,
	kNDASBusEnumeratorMount,
    kNDASBusEnumeratorUnmount,
	kNDASBusEnumeratorEdit,
	kNDASBusEnumeratorRefresh,
	// Bind
	kNDASBusEnumeratorBind,
	kNDASBusEnumeratorUnbind,
	kNDASBusEnumeratorBindEdit,
	kNDASBusEnumeratorRebind,
    kNDASNumberOfBusEnumeratorMethods
};

// NDAS Device Status.
enum{
	kNDASStatusOffline,
	kNDASStatusOnline,
	kNDASStatusNoPermission,
	kNDASStatusFailure,
	kNDASNumberOfStatus
};

// NDAS Unit Device Status.
enum {
	kNDASUnitStatusNotPresent,
	kNDASUnitStatusFailure,
	kNDASUnitStatusDisconnected,
	kNDASUnitStatusTryConnectRO,
	kNDASUnitStatusTryConnectRW,
	kNDASUnitStatusConnectedRO,
	kNDASUnitStatusConnectedRW,
	kNDASUnitStatusTryDisconnectRO,
	kNDASUnitStatusTryDisconnectRW,	
	kNDASUnitStatusReconnectRO,
	kNDASUnitStatusReconnectRW,
	kNDASNumberOfUnitStatus
};

// NDAS Device Configuration.
enum {
	kNDASConfigurationDisable,
	kNDASConfigurationEnable,
	kNDASNumberOfConfigurations
};

// NDAS Unit Device Configuration.
enum {
	kNDASUnitConfigurationUnmount,
	kNDASUnitConfigurationMountRO,
	kNDASUnitConfigurationMountRW,
	kNDASNumberOfUnitConfiguration
};

// NDAS Unit Media Type.
enum {
	kNDASUnitMediaTypeHDD,
	kNDASUnitMediaTypeODD,
	kNDASNumberOfType
};

// NDAS Unit Device Type.
enum {
	kNDASUnitDeviceTypeLogical,
	kNDASNumberOfUnitDeviceType
};

// NDAS RAID Sub Unit Device Status.
enum {
	kNDASRAIDSubUnitStatusNotPresent,
	kNDASRAIDSubUnitStatusNDASAddressMismatch,
	kNDASRAIDSubUnitStatusBadRMD,
	kNDASRAIDSubUnitStatusFailure,		// DIB, RMD is good.
	kNDASRAIDSubUnitStatusGood,
	kNDASRAIDSubUnitStatusNeedRecovery,
	kNDASRAIDSubUnitStatusRecovering,
	kNDASNumberOfRAIDSubUnitStatus
};

// NDAS RAID Sub Unit Device Type.
enum {
	kNDASRAIDSubUnitTypeNormal,
	kNDASRAIDSubUnitTypeSpare,
	kNDASRAIDSubUnitTypeReplacedBySpare,
	kNDASNumberOfRAIDSubUnitType
};

// NDAS RAID Status.
enum {
	kNDASRAIDStatusNotReady,
	kNDASRAIDStatusBadRAIDInformation,
	kNDASRAIDStatusBroken,
	kNDASRAIDStatusReadyToRecovery,						// For RAID 1, 4, 5
	kNDASRAIDStatusReadyToRecoveryWithReplacedUnit,		// For RAID 1, 4, 5
	kNDASRAIDStatusRecovering,							// For RAID 1, 4, 5
	kNDASRAIDStatusBrokenButWorkable,					// For RAID 1, 4, 5
	kNDASRAIDStatusGoodButPartiallyFailed,				// For RAID 1, 4, 5
	kNDASRAIDStatusGood,
	kNDASRAIDStatusGoodWithReplacedUnit,				// For RAID 1, 4, 5
	kNDASRAIDStatusUnsupported,
	kNDASNumberOfRAIDStatus
};

#define NDAS_PNPMESSAGE_INTERVAL_TIME					5					// 5 sec. Actual value of NDAS is 2.5 sec

#define NDAS_MAX_WAIT_TIME_FOR_BUS_THREAD				10					// 10 sec.
#define NDAS_MAX_WAIT_TIME_FOR_RAID_THREAD_NORMAL		5					// 5 sec.
#define NDAS_MAX_WAIT_TIME_FOR_RAID_THREAD_RECOVERING	500 * 1000 * 1000  	// 500 ms.
#define NDAS_MAX_WAIT_TIME_FOR_PNP_MESSAGE				10					// 10 sec.
#define NDAS_MAX_WAIT_TIME_FOR_TRY_MOUNT				15					// 15 sec.

#define NDAS_MAX_WAIT_TIME_FOR_RAID_REPLACE				3 * 60				// 3 min.

#define MAX_NDAS_NAME_SIZE								256

// LPX Ports.
#define LPX_PORT_CONNECTION	10000	
#define LPX_PORT_PNP_SEND	10001
#define LPX_PORT_PNP_LISTEN	10002

typedef struct unitDisk {
    unsigned char   ucUnitNumber;
    unsigned 	    ulUnitBlocks;
} UNITDISK, *PUNITDISK;

#define MAX_UNIT	256

typedef struct _NDAS_ENUM_REGISTER
{
	UInt32				SlotNo;
	
	char				UserPassword[8];
	char				SuperPassword[8];
	
	// NDAS Identifier. Mac Address.
	struct sockaddr_lpx	DevAddr;
	char				Name[MAX_NDAS_NAME_SIZE];
	char				DeviceIDString[MAX_NDAS_NAME_SIZE];
	
	bool				hasWriteRight;
	UInt32				Configuration;
	
	uint8_t				VID;
	
} NDAS_ENUM_REGISTER, *PNDAS_ENUM_REGISTER;

typedef struct _NDAS_ENUM_REGISTER_RESULT {
	bool		bDuplicated;
	UInt32		SlotNo;
} NDAS_ENUM_REGISTER_RESULT, *PNDAS_ENUM_REGISTER_RESULT;

typedef struct _NDAS_ENUM_UNREGISTER
{
	UInt32		SlotNo;
	
} NDAS_ENUM_UNREGISTER, *PNDAS_ENUM_UNREGISTER;

typedef struct _NDAS_ENUM_ENABLE
{
	UInt32		SlotNo;
	
} NDAS_ENUM_ENABLE, *PNDAS_ENUM_ENABLE;

typedef struct _NDAS_ENUM_DISABLE
{
	UInt32		SlotNo;
	
} NDAS_ENUM_DISABLE, *PNDAS_ENUM_DISABLE;

typedef struct _NDAS_ENUM_MOUNT
{
	UInt32		LogicalDeviceIndex;

	bool		Writeable;
	
} NDAS_ENUM_MOUNT, *PNDAS_ENUM_MOUNT;

typedef struct _NDAS_ENUM_UNMOUNT
{
	UInt32		LogicalDeviceIndex;
	
} NDAS_ENUM_UNMOUNT, *PNDAS_ENUM_UNMOUNT;

enum {
	kNDASEditTypeWriteAccessRight,
	kNDASEditTypeName,
	kNDASEditTypeRAIDType,
	kNDASNumberOfEditTypes
};

typedef struct _NDAS_ENUM_EDIT
{
	UInt32		SlotNo;
	
	UInt32		EditType;
	
	bool		Writable;
	
	char		Name[MAX_NDAS_NAME_SIZE];
	
} NDAS_ENUM_EDIT, *PNDAS_ENUM_EDIT;

typedef struct _NDAS_ENUM_REFRESH
{
	UInt32		SlotNo;
	
	UInt32		UnitNo;
	
} NDAS_ENUM_REFRESH, *PNDAS_ENUM_REFRESH;

typedef struct _NDAS_ENUM_BIND
{		
	UInt32		RAIDType;
	
	UInt32		NumberofSubUnitDevices;
	
	UInt32		LogicalDeviceIndexOfSubUnitDevices[NDAS_MAX_UNITS_IN_V2];
	
	bool		Sync; // Valid Wehn the RAID Type is RAID1

} NDAS_ENUM_BIND, *PNDAS_ENUM_BIND;

typedef struct _NDAS_ENUM_UNBIND
{
	UInt32		RAIDDeviceIndex;
		
} NDAS_ENUM_UNBIND, *PNDAS_ENUM_UNBIND;

typedef struct _NDAS_ENUM_BIND_EDIT
{
	UInt32		RAIDDeviceIndex;
	
	UInt32		EditType;
	
	char		Name[MAX_NDAS_NAME_SIZE];
	
	UInt32		RAIDType;
	
} NDAS_ENUM_BIND_EDIT, *PNDAS_ENUM_BIND_EDIT;

typedef struct _NDAS_ENUM_REBIND
{
	UInt32		RAIDDeviceIndex;
	
} NDAS_ENUM_REBIND, *PNDAS_ENUM_REBIND;

//
// NDASDevice Property Keys.
//
#define	NDASDEVICE_PROPERTY_KEY_SLOT_NUMBER		"Slot Number"
#define NDASDEVICE_PROPERTY_KEY_ADDRESS			"Address"
#define	NDASDEVICE_PROPERTY_KEY_STATUS			"Status"
#define	NDASDEVICE_PROPERTY_KEY_CONFIGURATION	"Configuration"
#define NDASDEVICE_PROPERTY_KEY_NAME			"Name"
#define NDASDEVICE_PROPERTY_KEY_DEVICEID_STRING "Device ID String"
#define NDASDEVICE_PROPERTY_KEY_WRITE_RIGHT		"Write Access Right"
#define NDASDEVICE_PROPERTY_KEY_HW_TYPE			"HW Type"
#define NDASDEVICE_PROPERTY_KEY_HW_VERSION		"HW Version"
#define NDASDEVICE_PROPERTY_KEY_SLOTS			"Slots"
#define NDASDEVICE_PROPERTY_KEY_TARGETS			"Targets"
#define NDASDEVICE_PROPERTY_KEY_MRB				"Max Reqeust Blocks"
#define NDASDEVICE_PROPERTY_KEY_VID				"VID"

#define NDASDEVICE_PROPERTY_KEY_UNIT_NUMBER				"Unit Number"
#define NDASDEVICE_PROPERTY_KEY_UNIT_STATUS				"Status"
#define NDASDEVICE_PROPERTY_KEY_UNIT_CONFIGURATION		"Configuration"
#define NDASDEVICE_PROPERTY_KEY_UNIT_NR_RW_HOST			"RW Hosts"
#define NDASDEVICE_PROPERTY_KEY_UNIT_NR_RO_HOST			"RO Hosts"
#define NDASDEVICE_PROPERTY_KEY_UNIT_MODEL				"Model"
#define NDASDEVICE_PROPERTY_KEY_UNIT_FIRMWARE			"Firmware"
#define NDASDEVICE_PROPERTY_KEY_UNIT_SERIALNUMBER		"Serial Number"
#define NDASDEVICE_PROPERTY_KEY_UNIT_TRANSFER_MODE		"Transfer Mode"
#define NDASDEVICE_PROPERTY_KEY_UNIT_CAPACITY			"Capacity"
#define NDASDEVICE_PROPERTY_KEY_UNIT_NUMBER_OF_SECTORS	"Number of Sectors"
#define NDASDEVICE_PROPERTY_KEY_UNIT_TYPE				"Type"
#define NDASDEVICE_PROPERTY_KEY_UNIT_DISK_ARRAY_TYPE	"Disk Array Type"
#define	NDASDEVICE_PROPERTY_KEY_UNIT_CONNECTED			"Connected"

#define	NDASDEVICE_PROPERTY_KEY_INDEX						"Index"
#define NDASDEVICE_PROPERTY_KEY_UNIT_DEVICE_TYPE			"Unit Device Type"
#define NDASDEVICE_PROPERTY_KEY_UNIT_DEVICE_ID				"ID of Unit Device"
#define NDASDEVICE_PROPERTY_KEY_UNIT_UNIT_DISK_LOCATION		"Unit Disk Location"

#define NDASDEVICE_PROPERTY_KEY_UNIT_DEVICE_BSD_NAME		"BSD Name"
#define NDASDEVICE_PROPERTY_KEY_PHYSICAL_DEVICE_SLOT_NUMBER	"Slot Number of Physical Device"
#define NDASDEVICE_PROPERTY_KEY_PHYSICAL_DEVICE_UNIT_NUMBER	"Unit Number of Physical Device"

#define NDASDEVICE_PROPERTY_KEY_RAID_DEVICE_COUNTOFUNITS			"Count of Unit Devices of RAID"
#define NDASDEVICE_PROPERTY_KEY_RAID_DEVICE_COUNTOFSPARES			"Count of Spare Devices of RAID"
#define	NDASDEVICE_PROPERTY_KEY_RAID_UNIT_DISK_LOCATION_OF_SUB_UNIT	"Unit Disk Location of Sub Unit Device %d"
#define NDASDEVICE_PROPERTY_KEY_RAID_STATUS_OF_SUB_UNIT				"Status of Sub Unit Device %d"
#define NDASDEVICE_PROPERTY_KEY_RAID_TYPE_OF_SUB_UNIT				"Type of Sub Unit Device %d"
#define NDASDEVICE_PROPERTY_KEY_RAID_BLOCK_SIZE						"RAID Block Size by Sector"
#define NDASDEVICE_PROPERTY_KEY_RAID_STATUS							"Status of RAID"
#define NDASDEVICE_PROPERTY_KEY_RAID_RECOVERING_SECTOR				"Recovering Sector"
#define NDASDEVICE_PROPERTY_KEY_RAID_CONFIG_ID						"Config ID of RAID Device"
#define NDASDEVICE_PROPERTY_KEY_RAID_RMD_SEQUENCE					"RMD Sequnce of RAID Device"

#define NDASDEVICE_PROPERTY_KEY_LOGICAL_DEVICE_LOWER_UNIT_DEVICE_ID	"ID of Lower Unit Device"

#endif
