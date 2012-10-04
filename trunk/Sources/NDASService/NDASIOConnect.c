#include <CoreServices/CoreServices.h>
#include <IOKit/IOKitLib.h>
#include <syslog.h>

#include "NDASIOConnect.h"

#include "NDASBusEnumeratorUserClientTypes.h"
#include "NDASIDTools.h"

#include "NDASID.h"
#include "hash.h"

int OpenDataPort(char *DriverClassName, io_connect_t *DataPort)
{
    kern_return_t	kernResult;
    mach_port_t		masterPort;
    io_service_t	serviceObject;
    io_connect_t	dataPort;
    io_iterator_t 	iterator;
    CFDictionaryRef	classToMatch;
	
	
    kernResult = IOMasterPort(MACH_PORT_NULL, &masterPort);
	
    if (kernResult != KERN_SUCCESS)
    {
        syslog(LOG_ERR, "OpenDataPort: IOMasterPort Failed. %d\n", kernResult);
        return kernResult;
    }
	
    classToMatch = IOServiceMatching(DriverClassName);
	
    if (classToMatch == NULL)
    {
        syslog(LOG_ERR, "OpenDataPort: IOServiceMatching returned a NULL dictionary.\n");
        return -1;
    }
	
    kernResult = IOServiceGetMatchingServices(masterPort, classToMatch, &iterator);
	
    if (kernResult != KERN_SUCCESS)
    {
        syslog(LOG_ERR, "OpenDataPort: IOServiceGetMatchingServices Failed %d\n", kernResult);
        return kernResult;
    }
	
    serviceObject = IOIteratorNext(iterator);
    IOObjectRelease(iterator);
    if (!serviceObject)
    {
        syslog(LOG_ERR, "OpenDataPort: Couldn't find any matches.\n");
        return kernResult;
    }
	
    kernResult = IOServiceOpen(serviceObject, mach_task_self(), 0, &dataPort);
    IOObjectRelease(serviceObject);
    if (kernResult != KERN_SUCCESS)
    {
        syslog(LOG_ERR, "OpenDataPort: IOServiceOpen Failed %d\n", kernResult);
        return kernResult;
    }
	
    *DataPort = dataPort;
	
    return 0;
}


void CloseDataPort(io_connect_t DataPort)
{
	
    kern_return_t	kernResult;
	
    kernResult = IOServiceClose(DataPort);
	
    if (kernResult != KERN_SUCCESS)
    {
        syslog(LOG_ERR, "IOServiceClose Failed %d\n\n", kernResult);
    }
	
    return;
}

