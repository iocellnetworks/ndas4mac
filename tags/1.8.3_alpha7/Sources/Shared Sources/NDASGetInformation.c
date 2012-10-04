#include <CoreServices/CoreServices.h>
#include <IOKit/IOKitLib.h>
#include <syslog.h>

#include "NDASBusEnumeratorUserClientTypes.h"

#include "NDASGetInformation.h"

bool NDASGetInformationBySlotNumber(PNDASPhysicalDeviceInformation parameter) 
{

	kern_return_t			kernResult;
	io_connect_t			dataPort;
	io_service_t			serviceObject;
    io_iterator_t			iterator;
    CFDictionaryRef			classToMatch;
	io_iterator_t			devices = 0;
	io_registry_entry_t		deviceUpNext = 0;
	io_registry_entry_t		device = 0;
	io_registry_entry_t		controller = 0;

	
	bool					result = false;
	CFMutableDictionaryRef	properties = NULL;
	io_name_t				name;
	CFNumberRef				cfnValue = NULL;
	int						slotNumber;
	
	CFMutableDictionaryRef	cfdElement = NULL;
	CFStringRef				cfsKey = NULL;
	
	CFBooleanRef	cfbWritable = NULL;
	CFNumberRef		cfnStatus = NULL;
	CFNumberRef		cfnConfiguration = NULL;
	CFDataRef		cfdName = NULL;
	CFStringRef		cfsDeviceIDString = NULL;
	CFDataRef		cfdAddress = NULL;
	CFNumberRef		cfnHWType = NULL;
	CFNumberRef		cfnHWVersion = NULL;
	CFNumberRef		cfnMRB = NULL;
	CFNumberRef		cfnVID = NULL;
					
	int				count;
	bool			found;
		
	// Obtain the I/O Kit communication handle.
	kernResult = IOMasterPort(MACH_PORT_NULL, &dataPort);
	if(kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "can't obtain I/O Kit's master port. %d\n", kernResult);
		
		result = false;
		
		goto cleanup;
	}
	
	// Obtain the NDASBusEnumerator service.
	classToMatch = IOServiceMatching(DriversIOKitClassName);
	if(classToMatch == NULL) {
		syslog(LOG_ERR, "IOServiceMatching returned a NULL dictionary.");
		
		result = false;
		
		goto cleanup;
	}
	
    kernResult = IOServiceGetMatchingServices(dataPort, classToMatch, &iterator);
	if(kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "IOServiceGetMatchingServices returned. %d\n", kernResult);
		
		result = false;
		
		goto cleanup;
	}
	
    serviceObject = IOIteratorNext(iterator);
    IOObjectRelease(iterator);
    if (!serviceObject)
    {
		syslog(LOG_ERR, "Couldn't find any matches.");
		
		result = false;
		
		goto cleanup;
    }
	
	kernResult = IORegistryEntryGetChildIterator(serviceObject, kIOServicePlane, &devices);
	if(kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "can't obtain devices. %d\n", kernResult);
		
		result = false;
		
		goto cleanup;
	}
	
	deviceUpNext = IOIteratorNext(devices);
		
	found = false;
	// Traverse over the children of this service.
	while(deviceUpNext && found == false) {
				
		device = deviceUpNext;
		deviceUpNext = IOIteratorNext(devices);
				
		IORegistryEntryGetNameInPlane(device, kIOServicePlane, name);
		if ( 0 != strcmp(name, "com_ximeta_driver_NDASDevice")) {
			//syslog(LOG_ERR, "Not NDAS Device %s\n", name, deviceUpNext);
			
			IOObjectRelease(device);

			continue;
		}
		
		// Get Properties.
		kernResult = IORegistryEntryCreateCFProperties(
													   device,
													   &properties,
													   kCFAllocatorDefault,
													   kNilOptions
													   );		
		
		if (KERN_SUCCESS == kernResult
			&& properties) {
			
			// SlotNumber is Key.
			if (CFDictionaryGetValueIfPresent(properties, CFSTR(NDASDEVICE_PROPERTY_KEY_SLOT_NUMBER), (const void**)&cfnValue)) { 
				
				CFRetain(cfnValue);
				
				CFNumberGetValue (cfnValue, kCFNumberSInt32Type, &slotNumber);
				
				// Compare Slotnumber
				if(slotNumber == parameter->slotNumber) {
					cfdElement = properties;
					CFRetain(cfdElement);
					
					found = true;
					
					//
					// Get Device Controller.
					//
					kernResult = IORegistryEntryGetChildEntry(device, kIOServicePlane, &controller);
					if(kernResult != KERN_SUCCESS) {
						syslog(LOG_ERR, "IORegistryEntryGetChildEntry returned. %d\n", kernResult);
						
						result = false;
						
						CFRelease(properties);
						
						IOObjectRelease(device);
						
						goto cleanup;
					}
				}
				
				CFRelease(cfnValue);			
			}
			
			CFRelease(properties);
		}
		
		IOObjectRelease(device);
		
	}
		
	if(cfdElement != NULL) {
		
		// Address.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_ADDRESS), (const void**)&cfdAddress)
		   && cfdAddress) {
			CFRetain(cfdAddress);
			memcpy(parameter->address, CFDataGetBytePtr(cfdAddress), 6);
		}
		
		// Writable.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_WRITE_RIGHT), (const void**)&cfbWritable)
		   && cfbWritable) {
			CFRetain(cfbWritable);
			parameter->writable = CFBooleanGetValue(cfbWritable);
		}
		
		// Status.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_STATUS), (const void**)&cfnStatus)
		   && cfnStatus) {
			CFRetain(cfnStatus);
			CFNumberGetValue(cfnStatus, kCFNumberSInt32Type, &parameter->status);
		}
		
		// Configuration.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_CONFIGURATION), (const void**)&cfnConfiguration)
		   && cfnConfiguration) {
			CFRetain(cfnConfiguration);
			CFNumberGetValue(cfnConfiguration, kCFNumberSInt32Type, &parameter->configuration);
		}
		
		// Name.
		//if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_NAME), (const void**)&cfsName)) {
		//	CFStringGetCString(cfsName, parameter->name, 256, kCFStringEncodingUTF8); //CFStringGetSystemEncoding());
		//}
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_NAME), (const void**)&cfdName)
		   && cfdName) {
			CFRetain(cfdName);
			memcpy(parameter->name, CFDataGetBytePtr(cfdName), MAX_NDAS_NAME_SIZE);
		}
		
		// Device ID String.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_DEVICEID_STRING), (const void**)&cfsDeviceIDString)
		   && cfsDeviceIDString) {
			CFRetain(cfsDeviceIDString);
			CFStringGetCString(cfsDeviceIDString, parameter->deviceIDString, 256, CFStringGetSystemEncoding());
		}
		
		// Version.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_HW_TYPE), (const void**)&cfnHWType)
		   && cfnHWType) {
			CFRetain(cfnHWType);
			CFNumberGetValue(cfnHWType, kCFNumberSInt32Type, &parameter->hwType);
		} else {
			parameter->hwType = 0;
		}
		
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_HW_VERSION), (const void**)&cfnHWVersion)
		   && cfnHWVersion) {
			CFRetain(cfnHWVersion);
			CFNumberGetValue(cfnHWVersion, kCFNumberSInt32Type, &parameter->hwVersion);
		} else {
			parameter->hwVersion = 0;
		}
		
		// Max Request Blocks.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_MRB), (const void**)&cfnMRB)
		   && cfnMRB) {
			CFRetain(cfnMRB);
			CFNumberGetValue(cfnMRB, kCFNumberSInt32Type, &parameter->maxRequestBlocks);
			parameter->maxRequestBlocks /= DISK_BLOCK_SIZE;
		} else {
			parameter->maxRequestBlocks = 0;
		}
		
		// VID.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_VID), (const void**)&cfnVID)
		   && cfnVID) {
			CFRetain(cfnVID);
			CFNumberGetValue(cfnVID, kCFNumberSInt8Type, &parameter->vid);
		} else {
			parameter->vid = 0;
		}
		
		//
		// Get Unit Devices.
		//
		
		//
		// Init.
		//
		for(count = 0; count < MAX_NR_OF_TARGETS_PER_DEVICE; count++) {
			parameter->Units[count].status = kNDASUnitStatusNotPresent;
			parameter->Units[count].configuration = kNDASUnitConfigurationUnmount;
		}
		
		if ( kNDASStatusOnline == parameter->status ) {
			
			io_iterator_t			UnitDevices = 0;
			io_registry_entry_t		UnitDeviceUpNext = 0;
			
			//
			// Iterate Unit Devices.
			//
			kernResult = IORegistryEntryGetChildIterator(controller, kIOServicePlane, &UnitDevices);
			if(kernResult != KERN_SUCCESS) {
				syslog(LOG_ERR, "can't obtain Unit Iterator. %d\n", kernResult);
				
				result = false;
				
				goto cleanup;
			}
			
			UnitDeviceUpNext = IOIteratorNext(UnitDevices);
			
			while(UnitDeviceUpNext) {

				io_registry_entry_t		UnitDevice = 0;
				CFMutableDictionaryRef	UnitProperties = NULL;
				
				CFNumberRef		cfnUnitNumber = NULL;
				UInt32			unitNumber;
				CFDataRef		cfdGUID = NULL;
				CFNumberRef		cfnunitDiskLocation = NULL;
				CFNumberRef		cfnUnitStatus = NULL;
				CFNumberRef		cfnUnitConfiguration = NULL;
				CFNumberRef		cfnUnitRWHost = NULL;
				CFNumberRef		cfnUnitROHost = NULL;
				CFStringRef		cfsUnitModel = NULL;
				CFStringRef		cfsUnitFirmware = NULL;
				CFStringRef		cfsUnitSerialNumber = NULL;
				CFNumberRef		cfnUnitCapacity = NULL;
				CFNumberRef		cfnUnitType = NULL;
				CFNumberRef		cfnUnitTransferMode = NULL;
				CFNumberRef		cfnUnitDiskArrayType = NULL;
				CFNumberRef		cfnUnitBlockSize = NULL;
				
				uint32_t		blockSize;
				uint64_t		numberOfBlocks;
				
				UnitDevice = UnitDeviceUpNext;
				UnitDeviceUpNext = IOIteratorNext(UnitDevices);
				
				IORegistryEntryGetNameInPlane(UnitDevice, kIOServicePlane, name);
				
				// Get Properties.
				kernResult = IORegistryEntryCreateCFProperties(
															   UnitDevice,
															   &UnitProperties,
															   kCFAllocatorDefault,
															   kNilOptions
															   );		
				if(kernResult != KERN_SUCCESS) {
					syslog(LOG_ERR, "can't obtain Unit Properties. %d\n", kernResult);
					
					result = false;
					
					goto cleanup;
				}
												
				// Unit Number.
				if(CFDictionaryGetValueIfPresent(UnitProperties, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_NUMBER), (const void**)&cfnUnitNumber)
				   && cfnUnitNumber) {
					CFRetain(cfnUnitNumber);
					CFNumberGetValue(cfnUnitNumber, kCFNumberSInt32Type, &unitNumber);
				} else {
					goto togo;
				}

				// Unit Disk GUID.
				if(CFDictionaryGetValueIfPresent(UnitProperties, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_DEVICE_ID), (const void**)&cfdGUID)
				   && cfdGUID) {
					CFRetain(cfdGUID);
					memcpy(&parameter->Units[unitNumber].deviceID, CFDataGetBytePtr(cfdGUID), sizeof(GUID));
				}
				
				// Unit Disk Location.
				if(CFDictionaryGetValueIfPresent(UnitProperties, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_UNIT_DISK_LOCATION), (const void**)&cfnunitDiskLocation)
				   && cfnunitDiskLocation) {
					CFRetain(cfnunitDiskLocation);
					CFNumberGetValue(cfnunitDiskLocation, kCFNumberSInt64Type, &parameter->Units[unitNumber].unitDiskLocation);
				}
				
				// Configuration.				
				if(CFDictionaryGetValueIfPresent(UnitProperties, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_CONFIGURATION), (const void**)&cfnUnitConfiguration)
				   && cfnUnitConfiguration) {
					CFRetain(cfnUnitConfiguration);
					CFNumberGetValue(cfnUnitConfiguration, kCFNumberSInt32Type, &parameter->Units[unitNumber].configuration);
				}
				
				// Status.
				if(CFDictionaryGetValueIfPresent(UnitProperties, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_STATUS), (const void**)&cfnUnitStatus)
				   && cfnUnitStatus) {
					CFRetain(cfnUnitStatus);
					CFNumberGetValue(cfnUnitStatus, kCFNumberSInt32Type, &parameter->Units[unitNumber].status);
				} else {
					parameter->Units[unitNumber].status = kNDASUnitStatusNotPresent;
				}
								
				if(kNDASUnitStatusNotPresent != parameter->Units[unitNumber].status) {
					
					// Number of RW Hosts.
					if(CFDictionaryGetValueIfPresent(UnitProperties, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_NR_RW_HOST), (const void**)&cfnUnitRWHost)
					   && cfnUnitRWHost) {
						CFRetain(cfnUnitRWHost);
						CFNumberGetValue(cfnUnitRWHost, kCFNumberSInt32Type, &parameter->Units[unitNumber].RWHosts);
					} else {
						parameter->Units[unitNumber].RWHosts = 0;
					}
					
					// Number of RO Hosts.
					if(CFDictionaryGetValueIfPresent(UnitProperties, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_NR_RO_HOST), (const void**)&cfnUnitROHost)
					   && cfnUnitROHost) {
						CFRetain(cfnUnitROHost);
						CFNumberGetValue(cfnUnitROHost, kCFNumberSInt32Type, &parameter->Units[unitNumber].ROHosts);
					} else {
						parameter->Units[unitNumber].ROHosts = 0;
					}
					
					// Model.
					if(CFDictionaryGetValueIfPresent(UnitProperties, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_MODEL), (const void**)&cfsUnitModel)
					   && cfsUnitModel) {
						CFRetain(cfsUnitModel);
						CFStringGetCString(cfsUnitModel, parameter->Units[unitNumber].model, 42, CFStringGetSystemEncoding());
					}
					
					// Firmware.
					if(CFDictionaryGetValueIfPresent(UnitProperties, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_FIRMWARE), (const void**)&cfsUnitFirmware)
					   && cfsUnitFirmware) {
						CFRetain(cfsUnitFirmware);
						CFStringGetCString(cfsUnitFirmware, parameter->Units[unitNumber].firmware, 10, CFStringGetSystemEncoding());
					}
					
					// Serial Number.
					if(CFDictionaryGetValueIfPresent(UnitProperties, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_SERIALNUMBER), (const void**)&cfsUnitSerialNumber)
					   && cfsUnitSerialNumber) {
						CFRetain(cfsUnitSerialNumber);
						CFStringGetCString(cfsUnitSerialNumber, parameter->Units[unitNumber].serialNumber, 22, CFStringGetSystemEncoding());
					}

					// Block Size.
					if(CFDictionaryGetValueIfPresent(UnitProperties, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_BLOCK_SIZE), (const void**)&cfnUnitBlockSize)
					   && cfnUnitBlockSize) {
						CFRetain(cfnUnitBlockSize);
						CFNumberGetValue(cfnUnitBlockSize, kCFNumberSInt32Type, &blockSize);
					} else {
						blockSize = DISK_BLOCK_SIZE;
					}
					
					// Number of Blocks.
					if(CFDictionaryGetValueIfPresent(UnitProperties, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_NUMBER_OF_SECTORS), (const void**)&cfnUnitCapacity)
					   && cfnUnitCapacity) {
						CFRetain(cfnUnitCapacity);
						CFNumberGetValue(cfnUnitCapacity, kCFNumberSInt64Type, &numberOfBlocks);
					} else {
						numberOfBlocks = 0;
					}
					
					// Capacity
					parameter->Units[unitNumber].capacity = blockSize * numberOfBlocks;
					
					// Transfer Mode.
					if(CFDictionaryGetValueIfPresent(UnitProperties, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_TRANSFER_MODE), (const void**)&cfnUnitTransferMode)
					   && cfnUnitTransferMode) {
						CFRetain(cfnUnitTransferMode);
						CFNumberGetValue(cfnUnitTransferMode, kCFNumberSInt8Type, &parameter->Units[unitNumber].transferType);
					}
					
					// Disk Array Type.
					if(CFDictionaryGetValueIfPresent(UnitProperties, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_DISK_ARRAY_TYPE), (const void**)&cfnUnitDiskArrayType)
					   && cfnUnitDiskArrayType) {
						CFRetain(cfnUnitDiskArrayType);
						CFNumberGetValue(cfnUnitDiskArrayType, kCFNumberSInt32Type, &parameter->Units[unitNumber].diskArrayType);
					}
					
					// Unit  Type.
					if(CFDictionaryGetValueIfPresent(UnitProperties, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_TYPE), (const void**)&cfnUnitType)
					   && cfnUnitType) {
						CFRetain(cfnUnitType);
						CFNumberGetValue(cfnUnitType, kCFNumberSInt8Type, &parameter->Units[unitNumber].type);
					}
				}
