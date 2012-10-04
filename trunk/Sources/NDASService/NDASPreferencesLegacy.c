#include <CoreServices/CoreServices.h>

#include "NDASPreferencesLegacy.h"

#include "NDASBusEnumeratorUserClientTypes.h"

#define KEY_ID_STRING						"ID"
#define	KEY_WRITEKEY_STRING					"Write Key"
#define	KEY_NAME_STRING						"Name"
#define	KEY_CONFIGURATION_OF_DEVICE_STRING	"Cofiguration"
#define KEY_CONFIGURATION_OF_UNIT_STRING	"Configuration of Unit"

#define KEY_ID_FIELD						"%s%d of Device %d"
#define KEY_WRITEKEY_FIELD					"%s of Device %d"
#define KEY_NAME_FIELD						"%s of Device %d"
#define KEY_CONFIGURATION_OF_DEVICE_FIELD	"%s of Device %d"
#define KEY_CONFIGURATION_OF_UNIT_FIELD		"%s %d of Device %d"

 bool NDASPreferencesRegisterLegacy(PNDASPreferencesParameterLegacy parameter)
 {
	 CFStringRef				strIDKey[4] = { NULL };
	 CFStringRef				strWriteKeyKey = NULL;
	 CFStringRef				strConfigurationKey = NULL;
	 CFStringRef				strNameKey = NULL;
	 
	 CFStringRef				strIDValue[4] = { NULL };
	 CFStringRef				strWriteKeyValue = NULL;
	 CFNumberRef				numberConfigurationValue = NULL;
	 CFStringRef				strNameValue = NULL;
	 
	 CFStringRef				cfsUnitConfigurationKey[MAX_NR_OF_TARGETS_PER_DEVICE] = { NULL };
	 CFNumberRef				cfnUnitConfigurationValue[MAX_NR_OF_TARGETS_PER_DEVICE] = { NULL };
	 
	 int						count;
	 
	 if(parameter->slotNumber == 0) {
		 return false;
	 }
	 	 
	 // Copy ID
	 for(count = 0; count < 4; count++) {
		 
		 // Create Key.
		 strIDKey[count] = CFStringCreateWithFormat(NULL, NULL, CFSTR(KEY_ID_FIELD), KEY_ID_STRING, count, parameter->slotNumber);
		 
		 // Create Value.
		 strIDValue[count] = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s"), parameter->chID[count]);		 
		 
		 CFPreferencesSetValue(
							   strIDKey[count], 
							   strIDValue[count], 
							   NDAS_PREFERENCES_FILE,
							   kCFPreferencesAnyUser, 
							   kCFPreferencesCurrentHost
							   );
		 
	 }
	 
	 // Copy WriteKey.
	 strWriteKeyKey = CFStringCreateWithFormat(NULL, NULL, CFSTR(KEY_WRITEKEY_FIELD), KEY_WRITEKEY_STRING, parameter->slotNumber);
	 strWriteKeyValue = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s"), parameter->chWriteKey);
	 CFPreferencesSetValue(
						   strWriteKeyKey, 
						   strWriteKeyValue, 
						   NDAS_PREFERENCES_FILE,
						   kCFPreferencesAnyUser, 
						   kCFPreferencesCurrentHost
						   );
	 
	 
	 // Copy Name
	 strNameKey = CFStringCreateWithFormat(NULL, NULL, CFSTR(KEY_NAME_FIELD), KEY_NAME_STRING, parameter->slotNumber);
	 //strNameValue = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s"), parameter->Name);
	 //strNameValue = CFStringCreateWithCharacters(kCFAllocatorDefault, (UniChar *)parameter->Name, MAX_NDAS_NAME_SIZE / 2);
	 
	 CFPreferencesSetValue(
						   strNameKey, 
						   parameter->cfsName, //strNameValue, 
						   NDAS_PREFERENCES_FILE,
						   kCFPreferencesAnyUser, 
						   kCFPreferencesCurrentHost
						   );
	 	 
	 // Copy Configuration.
	 strConfigurationKey = CFStringCreateWithFormat(NULL, NULL, CFSTR(KEY_CONFIGURATION_OF_DEVICE_FIELD), KEY_CONFIGURATION_OF_DEVICE_STRING, parameter->slotNumber);
	 numberConfigurationValue = CFNumberCreate (NULL, kCFNumberSInt32Type, &parameter->configuration);
	 CFPreferencesSetValue(
						   strConfigurationKey, 
						   numberConfigurationValue, 
						   NDAS_PREFERENCES_FILE,
						   kCFPreferencesAnyUser, 
						   kCFPreferencesCurrentHost
						   );
	 
	 // Unit Preferences.
	 for(count = 0; count < MAX_NR_OF_TARGETS_PER_DEVICE; count++) {
		 
		 // Copy Configuration.
		 cfsUnitConfigurationKey[count] = CFStringCreateWithFormat(NULL, NULL, CFSTR(KEY_CONFIGURATION_OF_UNIT_FIELD), KEY_CONFIGURATION_OF_UNIT_STRING, count, parameter->slotNumber);
		 cfnUnitConfigurationValue[count] = CFNumberCreate (NULL, kCFNumberSInt32Type, &parameter->Units[count].configuration);
		 CFPreferencesSetValue(
							   cfsUnitConfigurationKey[count], 
							   cfnUnitConfigurationValue[count], 
							   NDAS_PREFERENCES_FILE,
							   kCFPreferencesAnyUser, 
							   kCFPreferencesCurrentHost
							   );		 
	 }
	 
	 // Write out the preference data.
	 CFPreferencesSynchronize( 
							   NDAS_PREFERENCES_FILE,
							   kCFPreferencesAnyUser, 
							   kCFPreferencesCurrentHost
							   );
	 
	 	 
	 // Clean up.	 
	 if(strWriteKeyKey)				CFRelease(strWriteKeyKey);
	 if(strWriteKeyValue)			CFRelease(strWriteKeyValue);
	 
	 if(strConfigurationKey)		CFRelease(strConfigurationKey);
	 if(numberConfigurationValue)	CFRelease(numberConfigurationValue);
	 
	 if(strNameKey)					CFRelease(strNameKey);
	 if(strNameValue)				CFRelease(strNameValue);
	 	 
	 for(count = 0; count < 4; count++) {
		 if(strIDKey[count])		CFRelease(strIDKey[count]);
		 if(strIDValue[count])		CFRelease(strIDValue[count]);
	 }
	 
	 // Units.
	 for(count = 0; count < MAX_NR_OF_TARGETS_PER_DEVICE; count++) {
		 if(cfsUnitConfigurationKey[count])		CFRelease(cfsUnitConfigurationKey[count]);
		 if(cfnUnitConfigurationValue[count])	CFRelease(cfnUnitConfigurationValue[count]);
	 }
	 
	 return true;
 }
 
 bool NDASPreferencesUnregisterLegacy(UInt32 slotNumber)
 {
	 CFStringRef				strIDKey[4] = { NULL };
	 CFStringRef				strWriteKeyKey = NULL;
	 CFStringRef				strConfigurationKey = NULL;
	 CFStringRef				strNameKey = NULL;
	 CFStringRef				cfsUnitConfigurationKey[MAX_NR_OF_TARGETS_PER_DEVICE] = { NULL };
	 
	 int						count;
	 
	 if(slotNumber == 0) {
		 return false;
	 }
	 
	 // Copy ID
	 for(count = 0; count < 4; count++) {
		 
		 // Create Key.
		 strIDKey[count] = CFStringCreateWithFormat(NULL, NULL, CFSTR(KEY_ID_FIELD), KEY_ID_STRING, count, slotNumber);
		 		 
		 CFPreferencesSetValue(
							   strIDKey[count], 
							   NULL, 
							   NDAS_PREFERENCES_FILE,
							   kCFPreferencesAnyUser, 
							   kCFPreferencesCurrentHost
							   );
		 
	 }
	 
	 // Copy WriteKey.
	 strWriteKeyKey = CFStringCreateWithFormat(NULL, NULL, CFSTR(KEY_WRITEKEY_FIELD), KEY_WRITEKEY_STRING, slotNumber);
	 CFPreferencesSetValue(
						   strWriteKeyKey, 
						   NULL, 
						   NDAS_PREFERENCES_FILE,
						   kCFPreferencesAnyUser, 
						   kCFPreferencesCurrentHost
						   );
	 

	 // Copy Name.
	 strNameKey = CFStringCreateWithFormat(NULL, NULL, CFSTR(KEY_NAME_FIELD), KEY_NAME_STRING, slotNumber);
	 CFPreferencesSetValue(
						   strNameKey, 
						   NULL, 
						   NDAS_PREFERENCES_FILE,
						   kCFPreferencesAnyUser, 
						   kCFPreferencesCurrentHost
						   );
	 
	 // Copy Configuration.
	 strConfigurationKey = CFStringCreateWithFormat(NULL, NULL, CFSTR(KEY_CONFIGURATION_OF_DEVICE_FIELD), KEY_CONFIGURATION_OF_DEVICE_STRING, slotNumber);
	 CFPreferencesSetValue(
						   strConfigurationKey, 
						   NULL, 
						   NDAS_PREFERENCES_FILE,
						   kCFPreferencesAnyUser, 
						   kCFPreferencesCurrentHost
						   );
	 
	 // Unit Preferences.
	 for(count = 0; count < MAX_NR_OF_TARGETS_PER_DEVICE; count++) {
		 
		 // Copy Configuration.
		 cfsUnitConfigurationKey[count] = CFStringCreateWithFormat(NULL, NULL, CFSTR(KEY_CONFIGURATION_OF_UNIT_FIELD), KEY_CONFIGURATION_OF_UNIT_STRING, count, slotNumber);
		 CFPreferencesSetValue(
							   cfsUnitConfigurationKey[count], 
							   NULL, 
							   NDAS_PREFERENCES_FILE,
							   kCFPreferencesAnyUser, 
							   kCFPreferencesCurrentHost
							   );		 
	 }
	 
	 
	 // Write out the preference data.
	 CFPreferencesSynchronize( 
							   NDAS_PREFERENCES_FILE,
							   kCFPreferencesAnyUser, 
							   kCFPreferencesCurrentHost
							   );
	 
	 
	 // Clean up.	 
	 if(strWriteKeyKey)				CFRelease(strWriteKeyKey);
	 if(strConfigurationKey)		CFRelease(strConfigurationKey);
	 if(strNameKey)					CFRelease(strNameKey);
	 
	 for(count = 0; count < 4; count++) {
		 if(strIDKey[count])			CFRelease(strIDKey[count]);
	 }
	 
	 for(count = 0; count < MAX_NR_OF_TARGETS_PER_DEVICE; count++) {
		 if(cfsUnitConfigurationKey[count])		CFRelease(cfsUnitConfigurationKey[count]);
	 }
	 
	 return true;
 }
 
 bool NDASPreferencesQueryLegacy(PNDASPreferencesParameterLegacy parameter, int *NumberOfIDs)
 {
	 CFStringRef				strIDKey[4] = { NULL };
	 CFStringRef				strWriteKeyKey = NULL;
	 CFStringRef				strConfigurationKey = NULL;
	 CFStringRef				strNameKey = NULL;
	 
	 CFStringRef				strIDValue[4] = { NULL };
	 CFStringRef				strWriteKeyValue = NULL;
	 CFNumberRef				numberConfigurationValue = NULL;
	 CFStringRef				strNameValue = NULL;
	 
	 CFStringRef				cfsUnitConfigurationKey[MAX_NR_OF_TARGETS_PER_DEVICE] = { NULL };
	 CFNumberRef				cfnUnitConfigurationValue[MAX_NR_OF_TARGETS_PER_DEVICE] = { NULL };
	 
	 int						count;
//	 bool					present;
	 bool					result = true;
//	 CFArrayRef				cfaKeys = NULL;
	 
	 *NumberOfIDs = 0;
	 
	 if(parameter == NULL || parameter->slotNumber == 0) {
		 return false;
	 }
	 
	 // Copy ID
	 for(count = 0; count < 4; count++) {		 
		 // Create Key.
		 strIDKey[count] = CFStringCreateWithFormat(NULL, NULL, CFSTR(KEY_ID_FIELD), KEY_ID_STRING, count, parameter->slotNumber);
		 		 
	 }
	 
	 // Copy WriteKey.
	 strWriteKeyKey = CFStringCreateWithFormat(NULL, NULL, CFSTR(KEY_WRITEKEY_FIELD), KEY_WRITEKEY_STRING, parameter->slotNumber);
	 
	 // Copy Configuration.
	 strConfigurationKey = CFStringCreateWithFormat(NULL, NULL, CFSTR(KEY_CONFIGURATION_OF_DEVICE_FIELD), KEY_CONFIGURATION_OF_DEVICE_STRING, parameter->slotNumber);
	 
	 // Copy Name.
	 strNameKey = CFStringCreateWithFormat(NULL, NULL, CFSTR(KEY_NAME_FIELD), KEY_NAME_STRING, parameter->slotNumber);
	 
	 for(count = 0; count < MAX_NR_OF_TARGETS_PER_DEVICE; count++) {		 
		 // Create Key.
		 cfsUnitConfigurationKey[count] = CFStringCreateWithFormat(NULL, NULL, CFSTR(KEY_CONFIGURATION_OF_UNIT_FIELD), KEY_CONFIGURATION_OF_UNIT_STRING, count, parameter->slotNumber);
		 
	 }
	 
	 //
	 // Check and Copy Values.
	 //
	 	 
	 // ID
	 
	 *NumberOfIDs = 0;
	 for(count = 0; count < 4; count++) {
		 strIDValue[count] = CFPreferencesCopyValue(
										  strIDKey[count],
										  NDAS_PREFERENCES_FILE,
										  kCFPreferencesAnyUser, 
										  kCFPreferencesCurrentHost
										  );
										  
		 if(strIDValue[count] == NULL) {
			 // ID is required Key. This Entry is invalid. Delete It.
			 NDASPreferencesUnregisterLegacy(parameter->slotNumber);
			 
			 result = false;
			 
			 goto cleanup;
			 
		 } else {
			 // Convert CF string to normal string
			 CFStringGetCString(strIDValue[count], parameter->chID[count], 6, CFStringGetSystemEncoding());
			 
			 (*NumberOfIDs)++;
		 }
	 }
	 
	 // Write Key.
	 strWriteKeyValue = CFPreferencesCopyValue(
												strWriteKeyKey,
												NDAS_PREFERENCES_FILE,
												kCFPreferencesAnyUser, 
												kCFPreferencesCurrentHost
												);
	 if(strWriteKeyKey == NULL) {
		 // Write Key is Optional.
		 memset(parameter->chWriteKey, 0, 6);
	 } else {
		 // Convert CF string to normal string
		 CFStringGetCString(strWriteKeyValue, parameter->chWriteKey, 6, CFStringGetSystemEncoding());
	 }

	 // Name.
	 strNameValue = CFPreferencesCopyValue(
										   strNameKey,
										   NDAS_PREFERENCES_FILE,
										   kCFPreferencesAnyUser, 
										   kCFPreferencesCurrentHost
										   );
	 if(strNameValue == NULL) {
		 // No Name.
		 parameter->cfsName = NULL;
	 } else {
		 
		 parameter->cfsName = CFStringCreateCopy(kCFAllocatorDefault, strNameValue);
		 
		 // Convert CF string to normal string
		 //CFStringGetCString(strNameValue, parameter->Name, MAX_NDAS_NAME_SIZE, kCFStringEncodingUTF8); // CFStringGetSystemEncoding());
	 }
	 
	 // Configuration.
	 numberConfigurationValue = CFPreferencesCopyValue(
											   strConfigurationKey,
											   NDAS_PREFERENCES_FILE,
											   kCFPreferencesAnyUser, 
											   kCFPreferencesCurrentHost
											   );
	 if(numberConfigurationValue == NULL) {
		 // Write Key is Optional.
		 parameter->configuration = kNDASConfigurationDisable;
	 } else {
		 // Convert CF Number to int
		 CFNumberGetValue (numberConfigurationValue, kCFNumberSInt32Type, &parameter->configuration);
		 
		 if(parameter->configuration > kNDASNumberOfConfigurations) {
			 parameter->configuration = kNDASConfigurationDisable;
		 }
	 }
	 
	 // Units.
	 for(count = 0; count < MAX_NR_OF_TARGETS_PER_DEVICE; count++) {
		 // Configuration.
		 cfnUnitConfigurationValue[count] = CFPreferencesCopyValue(
														   cfsUnitConfigurationKey[count],
														   NDAS_PREFERENCES_FILE,
														   kCFPreferencesAnyUser, 
														   kCFPreferencesCurrentHost
														   );
		 if(cfnUnitConfigurationValue[count] == NULL) {
			 // Write Key is Optional.
			 parameter->Units[count].configuration = kNDASUnitConfigurationUnmount;
		 } else {
			 // Convert CF Number to int
			 CFNumberGetValue (cfnUnitConfigurationValue[count], kCFNumberSInt32Type, &parameter->Units[count].configuration);
			 
			 if(parameter->Units[count].configuration  > kNDASNumberOfUnitConfiguration) {
				 parameter->Units[count].configuration  = kNDASUnitConfigurationUnmount;
			 }
		 }
	 }
	 
cleanup:
		 
	 // clean up.		 
	 if(strWriteKeyKey)				CFRelease(strWriteKeyKey);
	 if(strWriteKeyValue)			CFRelease(strWriteKeyValue);
	 
	 if(strConfigurationKey)		CFRelease(strConfigurationKey);
	 if(numberConfigurationValue)	CFRelease(numberConfigurationValue);
	 
	 if(strNameKey)					CFRelease(strNameKey);
	 if(strNameValue)				CFRelease(strNameValue);

	 for(count = 0; count < 4; count++) {
		 if(strIDKey[count])		CFRelease(strIDKey[count]);
		 if(strIDValue[count])		CFRelease(strIDValue[count]);
	 }

	 for(count = 0; count < MAX_NR_OF_TARGETS_PER_DEVICE; count++) {		 
		 if ( cfsUnitConfigurationKey[count] )	CFRelease( cfsUnitConfigurationKey[count] );
		 if ( cfnUnitConfigurationValue[count] )	CFRelease( cfnUnitConfigurationValue[count] );
	 }
	 
	 return result;
 }

