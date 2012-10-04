#include <getopt.h>
#include <stdlib.h>
#include <syslog.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <unistd.h>
#include <mach-o/dyld.h>

#include <CoreServices/CoreServices.h>
#include <SystemConfiguration/SystemConfiguration.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/network/IOEthernetInterface.h>
#include <IOKit/network/IOEthernetController.h>

#include "GetEthernetAddrSample.h"

#include "NDASNotification.h"
#include "NDASFamilyIOMessage.h"
#include "NDASGetInformation.h"
#include "ndasHixUtil.h"

#include "NDASPreferences.h"
#include "NDASPreferencesLegacy.h"
#include "NDASIOConnect.h"

#include "IOKitUtil.h"

#include "../Inc/serial.h"
#include "../Inc/NDASID.h"

extern char ** environ;

// Global Variable.
CFUUIDRef					hixHostUUID;	
CFUUIDBytes					hostID;
CFMutableDictionaryRef		raidStatusDictionary;
char						hostIDKey[8] = { 0 };
io_object_t					IOResourcesNotification = NULL; 
io_object_t					DriverNotification = NULL; 

#define						NUMBER_OF_NICS	16
EnetData					ethernetData[NUMBER_OF_NICS];
UInt32						numberOfNics;

void DeviceDriverNotificationCallback( 
									 void * refcon, 
									 io_service_t service, 
									 natural_t messageType, 
									 void * messageArgument );

void IOResourcesNotificationCallback( 
									 void * refcon, 
									 io_service_t service, 
									 natural_t messageType, 
									 void * messageArgument );

void processPreferences();

void SendNotificationToUI(
						  uint32_t		operation,
						  uint32_t		errorCode,
						  CFStringRef	cfsSenderID
						  )
{
	// Send Notification to UI.	
	SInt32					osVersion; 
	CFMutableDictionaryRef	reply = NULL;
	CFStringRef				cfsOperationKey = NULL;
	CFNumberRef				cfnOperation = NULL;
	CFStringRef				cfsErrorCodeKey = NULL;
	CFNumberRef				cfnErrorCode = NULL;
	CFStringRef				cfsSenderIDKey = NULL;
	
	// Create Reply
	reply = CFDictionaryCreateMutable(kCFAllocatorDefault, 3, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if ( NULL != reply ) {
				
		// Operation.
		cfsOperationKey = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%s"), NDAS_NOTIFICATION_REPLY_OPERATION_KEY_STING);
		cfnOperation = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &operation);
		
		CFDictionaryAddValue (
							  reply,
							  cfsOperationKey,
							  cfnOperation
							  );
		
		// Error Code.
		cfsErrorCodeKey = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%s"), NDAS_NOTIFICATION_REPLY_ERROR_CODE_KEY_STRING);
		cfnErrorCode = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &errorCode);
		
		CFDictionaryAddValue (
							  reply,
							  cfsErrorCodeKey,
							  cfnErrorCode
							  );
				
		// Sender ID.
		cfsSenderIDKey = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%s"), NDAS_NOTIFICATION_REPLY_SENDER_ID_KEY_STRING);
		
		if(cfsSenderID) {
			CFDictionaryAddValue (
								  reply,
								  cfsSenderIDKey,
								  cfsSenderID
								  );					
		}
		
	}
	
	if (Gestalt(gestaltSystemVersion, &osVersion) == noErr) 
	{ 
		if ( osVersion >= 0x1030 ) 
		{
			CFNotificationCenterPostNotificationWithOptions (
															 CFNotificationCenterGetDistributedCenter(),
															 CFSTR(NDAS_UI_OBJECT_ID),
															 CFSTR(NDAS_NOTIFICATION),
															 reply,
															 kCFNotificationDeliverImmediately | kCFNotificationPostToAllSessions
															 );
			
		} else {
			CFNotificationCenterPostNotification(CFNotificationCenterGetDistributedCenter(),
												 CFSTR(NDAS_UI_OBJECT_ID),
												 CFSTR(NDAS_NOTIFICATION),
												 reply,
												 true);
			
		}
	} 
	
	if ( cfsOperationKey )	CFRelease ( cfsOperationKey );
	if ( cfnOperation )		CFRelease ( cfnOperation );
	if ( cfsErrorCodeKey )	CFRelease ( cfsErrorCodeKey );
	if ( cfnErrorCode )		CFRelease ( cfnErrorCode );
	if ( cfsSenderIDKey )	CFRelease ( cfsSenderIDKey );

	if ( reply )	CFRelease( reply );
}