togo:								
				IOObjectRelease(UnitDevice);

				if(UnitProperties)			CFRelease(UnitProperties);
				if(cfdGUID)					CFRelease(cfdGUID);
				if(cfnunitDiskLocation)		CFRelease(cfnunitDiskLocation);
				if(cfnUnitNumber)			CFRelease(cfnUnitNumber);
				if(cfnUnitStatus)			CFRelease(cfnUnitStatus);
				if(cfnUnitConfiguration)	CFRelease(cfnUnitConfiguration);
				if(cfnUnitRWHost)			CFRelease(cfnUnitRWHost);
				if(cfnUnitROHost)			CFRelease(cfnUnitROHost);
				if(cfsUnitModel)			CFRelease(cfsUnitModel);
				if(cfsUnitFirmware)			CFRelease(cfsUnitFirmware);
				if(cfsUnitSerialNumber)		CFRelease(cfsUnitSerialNumber);
				if(cfnUnitBlockSize)		CFRelease(cfnUnitBlockSize);
				if(cfnUnitCapacity)			CFRelease(cfnUnitCapacity);
				if(cfnUnitTransferMode)		CFRelease(cfnUnitTransferMode);
				if(cfnUnitType)				CFRelease(cfnUnitType);
				if(cfnUnitDiskArrayType)	CFRelease(cfnUnitDiskArrayType);
			}
			
			IOObjectRelease(UnitDevices); UnitDevices = 0;

		}
		
		result = true;
		
		IOObjectRelease(controller); controller = 0;

		}

cleanup:

	IOObjectRelease(devices); devices = 0;


	// cleanup
	if(cfsKey)				CFRelease(cfsKey);
	if(cfdAddress)			CFRelease(cfdAddress);
	if(cfbWritable)			CFRelease(cfbWritable);
	if(cfnStatus)			CFRelease(cfnStatus);
	if(cfnConfiguration)	CFRelease(cfnConfiguration);
	if(cfdName)				CFRelease(cfdName);
	if(cfsDeviceIDString)	CFRelease(cfsDeviceIDString);
	if(cfnHWType)			CFRelease(cfnHWType);
	if(cfnHWVersion)		CFRelease(cfnHWVersion);
	if(cfnMRB)				CFRelease(cfnMRB);
	if(cfnVID)				CFRelease(cfnVID);
	
	if(cfdElement)			CFRelease(cfdElement);
	
	return result;
}

