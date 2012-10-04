#include <iostream>

#include <sys/types.h>
#include <unistd.h>

#include "NDASGetInformation.h"
#include "NDASBusEnumeratorUserClientTypes.h"
#include "LanScsiDiskInformationBlock.h"
#include "NDASNotification.h"

void printPhysicalDeviceInformation()
{
	NDASPhysicalDeviceInformation	NDASInfo = { 0 };
	int								slotNumber, count;
	int								numberOfNDAS;
	uint32_t						slotNumbers[256];
		
	slotNumber = 1;
	count = 0;
	
	std::cout << "==============================\nPhysical NDAS Device List\n==============================\n\n";
	
	numberOfNDAS = NDASGetInformationGetCount(NDAS_DEVICE_CLASS_NAME, slotNumbers);

	for (count = 0; count < numberOfNDAS; count++) {
		
		NDASInfo.slotNumber = slotNumbers[count];
		
		if(NDASGetInformationBySlotNumber(&NDASInfo)) {
			
			int			unitCount;
			
			// Print Slot Number.
			std::cout << NDASInfo.slotNumber << '\t';
			
			// Print ID string.
			std::cout << NDASInfo.deviceIDString << '\t';
			
			// Prit Writable.			
			if (NDASInfo.writable) {
				std::cout << "(RW)";
			} else {
				std::cout << "(RO)";
			}
			
			std::cout << '\t';
			
			
			// Print Configuration.
			std::cout << "Configuration : ";
			
			switch (NDASInfo.configuration) {
			case kNDASConfigurationDisable:
				std::cout << "Disabled ";
				break;
			case kNDASConfigurationEnable:
				std::cout << "Enabled ";
				break;
			}
			
			std::cout << '\t';
			
			// Print Status.
			std::cout << "Status : ";
			
			switch (NDASInfo.status) {
				case kNDASStatusOffline:
					std::cout << "Offline ";
					break;
				case kNDASStatusOnline:
					std::cout << "Online ";
					break;
				case kNDASStatusFailure:
					std::cout << "Failure ";
					break;
				case kNDASStatusNoPermission:
					std::cout << "No Permission ";
					break;
			}
			
			std::cout << '\t';
			
			// Print Name.
			std::cout << "\n\tName : " << NDASInfo.name << '\t';
			
			if (NDASInfo.status == kNDASStatusOnline) {
				
				std::cout << "\t( ";
				
				// Print Version.
				std::cout << "Ver ";
				
				if( NDASInfo.hwType == 0 ) {
					switch( NDASInfo.hwVersion ) {
						case HW_VERSION_1_0:
							std::cout << "1.0 ";
							break;
						case HW_VERSION_1_1:
							std::cout << "1.1 ";
							break;
						case HW_VERSION_2_0:
							std::cout << "2.0 ";
							break;
						default:
							std::cout << "Undefined ";
					}
					
				}
				
				std::cout << '\t';
				
				// Print Max Request Blocks.
				std::cout << "MRB : " << NDASInfo.maxRequestBlocks << '\t';
			
				std::cout << " )\n";
				
				// Setting Units.
				for(unitCount = 0; unitCount < MAX_NR_OF_TARGETS_PER_DEVICE; unitCount++) {
					if ( kNDASUnitStatusNotPresent != NDASInfo.Units[unitCount].status ) {
						
						std::cout << "\t+- ";
						
						// Print Unit Number.
						std::cout << unitCount << '\t';
						
						// Print Device ID.
						CFUUIDRef	cfuID = NULL;
						CFStringRef cfsID = NULL;
						PGUID		tempGUID = &NDASInfo.Units[unitCount].deviceID;
						
						cfuID = CFUUIDCreateWithBytes(kCFAllocatorDefault,
													  tempGUID->guid[0], tempGUID->guid[1], tempGUID->guid[2], tempGUID->guid[3],
													  tempGUID->guid[4], tempGUID->guid[5], tempGUID->guid[6], tempGUID->guid[7],
													  tempGUID->guid[8], tempGUID->guid[9], tempGUID->guid[10], tempGUID->guid[11],
													  tempGUID->guid[12], tempGUID->guid[13], tempGUID->guid[14], tempGUID->guid[15]);							
						cfsID = CFUUIDCreateString(kCFAllocatorDefault, cfuID);
						
						if (cfuID && cfsID) {
							std::cout << '[' << CFStringGetCStringPtr(cfsID, CFStringGetSystemEncoding()) << "] \t"; 

							CFRelease(cfuID);
							CFRelease(cfsID);
						}
						
						// Print Type.
						std::cout << "Type : ";
						
						switch (NDASInfo.Units[unitCount].type) {
							case kNDASUnitMediaTypeHDD:
								std::cout << "HDD ";
								break;
							case kNDASUnitMediaTypeODD:
								std::cout << "ODD ";
								break;
						}
						std::cout << '\n';
						
						std::cout << "\t|\t ";

						// Print Configuration.
						std::cout << "Configuration : ";
						
						switch (NDASInfo.Units[unitCount].configuration) {
							case kNDASUnitConfigurationUnmount:
								std::cout << "Unmount ";
								break;
							case kNDASUnitConfigurationMountRO:
								std::cout << "Mount RO ";
								break;
							case  kNDASUnitConfigurationMountRW:
								std::cout << "Mount RW ";
								break;
						}
						
						std::cout << '\t';
						
						// Print Status.
						std::cout << "Status : ";
						
						switch (NDASInfo.Units[unitCount].status) {
							case kNDASUnitStatusDisconnected:
								std::cout << "Unmounted ";
								break;
							case kNDASUnitStatusNotPresent:
								std::cout << "Not Present ";
								break;
							case kNDASUnitStatusConnectedRO:
								std::cout << "Mount RO ";
								break;
							case kNDASUnitStatusConnectedRW:
								std::cout << "Mount RW ";
								break;
							case kNDASUnitStatusFailure:
								std::cout << "Failure ";
								break;
							case kNDASUnitStatusTryConnectRO:
								std::cout << "Try Mount RO ";
								break;
							case kNDASUnitStatusTryConnectRW:
								std::cout << "Try Mount RW ";
								break;
							case kNDASUnitStatusTryDisconnectRO:
								std::cout << "Try Unmount RO ";
								break;
							case kNDASUnitStatusTryDisconnectRW:
								std::cout << "Try Unmount RW ";
								break;
							case  kNDASUnitStatusReconnectRO:
								std::cout << "Remount RO ";
								break;
							case kNDASUnitStatusReconnectRW:
								std::cout << "Remount RW ";
								break;
							default:
								std::cout << "undefined ";
								break;
						}
						
						// Print RW/RO Hosts.
						std::cout << "Hosts(RW/RO) : " << NDASInfo.Units[unitCount].RWHosts << "/" << NDASInfo.Units[unitCount].ROHosts << " \t";
						
						std::cout << '\n';

						//Print Model.
						std::cout << "\t|\t Model : " << NDASInfo.Units[unitCount].model << '\n';
						
						//Print Firmware.
						std::cout << "\t|\t Firmware : " << NDASInfo.Units[unitCount].firmware << '\n';
						
						//Print Serial Number.
						std::cout << "\t|\t Model : " << NDASInfo.Units[unitCount].serialNumber << '\n';
						
						// Print Transfer Mode.
						std::cout << "\t|\t Transfer Mode : ";
						switch ( NDASInfo.Units[unitCount].transferType >> 4 ) {
							case 0:
							{
								std::cout << "PIO " << (NDASInfo.Units[unitCount].transferType & 0x07);
							}
								break;
							case 1:
							{
								std::cout << "Single-word DMA " << (NDASInfo.Units[unitCount].transferType & 0x0f);
							}
								break;
							case 2:
							{
								std::cout << "Multi-word DMA " << (NDASInfo.Units[unitCount].transferType & 0x0f);
							}
								break;
							case 4:
							{
								std::cout << "Ultra DMA " << (NDASInfo.Units[unitCount].transferType & 0x0f);
							}
								break;
							default:
								std::cout << "Undefined ";
						}
						
						std::cout << '\n';

						if(kNDASUnitMediaTypeHDD == NDASInfo.Units[unitCount].type) {
							
							// Capacity.
							UInt64	buffer = NDASInfo.Units[unitCount].capacity;
							
							std::cout << "\t|\t Capacity : ";
							
							if( buffer < 1024 ) {
								std::cout << buffer << " B";
							} else if ( (buffer /= 1024) < 1024 ) {
								std::cout << buffer << " KB";
							} else if ( (buffer /= 1024) < 1024 ) {
								std::cout << buffer << " MB";
							} else if ( (buffer /= 1024) < 1024 ) {
								std::cout << buffer << " GB";
							} else {
								buffer /= 1024;
								std::cout << buffer << " TB";
							}
							
							std::cout << '\n';
							
							// Print Transfer Mode.
							std::cout << "\t|\t Disk Array Type : ";
							switch(NDASInfo.Units[unitCount].diskArrayType) {
								case NMT_SINGLE:
									std::cout << "Single ";
									break;
								case NMT_MIRROR:
									std::cout << "Mirror ";
									break;
								case NMT_AGGREGATE:
									std::cout << "Aggregate ";
									break;
								case NMT_RAID0:
									std::cout << "RAID 0 ";
									break;
								case NMT_RAID1:
									std::cout << "RAID 1 ";
									break;
								case NMT_RAID4:
									std::cout << "RAID 4 ";
									break;
								case NMT_INVALID:
								default:
									std::cout << "Invalid ";
									break;
							}
							
						}
						
						std::cout << '\n';
						
					} else {
						// Empty Unit Number.
					}
				}
				
			}
			
			std::cout << '\n';
												
		} else {
			// Empty Slot.
		}
	}

	return;
}

