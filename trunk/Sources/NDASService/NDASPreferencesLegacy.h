#include <CoreServices/CoreServices.h>

#ifndef _NDAS_PREFERENCES_LEGACY_H_
#define _NDAS_PREFERENCES_LEGACY_H_

#include "LanScsiProtocol.h"

#include "NDASBusEnumeratorUserClientTypes.h"

#define NDAS_PREFERENCES_FILE	(CFSTR("com.ximeta.NDAS.service"))
#define NUMBER_OF_KEYS_PER_DEVICE 6

typedef struct _UnitPreferencesParameterLegacy {
	
	UInt32	configuration;
	
} UnitPreferencesParameterLegacy, *PUnitPreferencesParameterLegacy;

typedef struct _NDASPreferencesParameterLegacy {
    
	UInt32						slotNumber;
    char						chID[4][6];
    char						chWriteKey[6];
    UInt32						configuration;
	
	char						Name[MAX_NDAS_NAME_SIZE];
	CFStringRef					cfsName;
	char						DeviceIdString[MAX_NDAS_NAME_SIZE];
	
	UnitPreferencesParameterLegacy	Units[MAX_NR_OF_TARGETS_PER_DEVICE];
	
} NDASPreferencesParameterLegacy, *PNDASPreferencesParameterLegacy;

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus
	
//
// APIs
//
bool NDASPreferencesRegisterLegacy(PNDASPreferencesParameterLegacy parameter);
bool NDASPreferencesUnregisterLegacy(UInt32 slotNumber);

bool NDASPreferencesQueryLegacy(PNDASPreferencesParameterLegacy parameter, int *NumberOfIDs);

CFArrayRef	NDASPreferencesGetRegisteredSlotListLegacy(); 
UInt32		NDASPreferencesGetArbitrarySlotNumberOfRegisteredEntryLegacy();
UInt32		NDASPreferencesGetNumberOfRegisteredEntryLegacy();

#ifdef __cplusplus
}
#endif // __cplusplus
	
#endif