bool NDASGetInformationByAddress(PNDASPhysicalDeviceInformation parameter) 
{

	kern_return_t			kernResult;
	io_connect_t			dataPort;
	io_service_t			serviceObject;
    io_iterator_t			iterator;
    CFDictionaryRef			classToMatch;
	io_iterator_t			devices = 0;
	io_registry_entry_t		deviceUpNext = 0;
	io_registry_entry_t		device = 0;
	io_registry_entry_t		controller = 0;

	
	bool					result = false;
	CFMutableDictionaryRef	properties = NULL;
	io_name_t				name;
	CFNumberRef				cfnValue = NULL;
	
	CFMutableDictionaryRef	cfdElement = NULL;
	
	CFBooleanRef	cfbWritable = NULL;
	CFNumberRef		cfnStatus = NULL;
	CFNumberRef		cfnConfiguration = NULL;
	CFDataRef		cfdName = NULL;
	CFStringRef		cfsDeviceIDString = NULL;
	CFDataRef		cfdAddress = NULL;
	CFNumberRef		cfnHWType = NULL;
	CFNumberRef		cfnHWVersion = NULL;
	CFNumberRef		cfnMRB = NULL;
	CFNumberRef		cfnVID = NULL;
					
	int				count;
	bool			found;
		
	// Obtain the I/O Kit communication handle.
	kernResult = IOMasterPort(MACH_PORT_NULL, &dataPort);
	if(kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "can't obtain I/O Kit's master port. %d\n", kernResult);
		
		result = false;
		
		goto cleanup;
	}
	
	// Obtain the NDASBusEnumerator service.
	classToMatch = IOServiceMatching(DriversIOKitClassName);
	if(classToMatch == NULL) {
		syslog(LOG_ERR, "IOServiceMatching returned a NULL dictionary.");
		
		result = false;
		
		goto cleanup;
	}
	
    kernResult = IOServiceGetMatchingServices(dataPort, classToMatch, &iterator);
	if(kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "IOServiceGetMatchingServices returned. %d\n", kernResult);
		
		result = false;
		
		goto cleanup;
	}
	
    serviceObject = IOIteratorNext(iterator);
    IOObjectRelease(iterator);
    if (!serviceObject)
    {
		syslog(LOG_ERR, "Couldn't find any matches.");
		
		result = false;
		
		goto cleanup;
    }
	
	kernResult = IORegistryEntryGetChildIterator(serviceObject, kIOServicePlane, &devices);
	if(kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "can't obtain devices. %d\n", kernResult);
		
		result = false;
		
		goto cleanup;
	}
	
	deviceUpNext = IOIteratorNext(devices);
		
	found = false;
	// Traverse over the children of this service.
	while(deviceUpNext && found == false) {
				
		device = deviceUpNext;
		deviceUpNext = IOIteratorNext(devices);
				
		IORegistryEntryGetNameInPlane(device, kIOServicePlane, name);
		if ( 0 != strcmp(name, "com_ximeta_driver_NDASDevice")) {
			//syslog(LOG_ERR, "Not NDAS Device %s\n", name, deviceUpNext);
			
			IOObjectRelease(device);

			continue;
		}
		
		
		// Get Properties.
		kernResult = IORegistryEntryCreateCFProperties(
													   device,
													   &properties,
													   kCFAllocatorDefault,
													   kNilOptions
													   );		
		
		if (kernResult == KERN_SUCCESS
			&& properties) {
			
			// Address.
			if(CFDictionaryGetValueIfPresent(properties, CFSTR(NDASDEVICE_PROPERTY_KEY_ADDRESS), (const void**)&cfdAddress)
			   && cfdAddress) {
				CFRetain(cfdAddress);
				
				if (0 == memcmp(parameter->address, CFDataGetBytePtr(cfdAddress), 6)) {
					cfdElement = properties;
					
					//
					// Get Device Controller.
					//
					kernResult = IORegistryEntryGetChildEntry(device, kIOServicePlane, &controller);
					if(kernResult != KERN_SUCCESS) {
						syslog(LOG_ERR, "IORegistryEntryGetChildEntry returned. %d\n", kernResult);
						
						result = false;
						
						CFRelease(properties);
						
						IOObjectRelease(device);
						
						goto cleanup;
					}
					
					CFRetain(cfdElement);
					
					found = true;				
				}
				
				CFRelease(cfdAddress);
			}
			
			CFRelease(properties);
			
			IOObjectRelease(device);
		}
	}
		
	if(cfdElement != NULL) {
		
		// SlotNumber.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_SLOT_NUMBER), (const void**)&cfnValue)
		   && cfnValue) {
			CFRetain(cfnValue);
			CFNumberGetValue(cfnValue, kCFNumberSInt32Type, &parameter->slotNumber);
		}
		
		// Writable.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_WRITE_RIGHT), (const void**)&cfbWritable)
		   && cfbWritable) {
			CFRetain(cfbWritable);
			parameter->writable = CFBooleanGetValue(cfbWritable);
		}
		
		// Status.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_STATUS), (const void**)&cfnStatus)
		   && cfnStatus) {
			CFRetain(cfnStatus);
			CFNumberGetValue(cfnStatus, kCFNumberSInt32Type, &parameter->status);
		}
		
		// Configuration.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_CONFIGURATION), (const void**)&cfnConfiguration)
		   && cfnConfiguration) {
			CFRetain(cfnConfiguration);
			CFNumberGetValue(cfnConfiguration, kCFNumberSInt32Type, &parameter->configuration);
		}
		
		// Name.
		//if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_NAME), (const void**)&cfsName)) {
		//	CFStringGetCString(cfsName, parameter->name, 256, kCFStringEncodingUTF8); //CFStringGetSystemEncoding());
		//}
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_NAME), (const void**)&cfdName)
		   && cfdName) {
			CFRetain(cfdName);
			memcpy(parameter->name, CFDataGetBytePtr(cfdName), MAX_NDAS_NAME_SIZE);
		}
		
		// Device ID String.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_DEVICEID_STRING), (const void**)&cfsDeviceIDString)
		   && cfsDeviceIDString) {
			CFRetain(cfsDeviceIDString);
			CFStringGetCString(cfsDeviceIDString, parameter->deviceIDString, 256, CFStringGetSystemEncoding());
		}
		
		// Version.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_HW_TYPE), (const void**)&cfnHWType)
		   && cfnHWType) {
			CFRetain(cfnHWType);
			CFNumberGetValue(cfnHWType, kCFNumberSInt32Type, &parameter->hwType);
		} else {
			parameter->hwType = 0;
		}
		
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_HW_VERSION), (const void**)&cfnHWVersion)
		   && cfnHWVersion) {
			CFRetain(cfnHWVersion);
			CFNumberGetValue(cfnHWVersion, kCFNumberSInt32Type, &parameter->hwVersion);
		} else {
			parameter->hwVersion = 0;
		}
		
		// Max Request Blocks.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_MRB), (const void**)&cfnMRB)
		   && cfnMRB) {
			CFRetain(cfnMRB);
			CFNumberGetValue(cfnMRB, kCFNumberSInt32Type, &parameter->maxRequestBlocks);
			parameter->maxRequestBlocks /= DISK_BLOCK_SIZE;
		} else {
			parameter->maxRequestBlocks = 0;
		}
		
		// VID.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_VID), (const void**)&cfnVID)
		   && cfnVID) {
			CFRetain(cfnVID);
			CFNumberGetValue(cfnVID, kCFNumberSInt8Type, &parameter->vid);
		} else {
			parameter->vid = 0;
		}
		
		//
		// Get Unit Devices.
		//
		
		//
		// Init.
		//
		for(count = 0; count < MAX_NR_OF_TARGETS_PER_DEVICE; count++) {
			parameter->Units[count].status = kNDASUnitStatusNotPresent;
			parameter->Units[count].configuration = kNDASUnitConfigurationUnmount;
		}
	
		if ( kNDASStatusOnline == parameter->status ) {
			
			io_iterator_t			UnitDevices = 0;
			io_registry_entry_t		UnitDeviceUpNext = 0;
			
			//
			// Iterate Unit Devices.
			//
			kernResult = IORegistryEntryGetChildIterator(controller, kIOServicePlane, &UnitDevices);
			if(kernResult != KERN_SUCCESS) {
				syslog(LOG_ERR, "can't obtain Unit Iterator. %d\n", kernResult);
				
				result = false;
				
				goto cleanup;
			}
			
			UnitDeviceUpNext = IOIteratorNext(UnitDevices);
			
			while(UnitDeviceUpNext) {

				io_registry_entry_t		UnitDevice = 0;
				CFMutableDictionaryRef	UnitProperties = NULL;
				
				CFNumberRef		cfnUnitNumber = NULL;
				UInt32			unitNumber;
				CFDataRef		cfdGUID = NULL;
				CFNumberRef		cfnunitDiskLocation = NULL;
				CFNumberRef		cfnDeviceID = NULL;
				CFNumberRef		cfnUnitStatus = NULL;
				CFNumberRef		cfnUnitConfiguration = NULL;
				CFNumberRef		cfnUnitRWHost = NULL;
				CFNumberRef		cfnUnitROHost = NULL;
				CFStringRef		cfsUnitModel = NULL;
				CFStringRef		cfsUnitFirmware = NULL;
				CFStringRef		cfsUnitSerialNumber = NULL;
				CFNumberRef		cfnUnitCapacity = NULL;
				CFNumberRef		cfnUnitType = NULL;
				CFNumberRef		cfnUnitTransferMode = NULL;
				CFNumberRef		cfnUnitDiskArrayType = NULL;
				CFNumberRef		cfnUnitBlockSize = NULL;
				
				uint32_t		blockSize;
				uint64_t		numberOfBlocks;
				
				UnitDevice = UnitDeviceUpNext;
				UnitDeviceUpNext = IOIteratorNext(UnitDevices);
				
				IORegistryEntryGetNameInPlane(UnitDevice, kIOServicePlane, name);
				if ( 0 != strcmp(name, "com_ximeta_driver_NDASPhysicalUnitDevice")) {
					syslog(LOG_ERR, "Not NDAS Unit Device %s\n", name, deviceUpNext);
					
					IOObjectRelease(UnitDevice);
					
					continue;
				}
				
				// Get Properties.
				kernResult = IORegistryEntryCreateCFProperties(
															   UnitDevice,
															   &UnitProperties,
															   kCFAllocatorDefault,
															   kNilOptions
															   );		
				if(kernResult != KERN_SUCCESS) {
					syslog(LOG_ERR, "can't obtain Unit Properties. %d\n", kernResult);
					
					result = false;
					
					goto cleanup;
				}
								
				// Unit Number.
				if(CFDictionaryGetValueIfPresent(UnitProperties, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_NUMBER), (const void**)&cfnUnitNumber)
				   && cfnUnitNumber) {
					CFRetain(cfnUnitNumber);
					CFNumberGetValue(cfnUnitNumber, kCFNumberSInt32Type, &unitNumber);
					
					// Unit Disk GUID.
					if(CFDictionaryGetValueIfPresent(UnitProperties, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_DEVICE_ID), (const void**)&cfdGUID)
					   && cfdGUID) {
						CFRetain(cfdGUID);
						memcpy(&parameter->Units[unitNumber].deviceID, CFDataGetBytePtr(cfdGUID), sizeof(GUID));
					}
					
					// Unit Disk Location.
					if(CFDictionaryGetValueIfPresent(UnitProperties, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_UNIT_DISK_LOCATION), (const void**)&cfnunitDiskLocation)
					   && cfnunitDiskLocation) {
						CFRetain(cfnunitDiskLocation);
						CFNumberGetValue(cfnunitDiskLocation, kCFNumberSInt64Type, &parameter->Units[unitNumber].unitDiskLocation);
					}
					
					// Configuration.				
					if(CFDictionaryGetValueIfPresent(UnitProperties, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_CONFIGURATION), (const void**)&cfnUnitConfiguration)
					   && cfnUnitConfiguration) {
						CFRetain(cfnUnitConfiguration);
						CFNumberGetValue(cfnUnitConfiguration, kCFNumberSInt32Type, &parameter->Units[unitNumber].configuration);
					}
					
					// Status.
					if(CFDictionaryGetValueIfPresent(UnitProperties, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_STATUS), (const void**)&cfnUnitStatus)
					   && cfnUnitStatus) {
						CFRetain(cfnUnitStatus);
						CFNumberGetValue(cfnUnitStatus, kCFNumberSInt32Type, &parameter->Units[unitNumber].status);
					} else {
						parameter->Units[unitNumber].status = kNDASUnitStatusNotPresent;
					}
					
					if(kNDASUnitStatusNotPresent != parameter->Units[unitNumber].status) {
						
						// Number of RW Hosts.
						if(CFDictionaryGetValueIfPresent(UnitProperties, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_NR_RW_HOST), (const void**)&cfnUnitRWHost)
						   && cfnUnitRWHost) {
							CFRetain(cfnUnitRWHost);
							CFNumberGetValue(cfnUnitRWHost, kCFNumberSInt32Type, &parameter->Units[unitNumber].RWHosts);
						} else {
							parameter->Units[unitNumber].RWHosts = 0;
						}
						
						// Number of RO Hosts.
						if(CFDictionaryGetValueIfPresent(UnitProperties, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_NR_RO_HOST), (const void**)&cfnUnitROHost)
						   && cfnUnitROHost) {
							CFRetain(cfnUnitROHost);
							CFNumberGetValue(cfnUnitROHost, kCFNumberSInt32Type, &parameter->Units[unitNumber].ROHosts);
						} else {
							parameter->Units[unitNumber].ROHosts = 0;
						}
						
						// Model.
						if(CFDictionaryGetValueIfPresent(UnitProperties, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_MODEL), (const void**)&cfsUnitModel)
						   && cfsUnitModel) {
							CFRetain(cfsUnitModel);
							CFStringGetCString(cfsUnitModel, parameter->Units[unitNumber].model, 42, CFStringGetSystemEncoding());
						}
						
						// Firmware.
						if(CFDictionaryGetValueIfPresent(UnitProperties, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_FIRMWARE), (const void**)&cfsUnitFirmware)
						   && cfsUnitFirmware) {
							CFRetain(cfsUnitFirmware);
							CFStringGetCString(cfsUnitFirmware, parameter->Units[unitNumber].firmware, 10, CFStringGetSystemEncoding());
						}
						
						// Serial Number.
						if(CFDictionaryGetValueIfPresent(UnitProperties, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_SERIALNUMBER), (const void**)&cfsUnitSerialNumber)
						   && cfsUnitSerialNumber) {
							CFRetain(cfsUnitSerialNumber);
							CFStringGetCString(cfsUnitSerialNumber, parameter->Units[unitNumber].serialNumber, 22, CFStringGetSystemEncoding());
						}
						
						// Block Size.
						if(CFDictionaryGetValueIfPresent(UnitProperties, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_BLOCK_SIZE), (const void**)&cfnUnitBlockSize)
						   && cfnUnitBlockSize) {
							CFRetain(cfnUnitBlockSize);
							CFNumberGetValue(cfnUnitBlockSize, kCFNumberSInt32Type, &blockSize);
						} else {
							blockSize = DISK_BLOCK_SIZE;
						}
						
						// Number of Blocks.
						if(CFDictionaryGetValueIfPresent(UnitProperties, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_NUMBER_OF_SECTORS), (const void**)&cfnUnitCapacity)
						   && cfnUnitCapacity) {
							CFRetain(cfnUnitCapacity);
							CFNumberGetValue(cfnUnitCapacity, kCFNumberSInt64Type, &numberOfBlocks);
						} else {
							numberOfBlocks = 0;
						}
						
						// Capacity
						parameter->Units[unitNumber].capacity = blockSize * numberOfBlocks;
						
						// Transfer Mode.
						if(CFDictionaryGetValueIfPresent(UnitProperties, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_TRANSFER_MODE), (const void**)&cfnUnitTransferMode)
						   && cfnUnitTransferMode) {
							CFRetain(cfnUnitTransferMode);
							CFNumberGetValue(cfnUnitTransferMode, kCFNumberSInt8Type, &parameter->Units[unitNumber].transferType);
						}
						
						// Disk Array Type.
						if(CFDictionaryGetValueIfPresent(UnitProperties, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_DISK_ARRAY_TYPE), (const void**)&cfnUnitDiskArrayType)
						   && cfnUnitDiskArrayType) {
							CFRetain(cfnUnitDiskArrayType);
							CFNumberGetValue(cfnUnitDiskArrayType, kCFNumberSInt32Type, &parameter->Units[unitNumber].diskArrayType);
						}
						
						// Unit  Type.
						if(CFDictionaryGetValueIfPresent(UnitProperties, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_TYPE), (const void**)&cfnUnitType)
						   && cfnUnitType) {
							CFRetain(cfnUnitType);
							CFNumberGetValue(cfnUnitType, kCFNumberSInt8Type, &parameter->Units[unitNumber].type);
						}
					}
				}
								
				IOObjectRelease(UnitDevice);

				if(UnitProperties)			CFRelease(UnitProperties);
				
				if(cfnDeviceID)				CFRelease(cfnDeviceID);
				if(cfnUnitNumber)			CFRelease(cfnUnitNumber);
				if(cfdGUID)					CFRelease(cfdGUID);
				if(cfnunitDiskLocation)		CFRelease(cfnunitDiskLocation);
				if(cfnUnitStatus)			CFRelease(cfnUnitStatus);
				if(cfnUnitConfiguration)	CFRelease(cfnUnitConfiguration);
				if(cfnUnitRWHost)			CFRelease(cfnUnitRWHost);
				if(cfnUnitROHost)			CFRelease(cfnUnitROHost);
				if(cfsUnitModel)			CFRelease(cfsUnitModel);
				if(cfsUnitFirmware)			CFRelease(cfsUnitFirmware);
				if(cfsUnitSerialNumber)		CFRelease(cfsUnitSerialNumber);
				if(cfnUnitCapacity)			CFRelease(cfnUnitCapacity);
				if(cfnUnitTransferMode)		CFRelease(cfnUnitTransferMode);
				if(cfnUnitType)				CFRelease(cfnUnitType);
				if(cfnUnitDiskArrayType)	CFRelease(cfnUnitDiskArrayType);
				if(cfnUnitBlockSize)		CFRelease(cfnUnitBlockSize);
			}
			
			IOObjectRelease(UnitDevices); UnitDevices = 0;

		}
		
		result = true;
		
		IOObjectRelease(controller); controller = 0;
		
	}

cleanup:

	IOObjectRelease(devices); devices = 0;


	// cleanup
	if(cfbWritable)			CFRelease(cfbWritable);
	if(cfnStatus)			CFRelease(cfnStatus);
	if(cfnConfiguration)	CFRelease(cfnConfiguration);
	if(cfdName)				CFRelease(cfdName);
	if(cfsDeviceIDString)	CFRelease(cfsDeviceIDString);
	if(cfnHWType)			CFRelease(cfnHWType);
	if(cfnHWVersion)		CFRelease(cfnHWVersion);
	if(cfnMRB)				CFRelease(cfnMRB);
	if(cfnValue)			CFRelease(cfnValue);
	if(cfnVID)				CFRelease(cfnVID);

	if(cfdElement)			CFRelease(cfdElement);
	
	return result;
}

