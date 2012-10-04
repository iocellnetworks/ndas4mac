#include <CoreServices/CoreServices.h>

#ifndef _NDAS_PREFERENCES_H_
#define _NDAS_PREFERENCES_H_

#include "LanScsiProtocol.h"

#include "NDASBusEnumeratorUserClientTypes.h"

#define NDAS_PREFERENCES_FILE_REGISTER			(CFSTR("com.ximeta.NDAS.service.register"))
#define NDAS_PREFERENCES_FILE_CONFIGURATION		(CFSTR("com.ximeta.NDAS.service.configuration"))
#define NDAS_PREFERENCES_FILE_RAID_INFORMATION	(CFSTR("com.ximeta.NDAS.service.RAID.Information"))

#define NUMBER_OF_KEYS_PER_DEVICE 6

typedef struct _UnitPreferencesParameter {
	
	UInt32	configuration;
	
} UnitPreferencesParameter, *PUnitPreferencesParameter;

typedef struct _NDASPreferencesParameter {
    
	UInt32						slotNumber;		// Or Index.
    char						chID[4][6];
    char						chWriteKey[6];
    UInt32						configuration;
		
	CFStringRef					cfsName;
	char						DeviceIdString[MAX_NDAS_NAME_SIZE];
	
	UnitPreferencesParameter	Units[MAX_NR_OF_TARGETS_PER_DEVICE];
	
} NDASPreferencesParameter, *PNDASPreferencesParameter;

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus
	
//
// APIs
//
bool NDASPreferencesInsert(PNDASPreferencesParameter parameter);
bool NDASPreferencesDelete(UInt32 slotNumber);

SInt32 NDASPreferencesRegister(PNDASPreferencesParameter parameter);
SInt32 NDASPreferencesUnregister(UInt32 slotNumber);
SInt32 NDASPreferencesQueryBySlotNumber(PNDASPreferencesParameter parameter);
SInt32 NDASPreferencesQueryByID(PNDASPreferencesParameter parameter);
bool NDASPreferencesChangeConfigurationOfDevice(UInt32 slotNumber, UInt32 configuration);
bool NDASPreferencesChangeNameOfDevice(UInt32 slotNumber, CFStringRef name);
bool NDASPreferencesChangeWriteKeyOfDevice(UInt32 slotNumber, char *WriteKey);
CFArrayRef	NDASPreferencesGetRegisteredSlotList(); 

bool NDASPreferencesChangeConfigurationOfLogicalDevice(UInt32 index, UInt32 configuration);
UInt32 NDASPreferencesGetConfiguration(CFStringRef key);
CFArrayRef	NDASPreferencesGetLogicalDeviceConfigurationList();

bool NDASPreferencesChangeNameOfRAIDDevice(PGUID raidID, CFStringRef name);
CFStringRef NDASPreferencesGetRAIDNameByUUIDString(CFStringRef cfsUUIDString);
CFArrayRef	NDASPreferencesGetRAIDDeviceInformationList();

#ifdef __cplusplus
}
#endif // __cplusplus
	
#endif