kern_return_t NDASIOConnectRegister(PNDASPreferencesParameter	parameter, bool *bDuplicated)
{
	u_char						NDASAddress[6];
	bool						bWriteKey;
	NDAS_ENUM_REGISTER			registerData;
	NDAS_ENUM_REGISTER_RESULT	resultData;
	kern_return_t				kernResult;
	IOByteCount					resultSize = sizeof(NDAS_ENUM_REGISTER_RESULT);
	io_connect_t				dataPort;
	UInt32						result;
	u_int						uiPort;
	uint8_t						ui8vid;
	
	// Check ID.
	if(!checkSNValid(parameter->chID[0], parameter->chID[1], parameter->chID[2], parameter->chID[3], parameter->chWriteKey, (char *)NDASAddress, &bWriteKey, &ui8vid)) {
		syslog(LOG_ALERT, "NDASIOConnectRegister: Invalid ID.\n");
		
		return KERN_INVALID_ARGUMENT;
	}
	
	// make parametor.
	uiPort = LPX_PORT_CONNECTION;
	
	registerData.SlotNo = parameter->slotNumber;
	registerData.DevAddr.slpx_family = AF_LPX;
	registerData.DevAddr.slpx_len = sizeof(struct sockaddr_lpx);
	memcpy(registerData.DevAddr.slpx_node, NDASAddress, 6);
	registerData.DevAddr.slpx_port = htons(uiPort);
	registerData.hasWriteRight = bWriteKey;
	registerData.Configuration = kNDASConfigurationDisable;
	registerData.VID = ui8vid;
	
	if (NDAS_ID_EXTENSION_DEFAULT.VID == registerData.VID) {
		memcpy(registerData.SuperPassword, &NDAS_OEM_CODE_DEFAULT_SUPER, 8);
		memcpy(registerData.UserPassword, &NDAS_OEM_CODE_DEFAULT, 8);	
	} else if (NDAS_ID_EXTENSION_SEAGATE.VID == registerData.VID) {
		memcpy(registerData.SuperPassword, &NDAS_OEM_CODE_DEFAULT_SUPER, 8);
		memcpy(registerData.UserPassword, &NDAS_OEM_CODE_SEAGATE, 8);	
	}
			
	CFStringGetCString(parameter->cfsName, registerData.Name , MAX_NDAS_NAME_SIZE, kCFStringEncodingUTF8);
//	memcpy(registerData.Name, parameter->Name, MAX_NDAS_NAME_SIZE);
	memcpy(registerData.DeviceIDString, parameter->DeviceIdString, MAX_NDAS_NAME_SIZE);
	
	// Send Command.
	kernResult = OpenDataPort(DriversIOKitClassName, &dataPort);
	if(kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "NDASIOConnectRegister: OpenDataPort Error %d\n", kernResult);
		return kernResult;
	}
	   
	kernResult = IOConnectMethodScalarIScalarO(dataPort, kNDASBusEnumeratorUserClientOpen, 0, 0);
	if (kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "NDASIOConnectRegister: Client Open Failed! %d\n", kernResult);
			
		CloseDataPort(dataPort);
		return kernResult;
	}
		
	kernResult = IOConnectMethodStructureIStructureO(dataPort, kNDASBusEnumeratorRegister,  sizeof(registerData), &resultSize, &registerData, &resultData);
	if (kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "NDASIOConnectRegister: Register Failed 0x%x slot %d\n", kernResult, resultData.SlotNo);		
	} else {
		parameter->slotNumber = resultData.SlotNo;
		*bDuplicated = resultData.bDuplicated;

		if (*bDuplicated) {
			syslog(LOG_INFO, "NDASIOConnectRegister: Already Registered Device at slot %d\n", resultData.SlotNo);		
		} else {
			syslog(LOG_INFO, "NDASIOConnectRegister: The New NDAS Device was registered at slot %d\n", resultData.SlotNo);		
		}
	}	

	IOConnectMethodScalarIScalarO(dataPort, kNDASBusEnumeratorUserClientClose, 0, 0);
	   
	CloseDataPort(dataPort);
	 	
	return kernResult;
}

kern_return_t NDASIOConnectUnregister(UInt32 slotNumber)
{
	NDAS_ENUM_UNREGISTER 	unregisterData;
	kern_return_t			kernResult;
	IOByteCount				resultSize = sizeof(int);
	io_connect_t			dataPort;
	UInt32					result;
	
	unregisterData.SlotNo = slotNumber;
	
	// Send Command.
	kernResult = OpenDataPort(DriversIOKitClassName, &dataPort);
	if(kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "NDASIOConnectUnregister: OpenDataPort Error %d\n", kernResult);

		return kernResult;
	}
	   
	kernResult = IOConnectMethodScalarIScalarO(dataPort, kNDASBusEnumeratorUserClientOpen, 0, 0);
	if (kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "NDASIOConnectUnregister: Client Open Failed! %d\n", kernResult);
		
		CloseDataPort(dataPort);		
		return kernResult;
	}
	
	kernResult = IOConnectMethodStructureIStructureO(dataPort, kNDASBusEnumeratorUnregister,  sizeof(unregisterData), &resultSize, &unregisterData, &result);
	if (kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "NDASIOConnectUnregister: Unregister Failed %d\n", kernResult);
	} else {
		syslog(LOG_INFO, "NDASIOConnectUnregister: The NDAS at slot %d Unregistered.\n", result);
		kernResult = KERN_SUCCESS;
	}
	   
	IOConnectMethodScalarIScalarO(dataPort, kNDASBusEnumeratorUserClientClose, 0, 0);
	   
	CloseDataPort(dataPort);
	
	return kernResult;
}