CFArrayRef NDASPreferencesGetRegisteredSlotListLegacy() 
{
	return CFPreferencesCopyKeyList (
									 NDAS_PREFERENCES_FILE,
									 kCFPreferencesAnyUser, 
									 kCFPreferencesCurrentHost
									 );	
}

UInt32	NDASPreferencesGetNumberOfRegisteredEntryLegacy()
{
	CFArrayRef	keys;
	
	keys = NDASPreferencesGetRegisteredSlotListLegacy (
													   NDAS_PREFERENCES_FILE,
													   kCFPreferencesAnyUser, 
													   kCFPreferencesCurrentHost
													   );	
	if(keys == NULL) {
		return 0;
	} else {
		UInt32 slots;
		
		slots = CFArrayGetCount(keys);
		
		CFRelease(keys);
		
		return slots;
	}
}

UInt32	FindSlotNumberByComparingKey(CFStringRef cfsEntry, CFStringRef cfsKey)
{
	if (0 == CFStringCompareWithOptions(cfsEntry,
										cfsKey,
										CFRangeMake(0, CFStringGetLength(cfsKey)),
										0
										)) {
		CFStringRef cfsSlotNumber;
		
		if (cfsSlotNumber = CFStringCreateWithSubstring (
														 kCFAllocatorDefault,
														 cfsEntry,
														 CFRangeMake(CFStringGetLength(cfsKey) + 1, CFStringGetLength(cfsEntry))
														 )) {
			
			UInt32 slotNumber = CFStringGetIntValue(cfsSlotNumber);
			
			CFRelease(cfsSlotNumber);
			
			return slotNumber;
		}
		
	}
	
	return 0;
}