bool NDASGetLogicalDeviceInformationByIndex(PNDASLogicalDeviceInformation parameter) 
{

	kern_return_t			kernResult;
	io_connect_t			dataPort;
	io_service_t			serviceObject;
    io_iterator_t			iterator;
    CFDictionaryRef			classToMatch;
	io_iterator_t			devices = 0;
	io_registry_entry_t		deviceUpNext = 0;
	io_registry_entry_t		device = 0;

	bool					result = false;
	CFMutableDictionaryRef	properties = NULL;
	io_name_t				name;
	CFNumberRef				cfnValue = NULL;
	int						index;
	
	CFMutableDictionaryRef	cfdElement = NULL;
	CFStringRef				cfsKey = NULL;
	
	CFBooleanRef	cfbWritable = NULL;
	CFNumberRef		cfnStatus = NULL;
	CFDataRef		cfdDeviceID = NULL;
	CFNumberRef		cfnConfiguration = NULL;
	CFNumberRef		cfnUnitDeviceType = NULL;
	CFNumberRef		cfnPhysicalDeviceSlotNumber = NULL;
	CFNumberRef		cfnPhysicalDeviceUnitNumber = NULL;
	CFStringRef		cfsUnitBSDName = NULL;
	CFNumberRef		cfnRWHosts = NULL;
	CFNumberRef		cfnROHosts = NULL;
				
	bool			found;
		
	// Obtain the I/O Kit communication handle.
	kernResult = IOMasterPort(MACH_PORT_NULL, &dataPort);
	if(kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "can't obtain I/O Kit's master port. %d\n", kernResult);
		
		result = false;
		
		goto cleanup;
	}
	
	// Obtain the NDASBusEnumerator service.
	classToMatch = IOServiceMatching(DriversIOKitClassName);
	if(classToMatch == NULL) {
		syslog(LOG_ERR, "IOServiceMatching returned a NULL dictionary.");
		
		result = false;
		
		goto cleanup;
	}
	
    kernResult = IOServiceGetMatchingServices(dataPort, classToMatch, &iterator);
	if(kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "IOServiceGetMatchingServices returned. %d\n", kernResult);
		
		result = false;
		
		goto cleanup;
	}
	
    serviceObject = IOIteratorNext(iterator);
    IOObjectRelease(iterator);
    if (!serviceObject)
    {
		syslog(LOG_ERR, "Couldn't find any matches.");
		
		result = false;
		
		goto cleanup;
    }
	
	kernResult = IORegistryEntryGetChildIterator(serviceObject, kIOServicePlane, &devices);
	if(kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "can't obtain devices. %d\n", kernResult);
		
		result = false;
		
		goto cleanup;
	}
	
	deviceUpNext = IOIteratorNext(devices);
		
	found = false;
	// Traverse over the children of this service.
	while(deviceUpNext && found == false) {
				
		device = deviceUpNext;
		deviceUpNext = IOIteratorNext(devices);
				
		IORegistryEntryGetNameInPlane(device, kIOServicePlane, name);
		if ( 0 != strcmp(name, "com_ximeta_driver_NDASLogicalDevice")) {
			//syslog(LOG_ERR, "Not NDAS Device %s\n", name, deviceUpNext);
			
			IOObjectRelease(device);

			continue;
		}
		
		
		// Get Properties.
		kernResult = IORegistryEntryCreateCFProperties(
													   device,
													   &properties,
													   kCFAllocatorDefault,
													   kNilOptions
													   );		
		
		if (KERN_SUCCESS == kernResult
			&& properties) {
			// Index is Key.
			if (CFDictionaryGetValueIfPresent(properties, CFSTR(NDASDEVICE_PROPERTY_KEY_INDEX), (const void**)&cfnValue)) { 
				
				CFRetain(cfnValue);
				
				CFNumberGetValue (cfnValue, kCFNumberSInt32Type, &index);
				
				// Compare Slotnumber
				if(index == parameter->index) {
					cfdElement = properties;
					CFRetain(cfdElement);
					
					found = true;				
				}
				
				CFRelease(cfnValue);			
			}
			
			CFRelease(properties);
		}
		
		IOObjectRelease(device);
		
	}
		
	if(cfdElement != NULL) {
		
		// Lower Device ID.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_LOGICAL_DEVICE_LOWER_UNIT_DEVICE_ID), (const void**)&cfdDeviceID)
		   && cfdDeviceID) {
			CFRetain(cfdDeviceID);
			
			memcpy(&parameter->LowerUnitDeviceID, CFDataGetBytePtr(cfdDeviceID), sizeof(GUID));
		}
		
		// Writable.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_WRITE_RIGHT), (const void**)&cfbWritable)
		   && cfbWritable) {
			CFRetain(cfbWritable);
			parameter->writable = CFBooleanGetValue(cfbWritable);
		}
		
		// Status.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_STATUS), (const void**)&cfnStatus)
		   && cfnStatus) {
			CFRetain(cfnStatus);
			CFNumberGetValue(cfnStatus, kCFNumberSInt32Type, &parameter->status);
		}
		
		// Configuration.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_CONFIGURATION), (const void**)&cfnConfiguration)
		   && cfnConfiguration) {
			CFRetain(cfnConfiguration);
			CFNumberGetValue(cfnConfiguration, kCFNumberSInt32Type, &parameter->configuration);
		}

		// Unit Device Type.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_DEVICE_TYPE), (const void**)&cfnUnitDeviceType)
		   && cfnUnitDeviceType) {
			CFRetain(cfnUnitDeviceType);
			CFNumberGetValue(cfnUnitDeviceType, kCFNumberSInt32Type, &parameter->unitDeviceType);
		}

		 // BSD Name.
		 if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_DEVICE_BSD_NAME), (const void**)&cfsUnitBSDName)
			&& cfsUnitBSDName) {
			 CFRetain(cfsUnitBSDName);
			 CFStringGetCString(cfsUnitBSDName, parameter->BSDName, 256, CFStringGetSystemEncoding());
		 }
		
		// Number of RW hosts.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_NR_RW_HOST), (const void**)&cfnRWHosts)
		   && cfnRWHosts) {
			CFRetain(cfnRWHosts);
			CFNumberGetValue(cfnRWHosts, kCFNumberSInt32Type, &parameter->RWHosts);
		}
		
		// Number of RO hosts.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_NR_RO_HOST), (const void**)&cfnROHosts)
		   && cfnROHosts) {
			CFRetain(cfnROHosts);
			CFNumberGetValue(cfnROHosts, kCFNumberSInt32Type, &parameter->ROHosts);
		}
				
		switch ( parameter->unitDeviceType) {
			case NMT_SINGLE:
			case NMT_CDROM:
			{
				// Slot Number.
				if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_PHYSICAL_DEVICE_SLOT_NUMBER), (const void**)&cfnPhysicalDeviceSlotNumber)
				   && cfnPhysicalDeviceSlotNumber) {
					CFRetain(cfnPhysicalDeviceSlotNumber);
					CFNumberGetValue(cfnPhysicalDeviceSlotNumber, kCFNumberSInt32Type, &parameter->physicalUnitSlotNumber);
				}
				
				// Unit Number.
				if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_PHYSICAL_DEVICE_UNIT_NUMBER), (const void**)&cfnPhysicalDeviceUnitNumber)
				   && cfnPhysicalDeviceUnitNumber) {
					CFRetain(cfnPhysicalDeviceUnitNumber);
					CFNumberGetValue(cfnPhysicalDeviceUnitNumber, kCFNumberSInt32Type, &parameter->physicalUnitUnitNumber);
				}
				
			}
				break;
			default:
				break;
		}
		
		result = true;
		
		}

cleanup:

	if(devices) {
		IOObjectRelease(devices); 
		devices = 0;
	}
	
	// cleanup
	if(cfsKey)						CFRelease(cfsKey);

	if(cfbWritable)					CFRelease(cfbWritable);
	if(cfdDeviceID)					CFRelease(cfdDeviceID);
	if(cfnStatus)					CFRelease(cfnStatus);
	if(cfnConfiguration)			CFRelease(cfnConfiguration);
	if(cfnUnitDeviceType)			CFRelease(cfnUnitDeviceType);
	if(cfnPhysicalDeviceSlotNumber)	CFRelease(cfnPhysicalDeviceSlotNumber);
	if(cfnPhysicalDeviceUnitNumber)	CFRelease(cfnPhysicalDeviceUnitNumber);
	if(cfsUnitBSDName)				CFRelease(cfsUnitBSDName);
	if(cfnRWHosts)					CFRelease(cfnRWHosts);
	if(cfnROHosts)					CFRelease(cfnROHosts);
	
	if(cfdElement)					CFRelease(cfdElement);
	
	return result;
}

bool NDASGetLogicalDeviceInformationByLowerUnitDeviceID(PNDASLogicalDeviceInformation parameter) 
{

	kern_return_t			kernResult;
	io_connect_t			dataPort;
	io_service_t			serviceObject;
    io_iterator_t			iterator;
    CFDictionaryRef			classToMatch;
	io_iterator_t			devices = 0;
	io_registry_entry_t		deviceUpNext = 0;
	io_registry_entry_t		device = 0;

	bool					result = false;
	CFMutableDictionaryRef	properties = NULL;
	io_name_t				name;
	
	CFMutableDictionaryRef	cfdElement = NULL;
	CFStringRef				cfsKey = NULL;
	
	CFBooleanRef	cfbWritable = NULL;
	CFNumberRef		cfnIndex = NULL;
	CFNumberRef		cfnStatus = NULL;
	CFDataRef		cfdDeviceID = NULL;
	CFNumberRef		cfnConfiguration = NULL;
	CFNumberRef		cfnUnitDeviceType = NULL;
	CFNumberRef		cfnPhysicalDeviceSlotNumber = NULL;
	CFNumberRef		cfnPhysicalDeviceUnitNumber = NULL;
	CFStringRef		cfsUnitBSDName = NULL;
	CFNumberRef		cfnROHosts = NULL;
	CFNumberRef		cfnRWHosts = NULL;
				
	bool			found;
		
	// Obtain the I/O Kit communication handle.
	kernResult = IOMasterPort(MACH_PORT_NULL, &dataPort);
	if(kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "can't obtain I/O Kit's master port. %d\n", kernResult);
		
		result = false;
		
		goto cleanup;
	}
	
	// Obtain the NDASBusEnumerator service.
	classToMatch = IOServiceMatching(DriversIOKitClassName);
	if(classToMatch == NULL) {
		syslog(LOG_ERR, "IOServiceMatching returned a NULL dictionary.");
		
		result = false;
		
		goto cleanup;
	}
	
    kernResult = IOServiceGetMatchingServices(dataPort, classToMatch, &iterator);
	if(kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "IOServiceGetMatchingServices returned. %d\n", kernResult);
		
		result = false;
		
		goto cleanup;
	}
	
    serviceObject = IOIteratorNext(iterator);
    IOObjectRelease(iterator);
    if (!serviceObject)
    {
		syslog(LOG_ERR, "Couldn't find any matches.");
		
		result = false;
		
		goto cleanup;
    }
	
	kernResult = IORegistryEntryGetChildIterator(serviceObject, kIOServicePlane, &devices);
	if(kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "can't obtain devices. %d\n", kernResult);
		
		result = false;
		
		goto cleanup;
	}
	
	deviceUpNext = IOIteratorNext(devices);
		
	found = false;
	// Traverse over the children of this service.
	while(deviceUpNext && found == false) {
				
		device = deviceUpNext;
		deviceUpNext = IOIteratorNext(devices);
				
		IORegistryEntryGetNameInPlane(device, kIOServicePlane, name);
		if ( 0 != strcmp(name, "com_ximeta_driver_NDASLogicalDevice")) {
			//syslog(LOG_ERR, "Not NDAS Device %s\n", name, deviceUpNext);
			
			IOObjectRelease(device);

			continue;
		}
		
		// Get Properties.
		kernResult = IORegistryEntryCreateCFProperties(
													   device,
													   &properties,
													   kCFAllocatorDefault,
													   kNilOptions
													   );	
		
		if (KERN_SUCCESS == kernResult
			&& properties) {
			
			// Index is Lower Unit Device ID.
			if (CFDictionaryGetValueIfPresent(properties, CFSTR(NDASDEVICE_PROPERTY_KEY_LOGICAL_DEVICE_LOWER_UNIT_DEVICE_ID), (const void**)&cfdDeviceID)) { 
				
				CFRetain(cfdDeviceID);
				
				// Compare Slotnumber
				if(0 == memcmp(&parameter->LowerUnitDeviceID, CFDataGetBytePtr(cfdDeviceID), sizeof(GUID))) {
					
					cfdElement = properties;
					CFRetain(cfdElement);
					
					found = true;				
				}
				
				CFRelease(cfdDeviceID);
			}
			
			CFRelease(properties);
		}
		
		IOObjectRelease(device);
		
	}
		
	if(cfdElement != NULL) {
				
		// Index.
		if (CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_INDEX), (const void**)&cfnIndex)
			&& cfnIndex) { 
			CFRetain(cfnIndex);			
			CFNumberGetValue (cfnIndex, kCFNumberSInt32Type, &parameter->index);			
		}
		
		// Writable.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_WRITE_RIGHT), (const void**)&cfbWritable)
		   && cfbWritable) {
			CFRetain(cfbWritable);
			parameter->writable = CFBooleanGetValue(cfbWritable);
		}
		
		// Status.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_STATUS), (const void**)&cfnStatus)
		   && cfnStatus) {
			CFRetain(cfnStatus);
			CFNumberGetValue(cfnStatus, kCFNumberSInt32Type, &parameter->status);
		}
		
		// Configuration.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_CONFIGURATION), (const void**)&cfnConfiguration)
		   && cfnConfiguration) {
			CFRetain(cfnConfiguration);
			CFNumberGetValue(cfnConfiguration, kCFNumberSInt32Type, &parameter->configuration);
		}

		// Unit Device Type.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_DEVICE_TYPE), (const void**)&cfnUnitDeviceType)
		   && cfnUnitDeviceType) {
			CFRetain(cfnUnitDeviceType);
			CFNumberGetValue(cfnUnitDeviceType, kCFNumberSInt32Type, &parameter->unitDeviceType);
		}

		// BSD Name.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_DEVICE_BSD_NAME), (const void**)&cfsUnitBSDName)
		   && cfsUnitBSDName) {
			CFRetain(cfsUnitBSDName);
			CFStringGetCString(cfsUnitBSDName, parameter->BSDName, 256, CFStringGetSystemEncoding());
		}

		// Number of RW hosts.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_NR_RW_HOST), (const void**)&cfnRWHosts)
		   && cfnRWHosts) {
			CFRetain(cfnRWHosts);
			CFNumberGetValue(cfnRWHosts, kCFNumberSInt32Type, &parameter->RWHosts);
		}

		// Number of RO hosts.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_NR_RO_HOST), (const void**)&cfnROHosts)
		   && cfnROHosts) {
			CFRetain(cfnROHosts);
			CFNumberGetValue(cfnROHosts, kCFNumberSInt32Type, &parameter->ROHosts);
		}
		
		switch ( parameter->unitDeviceType) {
			case NMT_SINGLE:
			case NMT_CDROM:
			{
				// Slot Number.
				if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_PHYSICAL_DEVICE_SLOT_NUMBER), (const void**)&cfnPhysicalDeviceSlotNumber)
				   && cfnPhysicalDeviceSlotNumber) {
					CFRetain(cfnPhysicalDeviceSlotNumber);
					CFNumberGetValue(cfnPhysicalDeviceSlotNumber, kCFNumberSInt32Type, &parameter->physicalUnitSlotNumber);
				}
				
				// Unit Number.
				if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_PHYSICAL_DEVICE_UNIT_NUMBER), (const void**)&cfnPhysicalDeviceUnitNumber)
				   && cfnPhysicalDeviceUnitNumber) {
					CFRetain(cfnPhysicalDeviceUnitNumber);
					CFNumberGetValue(cfnPhysicalDeviceUnitNumber, kCFNumberSInt32Type, &parameter->physicalUnitUnitNumber);
				}
				
			}
				break;
			default:
				break;
		}
		
		result = true;
		
		}

