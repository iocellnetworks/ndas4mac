#include <CoreServices/CoreServices.h>

#include "NDASPreferences.h"

#include "NDASBusEnumeratorUserClientTypes.h"
#include "NDASGetInformation.h"
#include "Hash.h"

#define KEY_ID_STRING								"ID %d"
#define	KEY_WRITEKEY_STRING							"Write Key"
#define	KEY_NAME_STRING								"Name"
#define	KEY_CONFIGURATION_STRING					"Cofiguration"
#define KEY_AUTO_REGISTER_STRING					"Auto Register"
#define KEY_SLOT_NUMBER_STRING						"%d"

#define KEY_CONFIGURATION_OF_LOGICAL_DEVICE_FIELD	"%s of Logical Device %lld"

extern char hostIDKey[8];

#if defined(__BIG_ENDIAN__)
	const UInt32 ENCRYPT_KEY = 0xFE040930;
#else
	const UInt32 ENCRYPT_KEY = 0x300904FE;
#endif

//
// Read ID or Write Key and Decrypt it.
//
// Return :	NULL if not exist or decryped value.
//
CFStringRef NDASPreferencesGetIDorWriteKey(	CFDictionaryRef dictEntry, CFStringRef keyString )
{
	bool			present;
	CFStringRef		strValue;
	CFDataRef		cfdEncrypedData;
	UInt8			data[8];
	
	present = CFDictionaryGetValueIfPresent(
											dictEntry,
											keyString,
											(const void**)&cfdEncrypedData
											);
	if(!present || !cfdEncrypedData) {
		
		return NULL;		

	} else {
		
		if(CFDataGetTypeID() == CFGetTypeID(cfdEncrypedData)) {
			
			CFDataGetBytes(cfdEncrypedData, CFRangeMake(0, 8), data);
			
			Decrypt32(data, 8, (unsigned char*)&ENCRYPT_KEY, (unsigned char*)hostIDKey);
			
			strValue = CFStringCreateWithBytes (
												kCFAllocatorDefault,
												data,
												5,
												CFStringGetSystemEncoding(),
												false
												);
			
			return strValue;
		} else {			
			return NULL;
		}
	}
}

//
// Encrypt and Write ID or Write Key.
//
// Return :	false if Bad parameters or No memory.
//			true if success.
//
bool NDASPreferencesSetIDorWriteKey(CFMutableDictionaryRef dictEntry, CFStringRef keyString, CFStringRef valueString)
{
	
	UInt8		data[8] = { 0 };
	CFDataRef	cfdEncrypedData;
	
	if (NULL == keyString
		|| NULL == valueString
		|| NULL == dictEntry
		) {
		return false;
	}

	if (5 > CFStringGetBytes (
							  valueString,
							  CFRangeMake(0, 5),
							  CFStringGetSystemEncoding(),
							  0,
							  false,
							  data,
							  8,
							  NULL
							  )) {
		return false;
	}
	
	Encrypt32(data, 8, (unsigned char*)&ENCRYPT_KEY, (unsigned char*)hostIDKey);

	cfdEncrypedData = CFDataCreate (
									kCFAllocatorDefault,
									data,
									8
									);
	
	if (NULL == cfdEncrypedData) {
		return false;
	}
	
	CFDictionaryAddValue(dictEntry, keyString, cfdEncrypedData);		

	CFRelease(cfdEncrypedData);
	
	return true;
}