/***************************************************************************
*
*  _UserNotificationCallback
*    Callback to process incoming notification.
*
**************************************************************************/
void _UserNotificationCallback(
							   CFNotificationCenterRef	center,
                               void*					observer,
                               CFStringRef				cfsNotificationName,
                               const void*				object,
                               CFDictionaryRef			cfdUserInfo
							   )
{
	UInt32			request = 0;
	UInt32			slotNumber = 0;
	UInt32			unitNumber = 0;
	UInt32			index = 0;
	UInt32			options = 0;
	UInt32			configuration = kNDASNumberOfUnitConfiguration;
	UInt32			errorCode = 0;
	CFNumberRef 	cfnRequest = NULL;
	CFStringRef		cfsSenderID = NULL;
	CFNumberRef 	cfnSlotNumber = NULL;
	CFNumberRef		cfnUnitNumber = NULL;
	CFNumberRef		cfnIndex = NULL;
	CFStringRef		cfsName = NULL;
	CFStringRef		cfsIDSeg1 = NULL;
	CFStringRef		cfsIDSeg2 = NULL;
	CFStringRef		cfsIDSeg3 = NULL;
	CFStringRef		cfsIDSeg4 = NULL;
	CFStringRef		cfsWrKey  = NULL;
	CFNumberRef 	cfnConfig = NULL;
	CFNumberRef		cfnOptions = NULL;
	
	syslog(LOG_DEBUG, "_UserNotificationCallback: Entered.");
	
	// Paramets are passing by cfdUserInfo.
	if (cfdUserInfo) {

		CFRetain(cfdUserInfo);
				
		// Get Request.
		cfnRequest = (CFNumberRef)CFDictionaryGetValue(cfdUserInfo, CFSTR(NDAS_NOTIFICATION_REQUEST_KEY_STRING));
		if (cfnRequest) {
			CFRetain(cfnRequest);
			CFNumberGetValue(cfnRequest, kCFNumberIntType, &request);			
		} else {
			// No Request Key.
			errorCode = NDAS_NOTIFICATION_ERROR_INVALID_OPERATION;
		}

		// Get Sender ID.
		cfsSenderID = (CFStringRef)CFDictionaryGetValue(cfdUserInfo, CFSTR(NDAS_NOTIFICATION_SENDER_ID_KEY_STRING));
		if (cfsSenderID) {
			CFRetain(cfsSenderID);
		}

		// Get Slot Number.
		cfnSlotNumber = (CFNumberRef)CFDictionaryGetValue(cfdUserInfo, CFSTR(NDAS_NOTIFICATION_SLOTNUMBER_KEY_STRING));
		if (cfnSlotNumber) {
			CFRetain(cfnSlotNumber);
			CFNumberGetValue(cfnSlotNumber, kCFNumberIntType, &slotNumber);
		}

		// Get Unit Number.
		cfnUnitNumber = (CFNumberRef)CFDictionaryGetValue(cfdUserInfo, CFSTR(NDAS_NOTIFICATION_UNITNUMBER_KEY_STRING));
		if (cfnUnitNumber) {
			CFRetain(cfnUnitNumber);
			CFNumberGetValue(cfnUnitNumber, kCFNumberIntType, &unitNumber);
		}

		// Get Index.
		cfnIndex = (CFNumberRef)CFDictionaryGetValue(cfdUserInfo, CFSTR(NDAS_NOTIFICATION_INDEX_KEY_STRING));
		if (cfnIndex) {
			CFRetain(cfnIndex);
			CFNumberGetValue(cfnIndex, kCFNumberIntType, &index);			
		}

		// Get Options.
		cfnOptions = (CFNumberRef)CFDictionaryGetValue(cfdUserInfo, CFSTR(NDAS_NOTIFICATION_OPTIONS_KEY_STRING));
		if (cfnOptions) {
			CFRetain(cfnOptions);
			CFNumberGetValue(cfnOptions, kCFNumberIntType, &options);
		}
		
		syslog(LOG_DEBUG, "Request %u\n", request);
		
		switch(request) {
			case NDAS_NOTIFICATION_REQUEST_REGISTER:
			{
				NDASPreferencesParameter	parameter = { 0 };
				
				// Get Parameters.
				cfsName = (CFStringRef)CFDictionaryGetValue(cfdUserInfo, CFSTR(NDAS_NOTIFICATION_NAME_STRING));
				if(cfsName) CFRetain(cfsName);
				
				cfsIDSeg1 = (CFStringRef)CFDictionaryGetValue(cfdUserInfo, CFSTR(NDAS_NOTIFICATION_ID_1_KEY_STRING));
				if(cfsIDSeg1) { CFRetain(cfsIDSeg1); }
				else { break; }
				
				cfsIDSeg2 = (CFStringRef)CFDictionaryGetValue( cfdUserInfo, CFSTR(NDAS_NOTIFICATION_ID_2_KEY_STRING));
				if(cfsIDSeg2) { CFRetain(cfsIDSeg2); }
				else { break; }
				
				cfsIDSeg3 = (CFStringRef)CFDictionaryGetValue( cfdUserInfo, CFSTR(NDAS_NOTIFICATION_ID_3_KEY_STRING));
				if(cfsIDSeg3) { CFRetain(cfsIDSeg3); }
				else { break; }
				
				cfsIDSeg4 = (CFStringRef)CFDictionaryGetValue( cfdUserInfo, CFSTR(NDAS_NOTIFICATION_ID_4_KEY_STRING));
				if(cfsIDSeg4) CFRetain(cfsIDSeg4);
				else { break; }
				
				cfsWrKey  = (CFStringRef)CFDictionaryGetValue( cfdUserInfo, CFSTR(NDAS_NOTIFICATION_WRITEKEY_KEY_STRING));
				if(cfsWrKey) CFRetain(cfsWrKey);
				
				cfnConfig = (CFNumberRef)CFDictionaryGetValue( cfdUserInfo, CFSTR(NDAS_NOTIFICATION_CONFIGURATION_KEY_STRING));
				if(cfnConfig) CFRetain(cfnConfig);
				
				// Convert to NDASPreferenceParameter.
				parameter.slotNumber = slotNumber;
				
				if (cfsName) {
					parameter.cfsName = CFStringCreateCopy(kCFAllocatorDefault, cfsName);
				}
				
//				CFStringGetCString( cfsName, parameter.Name, MAX_NDAS_NAME_SIZE, kCFStringEncodingUTF8); //CFStringEncoding(cfsName));
				CFStringGetCString( cfsIDSeg1, parameter.chID[0], 6, CFStringGetSystemEncoding());
				CFStringGetCString( cfsIDSeg2, parameter.chID[1], 6, CFStringGetSystemEncoding());
				CFStringGetCString( cfsIDSeg3, parameter.chID[2], 6, CFStringGetSystemEncoding());
				CFStringGetCString( cfsIDSeg4, parameter.chID[3], 6, CFStringGetSystemEncoding());
				
				if (cfsWrKey) {
					CFStringGetCString( cfsWrKey, parameter.chWriteKey, 6, CFStringGetSystemEncoding());
				}
				
				if (cfnConfig) {
					CFNumberGetValue(cfnConfig, kCFNumberIntType, &parameter.configuration);
				}
				
				for(int count = 0; count < MAX_NR_OF_TARGETS_PER_DEVICE; count++) {
					parameter.Units[count].configuration = kNDASUnitConfigurationUnmount;
				}
				
				sprintf(parameter.DeviceIdString, "%s-%s-%s-xxxxx", parameter.chID[0], parameter.chID[1], parameter.chID[2]);
				
				parameter.AutoRegister = false;
				
				kern_return_t returnValue;
				bool			bDuplicated;
				
				if( ( returnValue = NDASIOConnectRegister(&parameter, &bDuplicated) ) == KERN_SUCCESS) {
					
					//CFStringGetCString( cfsName, parameter.Name, MAX_NDAS_NAME_SIZE, CFStringGetSystemEncoding());
					//memcpy(parameter.Name, CFStringGetCharactersPtr(cfsName), MAX_NDAS_NAME_SIZE / 2);
					if (!bDuplicated) {
						
						if (1 == NDASPreferencesRegister(&parameter)) {
							
							if(parameter.cfsName)	CFRelease(parameter.cfsName);
							
							NDASIOConnectEnable(parameter.slotNumber);
							NDASPreferencesChangeConfigurationOfDevice(parameter.slotNumber, kNDASConfigurationEnable);
							
							errorCode = NDAS_NOTIFICATION_ERROR_SUCCESS;
						} else {
							errorCode = NDAS_NOTIFICATION_ERROR_FAIL;
						}
					} else {
						errorCode = NDAS_NOTIFICATION_ERROR_ALREADY_REGISTERED;
						
						// NDAS info can be deleted.
						if (0 == NDASPreferencesQueryByID(&parameter)) {
							syslog(LOG_INFO, "NDAS Info was missing. rewrite NDAS device info. at slot %d\n", parameter.slotNumber);
							
							NDASPreferencesRegister(&parameter);			
						}
					}
					
				} else {
					errorCode = NDAS_NOTIFICATION_ERROR_FAIL;
				}
			}
				break;
			case NDAS_NOTIFICATION_REQUEST_UNREGISTER:
			{
				/*
				for(int count = 0; count < MAX_NR_OF_TARGETS_PER_DEVICE; count++) {
					NDASIOConnectUnmount(index);
					NDASPreferencesChangeConfigurationOfUnit(slotNumber, count, kNDASConfigurationDisable);
				}
				*/
				NDASIOConnectDisable(slotNumber);
				NDASPreferencesChangeConfigurationOfDevice(slotNumber, kNDASConfigurationDisable);
				
				NDASIOConnectUnregister(slotNumber);
				NDASPreferencesUnregister(slotNumber);
				
				errorCode = NDAS_NOTIFICATION_ERROR_SUCCESS;
			}
				break;
			case NDAS_NOTIFICATION_REQUEST_CONFIGURATE_NDAS:
			{
				cfnConfig = (CFNumberRef)CFDictionaryGetValue( cfdUserInfo, CFSTR(NDAS_NOTIFICATION_CONFIGURATION_KEY_STRING));
				if(cfnConfig) CFRetain(cfnConfig);
				
				CFNumberGetValue(cfnConfig, kCFNumberIntType, &configuration);
				
				switch(configuration) {
					case kNDASConfigurationEnable:
					{
						NDASIOConnectEnable(slotNumber);
					}
						break;
					case kNDASConfigurationDisable:
					default:
					{
						NDASIOConnectDisable(slotNumber);
					}
						break;
				}
				
				NDASPreferencesChangeConfigurationOfDevice(slotNumber, configuration);
				
				errorCode = NDAS_NOTIFICATION_ERROR_SUCCESS;
			}
				break;
			case NDAS_NOTIFICATION_REQUEST_CONFIGURATE_LOGICAL_DEVICE:
			{
				cfnConfig = (CFNumberRef)CFDictionaryGetValue( cfdUserInfo, CFSTR(NDAS_NOTIFICATION_CONFIGURATION_KEY_STRING));
				if(cfnConfig) {
					CFRetain(cfnConfig);
					CFNumberGetValue(cfnConfig, kCFNumberIntType, &configuration);
				}
								
				switch(configuration) {
					case kNDASUnitConfigurationMountRW:
					{
						NDASIOConnectMount(index, true);
					}
						break;
					case kNDASUnitConfigurationMountRO:
					{
						NDASIOConnectMount(index, false);
					}
						break;
					default:
					{
						NDASIOConnectUnmount(index);
					}
						break;
				}		
				
				NDASPreferencesChangeConfigurationOfLogicalDevice(index, configuration);
			}
				break;
			case NDAS_NOTIFICATION_REQUEST_EDIT_NDAS_NAME:
			{				
				cfsName = (CFStringRef)CFDictionaryGetValue(cfdUserInfo, CFSTR(NDAS_NOTIFICATION_NAME_STRING));
				if(cfsName) CFRetain(cfsName);
												
				if ( NULL != cfsName ) {
					// Change Name.
					NDASPreferencesParameter parameter;
					
					parameter.slotNumber = slotNumber;
					
					parameter.cfsName = cfsName; //CFStringCreateCopy(kCFAllocatorDefault, cfsName);
					
//					CFStringGetCString( cfsName, parameter.Name, MAX_NDAS_NAME_SIZE, kCFStringEncodingUTF8); //CFStringEncoding(cfsName));
					
					//parameter.cfsName = cfsName;
					
					if ( KERN_SUCCESS != NDASIOConnectEdit(kNDASEditTypeName, &parameter) ) {
						errorCode = NDAS_NOTIFICATION_ERROR_FAIL;
						
						syslog(LOG_ERR, "_UserNotificationCallback: Can't Edit Name");
					} else {
						errorCode = NDAS_NOTIFICATION_ERROR_SUCCESS;
						
//						CFStringGetCString( cfsName, parameter.Name, MAX_NDAS_NAME_SIZE, CFStringGetSystemEncoding());
						
						NDASPreferencesChangeNameOfDevice(slotNumber, cfsName);
						
					}		
					
//					if (parameter.cfsName)	CFRelease(parameter.cfsName);
				}
			}
				break;
			case NDAS_NOTIFICATION_REQUEST_EDIT_NDAS_WRITEKEY:
			{	
				NDASPreferencesParameter	parameter;
				
				// Get ID.
				parameter.slotNumber = slotNumber;
				
				if ( 1 != NDASPreferencesQueryBySlotNumber(&parameter) ) {
					errorCode = NDAS_NOTIFICATION_ERROR_INVALID_SLOT_NUMBER;
				} else {
				
					if ( cfsWrKey = (CFStringRef)CFDictionaryGetValue( cfdUserInfo, CFSTR(NDAS_NOTIFICATION_WRITEKEY_KEY_STRING)) ) {
						CFRetain(cfsWrKey);
						CFStringGetCString( cfsWrKey, parameter.chWriteKey, 6, CFStringGetSystemEncoding());
					} else {
						memset(parameter.chWriteKey, 0, 6);
					}
					
					if ( KERN_SUCCESS != NDASIOConnectEdit(kNDASEditTypeWriteAccessRight, &parameter) ) {
						errorCode = NDAS_NOTIFICATION_ERROR_FAIL;
					} else {
						errorCode = NDAS_NOTIFICATION_ERROR_SUCCESS;
						
						NDASPreferencesChangeWriteKeyOfDevice(slotNumber, parameter.chWriteKey);
					}
					
					if (parameter.cfsName) CFRelease(parameter.cfsName);
				}				
			}
				break;
			case NDAS_NOTIFICATION_REQUEST_SURRENDER_ACCESS:
			{
				// Get Address.
				NDASPhysicalDeviceInformation	parameter;
				
				parameter.slotNumber = slotNumber;
				
				if ( NDASGetInformationBySlotNumber(&parameter) ) {
					
					NDAS_HIX_UNITDEVICE_ENTRY_DATA	data;
					bool							result;
					struct sockaddr_lpx				hostAddr;
					int								numberOfHosts;
					
					data.AccessType = NHIX_UDA_WO;
					data.UnitNo = unitNumber;
					memcpy(data.DeviceId, parameter.address, 6);
					
					if (1 == (numberOfHosts = pSendDiscover(hostID,
															&data,
															&hostAddr))) {
						
						// Send Surrender Access request.
						result = pSendSurrenderAccess(
													  hostID,
													  &data,
													  &hostAddr
													  );
						
						
						if ( result ) {
							errorCode = NDAS_NOTIFICATION_ERROR_SUCCESS;
						} else {
							errorCode = NDAS_NOTIFICATION_ERROR_FAIL;
						}
					} else {
						syslog(LOG_WARNING, "_UserNotificationCallback: I didn't send Surrender Request. Hosts %d", numberOfHosts);
						if (0 == numberOfHosts) {
							errorCode = NDAS_NOTIFICATION_ERROR_SUCCESS;
						} else {
							errorCode = NDAS_NOTIFICATION_ERROR_FAIL;
						}
					}
					
				} else {
					errorCode = NDAS_NOTIFICATION_ERROR_INVALID_SLOT_NUMBER;
				}
			}
				break;				
			case NDAS_NOTIFICATION_REQUEST_BIND:
			{
				CFNumberRef		cfnNumber = NULL;
				CFBooleanRef	cfbSync = NULL;
				UInt32			numberOfLogicalDevices = 0;
				UInt32			raidType = NMT_INVALID;
				UInt32			LogicalDeviceArray[NDAS_MAX_UNITS_IN_V2] = { 0 };
				bool			bSync;
				
				// Get RAID Type.
				cfnNumber = (CFNumberRef)CFDictionaryGetValue( cfdUserInfo, CFSTR(NDAS_NOTIFICATION_RAID_TYPE_KEY_STRING));
				if(cfnNumber) {
					CFRetain(cfnNumber);
					CFNumberGetValue(cfnNumber, kCFNumberIntType, &raidType);
					if(cfnNumber) CFRelease(cfnNumber);				
				}
				
				syslog(LOG_DEBUG, "RAID Type %d\n", raidType);
					
				bSync = false;
				
				if (NMT_RAID1R3 == raidType) {
					// Get Sync.
					cfbSync = (CFBooleanRef)CFDictionaryGetValue( cfdUserInfo, CFSTR(NDAS_NOTIFICATION_RAID_SYNC_FLAG_STRING));
					if(cfbSync) {
						CFRetain(cfbSync);
						 bSync = CFBooleanGetValue(cfbSync);
						if(cfbSync) CFRelease(cfbSync);				
					}	
				}
				
				// Get Number Of Logical Devices.
				cfnNumber = (CFNumberRef)CFDictionaryGetValue( cfdUserInfo, CFSTR(NDAS_NOTIFICATION_NUMBER_OF_LOGICAL_DEVICES_KEY_STRING));
				if(cfnNumber) {
					CFRetain(cfnNumber);
					CFNumberGetValue(cfnNumber, kCFNumberIntType, &numberOfLogicalDevices);				
					if(cfnNumber) CFRelease(cfnNumber);				
				}
				
				if (numberOfLogicalDevices > NDAS_MAX_UNITS_IN_V2) {
					
					syslog(LOG_ERR, "_UserNotificationCallback: Too many Logical Devices %d\n", numberOfLogicalDevices);
					
					errorCode = NDAS_NOTIFICATION_ERROR_INVALID_PARAMETER;
					
					break;
				}

				int arrayIndex;
				UNIT_DISK_LOCATION	locationArray[NDAS_MAX_UNITS_IN_V2] = { 0 };

				arrayIndex = 0;
				
				// Get Logical Devices.
				for (int count = 0; count < numberOfLogicalDevices; count++) {
					
					CFStringRef	cfsKey = NULL;

					cfsKey = CFStringCreateWithFormat(NULL, NULL, CFSTR(NDAS_NOTIFICATION_LOGICAL_DEVICE_INDEX_KEY_STRING), count);

					// Get Number Of Logical Devices.
					cfnNumber = (CFNumberRef)CFDictionaryGetValue( cfdUserInfo, cfsKey );
					if(cfnNumber) {
						
						NDASLogicalDeviceInformation	LDInfo = { 0 };
						
						CFRetain(cfnNumber);
						CFNumberGetValue(cfnNumber, kCFNumberIntType, &LogicalDeviceArray[count]);			
						
						LDInfo.index = LogicalDeviceArray[count];
						
						if (NDASGetLogicalDeviceInformationByIndex(&LDInfo)) {
							
							memcpy(&locationArray[arrayIndex++], &(((char*)(&LDInfo.LowerUnitDeviceID))[8]), sizeof(UNIT_DISK_LOCATION));							
						}
						
						if(cfnNumber) CFRelease(cfnNumber);	
					}
				
					if(cfsKey) CFRelease(cfsKey);
				}
				
				GUID	raidID;
				
				if ( KERN_SUCCESS == NDASIOConnectBind(raidType, numberOfLogicalDevices, LogicalDeviceArray, &raidID, bSync)) {
					
					// Send HIX Message.
					for (int count = 0; count < arrayIndex; count++) {
						
						NDAS_HIX_UNITDEVICE_CHANGE_NOTIFY	unitDeviceNotify = { 0 };
						
						memcpy(unitDeviceNotify.DeviceId, locationArray[count].MACAddr, 6);
						unitDeviceNotify.UnitNo = locationArray[count].UnitNumber;
						
						pSendUnitDeviceChangeNotification(hostID, &unitDeviceNotify);						
					}
					
					// Get RAID Name.
					cfsName = (CFStringRef)CFDictionaryGetValue(cfdUserInfo, CFSTR(NDAS_NOTIFICATION_NAME_STRING));
					if(cfsName) {
												
						NDASRAIDDeviceInformation	NDASInfo = { 0 };
						NDASPreferencesParameter parameter = { 0 };
						
						CFRetain(cfsName);
						
						NDASInfo.deviceID = raidID;
						
						int retry = 15;
						bool result;
						
						while(!(result = NDASGetRAIDDeviceInformationByDeviceID(&NDASInfo)) && retry > 0) {
							retry--;
							sleep(1);	// Wait 1 sec.							
						}
						
						if (result) {
														
							parameter.slotNumber = NDASInfo.index; 
							parameter.cfsName = cfsName; //CFStringCreateCopy(kCFAllocatorDefault, cfsName);
														 //						CFStringGetCString( cfsName, parameter.Name, MAX_NDAS_NAME_SIZE, kCFStringEncodingUTF8); //CFStringEncoding(cfsName));
							
							NDASIOConnectBindEdit(kNDASEditTypeName, NDASInfo.index, parameter.cfsName, 0); 
												 
							NDASPreferencesChangeNameOfRAIDDevice(&NDASInfo.deviceID, cfsName);
						}
					}

				}
			}
				break;
			case NDAS_NOTIFICATION_REQUEST_UNBIND:
			{
				NDASRAIDDeviceInformation	raidInfo = { 0 };
				
				raidInfo.index = index;
				if (NDASGetRAIDDeviceInformationByIndex(&raidInfo)) {

					int arrayIndex;
					UNIT_DISK_LOCATION	locationArray[NDAS_MAX_UNITS_IN_V2] = { 0 };

					arrayIndex = 0;
					
					for (int count = 0; count < raidInfo.CountOfUnitDevices; count++) {
						memcpy(&locationArray[arrayIndex++], &raidInfo.unitDiskLocationOfSubUnits[count], sizeof(UNIT_DISK_LOCATION));
					}
					
					if (KERN_SUCCESS == NDASIOConnectUnbind(index)) {
												
						// Wait Unbinding Ends.						
						int retry = 15;
						bool result;
						
						while((result = NDASGetRAIDDeviceInformationByDeviceID(&raidInfo)) && retry > 0) {
							retry--;
							sleep(1);	// Wait 1 sec.							
						}
						
						// Send HIX Message.
						for (int count = 0; count < arrayIndex; count++) {
							
							NDAS_HIX_UNITDEVICE_CHANGE_NOTIFY	unitDeviceNotify = { 0 };
							
							memcpy(unitDeviceNotify.DeviceId, locationArray[count].MACAddr, 6);
							unitDeviceNotify.UnitNo = locationArray[count].UnitNumber;
							
							pSendUnitDeviceChangeNotification(hostID, &unitDeviceNotify);
							
						}
						
						// Delete RAID name.
						NDASPreferencesChangeNameOfRAIDDevice(&raidInfo.deviceID, NULL);
						
					}

				} 
				
			}
				break;		
			case NDAS_NOTIFICATION_REQUEST_BIND_EDIT:
			{
				cfsName = (CFStringRef)CFDictionaryGetValue(cfdUserInfo, CFSTR(NDAS_NOTIFICATION_NAME_STRING));
				if(cfsName) CFRetain(cfsName);
				
				if ( NULL != cfsName ) {
					// Change Name.
					if ( KERN_SUCCESS != NDASIOConnectBindEdit(kNDASEditTypeName, index, cfsName, 0) ) {
						errorCode = NDAS_NOTIFICATION_ERROR_FAIL;
					} else {
						
						NDASRAIDDeviceInformation	raidInfo = { 0 };

						errorCode = NDAS_NOTIFICATION_ERROR_SUCCESS;
									
						raidInfo.index = index;
						
						if (NDASGetRAIDDeviceInformationByIndex(&raidInfo)) {
							NDASPreferencesChangeNameOfRAIDDevice(&raidInfo.deviceID, cfsName);
						}						
					}					
				} 
				
				// Get RAID Type.
				CFNumberRef cfnNumber = NULL;
				UInt32		raidType;
				
				cfnNumber = (CFNumberRef)CFDictionaryGetValue( cfdUserInfo, CFSTR(NDAS_NOTIFICATION_RAID_TYPE_KEY_STRING));
				if(cfnNumber) {
					CFRetain(cfnNumber);
					CFNumberGetValue(cfnNumber, kCFNumberIntType, &raidType);
					if(cfnNumber) CFRelease(cfnNumber);				
					
					syslog(LOG_DEBUG, "new Type %d\n", raidType);
					
					if (NMT_INVALID !=  raidType) {
						
						NDASRAIDDeviceInformation	raidInfo = { 0 };
						
						// Change RAID Type.					
						raidInfo.index = index;
						if (NDASGetRAIDDeviceInformationByIndex(&raidInfo)) {
							
							int arrayIndex;
							UNIT_DISK_LOCATION	locationArray[NDAS_MAX_UNITS_IN_V2] = { 0 };
							
							arrayIndex = 0;
							
							for (int count = 0; count < raidInfo.CountOfUnitDevices; count++) {
								memcpy(&locationArray[arrayIndex++], &raidInfo.unitDiskLocationOfSubUnits[count], sizeof(UNIT_DISK_LOCATION));
							}
							
							if ( KERN_SUCCESS != NDASIOConnectBindEdit(kNDASEditTypeRAIDType, index, NULL, raidType) ) {
								errorCode = NDAS_NOTIFICATION_ERROR_FAIL;
							} else {
								errorCode = NDAS_NOTIFICATION_ERROR_SUCCESS;
								
								// Wait Unbinding Ends.						
								int retryCount = 15;
								bool result;
								
								while( !( result = NDASGetRAIDDeviceInformationByDeviceID(&raidInfo) &&
										raidType == raidInfo.unitDeviceType )
									  && retryCount > 0) {
									retryCount--;
									sleep(1);	// Wait 1 sec.							
								}
								
								// Send HIX Message.
								for (int count = 0; count < arrayIndex; count++) {
									
									NDAS_HIX_UNITDEVICE_CHANGE_NOTIFY	unitDeviceNotify = { 0 };
									
									memcpy(unitDeviceNotify.DeviceId, locationArray[count].MACAddr, 6);
									unitDeviceNotify.UnitNo = locationArray[count].UnitNumber;
									
									pSendUnitDeviceChangeNotification(hostID, &unitDeviceNotify);
									
								}
							}
						}
					}
				}
			}
				break;	
			case NDAS_NOTIFICATION_REQUEST_REBIND:
			{
				NDASRAIDDeviceInformation	raidInfo = { 0 };
				
				raidInfo.index = index;
				if (NDASGetRAIDDeviceInformationByIndex(&raidInfo)) {
					
					int arrayIndex;
					UNIT_DISK_LOCATION	locationArray[NDAS_MAX_UNITS_IN_V2] = { 0 };
					
					arrayIndex = 0;
					
					for (int count = 0; count < raidInfo.CountOfUnitDevices; count++) {
						memcpy(&locationArray[arrayIndex++], &raidInfo.unitDiskLocationOfSubUnits[count], sizeof(UNIT_DISK_LOCATION));
					}
					
					if (KERN_SUCCESS == NDASIOConnectRebind(index)) {
						
						// Wait Unbinding Ends.						
						int retry = 15;
						bool result;
						
						while((result = NDASGetRAIDDeviceInformationByDeviceID(&raidInfo)) && retry > 0) {
							retry--;
							sleep(1);	// Wait 1 sec.							
						}
						
						// Send HIX Message.
						for (int count = 0; count < arrayIndex; count++) {
							
							NDAS_HIX_UNITDEVICE_CHANGE_NOTIFY	unitDeviceNotify = { 0 };
							
							memcpy(unitDeviceNotify.DeviceId, locationArray[count].MACAddr, 6);
							unitDeviceNotify.UnitNo = locationArray[count].UnitNumber;
							
							pSendUnitDeviceChangeNotification(hostID, &unitDeviceNotify);
							
						}
						
						// Delete RAID name.
						NDASPreferencesChangeNameOfRAIDDevice(&raidInfo.deviceID, NULL);
						
					}
					
				} 
				
			}
				break;		
			case NDAS_NOTIFICATION_REQUEST_REFRESH:
			{
				
				switch(options) {
					case NDAS_NOTIFICATION_OPTION_FOR_PHYSICAL_UNIT:
					{
						// Send Refresh Request.
						NDASIOConnectRefresh(slotNumber, unitNumber);						
					}
						break;
					case NDAS_NOTIFICATION_OPTION_FOR_RAID_UNIT:
					{
						NDASRAIDDeviceInformation	raidInfo = { 0 };
						
						raidInfo.index = index;
						
						if (NDASGetRAIDDeviceInformationByIndex(&raidInfo)) {
							
							
							for (int count = 0; count < raidInfo.CountOfUnitDevices; count++) {
								
								// Search NDAS Unit.
								NDASPhysicalDeviceInformation	deviceInfo = { 0 };
								UNIT_DISK_LOCATION	subUnitLocation;
								
								memcpy(&subUnitLocation, &raidInfo.unitDiskLocationOfSubUnits[count], sizeof(UNIT_DISK_LOCATION));
								
								memcpy(deviceInfo.address, subUnitLocation.MACAddr, 6);
								
								if (NDASGetInformationByAddress(&deviceInfo)) {
									NDASIOConnectRefresh(deviceInfo.slotNumber, subUnitLocation.UnitNumber);
								}
							}
						}						
					}
						break;
				}				
			}
				break;
			default:
				
				syslog(LOG_ERR, "_UserNotificationCallback: Bad Request!");
				
				errorCode = NDAS_NOTIFICATION_ERROR_INVALID_OPERATION;
				
				break;
		}
		
		// Send Reply.
		SendNotificationToUI(
							 request,
							 errorCode,
							 cfsSenderID
							 );
	}
	

	if(cfnRequest)		CFRelease(cfnRequest);
	
	if(cfsSenderID)		CFRelease(cfsSenderID);
	if(cfnSlotNumber)	CFRelease(cfnSlotNumber);
	if(cfnUnitNumber)	CFRelease(cfnUnitNumber);
	if(cfnIndex)		CFRelease(cfnIndex);
	if(cfsName)			CFRelease(cfsName);
	if(cfsIDSeg1)		CFRelease(cfsIDSeg1);
	if(cfsIDSeg2)		CFRelease(cfsIDSeg2);
	if(cfsIDSeg3)		CFRelease(cfsIDSeg3);
	if(cfsIDSeg4)		CFRelease(cfsIDSeg4);
	if(cfsWrKey)		CFRelease(cfsWrKey);
	if(cfnConfig)		CFRelease(cfnConfig);
	if(cfnOptions)		CFRelease(cfnOptions);
	
	if(cfdUserInfo)		CFRelease(cfdUserInfo);

	return;
}