cleanup:

	if(devices) {
		IOObjectRelease(devices); 
		devices = 0;
	}
	
	// cleanup
	if(cfsKey)						CFRelease(cfsKey);

	if(cfbWritable)					CFRelease(cfbWritable);
	if(cfnIndex)					CFRelease(cfnIndex);
	if(cfnStatus)					CFRelease(cfnStatus);
	if(cfnConfiguration)			CFRelease(cfnConfiguration);
	if(cfnUnitDeviceType)			CFRelease(cfnUnitDeviceType);
	if(cfnPhysicalDeviceSlotNumber)	CFRelease(cfnPhysicalDeviceSlotNumber);
	if(cfnPhysicalDeviceUnitNumber)	CFRelease(cfnPhysicalDeviceUnitNumber);
	if(cfsUnitBSDName)				CFRelease(cfsUnitBSDName);
	if(cfnRWHosts)					CFRelease(cfnRWHosts);
	if(cfnROHosts)					CFRelease(cfnROHosts);
	
	if(cfdElement)					CFRelease(cfdElement);

	return result;
}

bool NDASGetRAIDDeviceInformationByIndex(PNDASRAIDDeviceInformation parameter) 
{

	kern_return_t			kernResult;
	io_connect_t			dataPort;
	io_service_t			serviceObject;
    io_iterator_t			iterator;
    CFDictionaryRef			classToMatch;
	io_iterator_t			devices = 0;
	io_registry_entry_t		deviceUpNext = 0;
	io_registry_entry_t		device = 0;

	bool					result = false;
	CFMutableDictionaryRef	properties = NULL;
	io_name_t				name;
	CFNumberRef				cfnValue = NULL;
	int						index;
	
	CFMutableDictionaryRef	cfdElement = NULL;
	CFStringRef				cfsKey = NULL;
	
	CFDataRef		cfdDeviceID = NULL;
	CFNumberRef		cfnUnitDiskLocation = NULL;
	CFNumberRef		cfnDiskArrayType = NULL;
	CFBooleanRef	cfbWritable = NULL;
	CFNumberRef		cfnStatus = NULL;
	CFNumberRef		cfnConfiguration = NULL;
	CFNumberRef		cfnUnitDeviceType = NULL;
	CFNumberRef		cfnPhysicalDeviceSlotNumber = NULL;
	CFNumberRef		cfnPhysicalDeviceUnitNumber = NULL;
	CFNumberRef		cfnCountOfUnitDevices = NULL;
	CFNumberRef		cfnCountOfSpareDevices = NULL;
	CFNumberRef		cfnUnitCapacity = NULL;
	CFNumberRef		cfnRWHost = NULL;
	CFNumberRef		cfnROHost = NULL;
	CFDataRef		cfdName = NULL;
	CFNumberRef		cfnRAIDStatus = NULL;
	CFNumberRef		cfnRecoveringSector = NULL;
	
	bool			found;
	int				count;
	
	// Obtain the I/O Kit communication handle.
	kernResult = IOMasterPort(MACH_PORT_NULL, &dataPort);
	if(kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "can't obtain I/O Kit's master port. %d\n", kernResult);
		
		result = false;
		
		goto cleanup;
	}
	
	// Obtain the NDASBusEnumerator service.
	classToMatch = IOServiceMatching(DriversIOKitClassName);
	if(classToMatch == NULL) {
		syslog(LOG_ERR, "IOServiceMatching returned a NULL dictionary.");
		
		result = false;
		
		goto cleanup;
	}
	
    kernResult = IOServiceGetMatchingServices(dataPort, classToMatch, &iterator);
	if(kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "IOServiceGetMatchingServices returned. %d\n", kernResult);
		
		result = false;
		
		goto cleanup;
	}
	
    serviceObject = IOIteratorNext(iterator);
    IOObjectRelease(iterator);
    if (!serviceObject)
    {
		syslog(LOG_ERR, "Couldn't find any matches.");
		
		result = false;
		
		goto cleanup;
    }
	
	kernResult = IORegistryEntryGetChildIterator(serviceObject, kIOServicePlane, &devices);
	if(kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "can't obtain devices. %d\n", kernResult);
		
		result = false;
		
		goto cleanup;
	}
	
	deviceUpNext = IOIteratorNext(devices);
		
	found = false;
	// Traverse over the children of this service.
	while(deviceUpNext && found == false) {
				
		device = deviceUpNext;
		deviceUpNext = IOIteratorNext(devices);
				
		IORegistryEntryGetNameInPlane(device, kIOServicePlane, name);
		if ( 0 != strcmp(name, "com_ximeta_driver_NDASRAIDUnitDevice")) {
//			syslog(LOG_ERR, "Not NDAS Device %s\n", name, deviceUpNext);
			
			IOObjectRelease(device);

			continue;
		}
		
		
		// Get Properties.
		kernResult = IORegistryEntryCreateCFProperties(
													   device,
													   &properties,
													   kCFAllocatorDefault,
													   kNilOptions
													   );		
		
		if (KERN_SUCCESS == kernResult
			&& properties) {
			
			// Index is Key.
			if (CFDictionaryGetValueIfPresent(properties, CFSTR(NDASDEVICE_PROPERTY_KEY_INDEX), (const void**)&cfnValue)) { 
				
				CFRetain(cfnValue);
				
				CFNumberGetValue (cfnValue, kCFNumberSInt32Type, &index);
				
				// Compare Slotnumber
				if(index == parameter->index) {
					cfdElement = properties;
					CFRetain(cfdElement);
					
					found = true;				
				}
				
				CFRelease(cfnValue);			
			}
			
			CFRelease(properties);
		}
		
		IOObjectRelease(device);
		
	}
				
	if(cfdElement != NULL) {

		// Device ID.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_DEVICE_ID), (const void**)&cfdDeviceID)
		   && cfdDeviceID) {
			CFRetain(cfdDeviceID);
			
			memcpy(&parameter->deviceID, CFDataGetBytePtr(cfdDeviceID), sizeof(GUID));
		}

		// Unit Disk Location.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_UNIT_DISK_LOCATION), (const void**)&cfnUnitDiskLocation)
		   && cfnUnitDiskLocation) {
			CFRetain(cfnUnitDiskLocation);
			CFNumberGetValue(cfnUnitDiskLocation, kCFNumberSInt64Type, &parameter->unitDiskLocation);
		}
		
		// Disk Array Type.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_DISK_ARRAY_TYPE), (const void**)&cfnDiskArrayType)
		   && cfnDiskArrayType) {
			CFRetain(cfnDiskArrayType);
			CFNumberGetValue(cfnDiskArrayType, kCFNumberSInt32Type, &parameter->unitDeviceType);
		}
		
		// Writable.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_WRITE_RIGHT), (const void**)&cfbWritable)
		   && cfbWritable) {
			CFRetain(cfbWritable);
			parameter->writable = CFBooleanGetValue(cfbWritable);
		}
		
		// Status.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_STATUS), (const void**)&cfnStatus)
		   && cfnStatus) {
			CFRetain(cfnStatus);
			CFNumberGetValue(cfnStatus, kCFNumberSInt32Type, &parameter->status);
		}
		
		// Configuration.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_CONFIGURATION), (const void**)&cfnConfiguration)
		   && cfnConfiguration) {
			CFRetain(cfnConfiguration);
			CFNumberGetValue(cfnConfiguration, kCFNumberSInt32Type, &parameter->configuration);
		}

		// Unit Device Type.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_DEVICE_TYPE), (const void**)&cfnUnitDeviceType)
		   && cfnUnitDeviceType) {
			CFRetain(cfnUnitDeviceType);
			CFNumberGetValue(cfnUnitDeviceType, kCFNumberSInt32Type, &parameter->unitDeviceType);
		}

		// Disk array Type.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_DISK_ARRAY_TYPE), (const void**)&cfnDiskArrayType)
		   && cfnDiskArrayType) {
			CFRetain(cfnDiskArrayType);
			CFNumberGetValue(cfnDiskArrayType, kCFNumberSInt32Type, &parameter->diskArrayType);
		}
		
		// Count of Sub Unit Devices.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_RAID_DEVICE_COUNTOFUNITS), (const void**)&cfnCountOfUnitDevices)
		   && cfnCountOfUnitDevices) {
			CFRetain(cfnCountOfUnitDevices);
			CFNumberGetValue(cfnCountOfUnitDevices, kCFNumberSInt32Type, &parameter->CountOfUnitDevices);
		}

		// Count of Spare Devices.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_RAID_DEVICE_COUNTOFSPARES), (const void**)&cfnCountOfSpareDevices)
		   && cfnCountOfSpareDevices) {
			CFRetain(cfnCountOfSpareDevices);
			CFNumberGetValue(cfnCountOfSpareDevices, kCFNumberSInt32Type, &parameter->CountOfSpareDevices);
		} else {
			parameter->CountOfSpareDevices = 0;
		}
		
		// Number of RW Hosts.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_NR_RW_HOST), (const void**)&cfnRWHost)
		   && cfnRWHost) {
			CFRetain(cfnRWHost);
			CFNumberGetValue(cfnRWHost, kCFNumberSInt32Type, &parameter->RWHosts);
		} else {
			parameter->RWHosts = 0;
		}
		
		// Number of RO Hosts.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_NR_RO_HOST), (const void**)&cfnROHost)
		   && cfnROHost) {
			CFRetain(cfnROHost);
			CFNumberGetValue(cfnROHost, kCFNumberSInt32Type, &parameter->ROHosts);
		} else {
			parameter->ROHosts = 0;
		}
		
		// Capacity.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_CAPACITY), (const void**)&cfnUnitCapacity)
		   && cfnUnitCapacity) {
			CFRetain(cfnUnitCapacity);
			CFNumberGetValue(cfnUnitCapacity, kCFNumberSInt64Type, &parameter->capacity);
			parameter->capacity *= DISK_BLOCK_SIZE;
		} else {			
			parameter->capacity = 0;
		}
		
		// RAID Status.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_RAID_STATUS), (const void**)&cfnRAIDStatus)
		   && cfnRAIDStatus) {
			CFRetain(cfnRAIDStatus);
			CFNumberGetValue(cfnRAIDStatus, kCFNumberSInt32Type, &parameter->RAIDStatus);
		} else {
			parameter->RAIDStatus = kNDASRAIDStatusNotReady;
		}				
		
		// Recovering Sector
		if(NMT_RAID1R3 == parameter->unitDeviceType
		   && kNDASRAIDStatusRecovering == parameter->RAIDStatus
		   && CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_RAID_RECOVERING_SECTOR), (const void**)&cfnRecoveringSector)
		   && cfnRecoveringSector) {
			CFRetain(cfnRecoveringSector);
			CFNumberGetValue(cfnRecoveringSector, kCFNumberSInt64Type, &parameter->RecoveringSector);
		} else {
			parameter->RecoveringSector = 0;
		}
		
		if (parameter->CountOfUnitDevices > NDAS_MAX_UNITS_IN_V2) {
			parameter->CountOfUnitDevices = NDAS_MAX_UNITS_IN_V2;
		}
		
		for (count = 0; count < parameter->CountOfUnitDevices + parameter->CountOfSpareDevices; count++ ) {
			
			CFStringRef		cfsKey = NULL;
			CFNumberRef		cfnUnitID = NULL;
			CFNumberRef		cfnRAIDSubUnitStatus = NULL;
			CFNumberRef		cfnRAIDSubUnitType = NULL;
			
			// Sub Unit Device ID.
			cfsKey = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR(NDASDEVICE_PROPERTY_KEY_RAID_UNIT_DISK_LOCATION_OF_SUB_UNIT), count);			
			if(CFDictionaryGetValueIfPresent(cfdElement, cfsKey, (const void**)&cfnUnitID)
			   && cfnUnitID) {
				CFRetain(cfnUnitID);
				CFNumberGetValue(cfnUnitID, kCFNumberSInt64Type, &parameter->unitDiskLocationOfSubUnits[count]);
			}			
			
			if (cfnUnitID)	CFRelease(cfnUnitID);
			if (cfsKey)		CFRelease(cfsKey);
			
			// Status of Sub Unit Device.
			cfsKey = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR(NDASDEVICE_PROPERTY_KEY_RAID_STATUS_OF_SUB_UNIT), count);
			if(CFDictionaryGetValueIfPresent(cfdElement, cfsKey, (const void**)&cfnRAIDSubUnitStatus)
			   && cfnRAIDSubUnitStatus) {
				CFRetain(cfnRAIDSubUnitStatus);
				CFNumberGetValue(cfnRAIDSubUnitStatus, kCFNumberSInt32Type, &parameter->RAIDSubUnitStatus[count]);
			}
			
			if (cfnRAIDSubUnitStatus)	CFRelease(cfnRAIDSubUnitStatus);
			if (cfsKey)		CFRelease(cfsKey);
			
			// Type of Sub Unit Device.
			cfsKey = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR(NDASDEVICE_PROPERTY_KEY_RAID_TYPE_OF_SUB_UNIT), count);
			if(CFDictionaryGetValueIfPresent(cfdElement, cfsKey, (const void**)&cfnRAIDSubUnitType)
			   && cfnRAIDSubUnitType) {
				CFRetain(cfnRAIDSubUnitType);
				CFNumberGetValue(cfnRAIDSubUnitType, kCFNumberSInt32Type, &parameter->RAIDSubUnitType[count]);
			} else {
				parameter->RAIDSubUnitType[count] = kNDASRAIDSubUnitTypeNormal;
			}
			
			if (cfnRAIDSubUnitStatus)	CFRelease(cfnRAIDSubUnitType);
			if (cfsKey)		CFRelease(cfsKey);
			
		}
		
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_NAME), (const void**)&cfdName)
		   && cfdName) {
			CFRetain(cfdName);
			memcpy(parameter->name, CFDataGetBytePtr(cfdName), MAX_NDAS_NAME_SIZE);
		}
		