kern_return_t NDASIOConnectEnable(UInt32 slotNumber)
{
	NDAS_ENUM_ENABLE 	enableData;
	kern_return_t		kernResult;
	IOByteCount			resultSize = sizeof(int);
	io_connect_t		dataPort;
	UInt32				result;
	
	enableData.SlotNo = slotNumber;
	
	// Send Command.
	kernResult = OpenDataPort(DriversIOKitClassName, &dataPort);
	if(kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "NDASIOConnectEnable: OpenDataPort Error %d\n", kernResult);
		
		return kernResult;
	}
	   
	kernResult = IOConnectMethodScalarIScalarO(dataPort, kNDASBusEnumeratorUserClientOpen, 0, 0);
	if (kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "NDASIOConnectEnable: Client Open Failed! %d\n", kernResult);
		
		CloseDataPort(dataPort);		
		return kernResult;
	}
	
	kernResult = IOConnectMethodStructureIStructureO(dataPort, kNDASBusEnumeratorEnable,  sizeof(enableData), &resultSize, &enableData, &result);
	if (kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "NDASIOConnectEnable: Enable Failed %d\n", kernResult);
	} else {
		syslog(LOG_INFO, "NDASIOConnectEnable: The NDAS at slot %d set to enable.\n", result);
		kernResult = KERN_SUCCESS;
	}
	   
	IOConnectMethodScalarIScalarO(dataPort, kNDASBusEnumeratorUserClientClose, 0, 0);
	   
	CloseDataPort(dataPort);
	
	return kernResult;
}

kern_return_t NDASIOConnectDisable(UInt32 slotNumber)
{
	NDAS_ENUM_DISABLE 	disableData;
	kern_return_t		kernResult;
	IOByteCount			resultSize = sizeof(int);
	io_connect_t		dataPort;
	UInt32				result;
	
	disableData.SlotNo = slotNumber;
	
	// Send Command.
	kernResult = OpenDataPort(DriversIOKitClassName, &dataPort);
	if(kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "NDASIOConnectDisable: OpenDataPort Error %d\n", kernResult);
		
		return kernResult;
	}
	   
	kernResult = IOConnectMethodScalarIScalarO(dataPort, kNDASBusEnumeratorUserClientOpen, 0, 0);
	if (kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "NDASIOConnectDisable: Client Open Failed! %d\n", kernResult);
		
		CloseDataPort(dataPort);		
		return kernResult;
	}
	
	kernResult = IOConnectMethodStructureIStructureO(dataPort, kNDASBusEnumeratorDisable,  sizeof(disableData), &resultSize, &disableData, &result);
	if (kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "NDASIOConnectDisable: Disable Failed %d\n", kernResult);
	} else {
		syslog(LOG_INFO, "NDASIOConnectDisable: The NDAS at slot %d set to disable.\n", result);
		kernResult = KERN_SUCCESS;
	}
	   
	IOConnectMethodScalarIScalarO(dataPort, kNDASBusEnumeratorUserClientClose, 0, 0);
	   
	CloseDataPort(dataPort);
	
	return kernResult;
}

kern_return_t NDASIOConnectMount(UInt32 index, bool writable)
{
	NDAS_ENUM_MOUNT 	mountData;
	kern_return_t		kernResult;
	IOByteCount			resultSize = sizeof(int);
	io_connect_t		dataPort;
	UInt32				result;
	
	mountData.LogicalDeviceIndex = index;
	mountData.Writeable = writable;
			
	// Send Command.
	kernResult = OpenDataPort(DriversIOKitClassName, &dataPort);
	if(kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "NDASIOConnectMount: OpenDataPort Error %d\n", kernResult);
		
		return kernResult;
	}
	   
	kernResult = IOConnectMethodScalarIScalarO(dataPort, kNDASBusEnumeratorUserClientOpen, 0, 0);
	if (kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "NDASIOConnectMount: Client Open Failed! %d\n", kernResult);
		
		CloseDataPort(dataPort);		
		return kernResult;
	}
	
	kernResult = IOConnectMethodStructureIStructureO(dataPort, kNDASBusEnumeratorMount,  sizeof(mountData), &resultSize, &mountData, &result);
	if (kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "NDASIOConnectMount: Mount Failed %d\n", kernResult);
	} else {
		if(writable)
			syslog(LOG_INFO, "NDASIOConnectMount: The NDAS at slot %d set to mount with read/write mode", result);
		else
			syslog(LOG_INFO, "NDASIOConnectMount: The NDAS at slot %d set to mount with read only mode", result);
	}
	   
	IOConnectMethodScalarIScalarO(dataPort, kNDASBusEnumeratorUserClientClose, 0, 0);
	   
	CloseDataPort(dataPort);
	
	return kernResult;
}