void printLogicalDeviceInformation()
{
	NDASLogicalDeviceInformation	NDASInfo = { 0 };
	int								index, count;
	int								numberOfNDAS;
	uint32_t						indexNumbers[256];
		
	index = 0;
	count = 0;
	
	std::cout << "==============================\nLogical NDAS Device List\n==============================\n\n";
	
	numberOfNDAS =NDASGetInformationGetCount(NDAS_LOGICAL_DEVICE_CLASS_NAME, indexNumbers);
	
	while(count < numberOfNDAS) {
		
		NDASInfo.index = index++;
		
		if(NDASGetLogicalDeviceInformationByIndex(&NDASInfo)) {
			
			// Print Index.
			std::cout << NDASInfo.index << '\t';
			
			// Print Index.
			switch(NDASInfo.unitDeviceType) {
				case NMT_SINGLE:
				case NMT_CDROM:
				{
					std::cout << "Physical Device at slot " << NDASInfo.physicalUnitSlotNumber 
								<< "[" << NDASInfo.physicalUnitUnitNumber << "] " ;
				}
					break;
				case NMT_AGGREGATE:
				{
					std::cout << "Aggregate RAID Device " ;
				}
					break;
				default:
					break;
			}
			
			if (NDASInfo.writable) {
				std::cout << "(RW) ";
			} else {
				std::cout << "(RO) ";
			}
			
			
			std::cout << '\t';
			
			// Lower Device ID.
			CFUUIDRef	cfuID = NULL;
			CFStringRef cfsID = NULL;
			PGUID		tempGUID = &NDASInfo.LowerUnitDeviceID;
			
			cfuID = CFUUIDCreateWithBytes(kCFAllocatorDefault,
										  tempGUID->guid[0], tempGUID->guid[1], tempGUID->guid[2], tempGUID->guid[3],
										  tempGUID->guid[4], tempGUID->guid[5], tempGUID->guid[6], tempGUID->guid[7],
										  tempGUID->guid[8], tempGUID->guid[9], tempGUID->guid[10], tempGUID->guid[11],
										  tempGUID->guid[12], tempGUID->guid[13], tempGUID->guid[14], tempGUID->guid[15]);							
			cfsID = CFUUIDCreateString(kCFAllocatorDefault, cfuID);
			
			if (cfuID && cfsID) {
				std::cout << '[' << CFStringGetCStringPtr(cfsID, CFStringGetSystemEncoding()) << "] \t"; 
				
				CFRelease(cfuID);
				CFRelease(cfsID);
			}			
			
			std::cout << '\t';

			// Print Configuration.
			std::cout << "Configuration : ";
			
			switch (NDASInfo.configuration) {
				case kNDASUnitConfigurationUnmount:
					std::cout << "Unmount ";
					break;
				case kNDASUnitConfigurationMountRO:
					std::cout << "Mount RO ";
					break;
				case  kNDASUnitConfigurationMountRW:
					std::cout << "Mount RW ";
					break;
			}
			
			std::cout << '\t';
			
			// Print Status.
			std::cout << "Status : ";
			
			switch (NDASInfo.status) {
				case kNDASUnitStatusDisconnected:
					std::cout << "Unmounted ";
					break;
				case kNDASUnitStatusNotPresent:
					std::cout << "Not Present ";
					break;
				case kNDASUnitStatusConnectedRO:
					std::cout << "Mount RO ";
					break;
				case kNDASUnitStatusConnectedRW:
					std::cout << "Mount RW ";
					break;
				case kNDASUnitStatusFailure:
					std::cout << "Failure ";
					break;
				case kNDASUnitStatusTryConnectRO:
					std::cout << "Try Mount RO ";
					break;
				case kNDASUnitStatusTryConnectRW:
					std::cout << "Try Mount RW ";
					break;
				case kNDASUnitStatusTryDisconnectRO:
					std::cout << "Try Unmount RO ";
					break;
				case kNDASUnitStatusTryDisconnectRW:
					std::cout << "Try Unmount RW ";
					break;
				case  kNDASUnitStatusReconnectRO:
					std::cout << "Remount RO ";
					break;
				case kNDASUnitStatusReconnectRW:
					std::cout << "Remount RW ";
					break;
				default:
					std::cout << "undefined ";
					break;
			}
			
			// Print RW/RO Hosts.
			std::cout << "Hosts(RW/RO) : " << NDASInfo.RWHosts << "/" << NDASInfo.ROHosts << " \t";
			
			std::cout << '\n';
									
			count++;
			
		} else {
			// Empty Slot.
		}
	}			

	return;
}