/*
		switch ( parameter->unitDeviceType) {
			case kNDASUnitDeviceTypePhysicalHDD:
			case kNDASUnitDeviceTypePhysicalODD:
			{
				// Slot Number.
				if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_PHYSICAL_DEVICE_SLOT_NUMBER), (const void**)&cfnPhysicalDeviceSlotNumber)
				   && cfnPhysicalDeviceSlotNumber) {
					CFRetain(cfnPhysicalDeviceSlotNumber);
					CFNumberGetValue(cfnPhysicalDeviceSlotNumber, kCFNumberSInt32Type, &parameter->physicalUnitSlotNumber);
				}
				
				// Unit Number.
				if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_PHYSICAL_DEVICE_UNIT_NUMBER), (const void**)&cfnPhysicalDeviceUnitNumber)
				   && cfnPhysicalDeviceUnitNumber) {
					CFRetain(cfnPhysicalDeviceUnitNumber);
					CFNumberGetValue(cfnPhysicalDeviceUnitNumber, kCFNumberSInt32Type, &parameter->physicalUnitUnitNumber);
				}
				
			}
				break;
			default:
				break;
		}
*/		
		result = true;
		
		}

cleanup:

	IOObjectRelease(devices); devices = 0;

	// cleanup
	if(cfsKey)						CFRelease(cfsKey);

	if(cfdDeviceID)					CFRelease(cfdDeviceID);
	if(cfnUnitDiskLocation)			CFRelease(cfnUnitDiskLocation);
	if(cfbWritable)					CFRelease(cfbWritable);
	if(cfnStatus)					CFRelease(cfnStatus);
	if(cfnConfiguration)			CFRelease(cfnConfiguration);
	if(cfnUnitDeviceType)			CFRelease(cfnUnitDeviceType);
	if(cfnPhysicalDeviceSlotNumber)	CFRelease(cfnPhysicalDeviceSlotNumber);
	if(cfnPhysicalDeviceUnitNumber)	CFRelease(cfnPhysicalDeviceUnitNumber);
	if(cfnCountOfUnitDevices)		CFRelease(cfnCountOfUnitDevices);
	if(cfnCountOfSpareDevices)		CFRelease(cfnCountOfSpareDevices);
	if(cfnUnitCapacity)				CFRelease(cfnUnitCapacity);
	if(cfdName)						CFRelease(cfdName);
	if(cfnRWHost)					CFRelease(cfnRWHost);
	if(cfnROHost)					CFRelease(cfnROHost);
	if(cfnRAIDStatus)				CFRelease(cfnRAIDStatus);
	if(cfnRecoveringSector)			CFRelease(cfnRecoveringSector);
	
	if(cfdElement)					CFRelease(cfdElement);
	
	return result;
}

bool NDASGetRAIDDeviceInformationByDeviceID(PNDASRAIDDeviceInformation parameter) 
{

	kern_return_t			kernResult;
	io_connect_t			dataPort;
	io_service_t			serviceObject;
    io_iterator_t			iterator;
    CFDictionaryRef			classToMatch;
	io_iterator_t			devices = 0;
	io_registry_entry_t		deviceUpNext = 0;
	io_registry_entry_t		device = 0;

	bool					result = false;
	CFMutableDictionaryRef	properties = NULL;
	io_name_t				name;
	
	CFMutableDictionaryRef	cfdElement = NULL;
	CFStringRef				cfsKey = NULL;
	
	CFNumberRef		cfnIndex = NULL;
	CFDataRef		cfdDeviceID = NULL;
	CFNumberRef		cfnUnitDiskLocation = NULL;
	CFNumberRef		cfnDiskArrayType = NULL;
	CFBooleanRef	cfbWritable = NULL;
	CFNumberRef		cfnStatus = NULL;
	CFNumberRef		cfnConfiguration = NULL;
	CFNumberRef		cfnUnitDeviceType = NULL;
	CFNumberRef		cfnPhysicalDeviceSlotNumber = NULL;
	CFNumberRef		cfnPhysicalDeviceUnitNumber = NULL;
	CFNumberRef		cfnCountOfUnitDevices = NULL;
	CFNumberRef		cfnCountOfSpareDevices = NULL;
	CFNumberRef		cfnUnitCapacity = NULL;
	CFNumberRef		cfnRWHost = NULL;
	CFNumberRef		cfnROHost = NULL;
	CFDataRef		cfdName = NULL;
	CFNumberRef		cfnRAIDStatus = NULL;
	CFNumberRef		cfnRecoveringSector = NULL;
		
	bool			found;
	int				count;
	
	// Obtain the I/O Kit communication handle.
	kernResult = IOMasterPort(MACH_PORT_NULL, &dataPort);
	if(kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "can't obtain I/O Kit's master port. %d\n", kernResult);
		
		result = false;
		
		goto cleanup;
	}
	
	// Obtain the NDASBusEnumerator service.
	classToMatch = IOServiceMatching(DriversIOKitClassName);
	if(classToMatch == NULL) {
		syslog(LOG_ERR, "IOServiceMatching returned a NULL dictionary.");
		
		result = false;
		
		goto cleanup;
	}
	
    kernResult = IOServiceGetMatchingServices(dataPort, classToMatch, &iterator);
	if(kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "IOServiceGetMatchingServices returned. %d\n", kernResult);
		
		result = false;
		
		goto cleanup;
	}
	
    serviceObject = IOIteratorNext(iterator);
    IOObjectRelease(iterator);
    if (!serviceObject)
    {
		syslog(LOG_ERR, "Couldn't find any matches.");
		
		result = false;
		
		goto cleanup;
    }
	
	kernResult = IORegistryEntryGetChildIterator(serviceObject, kIOServicePlane, &devices);
	if(kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "can't obtain devices. %d\n", kernResult);
		
		result = false;
		
		goto cleanup;
	}
	
	deviceUpNext = IOIteratorNext(devices);
	
	found = false;
	// Traverse over the children of this service.
	while(deviceUpNext && found == false) {
		
		device = deviceUpNext;
		deviceUpNext = IOIteratorNext(devices);
		
		IORegistryEntryGetNameInPlane(device, kIOServicePlane, name);
		if ( 0 != strcmp(name, "com_ximeta_driver_NDASRAIDUnitDevice")) {
			//syslog(LOG_ERR, "Not NDAS Device %s\n", name, deviceUpNext);
			
			IOObjectRelease(device);
			
			continue;
		}
		
		
		// Get Properties.
		kernResult = IORegistryEntryCreateCFProperties(
													   device,
													   &properties,
													   kCFAllocatorDefault,
													   kNilOptions
													   );		
		
		if (KERN_SUCCESS == kernResult
			&& properties) {
			
			// Index is Lower Unit Device ID.
			if (CFDictionaryGetValueIfPresent(properties, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_DEVICE_ID), (const void**)&cfdDeviceID)) { 
				
				CFRetain(cfdDeviceID);
				
				// Compare Slotnumber
				if(0 == memcmp(&parameter->deviceID, CFDataGetBytePtr(cfdDeviceID), sizeof(GUID))) {
					
					cfdElement = properties;
					CFRetain(cfdElement);
					
					found = true;				
				}
				
				CFRelease(cfdDeviceID);
			}
			
			CFRelease(properties);
		}
		
		IOObjectRelease(device);
		
	}
	
	if(cfdElement != NULL) {
		
		// Index.
		if (CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_INDEX), (const void**)&cfnIndex)
			&& cfnIndex) { 
			CFRetain(cfnIndex);			
			CFNumberGetValue (cfnIndex, kCFNumberSInt32Type, &parameter->index);			
		}
				
		// Unit Disk Location.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_UNIT_DISK_LOCATION), (const void**)&cfnUnitDiskLocation)
		   && cfnUnitDiskLocation) {
			CFRetain(cfnUnitDiskLocation);
			CFNumberGetValue(cfnUnitDiskLocation, kCFNumberSInt64Type, &parameter->unitDiskLocation);
		}
		
		// Disk Array Type.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_DISK_ARRAY_TYPE), (const void**)&cfnDiskArrayType)
		   && cfnDiskArrayType) {
			CFRetain(cfnDiskArrayType);
			CFNumberGetValue(cfnDiskArrayType, kCFNumberSInt32Type, &parameter->unitDeviceType);
		}
		
		// Writable.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_WRITE_RIGHT), (const void**)&cfbWritable)
		   && cfbWritable) {
			CFRetain(cfbWritable);
			parameter->writable = CFBooleanGetValue(cfbWritable);
		}
		
		// Status.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_STATUS), (const void**)&cfnStatus)
		   && cfnStatus) {
			CFRetain(cfnStatus);
			CFNumberGetValue(cfnStatus, kCFNumberSInt32Type, &parameter->status);
		}
		
		// Configuration.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_CONFIGURATION), (const void**)&cfnConfiguration)
		   && cfnConfiguration) {
			CFRetain(cfnConfiguration);
			CFNumberGetValue(cfnConfiguration, kCFNumberSInt32Type, &parameter->configuration);
		}

		// Unit Device Type.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_DEVICE_TYPE), (const void**)&cfnUnitDeviceType)
		   && cfnUnitDeviceType) {
			CFRetain(cfnUnitDeviceType);
			CFNumberGetValue(cfnUnitDeviceType, kCFNumberSInt32Type, &parameter->unitDeviceType);
		}
		
		// Disk array Type.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_DISK_ARRAY_TYPE), (const void**)&cfnDiskArrayType)
		   && cfnDiskArrayType) {
			CFRetain(cfnDiskArrayType);
			CFNumberGetValue(cfnDiskArrayType, kCFNumberSInt32Type, &parameter->diskArrayType);
		}
		
		// Count of Sub Unit Devices.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_RAID_DEVICE_COUNTOFUNITS), (const void**)&cfnCountOfUnitDevices)
		   && cfnCountOfUnitDevices) {
			CFRetain(cfnCountOfUnitDevices);
			CFNumberGetValue(cfnCountOfUnitDevices, kCFNumberSInt32Type, &parameter->CountOfUnitDevices);
		}

		// Count of Spare Devices.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_RAID_DEVICE_COUNTOFSPARES), (const void**)&cfnCountOfSpareDevices)
		   && cfnCountOfSpareDevices) {
			CFRetain(cfnCountOfSpareDevices);
			CFNumberGetValue(cfnCountOfSpareDevices, kCFNumberSInt32Type, &parameter->CountOfSpareDevices);
		} else {
			parameter->CountOfSpareDevices = 0;
		}

		// Number of RW Hosts.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_NR_RW_HOST), (const void**)&cfnRWHost)
		   && cfnRWHost) {
			CFRetain(cfnRWHost);
			CFNumberGetValue(cfnRWHost, kCFNumberSInt32Type, &parameter->RWHosts);
		} else {
			parameter->RWHosts = 0;
		}
		
		// Number of RO Hosts.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_NR_RO_HOST), (const void**)&cfnROHost)
		   && cfnROHost) {
			CFRetain(cfnROHost);
			CFNumberGetValue(cfnROHost, kCFNumberSInt32Type, &parameter->ROHosts);
		} else {
			parameter->ROHosts = 0;
		}
		
		// Capacity.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_CAPACITY), (const void**)&cfnUnitCapacity)
		   && cfnUnitCapacity) {
			CFRetain(cfnUnitCapacity);
			CFNumberGetValue(cfnUnitCapacity, kCFNumberSInt64Type, &parameter->capacity);
			parameter->capacity *= DISK_BLOCK_SIZE;
		} else {			
			parameter->capacity = 0;
		}
		
		// RAID Status.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_RAID_STATUS), (const void**)&cfnRAIDStatus)
		   && cfnRAIDStatus) {
			CFRetain(cfnRAIDStatus);
			CFNumberGetValue(cfnRAIDStatus, kCFNumberSInt32Type, &parameter->RAIDStatus);
		} else {
			parameter->RAIDStatus = kNDASRAIDStatusNotReady;
		}		
		
		// Recovering Sector
		if(NMT_RAID1R3 == parameter->unitDeviceType
		   && kNDASRAIDStatusRecovering == parameter->RAIDStatus
		   && CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_RAID_RECOVERING_SECTOR), (const void**)&cfnRecoveringSector)
		   && cfnRecoveringSector) {
			CFRetain(cfnRecoveringSector);
			CFNumberGetValue(cfnRecoveringSector, kCFNumberSInt64Type, &parameter->RecoveringSector);
		} else {
			parameter->RecoveringSector = 0;
		}
		
		if (parameter->CountOfUnitDevices > NDAS_MAX_UNITS_IN_V2) {
			parameter->CountOfUnitDevices = NDAS_MAX_UNITS_IN_V2;
		}
		
		for (count = 0; count < parameter->CountOfUnitDevices + parameter->CountOfSpareDevices; count++ ) {
			
			CFStringRef		cfsKey = NULL;
			CFNumberRef		cfnUnitID = NULL;
			CFNumberRef		cfnRAIDSubUnitStatus = NULL;
			CFNumberRef		cfnRAIDSubUnitType = NULL;
			
			// Sub Unit Device ID.
			cfsKey = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR(NDASDEVICE_PROPERTY_KEY_RAID_UNIT_DISK_LOCATION_OF_SUB_UNIT), count);			
			if(CFDictionaryGetValueIfPresent(cfdElement, cfsKey, (const void**)&cfnUnitID)
			   && cfnUnitID) {
				CFRetain(cfnUnitID);
				CFNumberGetValue(cfnUnitID, kCFNumberSInt64Type, &parameter->unitDiskLocationOfSubUnits[count]);
			}			
			
			if (cfnUnitID)	CFRelease(cfnUnitID);
			if (cfsKey)		CFRelease(cfsKey);
			
			// Status of Sub Unit Device.
			cfsKey = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR(NDASDEVICE_PROPERTY_KEY_RAID_STATUS_OF_SUB_UNIT), count);
			if(CFDictionaryGetValueIfPresent(cfdElement, cfsKey, (const void**)&cfnRAIDSubUnitStatus)
			   && cfnRAIDSubUnitStatus) {
				CFRetain(cfnRAIDSubUnitStatus);
				CFNumberGetValue(cfnRAIDSubUnitStatus, kCFNumberSInt32Type, &parameter->RAIDSubUnitStatus[count]);
			}
			
			if (cfnRAIDSubUnitStatus)	CFRelease(cfnRAIDSubUnitStatus);
			if (cfsKey)		CFRelease(cfsKey);
			
			// Type of Sub Unit Device.
			cfsKey = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR(NDASDEVICE_PROPERTY_KEY_RAID_TYPE_OF_SUB_UNIT), count);
			if(CFDictionaryGetValueIfPresent(cfdElement, cfsKey, (const void**)&cfnRAIDSubUnitType)
			   && cfnRAIDSubUnitType) {
				CFRetain(cfnRAIDSubUnitType);
				CFNumberGetValue(cfnRAIDSubUnitType, kCFNumberSInt32Type, &parameter->RAIDSubUnitType[count]);
			} else {
				parameter->RAIDSubUnitType[count] = kNDASRAIDSubUnitTypeNormal;
			}
			
			if (cfnRAIDSubUnitStatus)	CFRelease(cfnRAIDSubUnitType);
			if (cfsKey)		CFRelease(cfsKey);
			
		}
		
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_NAME), (const void**)&cfdName)
		   && cfdName) {
			CFRetain(cfdName);
			memcpy(parameter->name, CFDataGetBytePtr(cfdName), MAX_NDAS_NAME_SIZE);
		}
		
		result = true;
		
	}