kern_return_t NDASIOConnectUnmount(UInt32 index)
{
	NDAS_ENUM_UNMOUNT 	unmountData;
	kern_return_t		kernResult;
	IOByteCount			resultSize = sizeof(int);
	io_connect_t		dataPort;
	UInt32				result;
	
	unmountData.LogicalDeviceIndex = index;
		
	// Send Command.
	kernResult = OpenDataPort(DriversIOKitClassName, &dataPort);
	if(kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "NDASIOConnectUnmount: OpenDataPort Error %d\n", kernResult);
		
		return kernResult;
	}
	   
	kernResult = IOConnectMethodScalarIScalarO(dataPort, kNDASBusEnumeratorUserClientOpen, 0, 0);
	if (kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "NDASIOConnectUnmount: Client Open Failed! %d\n", kernResult);
		
		CloseDataPort(dataPort);		
		return kernResult;
	}
	
	kernResult = IOConnectMethodStructureIStructureO(dataPort, kNDASBusEnumeratorUnmount, sizeof(unmountData), &resultSize, &unmountData, &result);
	if (kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "NDASIOConnectUnmount: Unmount Failed %d\n", kernResult);
	} else {
		syslog(LOG_INFO, "NDASIOConnectUnmount: The NDAS at slot %d set to unmount.\n", result);
	}
	   
	IOConnectMethodScalarIScalarO(dataPort, kNDASBusEnumeratorUserClientClose, 0, 0);
	   
	CloseDataPort(dataPort);
	
	return kernResult;
}

kern_return_t NDASIOConnectEdit(
								UInt32						editType,
								PNDASPreferencesParameter	parameter
								)
{
	u_char				NDASAddress[6];
	bool				bWriteKey;
	NDAS_ENUM_EDIT		editData;
	kern_return_t		kernResult;
	IOByteCount			resultSize = sizeof(int);
	io_connect_t		dataPort;
	UInt32				result;
	u_int				uiPort;
	u_int64_t			ui64temp;
	uint8_t				ui8vid;

	// Make parameter.
	editData.SlotNo = parameter->slotNumber;
	editData.EditType = editType;
	
	switch ( editType ) {
		case kNDASEditTypeWriteAccessRight:
		{
			// Check ID.
			if(!checkSNValid(parameter->chID[0], parameter->chID[1], parameter->chID[2], parameter->chID[3], parameter->chWriteKey, (char *)NDASAddress, &editData.Writable, &ui8vid)) {
				//syslog(LOG_ALERT, "NDASIOConnectEdit: Invalid ID.\n");
			}
		}
			break;
		case kNDASEditTypeName:
		{
//			memcpy(editData.Name, parameter->Name, MAX_NDAS_NAME_SIZE);
			CFStringGetCString(parameter->cfsName, editData.Name , MAX_NDAS_NAME_SIZE, kCFStringEncodingUTF8);			
		}
			break;
		default:
			return KERN_INVALID_ARGUMENT;
	}

	// Send Command.
	kernResult = OpenDataPort(DriversIOKitClassName, &dataPort);
	if(kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "NDASIOConnectEdit: OpenDataPort Error %d\n", kernResult);
		return kernResult;
	}
	   
	kernResult = IOConnectMethodScalarIScalarO(dataPort, kNDASBusEnumeratorUserClientOpen, 0, 0);
	if (kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "NDASIOConnectEdit: Client Open Failed! %d\n", kernResult);
		
		CloseDataPort(dataPort);
		return kernResult;
	}
	
	kernResult = IOConnectMethodStructureIStructureO(dataPort, kNDASBusEnumeratorEdit,  sizeof(editData), &resultSize, &editData, &result);
	if (kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "NDASIOConnectEdit: Edit Failed %d\n", kernResult);
	} else {
		syslog(LOG_INFO, "NDASIOConnectEdit: Edit Success\n");
	}	
	   
	kernResult = IOConnectMethodScalarIScalarO(dataPort, kNDASBusEnumeratorUserClientClose, 0, 0);
	   
	CloseDataPort(dataPort);
		
	return kernResult;
}