void UpdateTimerCallback(
						 CFRunLoopTimerRef timer, 
						 void *info)
{
	syslog(LOG_DEBUG, "UpdateTimerCallback: Entered");
	
	// Send Mount Command.
	CFArrayRef	configurationArray = NDASPreferencesGetLogicalDeviceConfigurationList();
	
	if (configurationArray != NULL && 0 < CFArrayGetCount(configurationArray)) {
		
		for (int count = 0; count < CFArrayGetCount(configurationArray); count++) {
			CFStringRef	cfsKey = NULL;
			CFUUIDRef	cfuGuid = NULL;
				
			cfsKey = (CFStringRef)CFArrayGetValueAtIndex(configurationArray, count);
			
			if (cfsKey) {
				
				CFRetain(cfsKey);
				
				cfuGuid = CFUUIDCreateFromString(kCFAllocatorDefault, cfsKey);

				if (cfuGuid) {
					
					NDASLogicalDeviceInformation	NDASInfo = { 0 };
					
					
					memcpy(&NDASInfo.LowerUnitDeviceID, &(CFUUIDGetUUIDBytes(cfuGuid).byte0), sizeof(GUID));
					
					CFRelease(cfuGuid);
					
					//syslog(LOG_DEBUG, "DeviceDriverNotificationCallback: LUD ID %lld\n", NDASInfo.LowerUnitDeviceID);
					
					if (NDASGetLogicalDeviceInformationByLowerUnitDeviceID(&NDASInfo)) {
						
						// Get Configuration.
						
						switch(NDASPreferencesGetConfiguration(cfsKey)) {
							case kNDASUnitConfigurationMountRW:
							{
								NDASIOConnectMount(NDASInfo.index, true);
							}
								break;
							case kNDASUnitConfigurationMountRO:
							{
								NDASIOConnectMount(NDASInfo.index, false);
							}
								break;
						}
					} else {
						//syslog(LOG_DEBUG, "DeviceDriverNotificationCallback: No LUD");
					}
				}
				
				CFRelease(cfsKey);
			} 
		}
	}
	
	if (configurationArray != NULL) {
		CFRelease(configurationArray);
	}

	// Assingn RAID Name.
	CFArrayRef	raidArray = NDASPreferencesGetRAIDDeviceInformationList();
		
	if (raidArray != NULL && 0 < CFArrayGetCount(raidArray)) {
		
		for (int count = 0; count < CFArrayGetCount(raidArray); count++) {
			CFStringRef	cfsKey = NULL;
			
			cfsKey = (CFStringRef)CFArrayGetValueAtIndex(raidArray, count);
			
			if (cfsKey) {
												
				NDASRAIDDeviceInformation	NDASInfo = { 0 };
				CFUUIDRef					cfuGuid;
				
				CFRetain(cfsKey);
				
				cfuGuid = CFUUIDCreateFromString(kCFAllocatorDefault, cfsKey);
				if (cfuGuid) {
					
					*((CFUUIDBytes *)(&NDASInfo.deviceID)) = CFUUIDGetUUIDBytes(cfuGuid);
				}
									
				if ( NDASGetRAIDDeviceInformationByDeviceID(&NDASInfo) ) {
					
					CFNumberRef cfnIndex = NULL;
					CFNumberRef cfnRAIDStatus = NULL;
					
					cfnIndex = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &NDASInfo.index);
					
					if (cfnIndex) {
						// Update RAID Status.
						if (kNDASUnitStatusConnectedRW == NDASInfo.status) {
							if (cfnRAIDStatus = (CFNumberRef)CFDictionaryGetValue(raidStatusDictionary, cfnIndex)) {
								
								UInt32	prevRAIDStatus;
								
								if (CFNumberGetValue(cfnRAIDStatus, kCFNumberSInt32Type, &prevRAIDStatus)
									&& prevRAIDStatus != NDASInfo.RAIDStatus) {
									
									// Sent Refresh command.
									
									int arrayIndex;
									UNIT_DISK_LOCATION	locationArray[NDAS_MAX_UNITS_IN_V2] = { 0 };
									
									arrayIndex = 0;
									
									for (int count = 0; count < NDASInfo.CountOfUnitDevices; count++) {
										memcpy(&locationArray[arrayIndex++], &NDASInfo.unitDiskLocationOfSubUnits[count], sizeof(UNIT_DISK_LOCATION));
									}
									
									// Send HIX Message.
									for (int count = 0; count < arrayIndex; count++) {
										
										NDAS_HIX_UNITDEVICE_CHANGE_NOTIFY	unitDeviceNotify = { 0 };
										
										memcpy(unitDeviceNotify.DeviceId, locationArray[count].MACAddr, 6);
										unitDeviceNotify.UnitNo = locationArray[count].UnitNumber;
										
										pSendUnitDeviceChangeNotification(hostID, &unitDeviceNotify);
										
									}
									
									syslog(LOG_INFO, "Sent Refresh HIX Command");
									
								}
							}
						}
						
						cfnRAIDStatus = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &NDASInfo.RAIDStatus);
						if (cfnRAIDStatus) {
							CFDictionarySetValue(raidStatusDictionary, cfnIndex, cfnRAIDStatus);
							
							CFRelease(cfnRAIDStatus);
						}
					}

					// Update RAID name.
					NDASIOConnectBindEdit(kNDASEditTypeName, NDASInfo.index, NDASPreferencesGetRAIDNameByUUIDString(cfsKey), 0);
					
				}				
				
				CFRelease(cfsKey);
			}
		}
	}
	
	if (raidArray != NULL) {
		CFRelease(raidArray);
	}
}