cleanup:

	IOObjectRelease(devices); devices = 0;

	// cleanup
	if(cfsKey)						CFRelease(cfsKey);

	if(cfnIndex)					CFRelease(cfnIndex);
	if(cfnUnitDiskLocation)			CFRelease(cfnUnitDiskLocation);
	if(cfbWritable)					CFRelease(cfbWritable);
	if(cfnStatus)					CFRelease(cfnStatus);
	if(cfnConfiguration)			CFRelease(cfnConfiguration);
	if(cfnUnitDeviceType)			CFRelease(cfnUnitDeviceType);
	if(cfnPhysicalDeviceSlotNumber)	CFRelease(cfnPhysicalDeviceSlotNumber);
	if(cfnPhysicalDeviceUnitNumber)	CFRelease(cfnPhysicalDeviceUnitNumber);
	if(cfnCountOfUnitDevices)		CFRelease(cfnCountOfUnitDevices);
	if(cfnCountOfSpareDevices)		CFRelease(cfnCountOfSpareDevices);
	if(cfnUnitCapacity)				CFRelease(cfnUnitCapacity);
	if(cfdName)						CFRelease(cfdName);
	if(cfnRWHost)					CFRelease(cfnRWHost);
	if(cfnROHost)					CFRelease(cfnROHost);
	if(cfnRAIDStatus)				CFRelease(cfnRAIDStatus);
	if(cfnRecoveringSector)			CFRelease(cfnRecoveringSector);
	
	if(cfdElement)					CFRelease(cfdElement);
	
	return result;
}

bool NDASGetRAIDDeviceInformationByUnitDiskLocation(PNDASRAIDDeviceInformation parameter) 
{

	kern_return_t			kernResult;
	io_connect_t			dataPort;
	io_service_t			serviceObject;
    io_iterator_t			iterator;
    CFDictionaryRef			classToMatch;
	io_iterator_t			devices = 0;
	io_registry_entry_t		deviceUpNext = 0;
	io_registry_entry_t		device = 0;

	bool					result = false;
	CFMutableDictionaryRef	properties = NULL;
	io_name_t				name;
	CFNumberRef				cfnValue = NULL;
	UInt64					unitDiskLocation;
	
	CFMutableDictionaryRef	cfdElement = NULL;
	CFStringRef				cfsKey = NULL;
	
	CFNumberRef		cfnIndex = NULL;
	CFDataRef		cfdDeviceID = NULL;
	CFNumberRef		cfnUnitDiskLocation = NULL;
	CFNumberRef		cfnDiskArrayType = NULL;
	CFBooleanRef	cfbWritable = NULL;
	CFNumberRef		cfnStatus = NULL;
	CFNumberRef		cfnConfiguration = NULL;
	CFNumberRef		cfnUnitDeviceType = NULL;
	CFNumberRef		cfnPhysicalDeviceSlotNumber = NULL;
	CFNumberRef		cfnPhysicalDeviceUnitNumber = NULL;
	CFNumberRef		cfnCountOfUnitDevices = NULL;
	CFNumberRef		cfnCountOfSpareDevices = NULL;
	CFNumberRef		cfnUnitCapacity = NULL;
	CFNumberRef		cfnRWHost = NULL;
	CFNumberRef		cfnROHost = NULL;
	CFDataRef		cfdName = NULL;
	CFNumberRef		cfnRAIDStatus = NULL;
	CFNumberRef		cfnRecoveringSector = NULL;
	
	bool			found;
	int				count;
	
	// Obtain the I/O Kit communication handle.
	kernResult = IOMasterPort(MACH_PORT_NULL, &dataPort);
	if(kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "can't obtain I/O Kit's master port. %d\n", kernResult);
		
		result = false;
		
		goto cleanup;
	}
	
	// Obtain the NDASBusEnumerator service.
	classToMatch = IOServiceMatching(DriversIOKitClassName);
	if(classToMatch == NULL) {
		syslog(LOG_ERR, "IOServiceMatching returned a NULL dictionary.");
		
		result = false;
		
		goto cleanup;
	}
	
    kernResult = IOServiceGetMatchingServices(dataPort, classToMatch, &iterator);
	if(kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "IOServiceGetMatchingServices returned. %d\n", kernResult);
		
		result = false;
		
		goto cleanup;
	}
	
    serviceObject = IOIteratorNext(iterator);
    IOObjectRelease(iterator);
    if (!serviceObject)
    {
		syslog(LOG_ERR, "Couldn't find any matches.");
		
		result = false;
		
		goto cleanup;
    }
	
	kernResult = IORegistryEntryGetChildIterator(serviceObject, kIOServicePlane, &devices);
	if(kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "can't obtain devices. %d\n", kernResult);
		
		result = false;
		
		goto cleanup;
	}
	
	deviceUpNext = IOIteratorNext(devices);
		
	found = false;
	// Traverse over the children of this service.
	while(deviceUpNext && found == false) {
				
		device = deviceUpNext;
		deviceUpNext = IOIteratorNext(devices);
				
		IORegistryEntryGetNameInPlane(device, kIOServicePlane, name);
		if ( 0 != strcmp(name, "com_ximeta_driver_NDASRAIDUnitDevice")) {
			//syslog(LOG_ERR, "Not NDAS Device %s\n", name, deviceUpNext);
			
			IOObjectRelease(device);

			continue;
		}
		
		
		// Get Properties.
		kernResult = IORegistryEntryCreateCFProperties(
													   device,
													   &properties,
													   kCFAllocatorDefault,
													   kNilOptions
													   );		
		
		if (KERN_SUCCESS == kernResult
			&& properties) {
			
			// Unit Disk Location is Key.
			if (CFDictionaryGetValueIfPresent(properties, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_UNIT_DISK_LOCATION), (const void**)&cfnValue)) { 
				
				CFRetain(cfnValue);
				
				CFNumberGetValue (cfnValue, kCFNumberSInt64Type, &unitDiskLocation);
				
				// Compare Slotnumber
				if(unitDiskLocation == parameter->unitDiskLocation) {
					cfdElement = properties;
					CFRetain(cfdElement);
					
					found = true;				
				}
				
				CFRelease(cfnValue);			
			}
			
			CFRelease(properties);
		}
		
		IOObjectRelease(device);
		
	}
		
	if(cfdElement != NULL) {

		// Device ID.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_DEVICE_ID), (const void**)&cfdDeviceID)
		   && cfdDeviceID) {
			CFRetain(cfdDeviceID);
			
			memcpy(&parameter->deviceID, CFDataGetBytePtr(cfdDeviceID), sizeof(GUID));
		}

		// Index
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_INDEX), (const void**)&cfnIndex)
		   && cfnIndex) {
			CFRetain(cfnIndex);
			CFNumberGetValue(cfnIndex, kCFNumberSInt32Type, &parameter->index);
		}
		
		// Disk Array Type.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_DISK_ARRAY_TYPE), (const void**)&cfnDiskArrayType)
		   && cfnDiskArrayType) {
			CFRetain(cfnDiskArrayType);
			CFNumberGetValue(cfnDiskArrayType, kCFNumberSInt32Type, &parameter->unitDeviceType);
		}
		
		// Writable.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_WRITE_RIGHT), (const void**)&cfbWritable)
		   && cfbWritable) {
			CFRetain(cfbWritable);
			parameter->writable = CFBooleanGetValue(cfbWritable);
		}
		
		// Status.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_STATUS), (const void**)&cfnStatus)
		   && cfnStatus) {
			CFRetain(cfnStatus);
			CFNumberGetValue(cfnStatus, kCFNumberSInt32Type, &parameter->status);
		}
		
		// Configuration.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_CONFIGURATION), (const void**)&cfnConfiguration)
		   && cfnConfiguration) {
			CFRetain(cfnConfiguration);
			CFNumberGetValue(cfnConfiguration, kCFNumberSInt32Type, &parameter->configuration);
		}

		// Unit Device Type.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_DEVICE_TYPE), (const void**)&cfnUnitDeviceType)
		   && cfnUnitDeviceType) {
			CFRetain(cfnUnitDeviceType);
			CFNumberGetValue(cfnUnitDeviceType, kCFNumberSInt32Type, &parameter->unitDeviceType);
		}

		// Disk array Type.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_DISK_ARRAY_TYPE), (const void**)&cfnDiskArrayType)
		   && cfnDiskArrayType) {
			CFRetain(cfnDiskArrayType);
			CFNumberGetValue(cfnDiskArrayType, kCFNumberSInt32Type, &parameter->diskArrayType);
		}
		
		// Count of Sub Unit Devices.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_RAID_DEVICE_COUNTOFUNITS), (const void**)&cfnCountOfUnitDevices)
		   && cfnCountOfUnitDevices) {
			CFRetain(cfnCountOfUnitDevices);
			CFNumberGetValue(cfnCountOfUnitDevices, kCFNumberSInt32Type, &parameter->CountOfUnitDevices);
		}
		
		// Count of Spare Devices.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_RAID_DEVICE_COUNTOFSPARES), (const void**)&cfnCountOfSpareDevices)
		   && cfnCountOfSpareDevices) {
			CFRetain(cfnCountOfSpareDevices);
			CFNumberGetValue(cfnCountOfSpareDevices, kCFNumberSInt32Type, &parameter->CountOfSpareDevices);
		} else {
			parameter->CountOfSpareDevices = 0;
		}

		// Number of RW Hosts.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_NR_RW_HOST), (const void**)&cfnRWHost)
		   && cfnRWHost) {
			CFRetain(cfnRWHost);
			CFNumberGetValue(cfnRWHost, kCFNumberSInt32Type, &parameter->RWHosts);
		} else {
			parameter->RWHosts = 0;
		}
		
		// Number of RO Hosts.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_NR_RO_HOST), (const void**)&cfnROHost)
		   && cfnROHost) {
			CFRetain(cfnROHost);
			CFNumberGetValue(cfnROHost, kCFNumberSInt32Type, &parameter->ROHosts);
		} else {
			parameter->ROHosts = 0;
		}
		
		// Capacity.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_UNIT_CAPACITY), (const void**)&cfnUnitCapacity)
		   && cfnUnitCapacity) {
			CFRetain(cfnUnitCapacity);
			CFNumberGetValue(cfnUnitCapacity, kCFNumberSInt64Type, &parameter->capacity);
			parameter->capacity *= DISK_BLOCK_SIZE;
		} else {			
			parameter->capacity = 0;
		}
		
		// RAID Status.
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_RAID_STATUS), (const void**)&cfnRAIDStatus)
		   && cfnRAIDStatus) {
			CFRetain(cfnRAIDStatus);
			CFNumberGetValue(cfnRAIDStatus, kCFNumberSInt32Type, &parameter->RAIDStatus);
		} else {
			parameter->RAIDStatus = kNDASRAIDStatusNotReady;
		}				
		
		// Recovering Sector
		if(NMT_RAID1R3 == parameter->unitDeviceType
		   && kNDASRAIDStatusRecovering == parameter->RAIDStatus
		   && CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_RAID_RECOVERING_SECTOR), (const void**)&cfnRecoveringSector)
		   && cfnRecoveringSector) {
			CFRetain(cfnRecoveringSector);
			CFNumberGetValue(cfnRecoveringSector, kCFNumberSInt64Type, &parameter->RecoveringSector);
		} else {
			parameter->RecoveringSector = 0;
		}
		
		if (parameter->CountOfUnitDevices > NDAS_MAX_UNITS_IN_V2) {
			parameter->CountOfUnitDevices = NDAS_MAX_UNITS_IN_V2;
		}
		
		for (count = 0; count < parameter->CountOfUnitDevices + parameter->CountOfSpareDevices; count++ ) {
			
			CFStringRef		cfsKey = NULL;
			CFNumberRef		cfnUnitID = NULL;
			CFNumberRef		cfnRAIDSubUnitStatus = NULL;
			CFNumberRef		cfnRAIDSubUnitType = NULL;
			
			// Sub Unit Device ID.
			cfsKey = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR(NDASDEVICE_PROPERTY_KEY_RAID_UNIT_DISK_LOCATION_OF_SUB_UNIT), count);			
			if(CFDictionaryGetValueIfPresent(cfdElement, cfsKey, (const void**)&cfnUnitID)
			   && cfnUnitID) {
				CFRetain(cfnUnitID);
				CFNumberGetValue(cfnUnitID, kCFNumberSInt64Type, &parameter->unitDiskLocationOfSubUnits[count]);
			}			
			
			if (cfnUnitID)	CFRelease(cfnUnitID);
			if (cfsKey)		CFRelease(cfsKey);
			
			// Status of Sub Unit Device.
			cfsKey = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR(NDASDEVICE_PROPERTY_KEY_RAID_STATUS_OF_SUB_UNIT), count);
			if(CFDictionaryGetValueIfPresent(cfdElement, cfsKey, (const void**)&cfnRAIDSubUnitStatus)
			   && cfnRAIDSubUnitStatus) {
				CFRetain(cfnRAIDSubUnitStatus);
				CFNumberGetValue(cfnRAIDSubUnitStatus, kCFNumberSInt32Type, &parameter->RAIDSubUnitStatus[count]);
			}
			
			if (cfnRAIDSubUnitStatus)	CFRelease(cfnRAIDSubUnitStatus);
			if (cfsKey)		CFRelease(cfsKey);
			
			// Type of Sub Unit Device.
			cfsKey = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR(NDASDEVICE_PROPERTY_KEY_RAID_TYPE_OF_SUB_UNIT), count);
			if(CFDictionaryGetValueIfPresent(cfdElement, cfsKey, (const void**)&cfnRAIDSubUnitType)
			   && cfnRAIDSubUnitType) {
				CFRetain(cfnRAIDSubUnitType);
				CFNumberGetValue(cfnRAIDSubUnitType, kCFNumberSInt32Type, &parameter->RAIDSubUnitType[count]);
			} else {
				parameter->RAIDSubUnitType[count] = kNDASRAIDSubUnitTypeNormal;
			}
			
			if (cfnRAIDSubUnitStatus)	CFRelease(cfnRAIDSubUnitType);
			if (cfsKey)		CFRelease(cfsKey);
			
		}
		
		if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_NAME), (const void**)&cfdName)
		   && cfdName) {
			CFRetain(cfdName);
			memcpy(parameter->name, CFDataGetBytePtr(cfdName), MAX_NDAS_NAME_SIZE);
		}
		