kern_return_t NDASIOConnectRefresh(
								   UInt32 slotNumber, 
								   UInt32 unitNumber
								   )
{
	NDAS_ENUM_REFRESH 	refreshData;
	kern_return_t		kernResult;
	IOByteCount			resultSize = sizeof(int);
	io_connect_t		dataPort;
	UInt32				result;
	
	refreshData.SlotNo = slotNumber;
	refreshData.UnitNo = unitNumber;
	
	// Send Command.
	kernResult = OpenDataPort(DriversIOKitClassName, &dataPort);
	if(kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "NDASIOConnectRefresh: OpenDataPort Error %d\n", kernResult);
		
		return kernResult;
	}
	   
	kernResult = IOConnectMethodScalarIScalarO(dataPort, kNDASBusEnumeratorUserClientOpen, 0, 0);
	if (kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "NDASIOConnectRefresh: Client Open Failed! %d\n", kernResult);
		
		CloseDataPort(dataPort);		
		return kernResult;
	}
	
	kernResult = IOConnectMethodStructureIStructureO(dataPort, kNDASBusEnumeratorRefresh, sizeof(refreshData), &resultSize, &refreshData, &result);
	if (kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "NDASIOConnectRefresh: Refresh Failed %d\n", kernResult);
	} else {
		syslog(LOG_INFO, "NDASIOConnectRefresh: The NDAS at slot %d set to Refresh.\n", result);
	}
	   
	IOConnectMethodScalarIScalarO(dataPort, kNDASBusEnumeratorUserClientClose, 0, 0);
	   
	CloseDataPort(dataPort);
	
	return kernResult;
}

kern_return_t NDASIOConnectBind(UInt32 type, UInt32 NumberOfUnitDevices, UInt32 LogicalDeviceIndex[], PGUID raidID, bool sync)
{
	NDAS_ENUM_BIND		bindData;
	kern_return_t		kernResult;
	IOByteCount			resultSize = sizeof(GUID);
	io_connect_t		dataPort;
	int					count;
	
	bindData.RAIDType = type;
	bindData.NumberofSubUnitDevices = NumberOfUnitDevices;
	for (count = 0; count < NumberOfUnitDevices; count++) {
		bindData.LogicalDeviceIndexOfSubUnitDevices[count] = LogicalDeviceIndex[count];
	}
	bindData.Sync = sync;
	
	// Send Command.
	kernResult = OpenDataPort(DriversIOKitClassName, &dataPort);
	if(kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "NDASIOConnectBind: OpenDataPort Error %d\n", kernResult);
		
		return kernResult;
	}
	   
	kernResult = IOConnectMethodScalarIScalarO(dataPort, kNDASBusEnumeratorUserClientOpen, 0, 0);
	if (kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "NDASIOConnectBind: Client Open Failed! %d\n", kernResult);
		
		CloseDataPort(dataPort);		
		return kernResult;
	}
	
	kernResult = IOConnectMethodStructureIStructureO(dataPort, kNDASBusEnumeratorBind, sizeof(bindData), &resultSize, &bindData, raidID);
	if (kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "NDASIOConnectBind: Bind Failed 0x%x\n", kernResult);
	} else {
		syslog(LOG_INFO, "NDASIOConnectBind: The Binding success.\n");
	}
	   
	IOConnectMethodScalarIScalarO(dataPort, kNDASBusEnumeratorUserClientClose, 0, 0);
	   
	CloseDataPort(dataPort);
	
	return kernResult;
}

kern_return_t NDASIOConnectUnbind(UInt32 index)
{
	NDAS_ENUM_UNBIND 	unbindData;
	kern_return_t		kernResult;
	IOByteCount			resultSize = sizeof(int);
	io_connect_t		dataPort;
	UInt32				result;
	
	unbindData.RAIDDeviceIndex = index;
	
	// Send Command.
	kernResult = OpenDataPort(DriversIOKitClassName, &dataPort);
	if(kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "NDASIOConnectUnbind: OpenDataPort Error %d\n", kernResult);
		
		return kernResult;
	}
	   
	kernResult = IOConnectMethodScalarIScalarO(dataPort, kNDASBusEnumeratorUserClientOpen, 0, 0);
	if (kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "NDASIOConnectUnbind: Client Open Failed! %d\n", kernResult);
		
		CloseDataPort(dataPort);		
		return kernResult;
	}
	
	kernResult = IOConnectMethodStructureIStructureO(dataPort, kNDASBusEnumeratorUnbind, sizeof(unbindData), &resultSize, &unbindData, &result);
	if (kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "NDASIOConnectUnbind: Unbind Failed %d\n", kernResult);
	} else {
		syslog(LOG_INFO, "NDASIOConnectUnbind: The NDAS Logical Device at index %d was unbinded.\n", result);
	}
	   
	IOConnectMethodScalarIScalarO(dataPort, kNDASBusEnumeratorUserClientClose, 0, 0);
	   
	CloseDataPort(dataPort);
	
	return kernResult;
}