void printRAIDDeviceInformation()
{
	NDASRAIDDeviceInformation	NDASInfo = { 0 };
	int							index, count;
	int							numberOfNDAS;
	uint32_t					indexNumbers[256];
	
	index = 0;
	count = 0;
	
	std::cout << "==============================\nRAID NDAS Device List\n==============================\n\n";
	
	numberOfNDAS = NDASGetInformationGetCount(NDAS_RAID_DEVICE_CLASS_NAME, indexNumbers);
	
	while(count < numberOfNDAS) {
		
		NDASInfo.index = index++;
		
		if(NDASGetRAIDDeviceInformationByIndex(&NDASInfo)) {
			
			// Print Index.
			std::cout << NDASInfo.index << '\t';
			
			// Print Name.
			std::cout << "Name : " << NDASInfo.name << "\n\t";
			
			// Print Index.
			switch(NDASInfo.unitDeviceType) {
				case NMT_AGGREGATE:
				{
					std::cout << "Aggregated RAID ";
				}
					break;
				case NMT_RAID0:
				{
					std::cout << "RAID 0 ";
				}
					break;
				case NMT_RAID1R3:
				{
					std::cout << "RAID 1 ";
				}
					break;
				default:
				{
					std::cout << "Unknown RAID ";
				}
					break;
			}
						
			// Print Device ID.			
			CFUUIDRef	cfuID = NULL;
			CFStringRef cfsID = NULL;
			PGUID		tempGUID = &NDASInfo.deviceID;
			
			cfuID = CFUUIDCreateWithBytes(kCFAllocatorDefault,
										  tempGUID->guid[0], tempGUID->guid[1], tempGUID->guid[2], tempGUID->guid[3],
										  tempGUID->guid[4], tempGUID->guid[5], tempGUID->guid[6], tempGUID->guid[7],
										  tempGUID->guid[8], tempGUID->guid[9], tempGUID->guid[10], tempGUID->guid[11],
										  tempGUID->guid[12], tempGUID->guid[13], tempGUID->guid[14], tempGUID->guid[15]);							
			cfsID = CFUUIDCreateString(kCFAllocatorDefault, cfuID);
			
			if (cfuID && cfsID) {
				std::cout << '[' << CFStringGetCStringPtr(cfsID, CFStringGetSystemEncoding()) << "] \t"; 
				
				CFRelease(cfuID);
				CFRelease(cfsID);
			}			
			
			if (NDASInfo.writable) {
				std::cout << "(RW) ";
			} else {
				std::cout << "(RO) ";
			}
			
			// Count of Unit Devices.
			std::cout << "Count Of Unit Devices : " << NDASInfo.CountOfUnitDevices << "\t";
			
			std::cout << "\n\t";
			
			// Print Configuration.
			std::cout << "Configuration : ";
			
			switch (NDASInfo.configuration) {
				case kNDASUnitConfigurationUnmount:
					std::cout << "Unmount ";
					break;
				case kNDASUnitConfigurationMountRO:
					std::cout << "Mount RO ";
					break;
				case  kNDASUnitConfigurationMountRW:
					std::cout << "Mount RW ";
					break;
			}
			
			std::cout << '\t';
			
			// Print Status.
			std::cout << "Status : ";
			
			switch (NDASInfo.status) {
				case kNDASUnitStatusDisconnected:
					std::cout << "Unmounted ";
					break;
				case kNDASUnitStatusNotPresent:
					std::cout << "Not Present ";
					break;
				case kNDASUnitStatusConnectedRO:
					std::cout << "Mount RO ";
					break;
				case kNDASUnitStatusConnectedRW:
					std::cout << "Mount RW ";
					break;
				case kNDASUnitStatusFailure:
					std::cout << "Failure ";
					break;
				case kNDASUnitStatusTryConnectRO:
					std::cout << "Try Mount RO ";
					break;
				case kNDASUnitStatusTryConnectRW:
					std::cout << "Try Mount RW ";
					break;
				case kNDASUnitStatusTryDisconnectRO:
					std::cout << "Try Unmount RO ";
					break;
				case kNDASUnitStatusTryDisconnectRW:
					std::cout << "Try Unmount RW ";
					break;
				case  kNDASUnitStatusReconnectRO:
					std::cout << "Remount RO ";
					break;
				case kNDASUnitStatusReconnectRW:
					std::cout << "Remount RW ";
					break;
				default:
					std::cout << "undefined ";
					break;
			}
			
			// Print RW/RO Hosts.
			std::cout << "Hosts(RW/RO) : " << NDASInfo.RWHosts << "/" << NDASInfo.ROHosts << " \t";

			std::cout << "\n\t";
			
			// Capacity.
			UInt64	buffer = NDASInfo.capacity;
			
			std::cout << "Capacity : ";
			
			if( buffer < 1024 ) {
				std::cout << buffer << " B";
			} else if ( (buffer /= 1024) < 1024 ) {
				std::cout << buffer << " KB";
			} else if ( (buffer /= 1024) < 1024 ) {
				std::cout << buffer << " MB";
			} else if ( (buffer /= 1024) < 1024 ) {
				std::cout << buffer << " GB";
			} else {
				buffer /= 1024;
				std::cout << buffer << " TB";
			}
			
			// RAID Status.
			
			std::cout << "\t\tRAID Status : ";
			
			switch (NDASInfo.RAIDStatus) {
			case kNDASRAIDStatusNotReady:
				std::cout << "Not Ready";
				break;
			case kNDASRAIDStatusBroken:
				std::cout << "Broken";
				break;
			case kNDASRAIDStatusReadyToRecovery:
				std::cout << "Ready To Recovery";
				break;
			case kNDASRAIDStatusRecovering:
				std::cout << "Recovering";
				break;
			case kNDASRAIDStatusBrokenButWorkable:
				std::cout << "Broken But Workable";
				break;
			case kNDASRAIDStatusGood:
				std::cout << "Good";
				break;
			default:
				std::cout << "Undefined";
				break;
			}
			
			std::cout << '\n';
			
			// Sub Unit IDs.
			if (NDASInfo.CountOfUnitDevices > NDAS_MAX_UNITS_IN_V2) {
				NDASInfo.CountOfUnitDevices = NDAS_MAX_UNITS_IN_V2;
			}
			for (int unitCount = 0;  unitCount < NDASInfo.CountOfUnitDevices; unitCount++) {
				
				std::cout << "\t+- UDL of Sub Unit " << unitCount << " : [" << NDASInfo.unitDiskLocationOfSubUnits[unitCount] << "] \tStatus : ";
								
				switch (NDASInfo.RAIDSubUnitStatus[unitCount]) {
					case kNDASRAIDSubUnitStatusNotPresent:
						std::cout << "Not Ready";
						break;
					case kNDASRAIDSubUnitStatusFailure:
						std::cout << "Failure";
						break;
					case kNDASRAIDSubUnitStatusGood:
						std::cout << "Good";
						break;
					case kNDASRAIDSubUnitStatusNeedRecovery:
						std::cout << "Need Recover";
						break;
					case kNDASRAIDSubUnitStatusRecovering:
						std::cout << "Recovering";
						break;
					default:
						std::cout << "Undefined";
						break;
				}
				
				std::cout << "\n";
			}
		
			// Recovering Status
			if (NMT_RAID1R3 == NDASInfo.unitDeviceType
				&& (kNDASRAIDStatusReadyToRecovery == NDASInfo.RAIDStatus || kNDASRAIDStatusRecovering == NDASInfo.RAIDStatus)
				) {
				std::cout << "\t+- Recovering Sector : " << NDASInfo.RecoveringSector << "/" << NDASInfo.capacity / DISK_BLOCK_SIZE << "\t(" << (NDASInfo.RecoveringSector * 100) / (NDASInfo.capacity / DISK_BLOCK_SIZE ) << "%)\n";
			}
			
			count++;
			
		} else {
			// Empty Slot.
		}
	}			
	
	return;
}