/*
		switch ( parameter->unitDeviceType) {
			case kNDASUnitDeviceTypePhysicalHDD:
			case kNDASUnitDeviceTypePhysicalODD:
			{
				// Slot Number.
				if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_PHYSICAL_DEVICE_SLOT_NUMBER), (const void**)&cfnPhysicalDeviceSlotNumber)
				   && cfnPhysicalDeviceSlotNumber) {
					CFRetain(cfnPhysicalDeviceSlotNumber);
					CFNumberGetValue(cfnPhysicalDeviceSlotNumber, kCFNumberSInt32Type, &parameter->physicalUnitSlotNumber);
				}
				
				// Unit Number.
				if(CFDictionaryGetValueIfPresent(cfdElement, CFSTR(NDASDEVICE_PROPERTY_KEY_PHYSICAL_DEVICE_UNIT_NUMBER), (const void**)&cfnPhysicalDeviceUnitNumber)
				   && cfnPhysicalDeviceUnitNumber) {
					CFRetain(cfnPhysicalDeviceUnitNumber);
					CFNumberGetValue(cfnPhysicalDeviceUnitNumber, kCFNumberSInt32Type, &parameter->physicalUnitUnitNumber);
				}
				
			}
				break;
			default:
				break;
		}
*/		
		result = true;
		
		}

cleanup:

	IOObjectRelease(devices); devices = 0;

	// cleanup
	if(cfsKey)						CFRelease(cfsKey);

	if(cfnIndex)					CFRelease(cfnIndex);
	if(cfdDeviceID)					CFRelease(cfdDeviceID);
	if(cfnUnitDiskLocation)			CFRelease(cfnUnitDiskLocation);
	if(cfbWritable)					CFRelease(cfbWritable);
	if(cfnStatus)					CFRelease(cfnStatus);
	if(cfnConfiguration)			CFRelease(cfnConfiguration);
	if(cfnUnitDeviceType)			CFRelease(cfnUnitDeviceType);
	if(cfnPhysicalDeviceSlotNumber)	CFRelease(cfnPhysicalDeviceSlotNumber);
	if(cfnPhysicalDeviceUnitNumber)	CFRelease(cfnPhysicalDeviceUnitNumber);
	if(cfnCountOfUnitDevices)		CFRelease(cfnCountOfUnitDevices);
	if(cfnCountOfSpareDevices)		CFRelease(cfnCountOfSpareDevices);
	if(cfnUnitCapacity)				CFRelease(cfnUnitCapacity);
	if(cfdName)						CFRelease(cfdName);
	if(cfnRWHost)					CFRelease(cfnRWHost);
	if(cfnROHost)					CFRelease(cfnROHost);
	if(cfnRAIDStatus)				CFRelease(cfnRAIDStatus);
	if(cfnRecoveringSector)			CFRelease(cfnRecoveringSector);
	
	if(cfdElement)					CFRelease(cfdElement);
	
	return result;
}

CFMutableArrayRef NDASGetInformationGetIndexArray(const char *deviceClassName) 
{	
	kern_return_t			kernResult;
	io_connect_t			dataPort;
	io_service_t			serviceObject;
    io_iterator_t			iterator;
    CFDictionaryRef			classToMatch;
	io_iterator_t			devices = 0;
	io_registry_entry_t		deviceUpNext = 0;
	io_registry_entry_t		device = 0;
	io_name_t				name;
	int						numberOfDevices;
	CFMutableDictionaryRef	properties = NULL;
	CFNumberRef				cfnValue = NULL;
	CFMutableArrayRef		array = NULL;
	
	// Obtain the I/O Kit communication handle.
	kernResult = IOMasterPort(MACH_PORT_NULL, &dataPort);
	if(kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "can't obtain I/O Kit's master port. %d\n", kernResult);
				
		goto cleanup;
	}
	
	// Obtain the NDASBusEnumerator service.
	classToMatch = IOServiceMatching(DriversIOKitClassName);
	if(classToMatch == NULL) {
		syslog(LOG_ERR, "IOServiceMatching returned a NULL dictionary.");
		
		goto cleanup;
	}
	
    kernResult = IOServiceGetMatchingServices(dataPort, classToMatch, &iterator);
	if(kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "IOServiceGetMatchingServices returned. %d\n", kernResult);
		
		goto cleanup;
	}
	
    serviceObject = IOIteratorNext(iterator);
    IOObjectRelease(iterator);
    if (!serviceObject)
    {
		syslog(LOG_ERR, "Couldn't find any matches.");
		
		goto cleanup;
    }
	
	kernResult = IORegistryEntryGetChildIterator(serviceObject, kIOServicePlane, &devices);
	if(kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "can't obtain devices. %d\n", kernResult);
		
		goto cleanup;
	}
	
	deviceUpNext = IOIteratorNext(devices);
	
	numberOfDevices = 0;
	
	// Traverse over the children of this service.
	while(deviceUpNext) {
		
		device = deviceUpNext;
		deviceUpNext = IOIteratorNext(devices);
		
		IORegistryEntryGetNameInPlane(device, kIOServicePlane, name);
		if ( 0 != strcmp(name, deviceClassName)) {
			
			IOObjectRelease(device);
			
			continue;
		}
		
		numberOfDevices++;
		
		IOObjectRelease(device);
		
	}
	
	if (numberOfDevices > 0) {
		// create Array.
		array = CFArrayCreateMutable(kCFAllocatorDefault, numberOfDevices, &kCFTypeArrayCallBacks);
		if (array == NULL) {
			IOObjectRelease(devices); devices = 0;
			
			goto cleanup;
		}

		IOIteratorReset(devices);
		
		deviceUpNext = IOIteratorNext(devices);
				
		// Traverse over the children of this service.
		while(deviceUpNext) {
			
			device = deviceUpNext;
			deviceUpNext = IOIteratorNext(devices);
			
			IORegistryEntryGetNameInPlane(device, kIOServicePlane, name);
			if ( 0 != strcmp(name, deviceClassName)) {
				
				IOObjectRelease(device);
				
				continue;
			}
			
			// Get Properties.
			kernResult = IORegistryEntryCreateCFProperties(
														   device,
														   &properties,
														   kCFAllocatorDefault,
														   kNilOptions
														   );		
			
			if (KERN_SUCCESS == kernResult
				&& properties) { 
				// SlotNumber is Key.
				if (CFDictionaryGetValueIfPresent(properties, CFSTR(NDASDEVICE_PROPERTY_KEY_SLOT_NUMBER), (const void**)&cfnValue)) { 
					//CFNumberGetValue (cfnValue, kCFNumberSInt32Type, &array[arrayIndex++]);
					CFArrayAppendValue(array, cfnValue);
				} else if (CFDictionaryGetValueIfPresent(properties, CFSTR(NDASDEVICE_PROPERTY_KEY_INDEX), (const void**)&cfnValue)) { 
					//CFNumberGetValue (cfnValue, kCFNumberSInt32Type, &array[arrayIndex++]);
					CFArrayAppendValue(array, cfnValue);
				}
				
				CFRelease(properties);
			}
			
			IOObjectRelease(device);
			
		}
		
		CFArraySortValues(array, CFRangeMake(0, CFArrayGetCount(array)), (CFComparatorFunction)CFNumberCompare, NULL);
	}
	
	IOObjectRelease(devices); devices = 0;
	
	return array;
	
cleanup:
	
	return NULL;	
}
										 