kern_return_t NDASIOConnectRebind(UInt32 index)
{
	NDAS_ENUM_REBIND 	rebindData;
	kern_return_t		kernResult;
	IOByteCount			resultSize = sizeof(int);
	io_connect_t		dataPort;
	UInt32				result;
	
	rebindData.RAIDDeviceIndex = index;
	
	// Send Command.
	kernResult = OpenDataPort(DriversIOKitClassName, &dataPort);
	if(kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "NDASIOConnectRebind: OpenDataPort Error %d\n", kernResult);
		
		return kernResult;
	}
	   
	kernResult = IOConnectMethodScalarIScalarO(dataPort, kNDASBusEnumeratorUserClientOpen, 0, 0);
	if (kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "NDASIOConnectRebind: Client Open Failed! %d\n", kernResult);
		
		CloseDataPort(dataPort);		
		return kernResult;
	}
	
	kernResult = IOConnectMethodStructureIStructureO(dataPort, kNDASBusEnumeratorRebind, sizeof(rebindData), &resultSize, &rebindData, &result);
	if (kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "NDASIOConnectRebind: Unbind Failed %d\n", kernResult);
	} else {
		syslog(LOG_INFO, "NDASIOConnectRebind: The NDAS Logical Device at index %d was unbinded.\n", result);
	}
	   
	IOConnectMethodScalarIScalarO(dataPort, kNDASBusEnumeratorUserClientClose, 0, 0);
	   
	CloseDataPort(dataPort);
	
	return kernResult;
}

kern_return_t NDASIOConnectBindEdit(
									UInt32						editType,
									UInt32						index,
									CFStringRef					cfsName,
									UInt32						raidType
									)
{
	NDAS_ENUM_BIND_EDIT		editData;
	kern_return_t			kernResult;
	IOByteCount				resultSize = sizeof(UInt32);
	io_connect_t			dataPort;
	UInt32					result;
	u_int					uiPort;
	u_int64_t				ui64temp;
		
	// Make parameter.
	editData.RAIDDeviceIndex = index;
	editData.EditType = editType;
	
	switch ( editType ) {
		case kNDASEditTypeName:
		{
			if (!cfsName) {
				return KERN_INVALID_ARGUMENT;
			}
			
			//memcpy(editData.Name, parameter->Name, MAX_NDAS_NAME_SIZE);
			CFStringGetCString(cfsName, editData.Name , MAX_NDAS_NAME_SIZE, kCFStringEncodingUTF8);			
		}
			break;
		case kNDASEditTypeRAIDType:
		{
			editData.RAIDType = raidType;
		}
			break;
		default:
			return KERN_INVALID_ARGUMENT;
	}
	
	// Send Command.
	kernResult = OpenDataPort(DriversIOKitClassName, &dataPort);
	if(kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "NDASIOConnectEdit: OpenDataPort Error %d\n", kernResult);
		return kernResult;
	}
	   
	kernResult = IOConnectMethodScalarIScalarO(dataPort, kNDASBusEnumeratorUserClientOpen, 0, 0);
	if (kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "NDASIOConnectEdit: Client Open Failed! %d\n", kernResult);
		
		CloseDataPort(dataPort);
		return kernResult;
	}
	
	kernResult = IOConnectMethodStructureIStructureO(dataPort, kNDASBusEnumeratorBindEdit,  sizeof(editData), &resultSize, &editData, &result);
	if (kernResult != KERN_SUCCESS) {
		syslog(LOG_ERR, "NDASIOConnectEdit: Bind Edit Failed %d\n", kernResult);
	} else {
		syslog(LOG_INFO, "NDASIOConnectEdit: Bind Edit Success\n");
	}	
	   
	kernResult = IOConnectMethodScalarIScalarO(dataPort, kNDASBusEnumeratorUserClientClose, 0, 0);
	   
	CloseDataPort(dataPort);
	
	return kernResult;
}