void DeviceDriverNotificationCallback( 
									   void * refcon, 
									   io_service_t service, 
									   natural_t messageType, 
									   void * messageArgument )
{
	syslog(LOG_INFO, "DeviceDriverNotificationCallback: Entered. 0x%x\n", messageType);

	syslog(LOG_ERR, "DeviceDriverNotificationCallback: Entered. %d, %d\n", sizeof(NDAS_ENUM_REGISTER), sizeof(struct sockaddr_lpx));

	CFUUIDRef	cfuGuid = NULL;
	CFStringRef	cfsKey = NULL;
	
	switch (messageType) {
		case kNDASFamilyIOMessageNDASDeviceStatusWasChanged:
		{
			int32_t							slotNumber;
			bool							bResult;
			NDASPhysicalDeviceInformation	infos;
			
			memcpy(&slotNumber, &messageArgument, sizeof(int32_t));
			
			syslog(LOG_ERR, "DeviceDriverNotificationCallback: slotNumber %d", slotNumber);
			
			// Get NDAS Device Info.
			infos.slotNumber = slotNumber;
			
			bResult = NDASGetInformationBySlotNumber(&infos);
			
			if (kNDASStatusAutoRegister == infos.status) {
				
				syslog(LOG_ERR, "DeviceDriverNotificationCallback: Auto Register. slotNumber %d", slotNumber);

				SERIAL_INFO	serialInfo = { 0 };
				NDASPreferencesParameter	parameter = { 0 };
				char						nameString[256];

				switch(infos.vid) {
					case NDAS_VID_GENERAL_AUTO:
					{
						memcpy(serialInfo.ucAddr, infos.address, 6);
						serialInfo.ucRandom = NDAS_ID_EXTENSION_GENERAL.Seed;
						serialInfo.ucVid = (infos.vid & ~NDAS_VID_AUTO_MASK);		// Display Not autoregister key.
						serialInfo.reserved[0] = NDAS_ID_EXTENSION_GENERAL.Reserved[0];
						serialInfo.reserved[1] = NDAS_ID_EXTENSION_GENERAL.Reserved[1];
						memcpy(serialInfo.ucKey1, NDAS_ID_KEY_DEFAULT.Key1, 8);
						memcpy(serialInfo.ucKey2, NDAS_ID_KEY_DEFAULT.Key2, 8);

						if(!EncryptSerial(&serialInfo)) {
							syslog(LOG_ERR, "DeviceDriverNotificationCallback: Can't encrypt serial !!!!!!!!!!!");
							
							break;
						} 

						// Fill up parameters.
						parameter.AutoRegister = true;
						parameter.slotNumber = 0;		// Reassign.
						for(int i = 0; i < 4; i++) {
							memcpy(parameter.chID[i], serialInfo.ucSN[i], 5);
						}
						memcpy(parameter.chWriteKey, serialInfo.ucWKey, 5);
						sprintf(nameString, "NDAS Device (%s)", parameter.chID[0]);
						parameter.cfsName = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, nameString,  kCFStringEncodingUTF8, kCFAllocatorNull);
						parameter.configuration = kNDASConfigurationDisable;
						sprintf(parameter.DeviceIdString, "%s-%s-%s-xxxxx", parameter.chID[0], parameter.chID[1], parameter.chID[2]);
						for(int count = 0; count < MAX_NR_OF_TARGETS_PER_DEVICE; count++) {
							parameter.Units[count].configuration = kNDASUnitConfigurationUnmount;
						}						
						
						kern_return_t	returnValue;
						bool			bDuplicated;
						
						if( ( returnValue = NDASIOConnectRegister(&parameter, &bDuplicated) ) == KERN_SUCCESS) {
							
							//CFStringGetCString( cfsName, parameter.Name, MAX_NDAS_NAME_SIZE, CFStringGetSystemEncoding());
							//memcpy(parameter.Name, CFStringGetCharactersPtr(cfsName), MAX_NDAS_NAME_SIZE / 2);
							if (!bDuplicated) {
								
								if (1 == NDASPreferencesRegister(&parameter)) {
									
									if(parameter.cfsName)	CFRelease(parameter.cfsName);
									
									NDASIOConnectEnable(parameter.slotNumber);
									NDASPreferencesChangeConfigurationOfDevice(parameter.slotNumber, kNDASConfigurationEnable);
									
								} else {
									syslog(LOG_ERR, "DeviceDriverNotificationCallback:NDASPreferencesRegister failed!!!!!!!!!!!");
								}
							} else {								
								// NDAS info can be deleted.
								if (0 == NDASPreferencesQueryByID(&parameter)) {
									syslog(LOG_INFO, "NDAS Info was missing. rewrite NDAS device info. at slot %d\n", parameter.slotNumber);
									
									NDASPreferencesRegister(&parameter);			
								}
							}
							
						} else {
							syslog(LOG_ERR, "DeviceDriverNotificationCallback:NDASIOConnectRegister failed return %d!!!!!!!!!!!", returnValue);
						}
					}
						break;
					default:
						syslog(LOG_ERR, "DeviceDriverNotificationCallback: Unsupported VID %x", infos.vid);						
				}
			}
		}
			break;
		case kNDASFamilyIOMessageLogicalDeviceWasCreated:
		{
			PGUID	pguid = (PGUID)messageArgument;
			
			if (!pguid) {
				syslog(LOG_DEBUG, "DeviceDriverNotificationCallback: No GUID!!!");			
				
				break;
			}
			
			// Send Mount Command.
			cfuGuid = CFUUIDCreateWithBytes(kCFAllocatorDefault, 
											pguid->guid[0], pguid->guid[1], pguid->guid[2], pguid->guid[3], 
											pguid->guid[4], pguid->guid[5], pguid->guid[6], pguid->guid[7], 
											pguid->guid[8], pguid->guid[9], pguid->guid[10], pguid->guid[11], 
											pguid->guid[12], pguid->guid[13], pguid->guid[14], pguid->guid[15]);
			if (!cfuGuid) {
				syslog(LOG_DEBUG, "DeviceDriverNotificationCallback: Can't Make GUID!!!");			
				
				break;
			}
			
			cfsKey = CFUUIDCreateString(kCFAllocatorDefault, cfuGuid);
			if (!cfsKey) {
				syslog(LOG_DEBUG, "DeviceDriverNotificationCallback: Can't Make GUID String!!!");			
				
				break;
			}
			
			NDASLogicalDeviceInformation	NDASInfo = { 0 };
			
			memcpy(&NDASInfo.LowerUnitDeviceID, &(CFUUIDGetUUIDBytes(cfuGuid).byte0), sizeof(GUID));
			
			if (NDASGetLogicalDeviceInformationByLowerUnitDeviceID(&NDASInfo)) {
				
				switch(NDASPreferencesGetConfiguration(cfsKey)) {
					case kNDASUnitConfigurationMountRW:
					{
						NDASIOConnectMount(NDASInfo.index, true);
					}
						break;
					case kNDASUnitConfigurationMountRO:
					{
						NDASIOConnectMount(NDASInfo.index, false);
					}
						break;
				}
			} else {
				syslog(LOG_DEBUG, "DeviceDriverNotificationCallback: No LUD");
			}
		}
			break;
		case kNDASFamilyIOMessageRAIDDeviceWasCreated:
		{
			// Assign RAID Name.
			PGUID	pguid = (PGUID)messageArgument;
			
			if (!pguid) {
				syslog(LOG_DEBUG, "DeviceDriverNotificationCallback: No GUID!!!");			
				
				break;
			}
			
			cfuGuid = CFUUIDCreateWithBytes(kCFAllocatorDefault, 
											pguid->guid[0], pguid->guid[1], pguid->guid[2], pguid->guid[3], 
											pguid->guid[4], pguid->guid[5], pguid->guid[6], pguid->guid[7], 
											pguid->guid[8], pguid->guid[9], pguid->guid[10], pguid->guid[11], 
											pguid->guid[12], pguid->guid[13], pguid->guid[14], pguid->guid[15]);
			if (!cfuGuid) {
				syslog(LOG_DEBUG, "DeviceDriverNotificationCallback: Can't Make GUID!!!");			
				
				break;
			}
			
			cfsKey = CFUUIDCreateString(kCFAllocatorDefault, cfuGuid);
			if (!cfsKey) {
				syslog(LOG_DEBUG, "DeviceDriverNotificationCallback: Can't Make GUID String!!!");			
				
				break;
			}
			
			NDASRAIDDeviceInformation		NDASInfo = { 0 };
			
			memcpy(&NDASInfo.deviceID, &(CFUUIDGetUUIDBytes(cfuGuid).byte0), sizeof(GUID));
			
			if (NDASGetRAIDDeviceInformationByDeviceID(&NDASInfo)) {
				
				CFStringRef  cfsName = NDASPreferencesGetRAIDNameByUUIDString(cfsKey);
				
				if (cfsName) {
					NDASIOConnectBindEdit(kNDASEditTypeName, NDASInfo.index, cfsName, 0);
					
					CFRelease(cfsName);
				}
			} else {
				syslog(LOG_DEBUG, "DeviceDriverNotificationCallback: No RAID Unit Device");
			}
			
		}
			break;
		case kNDASFamilyIOMessageRAIDDeviceRAIDStatusWasChanged:
		{
			syslog(LOG_DEBUG, "DeviceDriverNotificationCallback: RAID Status was changed.");
			
			PGUID	pguid = (PGUID)messageArgument;
			
			if (!pguid) {
				syslog(LOG_DEBUG, "DeviceDriverNotificationCallback: No GUID!!!");			
				
				break;
			}
			
			cfuGuid = CFUUIDCreateWithBytes(kCFAllocatorDefault, 
											pguid->guid[0], pguid->guid[1], pguid->guid[2], pguid->guid[3], 
											pguid->guid[4], pguid->guid[5], pguid->guid[6], pguid->guid[7], 
											pguid->guid[8], pguid->guid[9], pguid->guid[10], pguid->guid[11], 
											pguid->guid[12], pguid->guid[13], pguid->guid[14], pguid->guid[15]);
			if (!cfuGuid) {
				syslog(LOG_DEBUG, "DeviceDriverNotificationCallback: Can't Make GUID!!!");			
				
				break;
			}
			
			cfsKey = CFUUIDCreateString(kCFAllocatorDefault, cfuGuid);
			if (!cfsKey) {
				syslog(LOG_DEBUG, "DeviceDriverNotificationCallback: Can't Make GUID String!!!");			
				
				break;
			}
			
			NDASRAIDDeviceInformation		NDASInfo = { 0 };
			
			memcpy(&NDASInfo.deviceID, &(CFUUIDGetUUIDBytes(cfuGuid).byte0), sizeof(GUID));
			
			if (NDASGetRAIDDeviceInformationByDeviceID(&NDASInfo)) {
				
				// Update RAID Status.
				if (kNDASUnitStatusConnectedRW == NDASInfo.status) {
					
					// Sent Refresh command.
					
					int arrayIndex;
					UNIT_DISK_LOCATION	locationArray[NDAS_MAX_UNITS_IN_V2] = { 0 };
					
					arrayIndex = 0;
					
					for (int count = 0; count < NDASInfo.CountOfUnitDevices; count++) {
						memcpy(&locationArray[arrayIndex++], &NDASInfo.unitDiskLocationOfSubUnits[count], sizeof(UNIT_DISK_LOCATION));
					}
					
					// Send HIX Message.
					for (int count = 0; count < arrayIndex; count++) {
						
						NDAS_HIX_UNITDEVICE_CHANGE_NOTIFY	unitDeviceNotify = { 0 };
						
						memcpy(unitDeviceNotify.DeviceId, locationArray[count].MACAddr, 6);
						unitDeviceNotify.UnitNo = locationArray[count].UnitNumber;
						
						pSendUnitDeviceChangeNotification(hostID, &unitDeviceNotify);
						
					}
					
					syslog(LOG_INFO, "Sent Refresh HIX Command");
					
				}
			}
		}
			break;
		default:
			break;
	}
	
	if (cfuGuid)	CFRelease(cfuGuid);
	if (cfsKey)		CFRelease(cfsKey);
	
	//
	// Wait Cleanup....
	//
	int maxWait = 5;		// 5 sec.
	
	if (kNDASFamilyIOMessageNDASDeviceWasDeleted == messageType) {
		
		NDASPhysicalDeviceInformation	NDASInfo = { 0 };
		
		uint64_t	retValue = (uint64_t)messageArgument;
		
		NDASInfo.slots = (UInt32)(retValue & 0xFFFFFFFF);
		
		while (NDASGetInformationBySlotNumber(&NDASInfo)
			   && maxWait > 0) {
			syslog(LOG_INFO, "Physical Device cleanup Wait...");
			sleep(1);
			maxWait--;
		}
		
	} else if (kNDASFamilyIOMessageRAIDDeviceWasDeleted == messageType) {
		
		NDASRAIDDeviceInformation		NDASInfo = { 0 };
		
		memcpy(&NDASInfo.deviceID, messageArgument, sizeof(GUID));
		
		while (NDASGetRAIDDeviceInformationByDeviceID(&NDASInfo)
			   && maxWait > 0) {
			syslog(LOG_INFO, "RAID cleanup Wait...");
			sleep(1);
			maxWait--;
		}
	} else if (kIOMessageServiceIsTerminated == messageType) {
		syslog(LOG_INFO, "kIOMessageServiceIsTerminated.");	
		
		releaseDeviceNotification(DriverNotification);
		DriverNotification = NULL;
		
		createDeviceNotification("IOResources", kIOBusyInterest, IOResourcesNotificationCallback, &IOResourcesNotification);
		
	}

	SendNotificationToUI(
						 NDAS_NOTIFICATION_CHANGED,
						 NDAS_NOTIFICATION_ERROR_SUCCESS,
						 0
						 );
	
	return;
}
	