UInt32	NDASPreferencesGetArbitrarySlotNumberOfRegisteredEntryLegacy()
{
	CFArrayRef	keys;
	int			count;
	UInt32		slotNumber;
	
	keys = NDASPreferencesGetRegisteredSlotListLegacy (
													   NDAS_PREFERENCES_FILE,
													   kCFPreferencesAnyUser, 
													   kCFPreferencesCurrentHost
													   );	
	if(keys == NULL) {
		return 0;
	} else {
		
		CFStringRef cfsEntry;
		UInt32		index = 0;
		
		while(index < CFArrayGetCount(keys)) {
			
			CFStringRef cfsKey;

			if(NULL == (cfsEntry = (CFStringRef)CFArrayGetValueAtIndex(keys, index))) {
				return 0;
			}
			
			// Check ID key.
			for(count = 0; count < 4; count++) {
								
				// Create Key.
				if (cfsKey = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s%d of Device"), KEY_ID_STRING, count)) {
										
					if (slotNumber = FindSlotNumberByComparingKey(cfsEntry, cfsKey)) {
						return slotNumber;
					}
					
					CFRelease(cfsKey);
				}
			}

			// Check Configuration of Unit Device.
			for(count = 0; count < 16; count++) {
								
				// Create Key.
				if (cfsKey = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s %d of Device"), KEY_CONFIGURATION_OF_UNIT_STRING, count)) {
					
					if (slotNumber = FindSlotNumberByComparingKey(cfsEntry, cfsKey)) {
						return slotNumber;
					}
					
					CFRelease(cfsKey);
				}
			}

			// Check Configuration of Device.
			if (cfsKey = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s of Device"), KEY_CONFIGURATION_OF_DEVICE_STRING)) {
												
				if (slotNumber = FindSlotNumberByComparingKey(cfsEntry, cfsKey)) {
					return slotNumber;
				}
				
				CFRelease(cfsKey);
			}				

			// Check Name of Device.
			if (cfsKey = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s of Device"), KEY_NAME_STRING)) {
				
				if (slotNumber = FindSlotNumberByComparingKey(cfsEntry, cfsKey)) {
					return slotNumber;
				}
				
				CFRelease(cfsKey);
			}				

			// Check WriteKey of Device.
			if (cfsKey = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s of Device"), KEY_WRITEKEY_STRING)) {
				
				if (slotNumber = FindSlotNumberByComparingKey(cfsEntry, cfsKey)) {
					return slotNumber;
				}
				
				CFRelease(cfsKey);
			}				
			
			
			index++;
		}
		return 0;
	}
}