void SendNotificationToService(
							   UInt32	operation,
							   CFMutableDictionaryRef request
							   )
{
	// Send Notification to Service.	
	SInt32					osVersion; 
	UInt32					pid;
	CFNumberRef				cfnSenderID = NULL;
	CFNumberRef				cfnOperation = NULL;
	
	if ( request )	CFRetain( request );
	
	pid = getpid();
	
	if (NULL == request ) {
		printf("No Request!!!\n");
		
		return;
	}
	
	// Sender ID.
	cfnSenderID = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &pid);
	
	CFDictionaryAddValue (
						  request,
						  CFSTR(NDAS_NOTIFICATION_SENDER_ID_KEY_STRING),
						  cfnSenderID
						  );
	
	// Request.
	cfnOperation = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &operation);
	
	CFDictionaryAddValue (
						  request,
						  CFSTR(NDAS_NOTIFICATION_REQUEST_KEY_STRING),
						  cfnOperation
						  );
	
	if (Gestalt(gestaltSystemVersion, &osVersion) == noErr) 
	{ 
		if ( osVersion >= 0x1030 ) 
		{
			CFNotificationCenterPostNotificationWithOptions (
															 CFNotificationCenterGetDistributedCenter(),
															 CFSTR(NDAS_NOTIFICATION),
															 CFSTR(NDAS_SERVICE_OBJECT_ID),
															 request,
															 kCFNotificationPostToAllSessions
															 );
			
		} else {
			CFNotificationCenterPostNotification(CFNotificationCenterGetDistributedCenter(),
												 CFSTR(NDAS_NOTIFICATION),
												 CFSTR(NDAS_SERVICE_OBJECT_ID),
												 request,
												 true);
			
		}
	} 

	if ( cfnSenderID )			CFRelease ( cfnSenderID );
	if ( cfnOperation )			CFRelease ( cfnOperation );
	
	if ( request )	CFRelease( request );
}