void IOResourcesNotificationCallback( 
									  void * refcon, 
									  io_service_t service, 
									  natural_t messageType, 
									  void * messageArgument )
{
	syslog(LOG_INFO, "IOResourcesNotificationCallback 0x%x\n", messageType);
	
	// Check NDAS Family Driver.
	if(isNDASFamilyLoaded()) {

		releaseDeviceNotification(IOResourcesNotification);
		IOResourcesNotification = NULL;

		createDeviceNotification(DriversIOKitClassName, kIOGeneralInterest, DeviceDriverNotificationCallback, &DriverNotification);
		
		processPreferences();
		
	} else {
		syslog(LOG_INFO, "No Driver!!!");		
	}	
}

#include <sys/param.h>
#include <sys/ucred.h>
#include <sys/mount.h>
#include <err.h>

static struct statfs *mntbuf;
static int mntsize;

typedef enum { MNTON, MNTFROM } mntwhat;

bool
IsMounted(const char *name, mntwhat what)
{
	int i;
	
	if ((mntsize = getmntinfo(&mntbuf, MNT_NOWAIT)) == 0) {
		warn("getmntinfo");
		return false;
	}
	
	char            deviceName[255];
	
    sprintf(deviceName, "/dev/%s", name);
	
	for (i = 0; i < mntsize; i++) {
		if ((what == MNTON) && !strncmp(mntbuf[i].f_mntfromname, deviceName, 10)) {
			return true;
		}
	}
	return false;
}