//
// Insert New NDAS device info.
//
// Return :	false if Bad parameters or No memory.
//			true if success.
//
bool NDASPreferencesInsert(PNDASPreferencesParameter parameter)
{
	CFStringRef				strEntryKey = NULL;
	CFStringRef				strIDKey[4] = { NULL };
	CFStringRef				strWriteKeyKey = NULL;
	CFStringRef				strConfigurationKey = NULL;
	
	CFStringRef				strIDValue[4] = { NULL };
	CFStringRef				strWriteKeyValue = NULL;
	CFNumberRef				numberConfigurationValue = NULL;
	CFStringRef				strNameKey = NULL;
	
	CFStringRef				strAutoRegisterKey = NULL;
	
	CFMutableDictionaryRef	dictEntry = NULL;
	int						count;
	bool					result = false;
	
	if(parameter->slotNumber == 0) {
		return false;
	}
		
	// Create Dict
	dictEntry = CFDictionaryCreateMutable (kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (NULL == dictEntry) {
		return false;
	}
	
	// Copy ID
	for(count = 0; count < 4; count++) {
		
		// Create Key.
		strIDKey[count] = CFStringCreateWithFormat(NULL, NULL, CFSTR(KEY_ID_STRING), count);		
		// Create Value.
		strIDValue[count] = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s"), parameter->chID[count]);
		if (NULL == strIDKey[count] || NULL == strIDValue[count]) {
			goto cleanup;
		}
				
		if (!NDASPreferencesSetIDorWriteKey(dictEntry, strIDKey[count], strIDValue[count])) {
			goto cleanup;
		}
	}
	
	// Copy WriteKey.
	strWriteKeyKey = CFStringCreateWithFormat(NULL, NULL, CFSTR(KEY_WRITEKEY_STRING));
	strWriteKeyValue = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s"), parameter->chWriteKey);
	if (NULL == strWriteKeyKey || NULL == strWriteKeyValue) {
		goto cleanup;
	}
	
	if(!NDASPreferencesSetIDorWriteKey(dictEntry, strWriteKeyKey, strWriteKeyValue)) {
		goto cleanup;
	}
	
	// Copy Name
	strNameKey = CFStringCreateWithFormat(NULL, NULL, CFSTR(KEY_NAME_STRING));
	if (NULL == strNameKey) {
		goto cleanup;
	}
	
	//strNameValue = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s"), parameter->Name);
	//strNameValue = CFStringCreateWithCharacters(kCFAllocatorDefault, (UniChar *)parameter->Name, MAX_NDAS_NAME_SIZE / 2);
	
	CFDictionaryAddValue(dictEntry, 
						 strNameKey, 
						 parameter->cfsName
						 );
	
	// Copy Configuration.
	strConfigurationKey = CFStringCreateWithFormat(NULL, NULL, CFSTR(KEY_CONFIGURATION_STRING));
	numberConfigurationValue = CFNumberCreate (NULL, kCFNumberSInt32Type, &parameter->configuration);
	if (NULL == strConfigurationKey || NULL == numberConfigurationValue) {
		goto cleanup;
	}
	
	CFDictionaryAddValue(dictEntry, strConfigurationKey, numberConfigurationValue);
	
	// Copy AutoRegister.
	strAutoRegisterKey = CFStringCreateWithFormat(NULL, NULL, CFSTR(KEY_AUTO_REGISTER_STRING));
	if (NULL == strAutoRegisterKey) {
		goto cleanup;
	}

	if (parameter->AutoRegister) {
		CFDictionaryAddValue(dictEntry, strAutoRegisterKey, kCFBooleanTrue);
	} else {
		CFDictionaryAddValue(dictEntry, strAutoRegisterKey, kCFBooleanFalse);
	}

	// Slot Number is the Key.
	strEntryKey = CFStringCreateWithFormat(NULL, NULL, CFSTR(KEY_SLOT_NUMBER_STRING), parameter->slotNumber);
	
	// Set Values.
	CFPreferencesSetValue(
						  strEntryKey, 
						  dictEntry, 
						  NDAS_PREFERENCES_FILE_REGISTER,
						  kCFPreferencesAnyUser, 
						  kCFPreferencesCurrentHost
						  );
	
	// Write out the preference data.
	CFPreferencesSynchronize( 
							 NDAS_PREFERENCES_FILE_REGISTER,
							 kCFPreferencesAnyUser, 
							 kCFPreferencesCurrentHost
							 );
	
	
	result = true;
	
cleanup:
	
	CFDictionaryRemoveAllValues(dictEntry);
	
	// Clean up.
	if(strEntryKey)					CFRelease(strEntryKey);
	if(dictEntry)					CFRelease(dictEntry);
	
	if(strWriteKeyKey)				CFRelease(strWriteKeyKey);
	if(strWriteKeyValue)			CFRelease(strWriteKeyValue);
	
	if(strConfigurationKey)			CFRelease(strConfigurationKey);
	if(numberConfigurationValue)	CFRelease(numberConfigurationValue);
	if(strNameKey)					CFRelease(strNameKey);
	
	if(strAutoRegisterKey)			CFRelease(strAutoRegisterKey);
	
	for(count = 0; count < 4; count++) {
		if(strIDKey[count])			CFRelease(strIDKey[count]);
		if(strIDValue[count])		CFRelease(strIDValue[count]);
	}
	
	return result;
}

//
// Delete a NDAS device info.
//
// Return :	false if No memory.
//			true if success.
//
bool NDASPreferencesDelete(UInt32 slotNumber)
{
	CFStringRef				strEntryKey = NULL;	
	
	// Slot Number is the Key.
	strEntryKey = CFStringCreateWithFormat(NULL, NULL, CFSTR(KEY_SLOT_NUMBER_STRING), slotNumber);
	if(NULL == strEntryKey) {
		return false;
	}
	
	// Set Values.
	CFPreferencesSetValue(
						  strEntryKey, 
						  NULL, 
						  NDAS_PREFERENCES_FILE_REGISTER,
						  kCFPreferencesAnyUser, 
						  kCFPreferencesCurrentHost
						  );
	
	// Write out the preference data.
	CFPreferencesSynchronize( 
							 NDAS_PREFERENCES_FILE_REGISTER,
							 kCFPreferencesAnyUser, 
							 kCFPreferencesCurrentHost
							 );
	// Clean up.
	if(strEntryKey)					CFRelease(strEntryKey);
	
	return true;
}

//
// Query NDAS info by Slot number.
//
// Return :	-1 if Bad parameters, No memory or bad Infos.
//			0 if Not exist
//			1 if success.
//
SInt32 NDASPreferencesQueryBySlotNumber(PNDASPreferencesParameter parameter)
{
	CFStringRef				strEntryKey = NULL;	
	CFStringRef				strIDKey[4] = { NULL };
	CFStringRef				strWriteKeyKey = NULL;
	CFStringRef				strConfigurationKey = NULL;
	CFStringRef				strNameKey = NULL;
	
	CFStringRef				strIDValue[4] = { NULL };
	CFStringRef				strWriteKeyValue = NULL;
	CFNumberRef				numberConfigurationValue = NULL;
	CFStringRef				strNameValue = NULL;
	CFStringRef				strAutoRegisterKey = NULL;
	CFBooleanRef			boolAutoRegisterValue = NULL;
	
	CFDictionaryRef			dictEntry = NULL;
	
	int						count;
	bool					present;
	SInt32					result = -1;
	
	if(parameter == NULL || parameter->slotNumber == 0) {
		return -1;
	}

	// Read Last Value.
	CFPreferencesSynchronize( 
							 NDAS_PREFERENCES_FILE_REGISTER,
							 kCFPreferencesAnyUser, 
							 kCFPreferencesCurrentHost
							 );
	
	// Slot Number is the Key.
	strEntryKey = CFStringCreateWithFormat(NULL, NULL, CFSTR(KEY_SLOT_NUMBER_STRING), parameter->slotNumber);
	if(NULL == strEntryKey) {
		return -1;
	}
	
	dictEntry = CFPreferencesCopyValue (
										strEntryKey,
										NDAS_PREFERENCES_FILE_REGISTER,
										kCFPreferencesAnyUser,
										kCFPreferencesCurrentHost
										);
	
	if(dictEntry == NULL) {
		result = 0;
		goto cleanup;
	}
	
	//
	// Check and Copy Values.
	//
	
	// Create Keys.
	for(count = 0; count < 4; count++) {		
		strIDKey[count] = CFStringCreateWithFormat(NULL, NULL, CFSTR(KEY_ID_STRING), count);
		if(NULL == strIDKey[count]) {
			goto cleanup;
		}
	}
	strWriteKeyKey = CFStringCreateWithFormat(NULL, NULL, CFSTR(KEY_WRITEKEY_STRING));
	strConfigurationKey = CFStringCreateWithFormat(NULL, NULL, CFSTR(KEY_CONFIGURATION_STRING));
	strAutoRegisterKey =CFStringCreateWithFormat(NULL, NULL, CFSTR(KEY_AUTO_REGISTER_STRING));
	if(NULL == strWriteKeyKey || NULL == strConfigurationKey || NULL == strAutoRegisterKey) {
		goto cleanup;
	}
	
	// ID
	for(count = 0; count < 4; count++) {
		
		strIDValue[count] = NDASPreferencesGetIDorWriteKey(dictEntry, strIDKey[count]);
		if (NULL == strIDValue[count]) {
			result = -1;
			goto cleanup;			
		}
		
		if(!CFStringGetCString(strIDValue[count], parameter->chID[count], 6, CFStringGetSystemEncoding())) {
			goto cleanup;
		}
	}
	
	// Write Key.
	strWriteKeyValue = NDASPreferencesGetIDorWriteKey(dictEntry, strWriteKeyKey);
	if (NULL == strWriteKeyValue) {
		// Write Key is Optional.
		memset(parameter->chWriteKey, 0, 6);
	}
	CFStringGetCString(strWriteKeyValue, parameter->chWriteKey, 6, CFStringGetSystemEncoding());
	
	// Copy Name.
	strNameKey = CFStringCreateWithFormat(NULL, NULL, CFSTR(KEY_NAME_STRING), KEY_NAME_STRING);
	if(NULL == strNameKey) {
		goto cleanup;
	}
	
	present = CFDictionaryGetValueIfPresent(
											dictEntry,
											strNameKey,
											(const void**)&strNameValue
											);
	if(!present || !strNameValue) {
		// Name is Optional.
		parameter->cfsName = NULL;
	} else {
		
		CFRetain(strNameValue);
		
		if(CFStringGetTypeID() == CFGetTypeID(strNameValue)) {
			parameter->cfsName = CFStringCreateCopy(kCFAllocatorDefault, strNameValue);
		}
	}
	
	// Configuration.
	present = CFDictionaryGetValueIfPresent(
											dictEntry,
											strConfigurationKey,
											(const void**)&numberConfigurationValue
											);
	if(!present || !numberConfigurationValue) {
		// configuraton is Optional.
		parameter->configuration = kNDASUnitConfigurationUnmount;
	} else {
		
		CFRetain(numberConfigurationValue);
		
		// Convert CF Number to int
		CFNumberGetValue (numberConfigurationValue, kCFNumberSInt32Type, &parameter->configuration);
		
		if(parameter->configuration > kNDASNumberOfUnitConfiguration) {
			parameter->configuration = kNDASUnitConfigurationUnmount;
		}
	}
	
	// Auto Register.
	present = CFDictionaryGetValueIfPresent(
											dictEntry,
											strAutoRegisterKey,
											(const void**)&boolAutoRegisterValue
											);
	if(!present || NULL == boolAutoRegisterValue) {
		// For older than 1.8.4
		parameter->AutoRegister = false;
	} else {
		
		CFRetain(boolAutoRegisterValue);
		parameter->AutoRegister = CFBooleanGetValue(boolAutoRegisterValue);
	}	
	
	result = 1;
	
cleanup:
	
	// clean up.	
	if(strEntryKey)					CFRelease(strEntryKey);
	if(dictEntry)					CFRelease(dictEntry);
	
	if(strWriteKeyKey)				CFRelease(strWriteKeyKey);
	if(strWriteKeyValue)			CFRelease(strWriteKeyValue);
	if(strNameKey)					CFRelease(strNameKey);
	if(strNameValue)				CFRelease(strNameValue);
	
	if(strConfigurationKey)			CFRelease(strConfigurationKey);
	if(numberConfigurationValue)	CFRelease(numberConfigurationValue);
	
	for(count = 0; count < 4; count++) {
		if(strIDKey[count])			CFRelease(strIDKey[count]);
		if(strIDValue[count])		CFRelease(strIDValue[count]);
	}
	
	if(strAutoRegisterKey)			CFRelease(strAutoRegisterKey);
	if(boolAutoRegisterValue)		CFRelease(boolAutoRegisterValue);
	
	return result;
}

//
// Query NDAS info by ID.
//
// Return :	-1 if Bad parameters or No memory or bad infos.
//			0 if not exist
//			1 if success.
//
SInt32 NDASPreferencesQueryByID(PNDASPreferencesParameter parameter)
{
	int						count;
	NDASPreferencesParameter queriedParameter = { 0 };
	
	if(parameter == NULL) {
		return -1;
	}
		
	CFArrayRef	deviceArray = NDASPreferencesGetRegisteredSlotList();
	
	if (deviceArray != NULL && 0 < CFArrayGetCount(deviceArray)) {
		
		CFRetain(deviceArray);
		
//		syslog(LOG_DEBUG, "[%d]registered slot list : %d\n", __LINE__, CFArrayGetCount(deviceArray));
		
		for (count = 0; count < CFArrayGetCount(deviceArray); count++) {
			CFStringRef	cfsKey = NULL;
			
			cfsKey = (CFStringRef)CFArrayGetValueAtIndex(deviceArray, count);
			
			if (cfsKey) {
				
				CFRetain(cfsKey);
				
				queriedParameter.slotNumber = CFStringGetIntValue(cfsKey);
				
				if(1 == NDASPreferencesQueryBySlotNumber(&queriedParameter)) {
					if (0 == memcmp(queriedParameter.chID[0], parameter->chID[0], 5)
						&& 0 == memcmp(queriedParameter.chID[1], parameter->chID[1], 5)
						&& 0 == memcmp(queriedParameter.chID[2], parameter->chID[2], 5)
						&& 0 == memcmp(queriedParameter.chID[3], parameter->chID[3], 5)
						&& queriedParameter.slotNumber != parameter->slotNumber) {
						
						// Same NDAS.
						memcpy(parameter, &queriedParameter, sizeof(NDASPreferencesParameter));
						
						CFRelease(cfsKey);
						
						CFRelease(deviceArray);
						
						return 1;
					}
				}
				
				CFRelease(cfsKey);
			}
		}
		
		CFRelease(deviceArray);
	}
	
	return 0;
}

//
// Query NDAS info by ID.
//
// Return :	-1 if Bad parameters or No memory or bad infos.
//			0 if exist.
//			1 if success.
//
SInt32 NDASPreferencesRegister(PNDASPreferencesParameter parameter)
{
	
	if(parameter->slotNumber == 0) {
		return -1;
	}
	
	// Already Registered?
	if (NDASPreferencesQueryByID(parameter)) {
		return 0;
	}
	
	if(NDASPreferencesInsert(parameter)) {
		return 1;
	} else {
		return -1;
	}
}

//
// Query NDAS info by ID.
//
// Return :	-1 if Bad parameters or No memory or bad infos.
//			0 if not exist.
//			1 if success.
//
SInt32 NDASPreferencesUnregister(UInt32 slotNumber)
{
	NDASPreferencesParameter parameter = { 0 };
	SInt32	result;
		
	if (0 == slotNumber) {
		return -1;
	}
	
	parameter.slotNumber = slotNumber;
	result = NDASPreferencesQueryBySlotNumber(&parameter);
	if(0 != result) {
	
		if (parameter.cfsName) CFRelease(parameter.cfsName);

		if (NDASPreferencesDelete(slotNumber)) {
			return 1;
		} else {
			return -1;
		}
	} else {
		// Not Exist.
		return 0;
	}
}

bool NDASPreferencesChangeConfigurationOfDevice(UInt32 slotNumber, UInt32 configuration)
{
	NDASPreferencesParameter	parameter = { 0 };
	
	parameter.slotNumber = slotNumber;
	
	if(1 == NDASPreferencesQueryBySlotNumber(&parameter)) {
		parameter.configuration = configuration;
		
		NDASPreferencesInsert(&parameter);
		
		if (parameter.cfsName) CFRelease(parameter.cfsName);
		
	} else {
		return false;
	}
}

bool NDASPreferencesChangeNameOfDevice(UInt32 slotNumber, CFStringRef name)
{
	NDASPreferencesParameter	parameter;
	
	parameter.slotNumber = slotNumber;
	
	if(1 == NDASPreferencesQueryBySlotNumber(&parameter)) {
		
		//memcpy(parameter.Name, name, MAX_NDAS_NAME_SIZE);
		
		if (parameter.cfsName) CFRelease(parameter.cfsName);
		
		parameter.cfsName = name;
		
		NDASPreferencesInsert(&parameter);
		
	} else {
		return false;
	}
}

bool NDASPreferencesChangeWriteKeyOfDevice(UInt32 slotNumber, char *WriteKey)
{
	NDASPreferencesParameter	parameter;
	
	parameter.slotNumber = slotNumber;
	
	if(1 == NDASPreferencesQueryBySlotNumber(&parameter)) {
		
		memcpy(parameter.chWriteKey, WriteKey,6);
		
		NDASPreferencesInsert(&parameter);
		
		if (parameter.cfsName) CFRelease(parameter.cfsName);
		
	} else {
		return false;
	}
}

bool NDASPreferencesChangeAutoRegister(UInt32 slotNumber, bool autoRegister)
{
	NDASPreferencesParameter	parameter;
	
	parameter.slotNumber = slotNumber;
	
	if(1 == NDASPreferencesQueryBySlotNumber(&parameter)) {
		
		parameter.AutoRegister = autoRegister;
		
		NDASPreferencesInsert(&parameter);
		
		if (parameter.cfsName) CFRelease(parameter.cfsName);
		
	} else {
		return false;
	}
}

CFArrayRef NDASPreferencesGetRegisteredSlotList() 
{
	
	// Read Last Value.
	CFPreferencesSynchronize( 
							 NDAS_PREFERENCES_FILE_REGISTER,
							 kCFPreferencesAnyUser, 
							 kCFPreferencesCurrentHost
							 );
	
	return CFPreferencesCopyKeyList (
									 NDAS_PREFERENCES_FILE_REGISTER,
									 kCFPreferencesAnyUser, 
									 kCFPreferencesCurrentHost
									 );	
}

//
// For Logical Deivces.
//

bool NDASPreferencesChangeConfigurationOfLogicalDevice(UInt32 index, UInt32 configuration)
{
	// Get Logical Device.
	NDASLogicalDeviceInformation	NDASInfo = { 0 };
	
	NDASInfo.index = index;
	
	if(NDASGetLogicalDeviceInformationByIndex(&NDASInfo)) {
	
		CFStringRef	cfsKey = NULL;
		CFNumberRef	cfnConfiguration = NULL;
		CFUUIDRef	cfuGuid = NULL;
		
		switch(configuration) {
			case kNDASUnitConfigurationMountRW:
			case kNDASUnitConfigurationMountRO:
			{
				cfnConfiguration = CFNumberCreate (NULL, kCFNumberSInt32Type, &configuration);
			}
				break;
			case kNDASUnitConfigurationUnmount:
			{
			}
				break;
		}
		
		// GUID is key.
		cfuGuid = CFUUIDCreateWithBytes(kCFAllocatorDefault, 
										NDASInfo.LowerUnitDeviceID.guid[0], NDASInfo.LowerUnitDeviceID.guid[1], NDASInfo.LowerUnitDeviceID.guid[2], NDASInfo.LowerUnitDeviceID.guid[3],
										NDASInfo.LowerUnitDeviceID.guid[4], NDASInfo.LowerUnitDeviceID.guid[5], NDASInfo.LowerUnitDeviceID.guid[6], NDASInfo.LowerUnitDeviceID.guid[7],
										NDASInfo.LowerUnitDeviceID.guid[8], NDASInfo.LowerUnitDeviceID.guid[9], NDASInfo.LowerUnitDeviceID.guid[10], NDASInfo.LowerUnitDeviceID.guid[11],
										NDASInfo.LowerUnitDeviceID.guid[12], NDASInfo.LowerUnitDeviceID.guid[13], NDASInfo.LowerUnitDeviceID.guid[14], NDASInfo.LowerUnitDeviceID.guid		[15]);
		
		cfsKey = CFUUIDCreateString(kCFAllocatorDefault, cfuGuid);
		
		CFRelease(cfuGuid);
		
		// Set Values.
		CFPreferencesSetValue(
							  cfsKey, 
							  cfnConfiguration, 
							  NDAS_PREFERENCES_FILE_CONFIGURATION,
							  kCFPreferencesAnyUser, 
							  kCFPreferencesCurrentHost
							  );
		
		// Write out the preference data.
		CFPreferencesSynchronize( 
								  NDAS_PREFERENCES_FILE_CONFIGURATION,
								  kCFPreferencesAnyUser, 
								  kCFPreferencesCurrentHost
								  );
		
		if (cfsKey)				CFRelease(cfsKey);
		if (cfnConfiguration)	CFRelease(cfnConfiguration);
		
	} else {
		return false;
	}
			
	return true;
}

UInt32 NDASPreferencesGetConfiguration(CFStringRef key)
{
	CFNumberRef				cfnConfigurationValue = NULL;
	UInt32					configuration;
	
	if(key == NULL) {
		return false;
	}
		
	cfnConfigurationValue = CFPreferencesCopyValue (
													key,
													NDAS_PREFERENCES_FILE_CONFIGURATION,
													kCFPreferencesAnyUser,
													kCFPreferencesCurrentHost
													);
	
	if(cfnConfigurationValue == NULL) {
		return false;
	} 

	CFNumberGetValue(cfnConfigurationValue, kCFNumberSInt32Type, &configuration);
	
	CFRelease(cfnConfigurationValue);
	
	return configuration;
}
	
CFArrayRef NDASPreferencesGetLogicalDeviceConfigurationList() 
{
	return CFPreferencesCopyKeyList (
									 NDAS_PREFERENCES_FILE_CONFIGURATION,
									 kCFPreferencesAnyUser, 
									 kCFPreferencesCurrentHost
									 );	
}

bool NDASPreferencesChangeNameOfRAIDDevice(PGUID raidID, CFStringRef name)
{		
	CFStringRef				cfsKey = NULL;
	CFMutableDictionaryRef	dictEntry = NULL;
	CFUUIDRef				cfuGuid= NULL;
	
	if (name) {
		
		// Create Dict
		dictEntry = CFDictionaryCreateMutable (kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		
		if (NULL == dictEntry) {
			return false;
		}
		
		CFDictionaryAddValue(dictEntry, 
							 CFSTR(KEY_NAME_STRING),  
							 name	
							 );
	}
	
	// GUID is key.
	cfuGuid = CFUUIDCreateWithBytes(kCFAllocatorDefault, 
						  raidID->guid[0], raidID->guid[1], raidID->guid[2], raidID->guid[3],
						  raidID->guid[4], raidID->guid[5], raidID->guid[6], raidID->guid[7],
						  raidID->guid[8], raidID->guid[9], raidID->guid[10], raidID->guid[11],
						  raidID->guid[12], raidID->guid[13], raidID->guid[14], raidID->guid[15]);
	if (NULL == cfuGuid) {
		CFRelease(dictEntry);
		return false;
	}
	
	
	cfsKey = CFUUIDCreateString (
								 kCFAllocatorDefault,
								 cfuGuid
								 );
	if (NULL == cfsKey) {
		CFRelease(cfuGuid);
		CFRelease(dictEntry);
		return false;
	}
	
	// Set Values.
	CFPreferencesSetValue(
						  cfsKey, 
						  dictEntry, 
						  NDAS_PREFERENCES_FILE_RAID_INFORMATION,
						  kCFPreferencesAnyUser, 
						  kCFPreferencesCurrentHost
						  );
	
	// Write out the preference data.
	CFPreferencesSynchronize( 
							  NDAS_PREFERENCES_FILE_RAID_INFORMATION,
							  kCFPreferencesAnyUser, 
							  kCFPreferencesCurrentHost
							  );
	
	if (cfsKey)			CFRelease(cfsKey);
	if (dictEntry)		CFRelease(dictEntry);
	if (cfuGuid)		CFRelease(cfuGuid);
	
	return true;
}

CFStringRef NDASPreferencesGetRAIDNameByUUIDString(CFStringRef cfsUUIDString)
{
	CFDictionaryRef			dictRef = NULL;
	CFStringRef				strNameValue = NULL;
	bool					present;
		
	if (NULL == cfsUUIDString) {
		return NULL;
	}
	
	dictRef = CFPreferencesCopyValue (
									  cfsUUIDString,
									  NDAS_PREFERENCES_FILE_RAID_INFORMATION,
									  kCFPreferencesAnyUser,
									  kCFPreferencesCurrentHost
									  );
	
	if(dictRef == NULL) {
		return NULL;
	} 
		
	present = CFDictionaryGetValueIfPresent(
											dictRef,
											CFSTR(KEY_NAME_STRING),
											(const void**)&strNameValue
											);
	
	CFRelease(dictRef);
	
	if(!present || !strNameValue) {
		// Write Key is Optional.
		return NULL;
	} else {
		
		CFRetain(strNameValue);
		
		if(CFStringGetTypeID() == CFGetTypeID(strNameValue)) {
			return strNameValue;
		}
	}
}

CFArrayRef NDASPreferencesGetRAIDDeviceInformationList() 
{
	return CFPreferencesCopyKeyList (
									 NDAS_PREFERENCES_FILE_RAID_INFORMATION,
									 kCFPreferencesAnyUser, 
									 kCFPreferencesCurrentHost
									 );	
}