void mount(UInt32 index, bool writable) 
{
	UInt32	configuration;
	CFMutableDictionaryRef	request = NULL;
	CFNumberRef				cfnParameter = NULL;

	if (writable) {
		configuration = kNDASUnitConfigurationMountRW;
	} else {
		configuration = kNDASUnitConfigurationMountRO;	
	}

	// Create Request
	request = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (request) {
				
		// Index.
		cfnParameter = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &index);
		CFDictionaryAddValue (
							  request,
							  CFSTR(NDAS_NOTIFICATION_INDEX_KEY_STRING),
							  cfnParameter
							  );	
		if (cfnParameter)	CFRelease(cfnParameter);
				
		// Configuration.
		cfnParameter = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &configuration);
		
		CFDictionaryAddValue (
							  request,
							  CFSTR(NDAS_NOTIFICATION_CONFIGURATION_KEY_STRING),
							  cfnParameter
							  );		
		if (cfnParameter)	CFRelease(cfnParameter);

		SendNotificationToService(
								  NDAS_NOTIFICATION_REQUEST_CONFIGURATE_LOGICAL_DEVICE,
								  request
								  );
		
		CFRelease(request);
	}
}

void unmount(UInt32 index)
{
	UInt32					configuration;
	CFMutableDictionaryRef	request = NULL;
	
	// Create Request
	request = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (request) {
		
		CFNumberRef	cfnParameter = NULL;
		
		// Index.
		cfnParameter = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &index);
		
		CFDictionaryAddValue (
							  request,
							  CFSTR(NDAS_NOTIFICATION_INDEX_KEY_STRING),
							  cfnParameter
							  );	
		
		if (cfnParameter)	CFRelease(cfnParameter);
		
		// Configuration.
		configuration = kNDASUnitConfigurationUnmount;
		cfnParameter = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &configuration);
		
		CFDictionaryAddValue (
							  request,
							  CFSTR(NDAS_NOTIFICATION_CONFIGURATION_KEY_STRING),
							  cfnParameter
							  );		
		
		if (cfnParameter)	CFRelease(cfnParameter);
		
		SendNotificationToService(
								  NDAS_NOTIFICATION_REQUEST_CONFIGURATE_LOGICAL_DEVICE,
								  request
								  );
		
		CFRelease(request);
	}
}