void HixCallBack (
				  CFSocketRef s, 
				  CFSocketCallBackType callbackType, 
				  CFDataRef address, 
				  const void *data, 
				  void *info
				  )
{
	UInt8			replyData[NHIX_MAX_MESSAGE_LEN];
	CFSocketError	socketError;
		
	//syslog(LOG_ERR, "HixCallBack: Entered. callback type : %d\n", callbackType);
	
	switch(callbackType) {
		case kCFSocketDataCallBack:
		{
			struct sockaddr_lpx *senderAddress;
			CFDataRef			cfdHixPacket;
			CFIndex				cfiPacketLength;
			PNDAS_HIX_HEADER	pHixHeader;
			
			// Check Sender Address.
			senderAddress = (struct sockaddr_lpx*)CFDataGetBytePtr(address);
			
//			syslog(LOG_ERR, "HixCallBack: Sender Address: 0x%x:0x%x:0x%x:0x%x:0x%x:0x%x[%d]\n",
//				   senderAddress->slpx_node[0], senderAddress->slpx_node[1], senderAddress->slpx_node[2],
//				   senderAddress->slpx_node[3], senderAddress->slpx_node[4], senderAddress->slpx_node[5],
//				   ntohs(senderAddress->slpx_port)
//				   );
			
			// Check HIX Packet.
			cfdHixPacket = (CFDataRef)data;
			
			if(cfdHixPacket == NULL) {
				syslog(LOG_ERR, "HixCallBack: Bad Packet. No Data.");
				
				break;
			}
			
			cfiPacketLength = CFDataGetLength(cfdHixPacket);
			
//			syslog(LOG_ERR, "HixCallBack: Packet Length: %d\n", cfiPacketLength);
			
			pHixHeader = (PNDAS_HIX_HEADER)CFDataGetBytePtr(cfdHixPacket);
			
			// Check Header.
			if(FALSE == pIsValidNHIXRequestHeader(cfiPacketLength, pHixHeader)) {
				syslog(LOG_ERR, "HixCallBack: Invalid Packet. 0x%x, %d, %d\n", pHixHeader->Signature, pHixHeader->Revision, pHixHeader->ReplyFlag);
				
				break;				
			}
							
//			syslog(LOG_ERR, "HixCallBack: Request %d, size %d\n", pHixHeader->Type, pHixHeader->Length);
			
			switch(pHixHeader->Type) {
				case NHIX_TYPE_DISCOVER:
				{
					PNDAS_HIX_DISCOVER_REQUEST	pDiscover;
					PNDAS_HIX_DISCOVER_REPLY	pReply;
					CFDataRef						cfdReply;
					
					// Check size
					if(pHixHeader->Length <= sizeof(NDAS_HIX_HEADER)) {
						syslog(LOG_ERR, "HixCallBack: No discover Data.");
						break;
					}
					
					pDiscover = (PNDAS_HIX_DISCOVER_REQUEST)pHixHeader;
					pReply = (PNDAS_HIX_DISCOVER_REPLY)replyData;
					
					UInt32	replyCount = 0;
					// Get 
					for(int count = 0; count < pDiscover->Data.EntryCount; count++) {
						
						NDASPhysicalDeviceInformation	NDASInfo;
						
						CFMutableArrayRef	indexArray = NULL;
						int numberOfObjects;
						
						indexArray = NDASGetInformationGetIndexArray(NDAS_DEVICE_CLASS_NAME);
						
						if (indexArray == NULL) {
							numberOfObjects = 0;
						} else {
							numberOfObjects = CFArrayGetCount(indexArray);
						}
						
						for (int innerCount = 0; innerCount < numberOfObjects; innerCount++) {
							
							CFNumberRef	cfnIndex = (CFNumberRef)CFArrayGetValueAtIndex(indexArray, innerCount);
							if(cfnIndex == NULL) {
								continue;
							}
							
							CFNumberGetValue(cfnIndex, kCFNumberSInt32Type, &NDASInfo.slotNumber);
							
							if (NDASInfo.slotNumber <= 0) {
								// Bypass internal use NDAS Devices.
								continue;
							}
							
							if(NDASGetInformationBySlotNumber(&NDASInfo)) {
								
								// Compare Address.
								if(0 == memcmp(pDiscover->Data.Entry[count].DeviceId, 
											   NDASInfo.address,
											   6
											   )) {
									
									switch(NDASInfo.Units[pDiscover->Data.Entry[count].UnitNo].status)  {
										case kNDASUnitStatusConnectedRO:
										{
											memcpy(pReply->Data.Entry[replyCount].DeviceId,
												   pDiscover->Data.Entry[count].DeviceId,
												   6
												   );
											pReply->Data.Entry[replyCount].UnitNo = pDiscover->Data.Entry[count].UnitNo;
											pReply->Data.Entry[replyCount].AccessType = NHIX_UDA_RO;
											
											replyCount++;
										}
											break;
										case kNDASUnitStatusConnectedRW:
										{
											memcpy(pReply->Data.Entry[replyCount].DeviceId,
												   pDiscover->Data.Entry[count].DeviceId,
												   6
												   );
											pReply->Data.Entry[replyCount].UnitNo = pDiscover->Data.Entry[count].UnitNo;
											pReply->Data.Entry[replyCount].AccessType = NHIX_UDA_RW;
											
											replyCount++;
										}
											break;
										default:
											break;
									}
								}
							}
						}
						
						if (indexArray) {
							CFRelease(indexArray);
						}
						
						if ( replyCount > 0 ) {
							pReply->Data.EntryCount = replyCount;
							
							UInt32 cbLength = sizeof(NDAS_HIX::DISCOVER::REPLY) + 
								sizeof(NDAS_HIX::UNITDEVICE_ENTRY_DATA) * (pReply->Data.EntryCount - 1);
							
							// Make Reply Header.
							pBuildNHIXHeader(
											 &pReply->Header,
											 hostID,
											 TRUE,
											 NHIX_TYPE_DISCOVER,
											 cbLength
											 );			
							
							cfdReply = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, (UInt8 *) pReply, cbLength, kCFAllocatorNull);
							
							socketError = CFSocketSendData (
															s,
															address,
															cfdReply,
															0
															);
							
							if(socketError != kCFSocketSuccess) {
								syslog(LOG_ERR, "HixCallBack: Error when send reply.");
								break;
							}
							
							if(cfdReply)	CFRelease(cfdReply);
						}
					}
					
				}
					break;
				case NHIX_TYPE_QUERY_HOST_INFO:
				{
					PNDAS_HIX_QUERY_HOST_INFO_REPLY	pQuery;
					CFDataRef						cfdReply;
			
					// Check size
					if(pHixHeader->Length <= sizeof(NDAS_HIX_HEADER)) {
						syslog(LOG_ERR, "HixCallBack: No discover Data.");
						break;
					}
					
					pQuery = (PNDAS_HIX_QUERY_HOST_INFO_REPLY)replyData;
					
					// Make Reply Data.
					UInt32 cbUsed = pGetHostInfo(NHIX_MAX_MESSAGE_LEN, &pQuery->Data.HostInfoData);
					UInt32 cbLength = cbUsed + sizeof(NDAS_HIX_QUERY_HOST_INFO_REPLY) - sizeof(NDAS_HIX_HOST_INFO_DATA); 
					// Make Reply Header.
					pBuildNHIXHeader(
									 &pQuery->Header,
									 hostID,
									 TRUE,
									 NHIX_TYPE_QUERY_HOST_INFO,
									 cbLength
									 );			
					
					cfdReply = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, (UInt8 *) pQuery, cbLength, kCFAllocatorNull);
					
					socketError = CFSocketSendData (
													s,
													address,
													cfdReply,
													0
													);
					
					if(socketError != kCFSocketSuccess) {
						syslog(LOG_ERR, "HixCallBack: Error when send reply.");
						break;
					}
					
					if(cfdReply)	CFRelease(cfdReply);
					
				}
					break;
				case NHIX_TYPE_SURRENDER_ACCESS:
				{
					PNDAS_HIX_SURRENDER_ACCESS_REQUEST	pRequest;
					PNDAS_HIX_SURRENDER_ACCESS_REPLY	pReply;
					CFDataRef							cfdReply;
					
					// Check size
					if(pHixHeader->Length <= sizeof(NDAS_HIX_HEADER)) {
						syslog(LOG_ERR, "HixCallBack: No discover Data.");
						break;
					}
					
					pRequest = (PNDAS_HIX_SURRENDER_ACCESS_REQUEST)pHixHeader;
					pReply = (PNDAS_HIX_SURRENDER_ACCESS_REPLY)replyData;
					
					if(1 != pRequest->Data.EntryCount) {
						syslog(LOG_ERR, "HixCallBack: EntryCount shuld be 1. %d\n", pRequest->Data.EntryCount);
						break;
					}
					
					// Get 
					NDASPhysicalDeviceInformation	NDASInfo = { 0 };
					NDASRAIDDeviceInformation		RAIDInfo = { 0 };
					BOOL							needReply = FALSE;
					bool							bRAIDDevice = false;
					
					// Search NDAS Device.
					memcpy(NDASInfo.address, pRequest->Data.Entry[0].DeviceId, 6);
					
					if (NDASGetInformationByAddress(&NDASInfo)) {
						
						NDASLogicalDeviceInformation	LogicalInfo = { 0 };
						bool							bLogicalPresent = false;
						
						if (NMT_SINGLE == NDASInfo.Units[pRequest->Data.Entry[0].UnitNo].diskArrayType) {
							
							LogicalInfo.LowerUnitDeviceID = NDASInfo.Units[pRequest->Data.Entry[0].UnitNo].deviceID;
							
							bLogicalPresent = NDASGetLogicalDeviceInformationByLowerUnitDeviceID(&LogicalInfo);
							bRAIDDevice = false;
							
						} else {
							UNIT_DISK_LOCATION			tempUnitDiskLocation = { 0 };
							
							// Search RAID Device.
							memcpy(tempUnitDiskLocation.MACAddr, pRequest->Data.Entry[0].DeviceId, 6);
							tempUnitDiskLocation.UnitNumber = pRequest->Data.Entry[0].UnitNo;
							tempUnitDiskLocation.VID = NDASInfo.vid;
							
							memcpy(&(RAIDInfo.unitDiskLocation), &tempUnitDiskLocation, sizeof(UNIT_DISK_LOCATION));
							
							if ( NDASGetRAIDDeviceInformationByUnitDiskLocation(&RAIDInfo) ) {
								
								if (NMT_SINGLE == RAIDInfo.diskArrayType) {
									
									LogicalInfo.LowerUnitDeviceID = RAIDInfo.deviceID;
									
									bLogicalPresent = NDASGetLogicalDeviceInformationByLowerUnitDeviceID(&LogicalInfo);
									bRAIDDevice = true;
									
									break;
									
								}
							}
						}
						
						if (bLogicalPresent) {
							
							switch(NDASInfo.Units[pRequest->Data.Entry[0].UnitNo].configuration)  {
								case kNDASUnitConfigurationMountRW:
								{
									if (kNDASUnitStatusConnectedRW != NDASInfo.Units[pRequest->Data.Entry[0].UnitNo].status
										&& kNDASUnitStatusTryConnectRW != NDASInfo.Units[pRequest->Data.Entry[0].UnitNo].status
										&& kNDASUnitStatusReconnectRW != NDASInfo.Units[pRequest->Data.Entry[0].UnitNo].status) {
										
										syslog(LOG_ERR, "HixCallBack: .Status of Unit Disk is %d\n", NDASInfo.Units[pRequest->Data.Entry[0].UnitNo].status);
										
										break;
									}
									
									if(pRequest->Data.Entry[0].AccessType == NHIX_UDA_RW 
									   || pRequest->Data.Entry[0].AccessType == NHIX_UDA_WO ) {
										
										char			command[256];
										SInt32			userResult;
										CFOptionFlags	responseFlags;
										CFStringRef		cfsHeader = NULL;
										CFStringRef		cfsHeaderFrame = NULL;
										CFURLRef		cfurlApplication = NULL;
										CFBundleRef		cfbUtility = NULL;
										CFStringRef		cfsNDASName = NULL;
										CFStringRef		cfsHostName = NULL;
										char			tempBuffer[256];
										
										CFStringRef name = SCDynamicStoreCopyConsoleUser(NULL, NULL, NULL);
										
										if (name) {
											
											// Someone login.
											
											cfurlApplication = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, 
																							 CFSTR("/Applications/Utilities/NDAS Utility.app"),
																							 kCFURLPOSIXPathStyle,
																							 true);
											
											// ShareDisk?
											if (NULL == cfurlApplication) {
												
												cfurlApplication = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, 
																								 CFSTR("/Applications/Utilities/Share Utility.app"),
																								 kCFURLPOSIXPathStyle,
																								 true);
											}
											
											// Freecom NDAS?
											if (NULL == cfurlApplication) {
												
												cfurlApplication = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, 
																								 CFSTR("/Applications/Utilities/Freecom NDAS Utility.app"),
																								 kCFURLPOSIXPathStyle,
																								 true);
											}

//											cfbUtility = CFBundleGetBundleWithIdentifier(CFSTR("com.ximeta.NDASUtility"));

											cfbUtility = CFBundleCreate (
																		 kCFAllocatorDefault,
																		 cfurlApplication
																		 );
											
											if(NULL != cfbUtility) {
												
												cfsHeaderFrame = CFCopyLocalizedStringFromTableInBundle (
																										 CFSTR("The host '%@' is requesting the Write Access of '%@'."),
																										 NULL,
																										 cfbUtility,
																										 "Surrender Alert Header"
																										 );
											} else {
												cfsHeaderFrame = CFSTR("The host '%@' is requesting the Write Access of '%@'.");
											}
											
											if (bRAIDDevice) { 
												cfsNDASName = CFStringCreateWithCStringNoCopy (
																							   kCFAllocatorDefault,
																							   RAIDInfo.name,
																							   kCFStringEncodingUTF8,
																							   kCFAllocatorNull
																							   );
											} else {
												cfsNDASName = CFStringCreateWithCStringNoCopy (
																							   kCFAllocatorDefault,
																							   NDASInfo.name,
																							   kCFStringEncodingUTF8,
																							   kCFAllocatorNull
																							   );
											}
											
											sprintf(tempBuffer, "%02X:%02X:%02X:%02X:%02X:%02X",
													senderAddress->slpx_node[0], senderAddress->slpx_node[1], senderAddress->slpx_node[2],
													senderAddress->slpx_node[3], senderAddress->slpx_node[4], senderAddress->slpx_node[5]
													);
											
											cfsHostName = CFStringCreateWithCStringNoCopy (
																						   kCFAllocatorDefault,
																						   tempBuffer,
																						   CFStringGetSystemEncoding(),
																						   kCFAllocatorNull
																						   );
											
											cfsHeader = CFStringCreateWithFormat (
																				  kCFAllocatorDefault,
																				  NULL,
																				  cfsHeaderFrame,
																				  cfsHostName,
																				  cfsNDASName
																				  );
											
											userResult = CFUserNotificationDisplayAlert(
																						15,	// 15 Secs.
																						kCFUserNotificationCautionAlertLevel,
																						NULL, // Default Icon
																						NULL,
																						cfurlApplication, // Must support localization.
																						cfsHeader,
																						CFSTR("Do you want to accept the request and unmount the NDAS Device?"),
																						CFSTR("PANEL-BUTTON : Cancel"),
																						CFSTR("PANEL-BUTTON : Unmount"),
																						NULL,
																						&responseFlags);
											
											CFRelease(name);
											
										} else {
											userResult = 0;
										}
										
										
										//syslog(LOG_ERR, "HixCallBack: userResult %d, response %d\n", userResult, responseFlags);
										
										if (userResult == 0 && responseFlags == 1) {
											
											// Try Eject.
											sprintf(command, "/usr/sbin/diskutil eject %s", LogicalInfo.BSDName);
											
											system(command);
											
											if (!IsMounted(LogicalInfo.BSDName, MNTON)) {
												
												// Unmount 
												NDASIOConnectUnmount(LogicalInfo.index);
												
												NDASPreferencesChangeConfigurationOfLogicalDevice(LogicalInfo.index, kNDASUnitConfigurationUnmount);
												
											} else {
												
												CFUserNotificationDisplayNotice(
																				15,	// 15 Secs.
																				kCFUserNotificationCautionAlertLevel,
																				NULL, // Default Icon
																				NULL,
																				cfurlApplication, // Must support localization.
																				CFSTR("The following Device could not be Ejected."),
																				cfsNDASName,
																				CFSTR("OK")
																				);
												
											}
											
											needReply = TRUE;
											
											pReply->Data.Status = NHIX_SURRENDER_REPLY_STATUS_QUEUED;
											//											syslog(LOG_ERR, "HixCallBack: Surrender Queued");
										}
										
										// Release Resources.												
										if(cfsHostName)			CFRelease(cfsHostName);
										if(cfsNDASName)			CFRelease(cfsNDASName);
										
										if (cfsHeaderFrame)		CFRelease(cfsHeaderFrame);
										if (cfsHeader)			CFRelease(cfsHeader);
										if (cfurlApplication)	CFRelease(cfurlApplication);
										if (cfbUtility)			CFRelease(cfbUtility);												
									}											
								}
									break;
								default:
								{
									pReply->Data.Status = NHIX_SURRENDER_REPLY_STATUS_NO_ACCESS;
								}
									break;
							}
						} else {
							// No Logical Device.
							syslog(LOG_ERR, "HixCallBack: Receive NHIX_TYPE_SURRENDER_ACCESS: No Logical Device");
						}						
					}

					if(needReply) {
						UInt32 cbLength = sizeof(NDAS_HIX::SURRENDER_ACCESS::REPLY);
						
						// Make Reply Header.
						pBuildNHIXHeader(
										 &pReply->Header,
										 hostID,
										 TRUE,
										 NHIX_TYPE_SURRENDER_ACCESS,
										 cbLength
										 );			
						
						cfdReply = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, (UInt8 *) pReply, cbLength, kCFAllocatorNull);
						
						socketError = CFSocketSendData (
														s,
														address,
														cfdReply,
														0
														);
						
						if(socketError != kCFSocketSuccess) {
							syslog(LOG_ERR, "HixCallBack: Error when send reply.");
							break;
						}
						
						if(cfdReply)	CFRelease(cfdReply);						
					}
				}
					break;
				case NHIX_TYPE_DEVICE_CHANGE:
				{
				}
					break;
				case NHIX_TYPE_UNITDEVICE_CHANGE:
				{
					// Renew Informations.
					PNDAS_HIX_UNITDEVICE_CHANGE_NOTIFY	notify;
					
					notify = (PNDAS_HIX_UNITDEVICE_CHANGE_NOTIFY)pHixHeader;
					
					// Get 
					NDASPhysicalDeviceInformation	NDASInfo;
					UInt32 innerCount = 0;
					
					CFMutableArrayRef indexArray = NULL;
					int numberOfObjects;
					
					indexArray = NDASGetInformationGetIndexArray(NDAS_DEVICE_CLASS_NAME);
					
					if(indexArray == NULL) {
						numberOfObjects = 0;
					} else {
						numberOfObjects = CFArrayGetCount(indexArray);
					}
										
					for (innerCount = 0; innerCount < numberOfObjects; innerCount++) {
					
						CFNumberRef cfnIndex;
						
						cfnIndex = (CFNumberRef)CFArrayGetValueAtIndex(indexArray, innerCount);
						
						if(cfnIndex == NULL) {
							continue;
						}
						
						CFNumberGetValue(cfnIndex, kCFNumberSInt32Type, &NDASInfo.slotNumber);
						
						if (NDASInfo.slotNumber <= 0) {
							// Bypass internal use NDAS Devices.
							continue;
						}
						
						if(NDASGetInformationBySlotNumber(&NDASInfo)) {
							
							// Compare Address.
							if(0 == memcmp(notify->DeviceId, NDASInfo.address, 6)) {
								
								// Send Refresh Request.
								NDASIOConnectRefresh(NDASInfo.slotNumber, notify->UnitNo);
								
								syslog(LOG_INFO, "Receive Refresh. SlotNumber %d\n", NDASInfo.slotNumber);
								
								break;
							}							
						}
					}
					
					if (indexArray) {
						CFRelease(indexArray);
					}
					
					SendNotificationToUI(
										 NDAS_NOTIFICATION_CHANGED,
										 NDAS_NOTIFICATION_ERROR_SUCCESS,
										 0
										 );
				}
					break;
				default:
					break;
			}
		}
			break;
		default:
			break;
	}
	
}

// Version Info.
extern const double NDASServiceVersionNumber;
extern const unsigned char NDASServiceVersionString[];

void processPreferences()
{
	//
	// Process Old Preferences file - Migration
	//
	UInt32 slotNumber;
	NDASPreferencesParameter	parameter;

	while(slotNumber = NDASPreferencesGetArbitrarySlotNumberOfRegisteredEntryLegacy()) {
		
		NDASPreferencesParameterLegacy	parameterLegacy;
		int								numOfIDs;
		
		parameterLegacy.slotNumber = slotNumber;
		
		if(NDASPreferencesQueryLegacy(&parameterLegacy, &numOfIDs)) {
			
			CFArrayRef	deviceArray = NDASPreferencesGetRegisteredSlotList();
			bool		foundSameDevice = false;
			
			if (deviceArray != NULL && 0 < CFArrayGetCount(deviceArray)) {
				
				for (int count = 0; count < CFArrayGetCount(deviceArray); count++) {
					CFStringRef	cfsKey = NULL;
					
					cfsKey = (CFStringRef)CFArrayGetValueAtIndex(deviceArray, count);
					
					if (cfsKey) {
						
						CFRetain(cfsKey);
						
						parameter.slotNumber = CFStringGetIntValue(cfsKey);
						
						CFRelease(cfsKey);
						
						if(1 == NDASPreferencesQueryBySlotNumber(&parameter)) {
							
							if (parameter.cfsName) CFRelease(parameter.cfsName);														
							
							if (0 == memcmp(parameterLegacy.chID, parameter.chID, 4 * 6)) {
								foundSameDevice = true;
								syslog(LOG_INFO, "Migration: Found Same Device.");
								break;
							}
							
						}
					}
				}
			}					
			
			if (!foundSameDevice) {
				
				// Create New Parameter.
				memset(&parameter, 0, sizeof(NDASPreferencesParameter));
				parameter.slotNumber = parameterLegacy.slotNumber;
				
				// Validate Slot Number.
				if(1 == NDASPreferencesQueryBySlotNumber(&parameter)) {
					
					syslog(LOG_INFO, "Migration: Already Registerd Slot %d\n", parameter.slotNumber);
					
					// Search Empty SlotNumber.
					int slotNumber = 0;
					
					do {
						if (parameter.cfsName) CFRelease(parameter.cfsName);						
						
						memset(&parameter, 0, sizeof(NDASPreferencesParameter));
						parameter.slotNumber = ++slotNumber;
						
					} while (1 == NDASPreferencesQueryBySlotNumber(&parameter));
					
					if (parameter.cfsName) CFRelease(parameter.cfsName);						
					
					syslog(LOG_INFO, "Migration: New Slot %d\n", parameter.slotNumber);
					
				} else {
					// Empty Slot.
					syslog(LOG_INFO, "Migration: Empty Slot %d\n", parameter.slotNumber);
				}
				
				memcpy(parameter.chID, parameterLegacy.chID, 4 * 6);
				memcpy(parameter.chWriteKey, parameterLegacy.chWriteKey, 6);
				parameter.configuration = parameterLegacy.configuration;
				memcpy(parameter.Units, parameterLegacy.Units, sizeof(UnitPreferencesParameter) * MAX_NR_OF_TARGETS_PER_DEVICE);
				parameter.cfsName = CFStringCreateCopy(kCFAllocatorDefault, parameterLegacy.cfsName);
				parameter.AutoRegister = false;
				
				// Register New Preference.
				if (1 != NDASPreferencesRegister(&parameter)) {
					syslog(LOG_INFO, "Error when Migration: Empty Slot %d\n", parameter.slotNumber);
				}
				
				if (parameter.cfsName) CFRelease(parameter.cfsName);
			}
			
			//Delete Old Preference.
			NDASPreferencesUnregisterLegacy(parameterLegacy.slotNumber);
			
			if (parameterLegacy.cfsName) CFRelease(parameterLegacy.cfsName);
		} else {
			syslog(LOG_INFO, "Query Failed slot %u\n", parameterLegacy.slotNumber);
		}
		
	} // Invalid List.
	
	//
	// Process Preferences file
	//
	CFArrayRef	deviceArray = NDASPreferencesGetRegisteredSlotList();
	
	if (deviceArray != NULL && 0 < CFArrayGetCount(deviceArray)) {
		
		for (int count = 0; count < CFArrayGetCount(deviceArray); count++) {
			CFStringRef	cfsKey = NULL;
			
			cfsKey = (CFStringRef)CFArrayGetValueAtIndex(deviceArray, count);
			
			if (cfsKey) {
				
				UInt32	slotNumber;
				
				CFRetain(cfsKey);
				
				slotNumber = CFStringGetIntValue(cfsKey);
				
				syslog(LOG_NOTICE, "Registering NDAS Deive... [Slot Number %d]\n", slotNumber);
				
				parameter.slotNumber = slotNumber;
				
				switch(NDASPreferencesQueryBySlotNumber(&parameter)) {
					case 1: 
					{
						
						// Create Device ID String.
						sprintf(parameter.DeviceIdString, "%s-%s-%s-xxxxx", parameter.chID[0], parameter.chID[1], parameter.chID[2]);
						
						//					if (parameter.cfsName) {
						//						CFStringGetCString(parameter.cfsName, parameter.Name, MAX_NDAS_NAME_SIZE, kCFStringEncodingUTF8);
						//					} 
						bool			bDuplicated;
						kern_return_t	ret;
						
						if (KERN_SUCCESS == (ret = NDASIOConnectRegister(&parameter, &bDuplicated))) {
							
							if (!bDuplicated) {
								switch(parameter.configuration) {
									case kNDASConfigurationEnable:
										NDASIOConnectEnable(parameter.slotNumber);
										break;
									case kNDASConfigurationDisable:
									default:
										//NDASIOConnectDisable(slotNumber);
										
										// Enable It. - BUG BUG BUG
										NDASIOConnectEnable(parameter.slotNumber);
										NDASPreferencesChangeConfigurationOfDevice(parameter.slotNumber, kNDASConfigurationEnable);
										
										break;
								}
								
								syslog(LOG_INFO, "The NDAS Device at slot %d configurated corrctly.\n", parameter.slotNumber);
							} else {
								
								// Parameter was wrong or already registered.
								syslog(LOG_INFO, "Already registered\n", parameter.slotNumber);
								
								if (slotNumber != parameter.slotNumber) {
									syslog(LOG_ERR, "Duplicated registeration. %d, %d. Delete %d\n", parameter.slotNumber, slotNumber, slotNumber);
									
									NDASPreferencesUnregister(slotNumber);
								}
							}
						} else if (KERN_INVALID_ARGUMENT == ret) {
							syslog(LOG_ERR, "Bad NDAS ID!!!");						
						} else {
							// Internal Error...
							syslog(LOG_ERR, "NDAS Family driver isn't work correctly!!!");
						}
						
						if (parameter.cfsName) CFRelease(parameter.cfsName);					
					}
						break;
					case -1:
					{						
						syslog(LOG_ERR, "Delete Bad Info. NDAS [SlotNo:%d].", parameter.slotNumber);						
						
						// Bad Info.
						NDASPreferencesUnregister(parameter.slotNumber);
					}
						break;
					default:
						break;
				}
				
				
				CFRelease(cfsKey);
			}
			
		}
		
		CFRelease(deviceArray);
	}	
}