void bind(UInt32 raidType, UInt32 numberOfLogicaliDevices, UInt32 LogicalDeviceArray[], char *name)
{
	CFMutableDictionaryRef	request = NULL;
	
	// Create Request
	request = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (request) {
		
		CFNumberRef	cfnParameter = NULL;
		CFBooleanRef	cfbSync = NULL;
		
		// RAID Type.
		cfnParameter = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &raidType);
		
		CFDictionaryAddValue (
							  request,
							  CFSTR(NDAS_NOTIFICATION_RAID_TYPE_KEY_STRING),
							  cfnParameter
							  );	
		
		if (cfnParameter)	CFRelease(cfnParameter);

		cfbSync = kCFBooleanTrue;

		CFDictionaryAddValue (
							  request,
							  CFSTR(NDAS_NOTIFICATION_RAID_SYNC_FLAG_STRING),
							  cfbSync
							  );	
		
		// Number Of Logical Devices.
		cfnParameter = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &numberOfLogicaliDevices);
		
		CFDictionaryAddValue (
							  request,
							  CFSTR(NDAS_NOTIFICATION_NUMBER_OF_LOGICAL_DEVICES_KEY_STRING),
							  cfnParameter
							  );	
		
		if (cfnParameter)	CFRelease(cfnParameter);
		
		for (int count = 0; count < numberOfLogicaliDevices; count++) {
			
			printf("%d\n", LogicalDeviceArray[count]);
			
			CFStringRef cfsKey = NULL;
			
			cfsKey = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR(NDAS_NOTIFICATION_LOGICAL_DEVICE_INDEX_KEY_STRING), count);
			cfnParameter = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &LogicalDeviceArray[count]);
			
			CFDictionaryAddValue (
								  request,
								  cfsKey,
								  cfnParameter
								  );	
			
			if(cfsKey)			CFRelease(cfsKey);
			if (cfnParameter)	CFRelease(cfnParameter);
		}

		// RAID Name.
		CFStringRef cfsName = CFStringCreateWithCString(kCFAllocatorDefault,
														name,
														CFStringGetSystemEncoding()
														);
		
		CFDictionaryAddValue (
							  request,
							  CFSTR(NDAS_NOTIFICATION_NAME_STRING),
							  cfsName
							  );	
		
		if (cfsName)	CFRelease(cfsName);
		
		SendNotificationToService(
								  NDAS_NOTIFICATION_REQUEST_BIND,
								  request
								  );
		
		CFRelease(request);
	}
}

void unbind(UInt32 index)
{
	CFMutableDictionaryRef	request = NULL;
	
	// Create Request
	request = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (request) {
		
		CFNumberRef	cfnParameter = NULL;
		
		// Index.
		cfnParameter = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &index);
		
		CFDictionaryAddValue (
							  request,
							  CFSTR(NDAS_NOTIFICATION_INDEX_KEY_STRING),
							  cfnParameter
							  );	
		
		if (cfnParameter)	CFRelease(cfnParameter);
				
		SendNotificationToService(
								  NDAS_NOTIFICATION_REQUEST_UNBIND,
								  request
								  );
		
		CFRelease(request);
	}
}

void rebind(UInt32 index)
{
	CFMutableDictionaryRef	request = NULL;
	
	// Create Request
	request = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (request) {
		
		CFNumberRef	cfnParameter = NULL;
		
		// Index.
		cfnParameter = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &index);
		
		CFDictionaryAddValue (
							  request,
							  CFSTR(NDAS_NOTIFICATION_INDEX_KEY_STRING),
							  cfnParameter
							  );	
		
		if (cfnParameter)	CFRelease(cfnParameter);
		
		SendNotificationToService(
								  NDAS_NOTIFICATION_REQUEST_REBIND,
								  request
								  );
		
		CFRelease(request);
	}
}