bool isLpxRunning()
{
	CFSocketRef				hixListenSocket = NULL;
		
	// Create Listen Socket.
	hixListenSocket = CFSocketCreate (
									  kCFAllocatorDefault,
									  PF_LPX,
									  SOCK_DGRAM,
									  0,
									  kCFSocketDataCallBack, // | kCFSocketReadCallBack,
									  HixCallBack,
									  NULL
									  );
	if(hixListenSocket == NULL) {
		
		return false;
	}
	
	CFRelease(hixListenSocket);
	
	return true;
}

void registerHIXListener()
{
	for(int i = 0; i < numberOfNics; i++) {
		
		CFDataRef				hixListenAddress = NULL;
		struct					sockaddr_lpx	addr = { 0 };
		CFSocketRef				hixListenSocket = NULL;
		CFRunLoopSourceRef		hixRunLoopSource = NULL;
		CFSocketError			sockError;
		
		addr.slpx_family = AF_LPX;
		addr.slpx_len = sizeof(struct sockaddr_lpx);
		addr.slpx_port = htons(NDAS_HIX_LISTEN_PORT);
		memcpy(addr.slpx_node, ethernetData[i].macAddress, 6);
		
		// Create Listen Socket.
		hixListenSocket = CFSocketCreate (
										  kCFAllocatorDefault,
										  PF_LPX,
										  SOCK_DGRAM,
										  0,
										  kCFSocketDataCallBack, // | kCFSocketReadCallBack,
										  HixCallBack,
										  NULL
										  );
		if(hixListenSocket == NULL) {
			
			continue;
		}
		
		hixListenAddress = CFDataCreateWithBytesNoCopy (
														kCFAllocatorDefault,
														(UInt8 *)&addr,
														sizeof(struct sockaddr_lpx),
														kCFAllocatorNull
														);
		
		if(hixListenAddress == NULL) {
			syslog(LOG_ERR, "Can't Allocate Address CFData");
			
			if(hixListenSocket)		CFRelease(hixListenSocket);
			
			continue;		
		}
		
		sockError = CFSocketSetAddress (
										hixListenSocket,
										hixListenAddress
										);
		if(sockError != kCFSocketSuccess) {
			syslog(LOG_ERR, "Can't Bind Address");
			
			if(hixListenSocket)		CFRelease(hixListenSocket);
			
			continue;				
		}
		
		CFRelease(hixListenAddress);
		
		hixRunLoopSource = CFSocketCreateRunLoopSource(
													   kCFAllocatorDefault,
													   hixListenSocket,
													   0
													   );
		if(hixRunLoopSource == NULL) {
			syslog(LOG_ERR, "Can't add Run Loop Source.");
			
			if(hixListenSocket)		CFRelease(hixListenSocket);
			
			continue;						
		}
		
		CFRunLoopAddSource(
						   CFRunLoopGetCurrent(), 
						   hixRunLoopSource,
						   kCFRunLoopCommonModes
						   );
		
		CFSocketEnableCallBacks(hixListenSocket, kCFSocketDataCallBack);
		
		if(hixListenSocket)		CFRelease(hixListenSocket);		
		
	}
}

void waitLpxTimerCallback(
						  CFRunLoopTimerRef timer, 
						  void *info)
{
	if(isLpxRunning()) {
		registerHIXListener();
	} else {
		syslog(LOG_WARNING, "LPX is not running.");
		
		//
		// Create Timer.
		//		
		CFRunLoopTimerRef CFTimerUdate = CFRunLoopTimerCreate (
											 kCFAllocatorDefault,
											 CFAbsoluteTimeGetCurrent() + 1,	// 1sec later.
											 0, 
											 0, 
											 0,
											 waitLpxTimerCallback,
											 NULL
											 );
		
		CFRunLoopAddTimer(
						  CFRunLoopGetCurrent(), 
						  CFTimerUdate,
						  kCFRunLoopCommonModes
						  );
		
		CFRelease(CFTimerUdate);
		
	}
}

void exitProgram(int sig) {
	
	//********************************
	// Stopping
	//********************************
	
	syslog(LOG_NOTICE, "Stopping NDAS Service.");
	
	//
	// Remove Notification observer
	//
	CFNotificationCenterRemoveObserver(CFNotificationCenterGetDistributedCenter(),
									   NULL,
									   CFSTR(NDAS_NOTIFICATION),
									   CFSTR(NDAS_SERVICE_OBJECT_ID));
	
	if(DriverNotification)		IOObjectRelease(DriverNotification);
	if(IOResourcesNotification)	IOObjectRelease(IOResourcesNotification);
	
	// 
	// Remove RAID Status Dictionary.
	//
	CFDictionaryRemoveAllValues(raidStatusDictionary);
	CFRelease(raidStatusDictionary);
	
	closelog();
	
	signal(sig, SIG_DFL);
	
	exit(0);
}

int main (int argc, const char ** argv) {
	
	
	CFRunLoopTimerRef		CFTimerUdate;
	
	//********************************
	// Daemonization
	// From : Technical Note TN2083
	//********************************
	if ( (argc >= 2) && (strcmp(argv[1], "daemon") == 0) ) {
		optind = 2;
		
	} else {
		char		**args;
		char		execPath[PATH_MAX];
		uint32_t	execPathSize;
		
		execPathSize = sizeof(execPath);
		(void)_NSGetExecutablePath(execPath, &execPathSize);
		
		args = (char **)malloc((argc - optind + 1) * sizeof(char *));
		args[0] = execPath;
		args[1] = (char *)"daemon";
		memcpy(&args[2], &argv[optind], (argc - optind + 1) * sizeof (char *));
		
		// Daemonize ourself.
		(void)daemon(0, 0);
		
		// exec ourself.
		(void)execve(execPath, args, environ);
		
		return EXIT_SUCCESS;
	}
	
	//********************************
	// Starting
	//********************************
		
	openlog("NDASService", (LOG_CONS|LOG_PERROR|LOG_PID), LOG_DAEMON);
		
	syslog(LOG_NOTICE, "Starting %s", NDASServiceVersionString);

	setlogmask(LOG_UPTO(LOG_NOTICE));

	// Add Signal Handler.
//	signal(SIGHUP, exitProgram);
	signal(SIGINT, exitProgram);
	signal(SIGTERM, exitProgram);
	
	//
	// Find first NIC address.
	//
	
	// Create HostID
	hixHostUUID = CFUUIDCreate(kCFAllocatorDefault);
	hostID = CFUUIDGetUUIDBytes(hixHostUUID);	
		
	//
	// Check NICs.
	//
	int	iRetry = 60 * 2;	// 2 Min.
	bool	bFound = false;
	
	while(iRetry > 0 && !bFound) {

		numberOfNics = NUMBER_OF_NICS;

		if (KERN_SUCCESS == GetEthernetAddressInfo(ethernetData, &numberOfNics) && numberOfNics > 0) {

			for(int i = 0; i < numberOfNics; i++) {
				if (strncmp((char *)ethernetData[i].bsdName, "en0", 3) == 0) {
					memcpy(hostIDKey, ethernetData[i].macAddress, 6);
					
					syslog(LOG_INFO, "[en0] Host ID Key %llx, i %d", *((uint64_t*)hostIDKey), i);
					
					bFound = true;
					
					break;
				}
			}					
		}
		
		iRetry--;
		sleep(1);	// 1sec.		
	}
	
	if(!bFound) {
		
		syslog(LOG_CRIT, "Can't Find any Ethernet NIC. Quitting...");
		closelog();
		
		return EXIT_FAILURE;
	}
	
	//
	// Create RAID Status Dictionary.
	//
	raidStatusDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault,
													 0,
													 &kCFTypeDictionaryKeyCallBacks,
													 &kCFTypeDictionaryValueCallBacks);
	if (!raidStatusDictionary) {
		syslog(LOG_ERR, "Can't create raidStatusDictionary!!!");
		
		return -1;
	}
			
	//
	// Add Notification observer from UI.
	//
	int Observer;
	
	CFNotificationCenterAddObserver(CFNotificationCenterGetDistributedCenter(),
									&Observer,						// Don't miss it. 10.2 need It!
									_UserNotificationCallback,
									CFSTR(NDAS_NOTIFICATION),
									CFSTR(NDAS_SERVICE_OBJECT_ID),
									CFNotificationSuspensionBehaviorHold);

	//
	// Add Notification observer from Device Driver.
	//
	if(isNDASFamilyLoaded()) {
		createDeviceNotification(DriversIOKitClassName, kIOGeneralInterest, DeviceDriverNotificationCallback, &DriverNotification);
		
		processPreferences();
	} else {
		syslog(LOG_INFO, "No Driver!!!");

		createDeviceNotification("IOResources", kIOBusyInterest, IOResourcesNotificationCallback, &IOResourcesNotification);
	}
	
	//
	// Register HIX handler.
	//
	
	if(isLpxRunning()) {
		registerHIXListener();
	} else {
		//
		// Create Timer.
		//		
		CFTimerUdate = CFRunLoopTimerCreate (
											 kCFAllocatorDefault,
											 CFAbsoluteTimeGetCurrent() + 1,	// 1sec later.
											 0, 
											 0, 
											 0,
											 waitLpxTimerCallback,
											 NULL
											 );
		
		CFRunLoopAddTimer(
						  CFRunLoopGetCurrent(), 
						  CFTimerUdate,
						  kCFRunLoopCommonModes
						  );
		
		CFRelease(CFTimerUdate);
	}
	
	//**********************************
	// Run Loop.
	//**********************************
	CFRunLoopRun();	

	// Never return.

    return 0;
}