int main (int argc, char * const argv[]) {

	if (1 == argc) {
		printPhysicalDeviceInformation();
		printRAIDDeviceInformation();
		printLogicalDeviceInformation();

		return 0;
	}
	
	if ( strcmp(argv[1], "mount") == 0 ) {
		UInt32	index;
		
		index = atoi(argv[2]);

		std::cout <<  "Mount " << index << '\n';

		mount(index, true);
		
	} else if ( strcmp(argv[1], "unmount") == 0 ) {
		UInt32	index;
		
		index = atoi(argv[2]);
		
		unmount(index);
	} else if (strcmp(argv[1], "bind") == 0) {
		UInt32	raidType;
		UInt32	numberOfLogicaliDevices;
		UInt32	LogicalDeviceArray[NDAS_MAX_UNITS_IN_V2] = { 0 };
		
		raidType = atoi(argv[2]);
		numberOfLogicaliDevices = atoi(argv[3]);
		
		if (numberOfLogicaliDevices != argc - 4) {
			fprintf(stderr, "Bad Parameters\n");
			return -1;
		}
		
		for (int count = 0; count < argc - 4; count++) {
			LogicalDeviceArray[count] = atoi(argv[count + 4]);
		}
		
		bind(raidType, numberOfLogicaliDevices, LogicalDeviceArray, "RAID Set");	
		
	}  else if (strcmp(argv[1], "unbind") == 0) {
		UInt32	index;
		
		index = atoi(argv[2]);
		
		unbind(index);		
	} else if (strcmp(argv[1], "rebind") == 0) {
		UInt32	index;
		
		index = atoi(argv[2]);
		
		rebind(index);		
	}
	
	
    return 0;
}
