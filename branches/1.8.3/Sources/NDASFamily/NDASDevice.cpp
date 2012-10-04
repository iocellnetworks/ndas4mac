/***************************************************************************
*
*  NDASDevice.cpp
*
*    NDAS Physical Device
*
*    Copyright (c) 2004 XiMeta, Inc. All rights reserved.
*
**************************************************************************/

#include <IOKit/IOLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/scsi/SCSICommandOperationCodes.h>
#include <IOKit/scsi/SCSICmds_INQUIRY_Definitions.h>
#include <IOKit/scsi/SCSICmds_READ_CAPACITY_Definitions.h>
#include <IOkit/scsi/SCSICmds_MODE_Definitions.h>

#include "NDASBusEnumerator.h"
#include "NDASDevice.h"

#include "NDASCommand.h"
#include "NDASPnpCommand.h"
#include "NDASChangeSettingCommand.h"
#include "NDASRefreshCommand.h"
#include "NDASSCSICommand.h"
#include "NDASManagingIOCommand.h"

#include "NDASDeviceController.h"
#include "NDASPhysicalUnitDevice.h"

#include "Utilities.h"

#include "hdreg.h"
#include "cdb.h"
#include "NDASDIB.h"
#include "NDASFamilyIOMessage.h"

#include "lsp_wrapper.h"

#include "../Inc/NDASID.h"

extern "C" {
#include <pexpert/pexpert.h>//This is for debugging purposes ONLY
}

#include "LanScsiSocket.h"
#include "Hash.h"

// Define my superclass
#define super IOService

// REQUIRED! This macro defines the class's constructors, destructors,
// and several other methods I/O Kit requires. Do NOT use super as the
// second parameter. You must use the literal name of the superclass.

OSDefineMetaClassAndStructors(com_ximeta_driver_NDASDevice, IOService)

bool com_ximeta_driver_NDASDevice::init(OSDictionary *dict)
{
	bool res = super::init(dict);
	int count;
	
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Initializing\n"));
	
	// Init Status.
	fType = 0xff;			// Undefined.
	fVersion = 0xff;		// Undefined.
	fStatus	= 0xffffffff;	// Undefined.

	fStatus = kNDASStatusDiscovered;	
	setProperty(NDASDEVICE_PROPERTY_KEY_STATUS, fStatus, 32);
	
	setConfiguration(kNDASConfigurationDisable);
	
	fInterfaceArrayIndex = NUMBER_OF_INTERFACES;
	fLinkSpeed = 0;

	// Flow Control
	for(count = 0 ; count < FLOW_NUMBER_OF_REQUEST_SIZE; count++) {
		fReadSpeed[count] = 0;
		fWriteSpeed[count] = 0;
	}
	
	fCurrentReadRequestSizeIndex = 0;
	fCurrentWriteRequestSizeIndex = 0;
	fReadStableCount = 0;
	fWriteStableCount = 0;
    	
	fPnpCommand = NULL;
	
	fWillTerminate = false;
	
	return res;
}

void com_ximeta_driver_NDASDevice::free(void)
{
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Freeing\n"));
		
	super::free();
}

bool com_ximeta_driver_NDASDevice::attach(IOService *provider)
{
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Attaching\n"));
		
	return super::attach(provider);
}

void com_ximeta_driver_NDASDevice::detach(IOService *provider)
{
    DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Detaching\n"));
		
    super::detach(provider);
}

IOReturn
com_ximeta_driver_NDASDevice::message(
									  UInt32 mess,
									  IOService * provider,
									  void * argument
									  )
{
	DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Entered. mess 0x%x\n", (uint32_t)mess));
	
	switch ( mess ) {
		case kIOMessageServiceIsRequestingClose:
			fWillTerminate = true;
		case kIOMessageServiceIsResumed:
		case kIOMessageServiceIsSuspended:
		case kIOMessageServiceIsTerminated:
			messageClients(mess);
			return kIOReturnSuccess;
		default:
			break;
	}
	
	return IOService::message(mess, provider, argument);
}

void com_ximeta_driver_NDASDevice::setSlotNumber(uint32_t SlotNumber)
{
	fSlotNumber = SlotNumber;
	
	setProperty(NDASDEVICE_PROPERTY_KEY_SLOT_NUMBER, fSlotNumber, 32);
}

uint32_t com_ximeta_driver_NDASDevice::slotNumber() 
{
	return fSlotNumber;
}

void com_ximeta_driver_NDASDevice::setUserPassword(char *Password)
{
	DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Entered. password %p\n", Password));
	
	if (Password) {
		memcpy(fUserPassword, Password, 8);
		
		DbgIOLog(DEBUG_MASK_PNP_INFO, ("Entered. password 0x%x:0x%x:0x%x:0x%x:0x%x:0x%x:0x%x:0x%x\n", fUserPassword[0], fUserPassword[1],  fUserPassword[2], fUserPassword[3], fUserPassword[4], fUserPassword[5],  fUserPassword[6], fUserPassword[7] ));

	}
}

char *com_ximeta_driver_NDASDevice::userPassword()
{
	return fUserPassword;
}

void com_ximeta_driver_NDASDevice::setSuperPassword(char *Password)
{
	if (Password) {
		memcpy(fSuperPassword, Password, 8);
	}
}

char *com_ximeta_driver_NDASDevice::superPassword()
{
	return fSuperPassword;
}

void com_ximeta_driver_NDASDevice::setAddress(struct sockaddr_lpx *Address)
{	
	memcpy(&fAddress, Address, sizeof(struct sockaddr_lpx));
	
	DbgIOLog(DEBUG_MASK_NDAS_INFO, ("setAddress: Address %x:%x:%x:%x:%x:%x[%d]\n",
								fAddress.slpx_node[0], fAddress.slpx_node[1],
								fAddress.slpx_node[2], fAddress.slpx_node[3],
								fAddress.slpx_node[4], fAddress.slpx_node[5],
								ntohs(fAddress.slpx_port)
								));

	OSData *osdAddress;
	osdAddress = OSData::withBytesNoCopy(fAddress.slpx_node, 6);

	setProperty(NDASDEVICE_PROPERTY_KEY_ADDRESS, osdAddress);
	
	osdAddress->release();
}

struct sockaddr_lpx *com_ximeta_driver_NDASDevice::address()
{
	return &fAddress;
}

void com_ximeta_driver_NDASDevice::setName(char *Name)
{
	memcpy(&fName[0], Name, MAX_NDAS_NAME_SIZE);
	
	//setProperty(NDASDEVICE_PROPERTY_KEY_NAME, fName);
		
	setProperty(NDASDEVICE_PROPERTY_KEY_NAME, OSData::withBytesNoCopy(fName, MAX_NDAS_NAME_SIZE));
}

char *com_ximeta_driver_NDASDevice::name()
{
	return &fName[0];
}

void com_ximeta_driver_NDASDevice::setDeviceIDString(char *deviceIDString)
{
	memcpy(&fDeviceIDString[0], deviceIDString, MAX_NDAS_NAME_SIZE);
	
	setProperty(NDASDEVICE_PROPERTY_KEY_DEVICEID_STRING, fDeviceIDString);
}

char *com_ximeta_driver_NDASDevice::deviceIDString()
{
	return &fDeviceIDString[0];
}

void com_ximeta_driver_NDASDevice::setWriteRight(bool HasWriteRight)
{
	fHasWriteRight = HasWriteRight;
	
	setProperty(NDASDEVICE_PROPERTY_KEY_WRITE_RIGHT, fHasWriteRight);
}

bool com_ximeta_driver_NDASDevice::writeRight()
{
	return fHasWriteRight;
}

void com_ximeta_driver_NDASDevice::setConfiguration(uint32_t configuration)
{
	fConfiguration = configuration;
	
	setProperty(NDASDEVICE_PROPERTY_KEY_CONFIGURATION, fConfiguration, 32);
}

uint32_t com_ximeta_driver_NDASDevice::configuration()
{
	return fConfiguration;
}

uint64_t com_ximeta_driver_NDASDevice::lastTimeReceivingPnPMessage()
{
	return fLastTimeRecevingPnPMessage;
}

uint64_t com_ximeta_driver_NDASDevice::intervalPnPMessage()
{
	return fIntervalPnPMessage;
}

void com_ximeta_driver_NDASDevice::setStatus(uint32_t status)
{
	if(fStatus != status || kNDASStatusAutoRegister == status ) {	// For missing event of Auto Register.
		fStatus = status;
		
		setProperty(NDASDEVICE_PROPERTY_KEY_STATUS, fStatus, 32);
		
		com_ximeta_driver_NDASBusEnumerator	*bus;
		
		bus = OSDynamicCast(com_ximeta_driver_NDASBusEnumerator, 
							getParentEntry(gIOServicePlane));
		
		if (NULL == bus) {
			DbgIOLog(DEBUG_MASK_NDAS_INFO, ("setStatus: Can't Find bus.\n"));
		} else {
			bus->messageClients(kNDASFamilyIOMessageNDASDeviceStatusWasChanged, &fSlotNumber, sizeof(int32_t));
		}
	}
}

uint32_t com_ximeta_driver_NDASDevice::status()
{
	return fStatus;
}

void com_ximeta_driver_NDASDevice::setTargetNeedRefresh(uint32_t TargetNo)
{
	com_ximeta_driver_NDASRefreshCommand	*command = NULL;
	
	// Create NDAS Command.
	command = OSTypeAlloc(com_ximeta_driver_NDASRefreshCommand);
	
	if(command == NULL || !command->init()) {
		DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("failed to alloc command class\n"));
		if (command) {
			command->release();
		}
		
		return;
	}
	
	command->setCommand(TargetNo);
	
	com_ximeta_driver_NDASDeviceController *controller = OSDynamicCast(com_ximeta_driver_NDASDeviceController, getClient());
	if(!controller) { 
		DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("Command gate is NULL!!!\n"));
		
		command->release();
		
		return;
	}
		
	controller->enqueueCommand(command);
	
	command->release();
}

void	com_ximeta_driver_NDASDevice::setMaxRequestBytes(uint32_t MRB)
{
	fMaxRequestBytes = MRB;
	
	setProperty(NDASDEVICE_PROPERTY_KEY_MRB, fMaxRequestBytes, 32);	
}

uint32_t	com_ximeta_driver_NDASDevice::MaxRequestBytes()
{
	return fMaxRequestBytes;
}

void	com_ximeta_driver_NDASDevice::setVID(UInt8 vid)
{
	fVID = vid;
	
	setProperty(NDASDEVICE_PROPERTY_KEY_VID, fVID, 8);
}

UInt8	com_ximeta_driver_NDASDevice::VID()
{
	return fVID;
}


void	com_ximeta_driver_NDASDevice::setAutoRegister(bool autoRegister)
{
	fAutoRegister = autoRegister;
	
	setProperty(NDASDEVICE_PROPERTY_KEY_AUTOREGISTER, fAutoRegister);	
}

bool	com_ximeta_driver_NDASDevice::autoRegister() {
	return fAutoRegister;
}

void com_ximeta_driver_NDASDevice::receivePnPMessage(
													 uint32_t receivedTime, 
													 UInt8 type,
													 UInt8 version,
													 uint32_t interfaceArrayIndex, 
													 char*	address,
													 uint64_t Speed
													 )
{
	com_ximeta_driver_NDASChangeSettingCommand	*command;
	bool										bChangeInterface;
	
	DbgIOLog(DEBUG_MASK_NDAS_TRACE, ("receivePnPMessage: Entered. index %d, Speed %llu type %d version %d\n", interfaceArrayIndex, Speed, type, version));
	
	bChangeInterface = false;
	
	if ( fWillTerminate ) {
		return;
	}
	
	// Check Interface.
	if(fInterfaceArrayIndex >= NUMBER_OF_INTERFACES) {
		// Initial 
		bChangeInterface = true;
	
		// to record received time.
		fInterfaceArrayIndex = interfaceArrayIndex;
		fLinkSpeed = Speed;

		DbgIOLog(DEBUG_MASK_NDAS_INFO, ("receivePnPMessage: Initial Assignment.\n"));
		
	} else if (fInterfaceArrayIndex != interfaceArrayIndex) {
		
		if(Speed > fLinkSpeed) {

			DbgIOLog(DEBUG_MASK_NDAS_WARNING, ("receivePnPMessage: Faster Interface.\n"));
			
			bChangeInterface = true;
			
		} else if((receivedTime - fLastTimeRecevingPnPMessage) > NDAS_PNPMESSAGE_INTERVAL_TIME ) {
			
			DbgIOLog(DEBUG_MASK_NDAS_WARNING, ("receivePnPMessage: Previous Interface was disconnected.\n"));
			
			bChangeInterface = true;
			
		}

	} else {
		fIntervalPnPMessage = receivedTime - fLastTimeRecevingPnPMessage;
	}

	// Update Time value.
	fLastTimeRecevingPnPMessage = receivedTime;

	if(true == bChangeInterface) {

		if (Speed <= fLinkSpeed) {
			// Prevent reconnection by pnp packet drop. 
			bChangeInterface = false;
		}
		
		DbgIOLog(DEBUG_MASK_NDAS_INFO, ("receivePnPMessage: Curr NIC# %d, new NIC# %d.\n", fInterfaceArrayIndex, interfaceArrayIndex));

		fInterfaceArrayIndex = interfaceArrayIndex;
		fLinkSpeed = Speed;
				
		// Create Interface Address.
		bzero(&fInterfaceAddress, sizeof(struct sockaddr_lpx));
		
		fInterfaceAddress.slpx_family = AF_LPX;
		fInterfaceAddress.slpx_len = sizeof(struct sockaddr_lpx);
		memcpy(fInterfaceAddress.slpx_node, address, 6);		
	}
			
	setHWType(type);
	setHWVersion(version);
	
	if ( fInterfaceArrayIndex == interfaceArrayIndex 	// Discard pnp message from not selected NIC to prevent duplicated discovery.
		 && NULL == fPnpCommand ) {
		
		// Create NDAS Command.
		command = OSTypeAlloc(com_ximeta_driver_NDASChangeSettingCommand);
		
		if(command == NULL || !command->init()) {
			DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("receivePnPMessage: failed to alloc command class\n"));
			if (command) {
				command->release();
			}
			
			return;
		}
		
		command->setCommand(interfaceArrayIndex, address, Speed, bChangeInterface);
		
		com_ximeta_driver_NDASDeviceController *controller = OSDynamicCast(com_ximeta_driver_NDASDeviceController, getClient());

		if(!controller) { 
			DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("receivePnPMessage: Command gate is NULL!!!\n"));
			
			command->release();
			
			return;
		}
		
		fPnpCommand = command;
		
		controller->enqueueCommand(command);
	}

	return;
}

void com_ximeta_driver_NDASDevice::completeChangeSettingMessage(
													  com_ximeta_driver_NDASChangeSettingCommand *command
													  )
{
	if ( NULL != fPnpCommand ) {
		
		if ( fPnpCommand != command ) {
			DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("completeChangeSettingMessage: Command missmatch!!!\n"));
		}
		
		command->release();
		fPnpCommand = NULL;
	}
}

int com_ximeta_driver_NDASDevice::getTargetData(TARGET_DATA *Targets, com_ximeta_driver_NDASPhysicalUnitDevice *unitDevice)
{
	DbgIOLog(DEBUG_MASK_LS_TRACE, ("Entered.\n"));
	
	int						error;
	int						status = kNDASStatusOnline;
	
	lsp_wrapper_context_ptr_t	lwcontext = NULL;
	
	if (unitDevice) {
		lwcontext = unitDevice->ConnectionData()->fLspWrapperContext;
		
		if (!lwcontext) {
			DbgIOLog(DEBUG_MASK_LS_ERROR, ("lwcontext is NULL!!!\n"));
			
			return kNDASStatusFailure;
		}
		
		if (!unitDevice->IsConnected()) {
			DbgIOLog(DEBUG_MASK_LS_ERROR, ("Not connected!!!\n"));
			
			return kNDASStatusFailure;			
		}
	} else {
		if(0 == fInterfaceAddress.slpx_len)
		{
			status = kNDASStatusFailure;
			goto Out;
		}
		
		lwcontext = (lsp_wrapper_context_ptr_t)IOMalloc(lsp_wrapper_context_size());
		if(NULL == lwcontext)
		{
			DbgIOLog(DEBUG_MASK_LS_ERROR, ("IOMalloc for session buffer Failed.\n"));
			status = kNDASStatusFailure;
			goto Out;
		}
		DbgIOLog(DEBUG_MASK_LS_TRACE, ("%p = IOMalloc(%d)\n", lwcontext, lsp_wrapper_context_size()));
		
		error = lsp_wrapper_connect(lwcontext, fAddress, fInterfaceAddress, NORMAL_DATACONNECTION_TIMEOUT);
		if(error) {
			DbgIOLog(DEBUG_MASK_LS_ERROR, ("xi_lpx_connect Failed(%d)\n", error));
			status = kNDASStatusFailure;
			goto Out;
		}
		DbgIOLog(DEBUG_MASK_LS_TRACE, ("%d = lsp_wrapper_connect()\n", error));
		
		error = lsp_wrapper_login(lwcontext, userPassword(), 1, 0, 0);
		if(error) {
			// Maybe the Discover port is busy. Try normal port.
			DbgIOLog(DEBUG_MASK_LS_ERROR, ("lsp_wrapper_login Retry...\n"));
			error = lsp_wrapper_login(lwcontext, userPassword(), 0, 0, 0);
			if(error) {
				DbgIOLog(DEBUG_MASK_LS_ERROR, ("lsp_wrapper_login Failed(%d)\n", error));
				status = kNDASStatusFailure;
				goto Out;
			}
		}
		DbgIOLog(DEBUG_MASK_LS_TRACE, ("%d = lsp_wrapper_login()\n", error));
	}
	
	// Get Text from NDAS.
	error = lsp_wrapper_text_command(lwcontext, &fNRTarget, Targets);
	if(error) {
		DbgIOLog(DEBUG_MASK_LS_ERROR, ("Text Failed\n"));
		
		status = kNDASStatusFailure;
		goto Out;
	}
	
	// For bug of chip 1.0
	if (fVersion == HW_VERSION_1_0) {
		Targets[1].bPresent = false;
	}
	
	if ( !unitDevice 
		&& kNDASStatusOnline == status ) {
		setMaxRequestBytes(lsp_wrapper_get_max_request_blocks(lwcontext) * DISK_BLOCK_SIZE);
	}
	
Out:
	if(!unitDevice 
	   && lwcontext)
	{
		error = lsp_wrapper_logout(lwcontext, false);
		DbgIOLog(DEBUG_MASK_LS_TRACE, ("%d = lsp_wrapper_logout()\n", error));
		error = lsp_wrapper_disconnect(lwcontext);
		DbgIOLog(DEBUG_MASK_LS_TRACE, ("%d = lsp_wrapper_disconnect()\n", error));
		IOFree(lwcontext, lsp_wrapper_context_size());
		lwcontext = NULL;
	}
	
	return status;
}

int com_ximeta_driver_NDASDevice::discovery(TARGET_DATA *Targets)
{
	int result;
	
	DbgIOLog(DEBUG_MASK_LS_TRACE, ("discovery entered.\n"));
	
	result = getTargetData(Targets, NULL);
	
	if(result == kNDASStatusOnline) {
		setMaxRequestBytes(Targets[0].MaxRequestBytes);
	}
	
	return result;
}

int com_ximeta_driver_NDASDevice::getUnitDiskInfo(TARGET_DATA *Targets)
{
	int						error;
	int						status = kNDASStatusOnline;
	
	lsp_wrapper_context_ptr_t	lwcontext = NULL;
		
	for(int count = 0; count < MAX_NR_OF_TARGETS_PER_DEVICE; count++)
	{
		
		if(Targets[count].bPresent != TRUE)
		{
			continue;
		}
		
		lwcontext = (lsp_wrapper_context_ptr_t)IOMalloc(lsp_wrapper_context_size());
		if(NULL == lwcontext)
		{
			DbgIOLog(DEBUG_MASK_LS_ERROR, ("[%s] IOMalloc for session buffer Failed.\n", __FUNCTION__));
			status = kNDASStatusFailure;
			goto Out;
		}
		DbgIOLog(DEBUG_MASK_LS_TRACE, ("%p = IOMalloc(%d)\n", lwcontext, lsp_wrapper_context_size()));
		
		error = lsp_wrapper_connect(lwcontext, fAddress, fInterfaceAddress, NORMAL_DATACONNECTION_TIMEOUT);
		if(error) {
			DbgIOLog(DEBUG_MASK_LS_ERROR, ("[%s] lsp_wrapper_connect Failed(%d)\n", __FUNCTION__, error));
			status = kNDASStatusFailure;
			goto Out;
		}
		DbgIOLog(DEBUG_MASK_LS_TRACE, ("%d = lsp_wrapper_connect()\n", error));
		
		error = lsp_wrapper_login(lwcontext, userPassword(), 0, count, 0);
		if(error) {
			DbgIOLog(DEBUG_MASK_LS_ERROR, ("[%s] lsp_wrapper_login Failed(%d)\n", __FUNCTION__, error));
			status = kNDASStatusFailure;
			goto Out;
		}
		DbgIOLog(DEBUG_MASK_LS_TRACE, ("%d = lsp_wrapper_login()\n", error));
		
		error = lsp_wrapper_fill_target_info(lwcontext, &Targets[count]);
		if(error) {
			DbgIOLog(DEBUG_MASK_LS_ERROR, ("[%s] lsp_wrapper_fill_target_info Failed(%d)\n", __FUNCTION__, error));
			status = kNDASStatusFailure;
			goto Out;
		}

		error = lsp_wrapper_logout(lwcontext, false);
		DbgIOLog(DEBUG_MASK_LS_TRACE, ("%d = lsp_wrapper_logout()\n", error));
		error = lsp_wrapper_disconnect(lwcontext);
		DbgIOLog(DEBUG_MASK_LS_TRACE, ("%d = lsp_wrapper_disconnect()\n", error));
		IOFree(lwcontext, lsp_wrapper_context_size());
		lwcontext = NULL;
	}
	
Out:
	if(lwcontext)
	{
		error = lsp_wrapper_logout(lwcontext, false);
		DbgIOLog(DEBUG_MASK_LS_TRACE, ("%d = lsp_wrapper_logout()\n", error));
		error = lsp_wrapper_disconnect(lwcontext);
		DbgIOLog(DEBUG_MASK_LS_TRACE, ("%d = lsp_wrapper_disconnect()\n",  error));
		IOFree(lwcontext, lsp_wrapper_context_size());
		lwcontext = NULL;
	}

	return status;
}	

UInt8 com_ximeta_driver_NDASDevice::checkVID()
{
	TARGET_DATA		targets;
	int				result;
	
	DbgIOLog(DEBUG_MASK_PNP_TRACE, ("Entered.\n"));
	
	// NDAS_OEM_CODE_GENERAL_AUTO
	setUserPassword((char *)NDAS_OEM_CODE_GENERAL_AUTO.Bytes);
	
	result = getTargetData(&targets, NULL);
	if (kNDASStatusOnline == result) {
		return NDAS_VID_GENERAL_AUTO;
	}
/*	
	// NDAS_OEM_CODE_GENERA
	setUserPassword((char *)NDAS_OEM_CODE_GENERAL.Bytes);
	
	result = getTargetData(&targets, NULL);
	if (kNDASStatusOnline == result) {
		return NDAS_VID_GENERAL;
	}
*/	
	return NDAS_VID_UNKNOWN;
}

bool com_ximeta_driver_NDASDevice::initDataConnection(com_ximeta_driver_NDASPhysicalUnitDevice *device, bool writable)
{
	int		error;
	int		count;
	int		timeout;
	bool	ret = false;
	
	lsp_wrapper_context_ptr_t	lwcontext = NULL;
	
	DbgIOLog(DEBUG_MASK_LS_TRACE, ("initDataConnection: Entered.\n"));
	
	if (NULL == device) {
		return false;
	}
	
	if (device->IsConnected()) {
		DbgIOLog(DEBUG_MASK_LS_WARNING, ("[%s] Already Connected.\n", __FUNCTION__));

		return false;
	}
	
	lwcontext = (lsp_wrapper_context_ptr_t)IOMalloc(lsp_wrapper_context_size());
	if(NULL == lwcontext)
	{
		DbgIOLog(DEBUG_MASK_LS_ERROR, ("[%s] IOMalloc for session buffer Failed.\n", __FUNCTION__));
		goto Out;
	}
	
	device->ConnectionData()->fLspWrapperContext = lwcontext;
	
	// Connect
	timeout = (device->TargetData()->bPacket) ?  PACKET_DATACONNECTION_TIMEOUT : NORMAL_DATACONNECTION_TIMEOUT;
	
	error = lsp_wrapper_connect(lwcontext, fAddress, fInterfaceAddress, timeout);
	if(error) {
		DbgIOLog(DEBUG_MASK_LS_ERROR, ("[%s] lsp_wrapper_connect Failed(%d)\n", __FUNCTION__, error));
		goto Out;
	}
	
	// old codes
	device->ConnectionData()->fTargetDataSessions.ui8HWType = fType;
	device->ConnectionData()->fTargetDataSessions.ui8HWVersion = fVersion;
	memcpy(device->ConnectionData()->fTargetDataSessions.Password, userPassword(), 8);
	
	error = lsp_wrapper_login(lwcontext, userPassword(), 0, device->unitNumber(), writable);
	if(error) {
		DbgIOLog(DEBUG_MASK_LS_ERROR, ("[%s] lsp_wrapper_login Failed(%d)\n", __FUNCTION__, error));
		goto Out;
	}
	
	device->setConnected(true);
	
	// Flow Control
	for(count = 0 ; count < FLOW_NUMBER_OF_REQUEST_SIZE; count++) {
		fReadSpeed[count] = 0;
		fWriteSpeed[count] = 0;
	}
	
	fCurrentReadRequestSizeIndex = 0;
	fCurrentWriteRequestSizeIndex = 0;
	fReadStableCount = 0;
	fWriteStableCount = 0;
	
	ret = true;
Out:
	
	if(!ret)
	{
		error = lsp_wrapper_logout(lwcontext, false);
		DbgIOLog(DEBUG_MASK_LS_TRACE, ("%d = lsp_wrapper_logout()\n", error));
		error = lsp_wrapper_disconnect(lwcontext);
		DbgIOLog(DEBUG_MASK_LS_TRACE, ("%d = lsp_wrapper_disconnect()\n", error));
		IOFree(lwcontext, lsp_wrapper_context_size());
		lwcontext = NULL;
		device->ConnectionData()->fLspWrapperContext = NULL;
		device->setConnected(false);
	}
	
	return ret;
}

bool com_ximeta_driver_NDASDevice::cleanupDataConnection(com_ximeta_driver_NDASPhysicalUnitDevice *device, bool connectionError)
{
	int error;
	lsp_wrapper_context_ptr_t	lwcontext = NULL;

	DbgIOLog(DEBUG_MASK_LS_TRACE, ("cleanupDataConnection: Entered.\n"));

	lwcontext = device->ConnectionData()->fLspWrapperContext;

#if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= 1060
	OSCompareAndSwapPtr(lwcontext, NULL, (void * volatile *)&(device->ConnectionData()->fLspWrapperContext));
#elif __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= 1050
	OSCompareAndSwap((UInt32)lwcontext, 0, (volatile UInt32*)&(device->ConnectionData()->fLspWrapperContext));
#else
	OSCompareAndSwap((UInt32)lwcontext, 0, (UInt32*)&(device->ConnectionData()->fLspWrapperContext));	
#endif
	DbgIOLog(DEBUG_MASK_LS_ERROR, ("cleanupDataConnection: lwcontext = %p, connectionErrro = %d\n", lwcontext, connectionError));
	
	if(lwcontext)
	{
		// Logout
		error = lsp_wrapper_logout(lwcontext, connectionError);
		DbgIOLog(DEBUG_MASK_LS_TRACE, ("%d = lsp_wrapper_logout()\n", error));
		
		error = lsp_wrapper_disconnect(lwcontext);
		DbgIOLog(DEBUG_MASK_LS_TRACE, ("%d = lsp_wrapper_disconnect()\n", error));
		
		IOFree(lwcontext, lsp_wrapper_context_size());			
		
		device->setConnected(false);
		
	}
	
	return true;
}

void com_ximeta_driver_NDASDevice::checkDataConnection(com_ximeta_driver_NDASPhysicalUnitDevice *device)
{
	lsp_wrapper_context_ptr_t	lwcontext = NULL;

	DbgIOLog(DEBUG_MASK_LS_TRACE, ("Entered.\n"));

	lwcontext = device->ConnectionData()->fLspWrapperContext;

//	DbgIOLog(DEBUG_MASK_LS_ERROR, ("lwcontext = %p\n", lwcontext));

	if(device->IsConnected()) {
		
		if(!lsp_wrapper_isconnected(lwcontext)) {
			
			DbgIOLog(DEBUG_MASK_LS_ERROR, ("Data Connection disconneted.\n"));
			
			cleanupDataConnection(device, true);
		}
	}	
}

bool com_ximeta_driver_NDASDevice::mount(com_ximeta_driver_NDASPhysicalUnitDevice *device)
{
	bool	writable = false;
	bool	bResult;
	
	DbgIOLog(DEBUG_MASK_LS_TRACE, ("mount: Entered.\n"));
	
	if (device->IsConnected()) {
	
		DbgIOLog(DEBUG_MASK_LS_WARNING, ("mount: Already mounted.\n"));

		return true;
	}
	
	// RW or RO
	switch(device->Configuration()) {
		case kNDASUnitConfigurationMountRO:
			writable = false;
			break;
		case kNDASUnitConfigurationMountRW:
		{
			if ( 0 < device->NRRWHosts() ) {
				
				DbgIOLog(DEBUG_MASK_LS_WARNING, ("mount: NRRW Hosts is not 0.\n"));
				
				return false; 
			}
			
			writable = true;
		}
			break;
		default:
			return device->Status();
	}
	
	// Init data session.
	bResult = initDataConnection(device, writable);
	
	return bResult;
}

bool com_ximeta_driver_NDASDevice::unmount(com_ximeta_driver_NDASPhysicalUnitDevice *device)
{
	bool	bResult;
	
	DbgIOLog(DEBUG_MASK_LS_ERROR, ("unmount: Entered.\n"));
	
	// Check Status.
//	switch(device->Status()) {
//		case kNDASUnitStatusMountRW:
//		case kNDASUnitStatusMountRO:
//		{
			// Unmount...
			bResult = cleanupDataConnection(device, false);
			
//		}
//			break;
//		default:
//			break;
//	}
		
	return bResult;	
}

bool com_ximeta_driver_NDASDevice::reconnect(com_ximeta_driver_NDASPhysicalUnitDevice *device)
{
	bool	writable = false;
	bool	bResult;
	
	DbgIOLog(DEBUG_MASK_LS_ERROR, ("reconnect: Entered.\n"));
	
	// Cleanup.
	cleanupDataConnection(device, false);
	
	// RW or RO
	switch(device->Configuration()) {
		case kNDASUnitConfigurationMountRO:
			writable = false;
			break;
		case kNDASUnitConfigurationMountRW:
		{
			if ( 0 < device->NRRWHosts() ) {
				
				if(kNDASUnitConfigurationMountRW != device->Configuration()) {
					// Can't Mount.
					return false;
				}
			}
			
			writable = true;
		}
			break;
		default:
			return device->Status();
	}
	
	// Init data session.
	bResult = initDataConnection(device, writable);
		
	return bResult;
}

void com_ximeta_driver_NDASDevice::setHWType(UInt8 type)
{
	if(fType != type) {
		fType = type;
		
		setProperty(NDASDEVICE_PROPERTY_KEY_HW_TYPE, fType, 8);
	}
}

void com_ximeta_driver_NDASDevice::setHWVersion(UInt8 version)
{
	if(fVersion != version) {
		fVersion = version;
		
		setProperty(NDASDEVICE_PROPERTY_KEY_HW_VERSION, fVersion, 8);
	}	
}

bool com_ximeta_driver_NDASDevice::processSrbCommand(PSCSICommand pScsiCommand)
{
	com_ximeta_driver_NDASPhysicalUnitDevice	*physicalDevice = NULL;
	bool result;
		
	physicalDevice = OSDynamicCast(com_ximeta_driver_NDASPhysicalUnitDevice, pScsiCommand->device);
	if(NULL == physicalDevice) {
		
		DbgIOLog(DEBUG_MASK_DISK_ERROR, ("No Physical Device!!!\n"));
		
		return false;
	}
	
	if(physicalDevice->TargetData()->bPacket) {
		result = processSrbCommandPacket(pScsiCommand);
	} else {
		
		uint64_t	LogicalBlockAddress;
		uint32_t	NumberOfBlocks;
		
//		DbgIOLog(DEBUG_MASK_ALL, ("processSrbCommand: Entered. cdb[0](%d)\n", pScsiCommand->cdb[0]));
		// If not a packet device.
		switch(pScsiCommand->cdb[0]) {
			case kSCSICmd_READ_10:
			{
//				DbgIOLog(DEBUG_MASK_ALL, ("processSrbCommand: READ. cdb[2](%d)\n", pScsiCommand->cdb[2]));
//				DbgIOLog(DEBUG_MASK_ALL, ("processSrbCommand: READ. cdb[3](%d)\n", pScsiCommand->cdb[3]));
//				DbgIOLog(DEBUG_MASK_ALL, ("processSrbCommand: READ. cdb[4](%d)\n", pScsiCommand->cdb[4]));
//				DbgIOLog(DEBUG_MASK_ALL, ("processSrbCommand: READ. cdb[5](%d)\n", pScsiCommand->cdb[5]));
//				DbgIOLog(DEBUG_MASK_ALL, ("processSrbCommand: READ. cdb[7](%d)\n", pScsiCommand->cdb[7]));
//				DbgIOLog(DEBUG_MASK_ALL, ("processSrbCommand: READ. cdb[8](%d)\n", pScsiCommand->cdb[8]));

				// Caclurate location and number of blocks.
				LogicalBlockAddress = ((((UInt8)pScsiCommand->cdb[2]) << 24) & 0xFF000000)
					| ((((UInt8)pScsiCommand->cdb[3]) << 16) & 0x00FF0000) 
					| ((((UInt8)pScsiCommand->cdb[4]) << 8) & 0x0000FF00)
					| (((UInt8)pScsiCommand->cdb[5]) & 0x000000FF);
				NumberOfBlocks = ((((UInt8)pScsiCommand->cdb[7]) << 8) & 0xFF00)
					| (((UInt8)pScsiCommand->cdb[8]) & 0x00FF);			
								
//				DbgIOLog(DEBUG_MASK_ALL, ("processSrbCommand: READ Location %llu, sectors %d, datalen %d\n",
//					LogicalBlockAddress, NumberOfBlocks, pScsiCommand->RequestedDataTransferCount));
				
				result = processReadCommand(pScsiCommand, LogicalBlockAddress, NumberOfBlocks, pScsiCommand->RequestedDataTransferCount);
				
				DbgIOLog(DEBUG_MASK_DISK_INFO, ("READ result %d\n", pScsiCommand->CmdData.xferCount));
				
			}
				break;
			case kSCSICmd_WRITE_10:
			{
				uint64_t	LogicalBlockAddress;
				uint32_t	NumberOfBlocks;

//				DbgIOLog(DEBUG_MASK_ALL, ("processSrbCommand: WRITE. cdb[2](%d)\n", pScsiCommand->cdb[2]));
//				DbgIOLog(DEBUG_MASK_ALL, ("processSrbCommand: WRITE. cdb[3](%d)\n", pScsiCommand->cdb[3]));
//				DbgIOLog(DEBUG_MASK_ALL, ("processSrbCommand: WRITE. cdb[4](%d)\n", pScsiCommand->cdb[4]));
//				DbgIOLog(DEBUG_MASK_ALL, ("processSrbCommand: WRITE. cdb[5](%d)\n", pScsiCommand->cdb[5]));
//				DbgIOLog(DEBUG_MASK_ALL, ("processSrbCommand: WRITE. cdb[7](%d)\n", pScsiCommand->cdb[7]));
//				DbgIOLog(DEBUG_MASK_ALL, ("processSrbCommand: WRITE. cdb[8](%d)\n", pScsiCommand->cdb[8]));
				
				// Caclurate location and number of blocks.
				LogicalBlockAddress = ((((UInt8)pScsiCommand->cdb[2]) << 24) & 0xFF000000)
					| ((((UInt8)pScsiCommand->cdb[3]) << 16) & 0x00FF0000) 
					| ((((UInt8)pScsiCommand->cdb[4]) << 8) & 0x0000FF00)
					| (((UInt8)pScsiCommand->cdb[5]) & 0x000000FF);
				NumberOfBlocks = ((((UInt8)pScsiCommand->cdb[7]) << 8) & 0xFF00)
					| (((UInt8)pScsiCommand->cdb[8]) & 0x00FF);			
				
				result = processWriteCommand(pScsiCommand, LogicalBlockAddress, NumberOfBlocks, pScsiCommand->RequestedDataTransferCount);
				
				DbgIOLog(DEBUG_MASK_DISK_INFO, ("Write result %d\n", pScsiCommand->CmdData.xferCount));
			}
				break;
			default:
			{
				// Common request.
				result = processSrbCommandNormal(pScsiCommand);
			}
				break;
		}		
	}
	
	// Update Unit Status.
	physicalDevice->updateStatus();
	
	return result;
}


bool com_ximeta_driver_NDASDevice::processSrbCommandNormal(PSCSICommand pScsiCommand)
{
	com_ximeta_driver_NDASPhysicalUnitDevice	*physicalDevice = NULL;		
	bool	bConnectionError = false;
	
	pScsiCommand->CmdData.results = kIOReturnSuccess;

	DbgIOLog(DEBUG_MASK_DISK_TRACE, ("Entered. cdb[0](%x), transfersize %lld, dir %d\n", pScsiCommand->cdb[0], pScsiCommand->RequestedDataTransferCount, pScsiCommand->DataTransferDirection));
			 
	physicalDevice = OSDynamicCast(com_ximeta_driver_NDASPhysicalUnitDevice, pScsiCommand->device);
	if(NULL == physicalDevice) {
		
		DbgIOLog(DEBUG_MASK_DISK_ERROR, ("No Physical Device!!!\n"));
		
		return false;
	}
	
	switch(pScsiCommand->cdb[0])
	{
		case kSCSICmd_INQUIRY:
		{
			SCSICmd_INQUIRY_StandardData 	inqData;
			IOMemoryDescriptor 	*inqDataDesc;
			uint32_t          	transferCount;
			uint32_t				xferBlocks;
			char				buffer[kINQUIRY_PRODUCT_IDENTIFICATION_Length] = { 0 };

			bzero(&inqData, sizeof(inqData));
			
			inqData.PERIPHERAL_DEVICE_TYPE = kINQUIRY_PERIPHERAL_TYPE_DirectAccessSBCDevice;
			
			if(pScsiCommand->device->Configuration() == kNDASUnitConfigurationMountRO) {
				inqData.RMB = kINQUIRY_PERIPHERAL_RMB_MediumRemovable;
			}
			//inqData.RMB = kINQUIRY_PERIPHERAL_RMB_MediumRemovable;
			memcpy(inqData.VENDOR_IDENTIFICATION, VENDOR_ID, kINQUIRY_VENDOR_IDENTIFICATION_Length);

			mySnprintf(buffer, sizeof(buffer), "BASIC_%d[%d]", slotNumber(), physicalDevice->unitNumber());
			memcpy(inqData.PRODUCT_IDENTIFICATION, buffer, kINQUIRY_PRODUCT_IDENTIFICATION_Length);
			
			memcpy(inqData.PRODUCT_REVISION_LEVEL, ND_PRODUCT_REVISION_LEVEL, kINQUIRY_PRODUCT_REVISION_LEVEL_Length);
			
			//LanScsiAdapter->Cmd->getPointers(&inqDataDesc, &transferCount, NULL);
			inqDataDesc = pScsiCommand->MemoryDescriptor_ptr;
			transferCount = pScsiCommand->RequestedDataTransferCount;
			
			xferBlocks = transferCount < sizeof(inqData) ? transferCount : sizeof(inqData);
			//            PreIOLog("transfrCount = %d, inqData = %d, xferBlocks  = %d\n", transferCount, sizeof(inqData), xferBlocks);
			inqDataDesc->writeBytes(0, &inqData, xferBlocks);
			
			pScsiCommand->CmdData.xferCount = xferBlocks;
			pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_GOOD;
			
			pScsiCommand->CmdData.results = kIOReturnSuccess;        
			break;
		}
			
		case kSCSICmd_TEST_UNIT_READY:
		{
			pScsiCommand->CmdData.xferCount = 0; 
			pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_GOOD;
			
			pScsiCommand->CmdData.results = kIOReturnSuccess;        
			break;
		} 
			
		case kSCSICmd_READ_CAPACITY:
		{
			SCSI_Capacity_Data    	readCapacityData;
			IOMemoryDescriptor  	*readCapacityDesc;
			uint32_t				transferCount;
			uint32_t				xferBlocks;
			
			DbgIOLog(DEBUG_MASK_DISK_INFO, ("cdb[0] : 0x%x, cdb[1] : 0x%x, cdb[2] : 0x%x, cdb[3] : 0x%x, cdb[4] : 0x%x, cdb[5] : 0x%x, cdb[6] : 0x%x, cdb[7] : 0x%x, cdb[8] : 0x%x, cdb[9] : 0x%x\n",
											 pScsiCommand->cdb[0], pScsiCommand->cdb[1], pScsiCommand->cdb[2],
											 pScsiCommand->cdb[3], pScsiCommand->cdb[4], pScsiCommand->cdb[5],
											 pScsiCommand->cdb[6], pScsiCommand->cdb[7], pScsiCommand->cdb[8], pScsiCommand->cdb[9]
											 ));			
			
			bzero(&readCapacityData, sizeof(readCapacityData));
			readCapacityData.RETURNED_LOGICAL_BLOCK_ADDRESS = NDASSwap32HostToBig( (uint32_t)(physicalDevice->Capacity() - 1));
			readCapacityData.BLOCK_LENGTH_IN_BYTES = NDASSwap32HostToBig(physicalDevice->blockSize());
			DbgIOLog(DEBUG_MASK_DISK_INFO, ("readCapacityData.logicalBlockAddress(%d 0x%x)",
				(uint32_t)readCapacityData.RETURNED_LOGICAL_BLOCK_ADDRESS, (uint32_t)readCapacityData.RETURNED_LOGICAL_BLOCK_ADDRESS));
			DbgIOLog(DEBUG_MASK_DISK_INFO, ("readCapacityData.bytesPerBlock(%d, 0x%x)",
				(uint32_t)readCapacityData.BLOCK_LENGTH_IN_BYTES, (uint32_t)readCapacityData.BLOCK_LENGTH_IN_BYTES));
			
			//LanScsiAdapter->Cmd->getPointers(&readCapacityDesc, &transferCount, NULL);
			readCapacityDesc = pScsiCommand->MemoryDescriptor_ptr;
			transferCount = pScsiCommand->RequestedDataTransferCount;
			
			xferBlocks = transferCount < sizeof(readCapacityData) ? transferCount : sizeof(readCapacityData);
			
			readCapacityDesc->writeBytes(0, &readCapacityData, xferBlocks);
			pScsiCommand->CmdData.xferCount = xferBlocks;
			
			pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_GOOD;
			
			pScsiCommand->CmdData.results = kIOReturnSuccess;        
			
			break; 
		}
			
		case kSCSICmd_MODE_SENSE_6:
		{
			uint32_t							transferCount; 
			uint32_t							xferBlocks;
			char								controlModePage[256];
			IOMemoryDescriptor					*controlModePageDesc;
			SPCModeParameterHeader6				*header;
			DASDModeParameterBlockDescriptor	*block;

			DbgIOLog(DEBUG_MASK_DISK_INFO, ("cdb[0] : 0x%x, cdb[1] : 0x%x, cdb[2] : 0x%x, cdb[3] : 0x%x, cdb[4] : 0x%x, cdb[5] : 0x%x\n",
											 pScsiCommand->cdb[0], pScsiCommand->cdb[1], pScsiCommand->cdb[2],
											 pScsiCommand->cdb[3], pScsiCommand->cdb[4], pScsiCommand->cdb[5]
											 ));
			
			bzero ( controlModePage, 256 );
			
			// Header.
			header = (SPCModeParameterHeader6*)controlModePage;

			header->MODE_DATA_LENGTH = sizeof( SPCModeParameterHeader6 ) - sizeof(header->MODE_DATA_LENGTH);
			header->MEDIUM_TYPE = 0x0;																					// Default medium type See. sbc-r08c.pdf(Table 71)
			if(pScsiCommand->device->Configuration() == kNDASUnitConfigurationMountRO) {
				header->DEVICE_SPECIFIC_PARAMETER |= kModeSenseSBCDeviceSpecific_WriteProtectMask; // Write Protect;
				DbgIOLog(DEBUG_MASK_DISK_INFO, ("Write Protected: modeBuffer[2] = %d\n", controlModePage[2]));
				//controlModePage[15] |= 0x04;	// Write Protect for Reduced Block Dev.
			}
			header->BLOCK_DESCRIPTOR_LENGTH = sizeof( DASDModeParameterBlockDescriptor );
			
			transferCount = sizeof(SPCModeParameterHeader6);
			
			// Mode Parameter Block Descriptor.
			block = (DASDModeParameterBlockDescriptor*)(controlModePage + sizeof( SPCModeParameterHeader6 ));

			if (physicalDevice->Capacity() >= 0xFFFFFFFF) {
				block->NUMBER_OF_BLOCKS = 0xFFFFFFFF;
			} else {
				block->NUMBER_OF_BLOCKS = (uint32_t)(physicalDevice->Capacity());
			}
			block->NUMBER_OF_BLOCKS = NDASSwap32HostToBig(block->NUMBER_OF_BLOCKS);
			block->DENSITY_CODE = 0x0;																	// Reserved for direct-access block device.
			
			block->BLOCK_LENGTH[0] = (physicalDevice->blockSize() >> 16) & 0xFF;
			block->BLOCK_LENGTH[1] = (physicalDevice->blockSize() >> 8) & 0xFF;
			block->BLOCK_LENGTH[2] = (physicalDevice->blockSize()) & 0xFF;
			
			transferCount += sizeof(DASDModeParameterBlockDescriptor);
			header->MODE_DATA_LENGTH += sizeof(DASDModeParameterBlockDescriptor);
			
			switch(pScsiCommand->cdb[2] & 0x3F) {
				case kSBCModePageFormatDeviceCode:	// Format device page.
				{
					SBCModePageFormatDevice	*page;
					
					page = (SBCModePageFormatDevice*)((char *)block + sizeof(DASDModeParameterBlockDescriptor));
					
					page->header.PS_PAGE_CODE = kSBCModePageFormatDeviceCode;
					page->header.PAGE_LENGTH = 0x16;
					
					transferCount += sizeof(SBCModePageFormatDevice);
					header->MODE_DATA_LENGTH += sizeof(SBCModePageFormatDevice);
				}					
					break;
				case kSBCModePageRigidDiskGeometryCode:	// Rigid disk geometry page.
				{
					SBCModePageRigidDiskGeometry	*page;
					
					page = (SBCModePageRigidDiskGeometry*)((char *)block + sizeof(DASDModeParameterBlockDescriptor));
					
					page->header.PS_PAGE_CODE = kSBCModePageRigidDiskGeometryCode;
					page->header.PAGE_LENGTH = 0x16;
					
					transferCount += sizeof(SBCModePageRigidDiskGeometry);
					header->MODE_DATA_LENGTH += sizeof(SBCModePageRigidDiskGeometry);
				}					
					break;
				case kSBCModePageCachingCode:	// Caching page.
				{
					SBCModePageCaching	*page;
					
					page = (SBCModePageCaching*)((char *)block + sizeof(DASDModeParameterBlockDescriptor));
					
					page->header.PS_PAGE_CODE = kSBCModePageCachingCode;
					page->header.PAGE_LENGTH = 0x12;
					
					transferCount += sizeof(SBCModePageCaching);
					header->MODE_DATA_LENGTH += sizeof(SBCModePageCaching);
				}
					break;
/*				case 0x3F:
				{
					SBCModePageFormatDevice	*page1;
					
					page1 = (SBCModePageFormatDevice*)(block + sizeof(DASDModeParameterBlockDescriptor));
					
					page1->header.PS_PAGE_CODE = kSBCModePageFormatDeviceCode;
					page1->header.PAGE_LENGTH = 0x16;
					
					transferCount += sizeof(SBCModePageFormatDevice);
					header->MODE_DATA_LENGTH += sizeof(SBCModePageFormatDevice);
					
					SBCModePageRigidDiskGeometry	*page2;
					
					page2 = (SBCModePageRigidDiskGeometry*)(page1 + sizeof(SBCModePageFormatDevice));
					
					page2->header.PS_PAGE_CODE = kSBCModePageRigidDiskGeometryCode;
					page2->header.PAGE_LENGTH = 0x16;
					
					transferCount += sizeof(SBCModePageRigidDiskGeometry);
					header->MODE_DATA_LENGTH += sizeof(SBCModePageRigidDiskGeometry);
					
					SBCModePageCaching	*page3;
					
					page3 = (SBCModePageCaching*)(page2 + sizeof(SBCModePageRigidDiskGeometry));
					
					page3->header.PS_PAGE_CODE = kSBCModePageCachingCode;
					page3->header.PAGE_LENGTH = 0x12;
					
					transferCount += sizeof(SBCModePageCaching);
					header->MODE_DATA_LENGTH += sizeof(SBCModePageCaching);					
				}
					break;
*/				default:
					DbgIOLog(DEBUG_MASK_DISK_ERROR, ("Unsupported page : 0x%x\n", pScsiCommand->cdb[2] & 0x3F));
			}
			
			controlModePageDesc = pScsiCommand->MemoryDescriptor_ptr;
			
			DbgIOLog(DEBUG_MASK_DISK_INFO, ("Len %x, Type %x, Param %x, BDLen %x, transferCount %d\n",
											 header->MODE_DATA_LENGTH,
											 header->MEDIUM_TYPE,
											 header->DEVICE_SPECIFIC_PARAMETER,
											 header->BLOCK_DESCRIPTOR_LENGTH,
											 transferCount
											 ));
			
			xferBlocks = transferCount < 256 ? transferCount : 256;
			controlModePageDesc->writeBytes(0, controlModePage, xferBlocks);
			pScsiCommand->CmdData.xferCount = xferBlocks;
			pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_GOOD;
			
			pScsiCommand->CmdData.results = kIOReturnSuccess;        

			break;
		}
			
		case kSCSICmd_MODE_SENSE_10:
		{
			uint32_t							transferCount; 
			uint32_t							xferBlocks;
			uint8_t								controlModePage[1024];
			IOMemoryDescriptor					*controlModePageDesc;
			SPCModeParameterHeader10			*header;
			LongLBAModeParameterBlockDescriptor	*block;
			
			bzero(controlModePage, 1024);

			// Header.
			header = (SPCModeParameterHeader10*)controlModePage;
			
			header->MODE_DATA_LENGTH = sizeof( SPCModeParameterHeader10 ) - sizeof(header->MODE_DATA_LENGTH);
			header->MEDIUM_TYPE = 0x0;																					// Default medium type See. sbc-r08c.pdf(Table 71)
			if(pScsiCommand->device->Configuration() == kNDASUnitConfigurationMountRO) {
				header->DEVICE_SPECIFIC_PARAMETER |= kModeSenseSBCDeviceSpecific_WriteProtectMask; // Write Protect;
				DbgIOLog(DEBUG_MASK_DISK_INFO, ("Write Protected: modeBuffer[2] = %d\n", controlModePage[2]));
				//controlModePage[15] |= 0x04;	// Write Protect for Reduced Block Dev.
			}
			header->LONGLBA |= kModeSenseParameterHeader10_LongLBAMask;
			header->BLOCK_DESCRIPTOR_LENGTH = NDASSwap16HostToBig( sizeof( LongLBAModeParameterBlockDescriptor ) );
			
			transferCount = sizeof(SPCModeParameterHeader10);
			
			// Mode Parameter Block Descriptor.
			block = (LongLBAModeParameterBlockDescriptor*)(controlModePage + sizeof( SPCModeParameterHeader10 ));
			
			block->NUMBER_OF_BLOCKS = physicalDevice->Capacity();
			block->NUMBER_OF_BLOCKS = NDASSwap64HostToBig(block->NUMBER_OF_BLOCKS);
			block->DENSITY_CODE = 0x0;																	// Reserved for direct-access block device.
			
			block->BLOCK_LENGTH = NDASSwap32HostToBig(physicalDevice->blockSize());
			
			transferCount += sizeof(LongLBAModeParameterBlockDescriptor);
			header->MODE_DATA_LENGTH += sizeof(LongLBAModeParameterBlockDescriptor);
			
			switch(pScsiCommand->cdb[2] & 0x3F) {
				case kSBCModePageFormatDeviceCode:	// Format device page.
				{
					SBCModePageFormatDevice	*page;
					
					page = (SBCModePageFormatDevice*)((char *)block + sizeof(LongLBAModeParameterBlockDescriptor));
					
					page->header.PS_PAGE_CODE = kSBCModePageFormatDeviceCode;
					page->header.PAGE_LENGTH = 0x16;
					
					transferCount += sizeof(SBCModePageFormatDevice);
					header->MODE_DATA_LENGTH += sizeof(SBCModePageFormatDevice);
				}					
					break;
				case kSBCModePageRigidDiskGeometryCode:	// Rigid disk geometry page.
				{
					SBCModePageRigidDiskGeometry	*page;
					
					page = (SBCModePageRigidDiskGeometry*)((char *)block + sizeof(LongLBAModeParameterBlockDescriptor));
					
					page->header.PS_PAGE_CODE = kSBCModePageRigidDiskGeometryCode;
					page->header.PAGE_LENGTH = 0x16;
					
					transferCount += sizeof(SBCModePageRigidDiskGeometry);
					header->MODE_DATA_LENGTH += sizeof(SBCModePageRigidDiskGeometry);
				}					
					break;
				case kSBCModePageCachingCode:	// Caching page.
				{
					SBCModePageCaching	*page;
					
					page = (SBCModePageCaching*)((char *)block + sizeof(LongLBAModeParameterBlockDescriptor));
					
					page->header.PS_PAGE_CODE = kSBCModePageCachingCode;
					page->header.PAGE_LENGTH = 0x12;
					
					transferCount += sizeof(SBCModePageCaching);
					header->MODE_DATA_LENGTH += sizeof(SBCModePageCaching);
				}
					break;
				case 0x3F:				// Return all page.
				{
					SBCModePageFormatDevice	*page1;
					
					page1 = (SBCModePageFormatDevice*)((char *)block + sizeof(LongLBAModeParameterBlockDescriptor));
					
					page1->header.PS_PAGE_CODE = kSBCModePageFormatDeviceCode;
					page1->header.PAGE_LENGTH = 0x16;
					
					transferCount += sizeof(SBCModePageFormatDevice);
					header->MODE_DATA_LENGTH += sizeof(SBCModePageFormatDevice);

					SBCModePageRigidDiskGeometry	*page2;
					
					page2 = (SBCModePageRigidDiskGeometry*)((char *)page1 + sizeof(SBCModePageFormatDevice));
					
					page2->header.PS_PAGE_CODE = kSBCModePageRigidDiskGeometryCode;
					page2->header.PAGE_LENGTH = 0x16;
					
					transferCount += sizeof(SBCModePageRigidDiskGeometry);
					header->MODE_DATA_LENGTH += sizeof(SBCModePageRigidDiskGeometry);

					SBCModePageCaching	*page3;
					
					page3 = (SBCModePageCaching*)((char *)page2 + sizeof(SBCModePageRigidDiskGeometry));
					
					page3->header.PS_PAGE_CODE = kSBCModePageCachingCode;
					page3->header.PAGE_LENGTH = 0x12;
					
					transferCount += sizeof(SBCModePageCaching);
					header->MODE_DATA_LENGTH += sizeof(SBCModePageCaching);
				}
					break;
				default:
					DbgIOLog(DEBUG_MASK_DISK_ERROR, ("Unsupported page : 0x%x\n", pScsiCommand->cdb[2] & 0x3F));
			}
			
			header->MODE_DATA_LENGTH = NDASSwap16HostToBig(header->MODE_DATA_LENGTH);
			
			controlModePageDesc = pScsiCommand->MemoryDescriptor_ptr;
			
			xferBlocks = transferCount < 1024 ? transferCount : 1024;
			controlModePageDesc->writeBytes(0, controlModePage, xferBlocks);
			pScsiCommand->CmdData.xferCount = xferBlocks;
			pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_GOOD;
			
			pScsiCommand->CmdData.results = kIOReturnSuccess;        
			
			break;
		}
			
		case kSCSICmd_MODE_SELECT_6:
		{
			pScsiCommand->CmdData.xferCount = 0;
			pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_GOOD;
			
			pScsiCommand->CmdData.results = kIOReturnSuccess;        
			break;
		}
			
		case kSCSICmd_START_STOP_UNIT:
		{
			pScsiCommand->CmdData.xferCount = 0;
			pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_GOOD;
			
			pScsiCommand->CmdData.results = kIOReturnSuccess;        
			break;
		}      
			
		case kSCSICmd_REQUEST_SENSE:
		{
			SCSI_Sense_Data		scsiSenseData;
			IOMemoryDescriptor 	*scsiSenseDataDesc;
			uint32_t          	transferCount;
			uint32_t		xferBlocks;
			
			bzero(&scsiSenseData, sizeof(scsiSenseData));
			
			scsiSenseData.VALID_RESPONSE_CODE = kSENSE_RESPONSE_CODE_Current_Errors | kSENSE_DATA_VALID;
			scsiSenseData.SENSE_KEY = physicalDevice->ConnectionData()->fTargetSenseKey;
			scsiSenseData.ADDITIONAL_SENSE_LENGTH = 0x0B;
			scsiSenseData.ADDITIONAL_SENSE_CODE = physicalDevice->ConnectionData()->fTargetASC;
			scsiSenseData.ADDITIONAL_SENSE_CODE_QUALIFIER = physicalDevice->ConnectionData()->fTargetASCQ;
			
			DbgIOLog(DEBUG_MASK_DISK_INFO, ("scsiSenseData.senseKey = %x\n", scsiSenseData.SENSE_KEY));
			physicalDevice->ConnectionData()->fTargetSenseKey = kSENSE_KEY_NO_SENSE;           
			physicalDevice->ConnectionData()->fTargetASC = 0;
			physicalDevice->ConnectionData()->fTargetASCQ = 0;
			
			scsiSenseDataDesc = pScsiCommand->MemoryDescriptor_ptr;
			transferCount = pScsiCommand->RequestedDataTransferCount;
			
			xferBlocks = transferCount < sizeof(scsiSenseData) ? transferCount : sizeof(scsiSenseData);
			scsiSenseDataDesc->writeBytes(0, &scsiSenseData, xferBlocks);
			
			pScsiCommand->CmdData.xferCount = xferBlocks;
			pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_GOOD;
			
			break;
		}
			
		case kSCSICmd_PREVENT_ALLOW_MEDIUM_REMOVAL:
		{
			// We don't support this command.
			// INVALID FIELD IN CDB 5/24/0
			physicalDevice->ConnectionData()->fTargetSenseKey = kSENSE_KEY_ILLEGAL_REQUEST;
			physicalDevice->ConnectionData()->fTargetASC = 0x24;	
			physicalDevice->ConnectionData()->fTargetASCQ = 0;
			
			pScsiCommand->CmdData.xferCount = 0;
			pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_CHECK_CONDITION;
			pScsiCommand->CmdData.results = kIOReturnSuccess;
		}
			break;	
		default: 
		{
			DbgIOLog(DEBUG_MASK_DISK_WARNING, ("Unsupported Operaion = %d\n", pScsiCommand->cdb[0]));
			
			//            SCSISenseData	scsiSendseData;
			// INVALID FIELD IN CDB 5/24/0
			physicalDevice->ConnectionData()->fTargetSenseKey = kSENSE_KEY_ILLEGAL_REQUEST;
			physicalDevice->ConnectionData()->fTargetASC = 0x24;	
			physicalDevice->ConnectionData()->fTargetASCQ = 0;
			
			pScsiCommand->CmdData.xferCount = 0;
			pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_CHECK_CONDITION;
			pScsiCommand->CmdData.results = kIOReturnSuccess;
			
			break;
		}
			
	}
	
	if(bConnectionError && physicalDevice->IsConnected()) {
		
		DbgIOLog(DEBUG_MASK_DISK_WARNING, ("!!!!!! DISCONNECTED !!!!!\n"));
		
		cleanupDataConnection(physicalDevice, true);		
	}
	
		
	return true;
}

bool com_ximeta_driver_NDASDevice::processReadCommand(
													  PSCSICommand pScsiCommand, 
													  uint64_t LogicalBlockAddress, 
													  uint32_t NumberOfBlocks,
													  uint32_t BufferSize
													  )
{
	uint32_t          	transferCount; 
	uint32_t				xferBlocks;
	uint32_t				targetNo;
	uint32_t				iResult;
	bool				bConnectionError;
	
	DbgIOLog(DEBUG_MASK_DISK_TRACE, ("Entered. Addr %llu, Blocks %u, buffersize %u\n", 
									  LogicalBlockAddress,
									  NumberOfBlocks,
									  BufferSize));
	
	com_ximeta_driver_NDASPhysicalUnitDevice	*physicalDevice = NULL;		

	physicalDevice = OSDynamicCast(com_ximeta_driver_NDASPhysicalUnitDevice, pScsiCommand->device);
	if(NULL == physicalDevice) {
		
		DbgIOLog(DEBUG_MASK_DISK_ERROR, ("No Physical Device!!!\n"));
		
		return false;
	}
	
	
	// Init.
	targetNo = pScsiCommand->targetNo;
	bConnectionError = false;
	
	// Check Buffer.	
	pScsiCommand->MemoryDescriptor_ptr->retain();
	IOMemoryMap *map = map = pScsiCommand->MemoryDescriptor_ptr->map();
	if(NULL == map) {
		
		DbgIOLog(DEBUG_MASK_DISK_ERROR, ("Can't map buffer!!!\n"));
		
		return false;
	}
	
	//
	// Check Parameter.
	//

/*	if(NumberOfBlocks > fTargetInfos[targetNo].fTargetDataSessions.MaxRequestBlocks) {
		DbgIOLog(DEBUG_MASK_DISK_WARNING, ("processReadCommand: too big, numBlocks = %u\n", (int)NumberOfBlocks));
		
		fTargetInfos[pScsiCommand->targetNo].fTargetSenseKey = kSENSE_KEY_ILLEGAL_REQUEST;
		pScsiCommand->CmdData.xferCount = 0;
		pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_CHECK_CONDITION;
		pScsiCommand->CmdData.results = kIOReturnSuccess;
		
		goto Out;
	}
*/
	if((LogicalBlockAddress + NumberOfBlocks - 1) >
	   (physicalDevice->TargetData()->SectorCount - 1 - NDAS_BLOCK_SIZE_XAREA)) {
		DbgIOLog(DEBUG_MASK_DISK_WARNING, ("Overflow Actual %llu request %llu\n", 
										  physicalDevice->TargetData()->SectorCount - 1 - NDAS_BLOCK_SIZE_XAREA,
										  (LogicalBlockAddress + NumberOfBlocks - 1)
										  ));
		// 5/21/1 INVALID ELEMENT ADDRESS
		physicalDevice->ConnectionData()->fTargetSenseKey = kSENSE_KEY_ILLEGAL_REQUEST;
		physicalDevice->ConnectionData()->fTargetASC = 0x21;	
		physicalDevice->ConnectionData()->fTargetASCQ = 1;
		
		pScsiCommand->CmdData.xferCount = 0;
		pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_CHECK_CONDITION;
		pScsiCommand->CmdData.results = kIOReturnSuccess;
		
		goto Out;
	}
		
	if(BufferSize < pScsiCommand->RequestedDataTransferCount ||
		BufferSize < NumberOfBlocks * physicalDevice->blockSize()) {

		DbgIOLog(DEBUG_MASK_DISK_WARNING, ("Buffer is too small. Buffer %d SRB %d  request %lld\n",
										  BufferSize,
										  NumberOfBlocks * physicalDevice->blockSize(),
										  pScsiCommand->RequestedDataTransferCount
										  ));
		
		// 5/55/0 SYSTEM RESOURCE FAILURE
		physicalDevice->ConnectionData()->fTargetSenseKey = kSENSE_KEY_ILLEGAL_REQUEST;
		physicalDevice->ConnectionData()->fTargetASC = 0x55;
		physicalDevice->ConnectionData()->fTargetASCQ = 0;
		
		pScsiCommand->CmdData.xferCount = 0;
		pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_CHECK_CONDITION;
		pScsiCommand->CmdData.results = kIOReturnSuccess;
		
		goto Out;
	}
		
	//
	// Flow Control.
	//
	
	transferCount = 0;
	
	uint64_t startTime;
	uint64_t endTime;
	
	NDAS_clock_get_uptime(&startTime);
	
	while(transferCount < pScsiCommand->RequestedDataTransferCount) {
	
		xferBlocks = (CURRENT_REQUEST_BYTES(fCurrentReadRequestSizeIndex, physicalDevice->blockSize()) < (BufferSize - transferCount) ) ?
			CURRENT_REQUEST_BYTES(fCurrentReadRequestSizeIndex, physicalDevice->blockSize()) : (BufferSize - transferCount);
		xferBlocks /= physicalDevice->blockSize();
			
		DbgIOLog(DEBUG_MASK_DISK_INFO, ("xferBlocks %d\n", xferBlocks));
				
		if((iResult = lsp_wrapper_ide_read(
			physicalDevice->ConnectionData()->fLspWrapperContext,
			 LogicalBlockAddress + (transferCount / physicalDevice->blockSize()), 
			 xferBlocks, 
			 (map->getVirtualAddress() + pScsiCommand->BufferOffset + transferCount),
			 xferBlocks * physicalDevice->blockSize())) != 0)
			
		{
			DbgIOLog(DEBUG_MASK_DISK_WARNING, ("lsp_wrapper_ide_read Error! %d\n", iResult));
			
			// Sense Key.
			if ( -2 == iResult ) {
				physicalDevice->ConnectionData()->fTargetSenseKey = kSENSE_KEY_MEDIUM_ERROR;
				physicalDevice->ConnectionData()->fTargetASC = 0x11;
				physicalDevice->ConnectionData()->fTargetASCQ = 0;
			}
			
			break;
		} 
/*		
		// write Data.
		pScsiCommand->MemoryDescriptor_ptr->writeBytes(pScsiCommand->BufferOffset + transferCount,
													  physicalDevice->DataBuffer(),
													  xferBlocks * SECTOR_SIZE);
*/		
		transferCount += xferBlocks * physicalDevice->blockSize();
		
		DbgIOLog(DEBUG_MASK_DISK_INFO, ("transferCount %d\n", transferCount));
		
	}

	NDAS_clock_get_uptime(&endTime);
	
	//
	// Update Flow control parameters.
	//
	if(NumberOfBlocks >= CURRENT_REQUEST_BLOCKS(fCurrentReadRequestSizeIndex, physicalDevice->blockSize())) {	// Compare with min value.
		
		uint64_t	speed, endTimeNano;
		
		endTimeNano = endTime - startTime;
		
		speed = endTimeNano / NumberOfBlocks;	// nsec per 1Block.
		
		DbgIOLog(DEBUG_MASK_DISK_INFO, ("NumberOf Blocks %d Time %llu, Speed %llu\n", 
										NumberOfBlocks, endTimeNano, speed
										));
		
		if(fReadSpeed[fCurrentReadRequestSizeIndex] == 0) {
			// Initial Time.
			fReadSpeed[fCurrentReadRequestSizeIndex] = speed;
			
			DbgIOLog(DEBUG_MASK_DISK_INFO, ("Initial %d\n", fCurrentReadRequestSizeIndex));
		} else if(endTimeNano > RETRANSMIT_TIME_NANO) { // if((fReadSpeed[fCurrentReadRequestSizeIndex] * 2) < speed) {
			
			//
			// Maybe Retransmission happened.
			
			DbgIOLog(DEBUG_MASK_DISK_INFO, ("too Long time!!! Speed %llu curr %llu\n", speed, fReadSpeed[fCurrentReadRequestSizeIndex]));
			if(fCurrentReadRequestSizeIndex > 0 &&
			   fReadSpeed[fCurrentReadRequestSizeIndex - 1] < speed) {
				// Reduce Request size.
				fCurrentReadRequestSizeIndex--;
//				fCurrentReadRequestSizeIndex /= 2;
				
				fReadSpeed[fCurrentReadRequestSizeIndex - 1] = 1;
				
				DbgIOLog(DEBUG_MASK_DISK_ERROR, ("too Long time!!! Reduced %d\n", fCurrentReadRequestSizeIndex));
			} 
			
			fReadStableCount = 0;
		
		} else {
						
			// Keep request size.
			fReadSpeed[fCurrentReadRequestSizeIndex] = fReadSpeed[fCurrentReadRequestSizeIndex] * 9 / 10;
			fReadSpeed[fCurrentReadRequestSizeIndex] += (speed / (10));

			if(fReadStableCount >= MAX_STABLE_COUNT) {
				
				if (CURRENT_REQUEST_BYTES(fCurrentReadRequestSizeIndex, physicalDevice->blockSize()) < MaxRequestBytes() &&
					speed <= fReadSpeed[fCurrentReadRequestSizeIndex]) { // &&
//					fReadSpeed[fCurrentReadRequestSizeIndex] < fReadSpeed[fCurrentReadRequestSizeIndex - 1]) {
					fCurrentReadRequestSizeIndex++;
					
					DbgIOLog(DEBUG_MASK_DISK_INFO, ("Incremented %d\n", fCurrentReadRequestSizeIndex));
				} else {
					while (fCurrentReadRequestSizeIndex > 0 &&
						   fReadSpeed[fCurrentReadRequestSizeIndex] > fReadSpeed[fCurrentReadRequestSizeIndex - 1] &&
						speed > fReadSpeed[fCurrentReadRequestSizeIndex - 1]) {
						fCurrentReadRequestSizeIndex--;

						DbgIOLog(DEBUG_MASK_DISK_INFO, ("Reduced %d\n", fCurrentReadRequestSizeIndex));
					}
				}
				
/*				
				// Need Reduce?
				if(fCurrentReadRequestSizeIndex > 0 &&
				   speed > fReadSpeed[fCurrentReadRequestSizeIndex - 1]) {
					
					fCurrentReadRequestSizeIndex--;
					
					DbgIOLog(DEBUG_MASK_DISK_ERROR, ("Reduced %d\n", fCurrentReadRequestSizeIndex));
					
				} else if(CURRENT_REQUEST_BLOCKS(fCurrentReadRequestSizeIndex) < MaxRequestBlocks()) {
					fCurrentReadRequestSizeIndex++;
					
					DbgIOLog(DEBUG_MASK_DISK_ERROR, ("Incremented %d\n", fCurrentReadRequestSizeIndex));
				}
 */
				fReadStableCount = 0;
			} else {		
				fReadStableCount++;
			}			
		}
#if 0
		DbgIOLog(DEBUG_MASK_DISK_INFO, ("Curr Request %d, Array[%d] %llu, Array[%d] %llu, Array[%d] %llu, Array[%d], %llu, Array[%d] %llu.\n",
										  fCurrentReadRequestSizeIndex,
										  (fCurrentReadRequestSizeIndex - 2 < 0 ? 0 : fCurrentReadRequestSizeIndex - 2), fReadSpeed[(fCurrentReadRequestSizeIndex - 2 < 0 ? 0 : fCurrentReadRequestSizeIndex - 2)], 
										  (fCurrentReadRequestSizeIndex - 1 < 0 ? 0 : fCurrentReadRequestSizeIndex - 1), fReadSpeed[(fCurrentReadRequestSizeIndex - 1 < 0 ? 0 : fCurrentReadRequestSizeIndex - 1)],
										  (fCurrentReadRequestSizeIndex), fReadSpeed[fCurrentReadRequestSizeIndex],
										  (fCurrentReadRequestSizeIndex + 1 >= FLOW_NUMBER_OF_REQUEST_SIZE ? FLOW_NUMBER_OF_REQUEST_SIZE - 1 : fCurrentReadRequestSizeIndex + 1), fReadSpeed[(fCurrentReadRequestSizeIndex + 1 >= FLOW_NUMBER_OF_REQUEST_SIZE ? FLOW_NUMBER_OF_REQUEST_SIZE - 1 : fCurrentReadRequestSizeIndex + 1)],
										  (fCurrentReadRequestSizeIndex + 2 >= FLOW_NUMBER_OF_REQUEST_SIZE ? FLOW_NUMBER_OF_REQUEST_SIZE - 1 : fCurrentReadRequestSizeIndex + 2), fReadSpeed[(fCurrentReadRequestSizeIndex + 1 >= FLOW_NUMBER_OF_REQUEST_SIZE ? FLOW_NUMBER_OF_REQUEST_SIZE - 1 : fCurrentReadRequestSizeIndex + 1)]
										   ));
#endif
	}
	
	switch(iResult) {
		case 0:
		{
			pScsiCommand->CmdData.xferCount = transferCount;
			pScsiCommand->CmdData.serviceResponse = kSCSIServiceResponse_TASK_COMPLETE;
			pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_GOOD;
			pScsiCommand->CmdData.results = kIOReturnSuccess;    
		}
			break;
		case -3:
		{	
			pScsiCommand->CmdData.xferCount = 0;
			pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_BUSY;
			pScsiCommand->CmdData.results = kIOReturnBusy;    
			
			bConnectionError = true;
		}
			break;
		default:
		{
			pScsiCommand->CmdData.xferCount = transferCount;
			pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_CHECK_CONDITION;
			pScsiCommand->CmdData.results = kIOReturnSuccess;    
		}
			break;
	}
	

Out:
		
	if(map) {
		map->release();
	}
	pScsiCommand->MemoryDescriptor_ptr->release();

	if(bConnectionError && physicalDevice->IsConnected()) {
			
		DbgIOLog(DEBUG_MASK_DISK_WARNING, ("!!!!!! DISCONNECTED !!!!!\n"));
			
		//unmount(pScsiCommand->targetNo);
		cleanupDataConnection(physicalDevice, true);

	}
	
	return true;
	
}

bool com_ximeta_driver_NDASDevice::processWriteCommand(
													  PSCSICommand pScsiCommand, 
													  uint64_t LogicalBlockAddress, 
													  uint32_t NumberOfBlocks,
													  uint32_t BufferSize
													  )
{
	uint32_t          	transferCount; 
	uint32_t				xferBlocks;
	uint32_t				targetNo;
	uint32_t				iResult;
	bool				bConnectionError;
	
	DbgIOLog(DEBUG_MASK_DISK_TRACE, ("Entered. Addr %llu, Blocks %u, buffersize %u\n", 
									  LogicalBlockAddress,
									  NumberOfBlocks,
									  BufferSize));
	
	com_ximeta_driver_NDASPhysicalUnitDevice	*physicalDevice = NULL;		
	
	physicalDevice = OSDynamicCast(com_ximeta_driver_NDASPhysicalUnitDevice, pScsiCommand->device);
	if(NULL == physicalDevice) {
		
		DbgIOLog(DEBUG_MASK_DISK_ERROR, ("No Physical Device!!!\n"));
		
		return false;
	}
	
	// Init.
	targetNo = pScsiCommand->targetNo;
	bConnectionError = false;

	// Mapping Buffer.
	pScsiCommand->MemoryDescriptor_ptr->retain();
	IOMemoryMap *map = pScsiCommand->MemoryDescriptor_ptr->map();
	if(NULL == map) {
		
		DbgIOLog(DEBUG_MASK_DISK_ERROR, ("Can't map buffer!!!\n"));
		
		return false;
	}
	
//	DbgIOASSERT(map);
		
	//
	// Check Parameter.
	//
	/*
	if(NumberOfBlocks > fTargetInfos[targetNo].fTargetDataSessions.MaxRequestBlocks) {
		DbgIOLog(DEBUG_MASK_DISK_WARNING, ("processWriteCommand: too big, numBlocks = %u\n", (int)NumberOfBlocks));
		
		fTargetInfos[pScsiCommand->targetNo].fTargetSenseKey = kSENSE_KEY_ILLEGAL_REQUEST;
		pScsiCommand->CmdData.xferCount = 0;
		pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_CHECK_CONDITION;
		pScsiCommand->CmdData.results = kIOReturnSuccess;
		
		goto Out;
	}
	*/
	
	if((LogicalBlockAddress + NumberOfBlocks - 1) >
	   (physicalDevice->TargetData()->SectorCount - 1 - NDAS_BLOCK_SIZE_XAREA)) {
		DbgIOLog(DEBUG_MASK_DISK_WARNING, ("Overflow Actual %llu request %llu\n", 
										  physicalDevice->TargetData()->SectorCount - 1 - NDAS_BLOCK_SIZE_XAREA,
										  (LogicalBlockAddress + NumberOfBlocks - 1)
										  ));
		// 5/21/1 INVALID ELEMENT ADDRESS
		physicalDevice->ConnectionData()->fTargetSenseKey = kSENSE_KEY_ILLEGAL_REQUEST;
		physicalDevice->ConnectionData()->fTargetASC = 0x21;
		physicalDevice->ConnectionData()->fTargetASCQ = 1;
		
		pScsiCommand->CmdData.xferCount = 0;
		pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_CHECK_CONDITION;
		pScsiCommand->CmdData.results = kIOReturnSuccess;
		
		goto Out;
	}
	
	if(BufferSize < pScsiCommand->RequestedDataTransferCount ||
		BufferSize < NumberOfBlocks * physicalDevice->blockSize()) {
		
		DbgIOLog(DEBUG_MASK_DISK_WARNING, ("Buffer is too small. Buffer %u SRB %u request %lld \n",
										  BufferSize,
										  NumberOfBlocks * physicalDevice->blockSize(),
										  pScsiCommand->RequestedDataTransferCount
										  ));
		// 5/55/0 SYSTEM RESOURCE FAILURE
		physicalDevice->ConnectionData()->fTargetSenseKey = kSENSE_KEY_ILLEGAL_REQUEST;
		physicalDevice->ConnectionData()->fTargetASC = 0x55;
		physicalDevice->ConnectionData()->fTargetASCQ = 0;
		
		pScsiCommand->CmdData.xferCount = 0;
		pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_CHECK_CONDITION;
		pScsiCommand->CmdData.results = kIOReturnSuccess;
		
		goto Out;
	}

	//
	// Flow Control.
	//
	
	transferCount = 0;
	
	uint64_t startTime;
	uint64_t endTime;
	
	NDAS_clock_get_uptime(&startTime);
	
	while(transferCount < pScsiCommand->RequestedDataTransferCount) {
		
		xferBlocks = (CURRENT_REQUEST_BYTES(fCurrentWriteRequestSizeIndex, physicalDevice->blockSize()) < (BufferSize - transferCount) ) ?
			CURRENT_REQUEST_BYTES(fCurrentWriteRequestSizeIndex, physicalDevice->blockSize()) : (BufferSize - transferCount);
		
		xferBlocks /= physicalDevice->blockSize();
/*		
		// Read Data.
		pScsiCommand->MemoryDescriptor_ptr->readBytes(pScsiCommand->BufferOffset + transferCount,
													  physicalDevice->DataBuffer(),
													  xferBlocks * SECTOR_SIZE);
*/		

		if((iResult = lsp_wrapper_ide_write(
			physicalDevice->ConnectionData()->fLspWrapperContext,
			 LogicalBlockAddress + (transferCount / physicalDevice->blockSize()), 
			 xferBlocks, 
			 (char *)(map->getVirtualAddress() + pScsiCommand->BufferOffset + transferCount), // physicalDevice->DataBuffer(), 
			 xferBlocks * physicalDevice->blockSize())) != 0)
		{
			DbgIOLog(DEBUG_MASK_DISK_WARNING, ("lsp_wrapper_ide_write Error! %d Sector %llu, Size %u\n", iResult, LogicalBlockAddress + (transferCount / SECTOR_SIZE), xferBlocks * SECTOR_SIZE));
			
			// Sense Key.
			if ( -2 == iResult ) {
				// 3/0C/0 WRITE ERROR
				physicalDevice->ConnectionData()->fTargetSenseKey = kSENSE_KEY_MEDIUM_ERROR;
				physicalDevice->ConnectionData()->fTargetASC = 0x0C;
				physicalDevice->ConnectionData()->fTargetASCQ = 0;
			}

			break;
		} 
		
		transferCount += xferBlocks * physicalDevice->blockSize();
	}
	
	NDAS_clock_get_uptime(&endTime);
	
	//
	// Update Flow control parameters.
	//
	if(NumberOfBlocks >= CURRENT_REQUEST_BLOCKS(fCurrentWriteRequestSizeIndex, physicalDevice->blockSize())) {	// Compare with min value.
						
		uint64_t	speed, endTimeNano;
		
		endTimeNano = endTime - startTime;
		
		speed = endTimeNano / NumberOfBlocks;	// nsec per 1Block.
		
		DbgIOLog(DEBUG_MASK_DISK_INFO, ("Time %llu, Speed %llu\n", 
										  endTimeNano, speed
										  ));
		
		if(fWriteSpeed[fCurrentWriteRequestSizeIndex] == 0) {
			// Initial Time.
			fWriteSpeed[fCurrentWriteRequestSizeIndex] = speed;
			
			DbgIOLog(DEBUG_MASK_DISK_INFO, ("Initial %d\n", fCurrentWriteRequestSizeIndex));
		} else if(endTimeNano > RETRANSMIT_TIME_NANO) { // if((fWriteSpeed[fCurrentWriteRequestSizeIndex] * 2) < speed) {
						 
			// Maybe Retransmission.
			if(fCurrentWriteRequestSizeIndex > 0 &&
			   fWriteSpeed[fCurrentWriteRequestSizeIndex - 1] < speed) {
				// Reduce Request size.
				fCurrentWriteRequestSizeIndex--;
				//fCurrentWriteRequestSizeIndex /= 2;
			
				fWriteSpeed[fCurrentWriteRequestSizeIndex - 1] = 1;
				
				DbgIOLog(DEBUG_MASK_DISK_ERROR, ("Reduced by retransmission. %d\n", fCurrentWriteRequestSizeIndex));
			} 			
		} else {
			
			// Keep request size.
			fWriteSpeed[fCurrentWriteRequestSizeIndex] = fWriteSpeed[fCurrentWriteRequestSizeIndex] * 9 / 10;
			fWriteSpeed[fCurrentWriteRequestSizeIndex] += (speed / 10);
			
			if(fWriteStableCount >= MAX_STABLE_COUNT) {
				
				if (CURRENT_REQUEST_BYTES(fCurrentWriteRequestSizeIndex, physicalDevice->blockSize()) < MaxRequestBytes() &&
					speed <= fWriteSpeed[fCurrentWriteRequestSizeIndex]) { // &&
					//					fWriteSpeed[fCurrentReadRequestSizeIndex] < fWriteSpeed[fCurrentReadRequestSizeIndex - 1]) {
					fCurrentWriteRequestSizeIndex++;
					
					DbgIOLog(DEBUG_MASK_DISK_INFO, ("Incremented %d\n", fCurrentWriteRequestSizeIndex));
				} else {
					while (fCurrentWriteRequestSizeIndex > 0 &&
						   fWriteSpeed[fCurrentWriteRequestSizeIndex] > fWriteSpeed[fCurrentWriteRequestSizeIndex - 1] &&
						   speed > fWriteSpeed[fCurrentWriteRequestSizeIndex - 1]) {
						fCurrentWriteRequestSizeIndex--;
						
						DbgIOLog(DEBUG_MASK_DISK_INFO, ("Reduced %d\n", fCurrentWriteRequestSizeIndex));
					}
				}
/*				
			if(fWriteStableCount >= MAX_STABLE_COUNT) {
				
				// Need Reduce?
				if(fCurrentWriteRequestSizeIndex > 0 &&
				   speed > fWriteSpeed[fCurrentWriteRequestSizeIndex - 1]) {
			
					fCurrentWriteRequestSizeIndex--;

					DbgIOLog(DEBUG_MASK_DISK_INFO, ("Reduced %d\n", fCurrentWriteRequestSizeIndex));

				} else if(CURRENT_REQUEST_BLOCKS(fCurrentWriteRequestSizeIndex) < MaxRequestBlocks()) {
						
					fCurrentWriteRequestSizeIndex++;
					
					DbgIOLog(DEBUG_MASK_DISK_INFO, ("Incremented %d\n", fCurrentWriteRequestSizeIndex));
				}
 */
				fWriteStableCount = 0;
			} else {		
				fWriteStableCount++;
			}			
		}
#if 0		
		DbgIOLog(DEBUG_MASK_DISK_INFO, ("Curr Request %d, Array[%d] %llu, Array[%d] %llu, Array[%d] %llu, Array[%d], %llu, Array[%d] %llu.\n",
										  fCurrentWriteRequestSizeIndex,
										  (fCurrentWriteRequestSizeIndex - 2 < 0 ? 0 : fCurrentWriteRequestSizeIndex - 2), fWriteSpeed[(fCurrentWriteRequestSizeIndex - 2 < 0 ? 0 : fCurrentWriteRequestSizeIndex - 2)], 
										  (fCurrentWriteRequestSizeIndex - 1 < 0 ? 0 : fCurrentWriteRequestSizeIndex - 1), fWriteSpeed[(fCurrentWriteRequestSizeIndex - 1 < 0 ? 0 : fCurrentWriteRequestSizeIndex - 1)],
										  (fCurrentWriteRequestSizeIndex), fWriteSpeed[fCurrentWriteRequestSizeIndex],
										  (fCurrentWriteRequestSizeIndex + 1 >= FLOW_NUMBER_OF_REQUEST_SIZE ? FLOW_NUMBER_OF_REQUEST_SIZE - 1 : fCurrentWriteRequestSizeIndex + 1), fWriteSpeed[(fCurrentWriteRequestSizeIndex + 1 >= FLOW_NUMBER_OF_REQUEST_SIZE ? FLOW_NUMBER_OF_REQUEST_SIZE - 1 : fCurrentWriteRequestSizeIndex + 1)],
										  (fCurrentWriteRequestSizeIndex + 2 >= FLOW_NUMBER_OF_REQUEST_SIZE ? FLOW_NUMBER_OF_REQUEST_SIZE - 1 : fCurrentWriteRequestSizeIndex + 2), fWriteSpeed[(fCurrentWriteRequestSizeIndex + 1 >= FLOW_NUMBER_OF_REQUEST_SIZE ? FLOW_NUMBER_OF_REQUEST_SIZE - 1 : fCurrentWriteRequestSizeIndex + 1)]
										  ));
#endif
	}
	
	switch(iResult) {
		case 0:
		{
			pScsiCommand->CmdData.xferCount = transferCount;
			pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_GOOD;
			pScsiCommand->CmdData.results = kIOReturnSuccess;    
		}
			break;
		case -3:
		{	
			pScsiCommand->CmdData.xferCount = 0;
			pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_BUSY;
			pScsiCommand->CmdData.results = kIOReturnBusy;    
			
			bConnectionError = true;
		}
			break;
		default:
		{
			pScsiCommand->CmdData.xferCount = transferCount;
			pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_CHECK_CONDITION;
			pScsiCommand->CmdData.results = kIOReturnSuccess;    
		}
			break;
	}
	
Out:
	
	if(map) {
		map->release();
	}
	pScsiCommand->MemoryDescriptor_ptr->release();
		
		
		if(bConnectionError && physicalDevice->IsConnected()) {
			
			DbgIOLog(DEBUG_MASK_DISK_WARNING, ("!!!!!! DISCONNECTED !!!!!\n"));
			
			//unmount(pScsiCommand->targetNo);
			cleanupDataConnection(physicalDevice, true);
		}
	
	return true;
	
}

bool com_ximeta_driver_NDASDevice::sendIDECommand(PSCSICommand pCommand, com_ximeta_driver_NDASPhysicalUnitDevice *physicalDevice)
{
	bool result = false;
		
	DbgIOLog(DEBUG_MASK_DISK_INFO, ("processSrbCommandPacket: DIRECTION %d\n", pCommand->DataTransferDirection));
	
	IOMemoryMap *map = pCommand->MemoryDescriptor_ptr->map();
	if(NULL == map) {
		
		DbgIOLog(DEBUG_MASK_DISK_ERROR, ("Can't map buffer!!!\n"));
		
		return false;
	}
/*	
	if(kSCSIDataTransfer_FromInitiatorToTarget == pCommand->DataTransferDirection) {
		IOByteCount result;
		
		result = pCommand->MemoryDescriptor_ptr->readBytes(pCommand->BufferOffset,  physicalDevice->DataBuffer(), (IOByteCount)pCommand->RequestedDataTransferCount);
		
		DbgIOLog(DEBUG_MASK_DISK_INFO, ("processSrbCommandPacket: ReadBytes %d\n", result));
	}
*/	
	int		iResult = 0;
	bool	bConnectionError = false;
	
	lsp_wrapper_context_ptr_t	lwcontext = NULL;

	lwcontext = physicalDevice->ConnectionData()->fLspWrapperContext;

	iResult = lsp_wrapper_ide_packet_cmd(
		lwcontext,
		(PCDBd)pCommand->cdb,
		(kSCSIDataTransfer_NoDataTransfer == pCommand->DataTransferDirection) ? NULL : (char *)(map->getVirtualAddress()), // physicalDevice->DataBuffer(),
		(uint32_t)pCommand->RequestedDataTransferCount,
		(kSCSIDataTransfer_FromInitiatorToTarget == pCommand->DataTransferDirection) ? TRUE : FALSE);
	
	switch(iResult) {
		case 0:
		{
			pCommand->CmdData.xferCount = pCommand->RequestedDataTransferCount;
			pCommand->CmdData.taskStatus = kSCSITaskStatus_GOOD;
			pCommand->CmdData.results = kIOReturnSuccess;
/*			
			if(kSCSIDataTransfer_FromTargetToInitiator == pCommand->DataTransferDirection) {
				IOByteCount result;
				
				result = pCommand->MemoryDescriptor_ptr->writeBytes(pCommand->BufferOffset,  physicalDevice->DataBuffer(), (IOByteCount)pCommand->RequestedDataTransferCount);
				
				DbgIOLog(DEBUG_MASK_DISK_INFO, ("processSrbCommandPacket: WriteBytes %d\n", result));
			}
*/			
			result = true;
			
		}
			break;
		case -3:
		{	
			pCommand->CmdData.xferCount = 0;
			pCommand->CmdData.taskStatus = kSCSITaskStatus_BUSY;
			pCommand->CmdData.results = kIOReturnBusy;
			
			bConnectionError = true;
			
			DbgIOLog(DEBUG_MASK_ALL, ("Packet command returned -3 for CDB[0](%d)", ((uint8_t*)pCommand->cdb)[0]));
//			PE_enter_debugger("Packet command returned -3 for CDB[0](%d)",);


		}
			break;
		default:
		{
			pCommand->CmdData.xferCount = pCommand->RequestedDataTransferCount;
			pCommand->CmdData.taskStatus = kSCSITaskStatus_CHECK_CONDITION;
			pCommand->CmdData.results = kIOReturnSuccess;    
		}
			break;
	}
	
	if(bConnectionError && physicalDevice->IsConnected()) {
		
		DbgIOLog(DEBUG_MASK_DISK_WARNING, ("processSrbCommandPacket: !!!!!! DISCONNECTED !!!!!\n"));
		
		//unmount(pScsiCommand->targetNo);
		cleanupDataConnection(physicalDevice, true);
		
	}
	
	if(map) {
		map->release();
	}
	
	return result;
}

bool com_ximeta_driver_NDASDevice::processSrbCommandPacket(PSCSICommand pScsiCommand)
{
	DbgIOLog(DEBUG_MASK_DISK_TRACE, ("processSrbCommandPacket: Entered.\n"));

	com_ximeta_driver_NDASPhysicalUnitDevice	*physicalDevice = NULL;		
	
	physicalDevice = OSDynamicCast(com_ximeta_driver_NDASPhysicalUnitDevice, pScsiCommand->device);
	if(NULL == physicalDevice) {
		
		DbgIOLog(DEBUG_MASK_DISK_ERROR, ("processSrbCommandNormal: No Physical Device!!!\n"));
		
		return false;
	}
	
#if 0
	if(NULL != pScsiCommand->MemoryDescriptor_ptr) {
		DbgIOLog(DEBUG_MASK_DISK_INFO, ("processSrbCommandPacket: Buffer Size %d\n", pScsiCommand->MemoryDescriptor_ptr->getLength()));
	}
#endif
	
	//
	// Modify CDB if nessary.
	//
	DbgIOLog(DEBUG_MASK_DISK_ERROR, ("processSrbCommandPacket: Command 0x%x. Data Len %lld Dir %d\n", pScsiCommand->cdb[0], pScsiCommand->RequestedDataTransferCount, pScsiCommand->DataTransferDirection));
						
	switch(pScsiCommand->cdb[0]) {
		case kSCSICmd_READ_CAPACITY:
		case kSCSICmd_MODE_SENSE_6:
		case kSCSICmd_MODE_SENSE_10:
		{
			pScsiCommand->DataTransferDirection = kSCSIDataTransfer_FromTargetToInitiator;
		}
			break;
		case kSCSICmd_READ_TOC_PMA_ATIP:
		{
			
			DbgIOLog(DEBUG_MASK_DISK_ERROR, ("processSrbCommandPacket: kSCSICmd_READ_TOC_PMA_ATIP 0x%x\n", pScsiCommand->cdb[2]));

			//
			// Due to track merging, TOC form 0 reports each closed session as a track.  Since Double Layer 
			// DVD+R supports at most 127 sessions, TOC form 0 may have at most 127 track descriptors.  
			// Thus, the maximum size of returned data for TOC form 0 is 1 020 (i.e. 4 + 8*127). 
			//
			int temp = 4 + 8 * 127;
			
			if(pScsiCommand->RequestedDataTransferCount > temp) {
				
				pScsiCommand->RequestedDataTransferCount = temp;
				
				pScsiCommand->cdb[8] = (uint8_t)(temp) & 0xff;
				pScsiCommand->cdb[7] = (uint8_t)(temp) >> 8;
			}
		}
			break;
			
		case kSCSICmd_GET_CONFIGURATION:
		{
			int temp = 4; // + 8 * 300;
			
			UInt16 allocation = (UInt16)(pScsiCommand->cdb[7] << 8 
										 | pScsiCommand->cdb[8] );
			
			DbgIOLog(DEBUG_MASK_DISK_ERROR, ("processSrbCommandPacket: kSCSICmd_GET_CONFIGURATION allocation %d, 0x%x\n", allocation, pScsiCommand->cdb[3]));
			
			if(pScsiCommand->RequestedDataTransferCount > 4096) {
				pScsiCommand->RequestedDataTransferCount = temp;
				
				pScsiCommand->cdb[8] = (uint8_t)(temp) & 0xff;
				pScsiCommand->cdb[7] = (uint8_t)(temp) >> 8;
			}
		}
			break;
		case kSCSICmd_READ_10:
		case kSCSICmd_WRITE_10:
		case kSCSICmd_WRITE_AND_VERIFY_10:
		{
			
			uint32_t	ulAddr;
			UInt16	usBlocks;
			uint32_t	ulBlockSize;
			PCDBd	pcdb = ((PCDBd)pScsiCommand->cdb);
			
			ulAddr = pcdb->CDB10.LogicalBlockByte3
				| pcdb->CDB10.LogicalBlockByte2 << 8
				| pcdb->CDB10.LogicalBlockByte1 << 16 
				| pcdb->CDB10.LogicalBlockByte0 << 24;
			
			usBlocks = (UInt16)(pcdb->CDB10.TransferBlocksMsb << 8 
								| pcdb->CDB10.TransferBlocksLsb );
			
			ulBlockSize = pScsiCommand->RequestedDataTransferCount / usBlocks;
			
			if(pScsiCommand->RequestedDataTransferCount > (uint32_t)(MaxRequestBytes()) ) {
								
				DbgIOLog(DEBUG_MASK_DISK_ERROR, ("processSrbCommandPacket: Too Big Command!!!!!!!! 0x%x Req = %lld, Blocks %d\n", pScsiCommand->cdb[0], pScsiCommand->RequestedDataTransferCount, usBlocks));
/*
				SCSICommand	tempCommand;
				UInt16		ulRemainedBlocks = usBlocks;

				memcpy(&tempCommand, pScsiCommand, sizeof(SCSICommand));
				
				while(ulRemainedBlocks) {
					
					usBlocks = min((MaxRequestBlocks() * SECTOR_SIZE) / ulBlockSize, ulRemainedBlocks);

					((PCDBd)&tempCommand.cdb)->CDB10.TransferBlocksMsb = (UInt8)(usBlocks >> 8);
					((PCDBd)&tempCommand.cdb)->CDB10.TransferBlocksLsb = (UInt8)usBlocks;
					
					tempCommand.RequestedDataTransferCount = usBlocks * ulBlockSize;
					
					DbgIOLog(DEBUG_MASK_DISK_WARNING, ("processSrbCommandPacket: Remained %d, Blocks %d, dataCount %lld\n", ulRemainedBlocks, usBlocks, tempCommand.RequestedDataTransferCount));

					if (false == sendIDECommand(&tempCommand, physicalDevice)) {
						
						pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_BUSY;
						pScsiCommand->CmdData.results = kIOReturnBusy;    

						return true;
					} 
				
					ulAddr += usBlocks;
					
					((PCDBd)&tempCommand.cdb)->CDB10.LogicalBlockByte3 = (UInt8)ulAddr; 
					((PCDBd)&tempCommand.cdb)->CDB10.LogicalBlockByte2 = (UInt8)(ulAddr >> 8);
					((PCDBd)&tempCommand.cdb)->CDB10.LogicalBlockByte1 = (UInt8)(ulAddr >> 16);
					((PCDBd)&tempCommand.cdb)->CDB10.LogicalBlockByte0 = (UInt8)(ulAddr >> 24);
					
					tempCommand.BufferOffset += usBlocks * ulBlockSize;
					
					ulRemainedBlocks -= usBlocks;
				}	
*/				
				
				pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_BUSY;
				pScsiCommand->CmdData.results = kIOReturnBusy;    
				
				return true;
				
			}
#if 0			
		
			if(usBlocks > fTargetInfos[pScsiCommand->targetNo].fTargetDataSessions.MaxRequestBlocks * BLOCK_SIZE / 2352) {
			
				usBlocks = fTargetInfos[pScsiCommand->targetNo].fTargetDataSessions.MaxRequestBlocks * BLOCK_SIZE / 2352;
				
				cdb.CDB10.TransferBlocksMsb = (UInt8)(usBlocks >> 8);
				cdb.CDB10.TransferBlocksLsb = (UInt8)usBlocks;
				
				pScsiCommand->RequestedDataTransferCount = usBlocks * 2352;
				
				DbgIOLog(DEBUG_MASK_DISK_WARNING, ("processSrbCommandPacket: Modifyed Blocks %d\n", usBlocks));
				
			}

			if(pScsiCommand->RequestedDataTransferCount < (uint32_t)( )) {
				
				DbgIOLog(DEBUG_MASK_DISK_INFO, ("processSrbCommandPacket: Data %d, ul %d, Addr %d\n",
												   pScsiCommand->RequestedDataTransferCount, usBlocks, ulAddr));
				
				usBlocks = (UInt16)(pScsiCommand->RequestedDataTransferCount / pScsiCommand->BlockSize);
				//ulAddr += pCcb->ulBlockOffset;
				
				cdb.CDB10.LogicalBlockByte3 = (UInt8)ulAddr; 
				cdb.CDB10.LogicalBlockByte2 = (UInt8)(ulAddr >> 8);
				cdb.CDB10.LogicalBlockByte1 = (UInt8)(ulAddr >> 16);
				cdb.CDB10.LogicalBlockByte0 = (UInt8)(ulAddr >> 24);
				
				cdb.CDB10.TransferBlocksMsb = (UInt8)(usBlocks >> 8);
				cdb.CDB10.TransferBlocksLsb = (UInt8)usBlocks; 
				
				ulAddr = (cdb.CDB10.LogicalBlockByte3
						  | cdb.CDB10.LogicalBlockByte2 << 8
						  | cdb.CDB10.LogicalBlockByte1 << 16 
						  | cdb.CDB10.LogicalBlockByte0 << 24);
				
				usBlocks = (UInt16)(cdb.CDB10.TransferBlocksMsb << 8 
									| cdb.CDB10.TransferBlocksLsb );
				
				DbgIOLog(DEBUG_MASK_DISK_INFO, ("processSrbCommandPacket: Data %d, ul %d, Off %d Addr %d, blk %d\n",
												   pCcb->DataTransferLength, usBlocks, pCcb->ulBlockOffset, ulAddr, usBlocks));
				
			} else {
				
				pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_BUSY;
				pScsiCommand->CmdData.results = kIOReturnBusy;    

				return false;
			}
#endif
		}
			break;
		case kSCSICmd_READ_12:
		case kSCSICmd_WRITE_12:
		{
			if(pScsiCommand->RequestedDataTransferCount > (uint32_t)(MaxRequestBytes()) ) {
				
				DbgIOLog(DEBUG_MASK_DISK_ERROR, ("processSrbCommandPacket: Too Big Command!!!!!!!! 0x%x Req = %lld\n", pScsiCommand->cdb[0], pScsiCommand->RequestedDataTransferCount));
				
				pScsiCommand->CmdData.taskStatus = kSCSITaskStatus_CHECK_CONDITION;
				pScsiCommand->CmdData.results = kIOReturnBadArgument;  
				
				return true;
			}
			
#if 0
			uint32_t	ulAddr;
			uint32_t	ulBlocks;
			
			ulAddr = cdb.CDB12.LogicalBlock[3]
				| cdb.CDB12.LogicalBlock[2] << 8
				| cdb.CDB12.LogicalBlock[1] << 16 
				| cdb.CDB12.LogicalBlock[0] << 24;
			
			ulBlocks = cdb.CDB12.TransferLength[3]
				| cdb.CDB12.TransferLength[2] << 8
				| cdb.CDB12.TransferLength[1] << 16 
				| cdb.CDB12.TransferLength[0] << 24;
			
			if(pScsiCommand->RequestedDataTransferCount < (uint32_t)(ulBlocks * pScsiCommand->BlockSize)) {
/*				
				DbgIOLog(DEBUG_MASK_DISK_INFO, ("processSrbCommandPacket: Data %d, ul %d, Off %d Addr %d\n",
												   pCcb->DataTransferLength, ulBlocks, pCcb->ulBlockOffset, ulAddr));
*/				
				ulBlocks = (UInt16)(pScsiCommand->RequestedDataTransferCount / pScsiCommand->BlockSize);
//				ulAddr += pCcb->ulBlockOffset;
				
				cdb.CDB12.LogicalBlock[3] = (UInt8)ulAddr; 
				cdb.CDB12.LogicalBlock[2] = (UInt8)(ulAddr >> 8);
				cdb.CDB12.LogicalBlock[1] = (UInt8)(ulAddr >> 16);
				cdb.CDB12.LogicalBlock[0] = (UInt8)(ulAddr >> 24);
				
				cdb.CDB12.TransferLength[3] = (UInt8)ulBlocks; 
				cdb.CDB12.TransferLength[2] = (UInt8)(ulBlocks >> 8);
				cdb.CDB12.TransferLength[1] = (UInt8)(ulBlocks >> 16);
				cdb.CDB12.TransferLength[0] = (UInt8)(ulBlocks >> 24);

				ulAddr = (cdb.CDB12.LogicalBlock[3]
						  | cdb.CDB12.LogicalBlock[2] << 8
						  | cdb.CDB12.LogicalBlock[1] << 16 
						  | cdb.CDB12.LogicalBlock[0] << 24);
				
				ulBlocks = (cdb.CDB12.TransferLength[3]
							| cdb.CDB12.TransferLength[2] << 8
							| cdb.CDB12.TransferLength[1] << 16 
							| cdb.CDB12.TransferLength[0] << 24);
				
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("processSrbCommandPacket: Data %d, ul %d, Off %d Addr %d\n",
												   pCcb->DataTransferLength, ulBlocks, pCcb->ulBlockOffset, ulAddr));
			}
#endif

		}
			break;
		case kSCSICmd_READ_CD:
		{
			debugLevel = 0x00F00000;
			
			uint32_t	ulBlocks = 0;

			ulBlocks = (uint32_t)(pScsiCommand->cdb[6] << 16
								| pScsiCommand->cdb[7] << 8
								| pScsiCommand->cdb[8] );
			
			DbgIOLog(DEBUG_MASK_DISK_ERROR, ("processSrbCommandPacket: READ_CD  Blocks %d\n", ulBlocks));
			
			/*
			uint32_t	ulBlocks = 0;
			uint32_t	ulBlockSize;
			
			switch(cdb.READ_CD.ExpectedSectorType) {
				case 1:
					ulBlockSize = 2352;
					break;
				case 2:
					ulBlockSize = 2048;
					break;
				case 3:
					ulBlockSize = 2336;
					break;
				case 4:
					ulBlockSize = 2048;
					break;
				case 5:
					ulBlockSize = 2328;
					break;
				default:
					ulBlockSize = 2048;
			}
			
			
			if(pScsiCommand->RequestedDataTransferCount > ulBlocks * ulBlockSize) {
				pScsiCommand->RequestedDataTransferCount = ulBlocks * ulBlockSize;
			}
			 */
		}
			break;
		default:
			break;
	}
	
	sendIDECommand(pScsiCommand, physicalDevice);
		
	return true;
}

bool	com_ximeta_driver_NDASDevice::processManagingIOCommand(PManagingIOCommand pManagingIOCommand)
{
	int						error;
	bool					bResult = true;
	TARGET_DATA				targetData;
	com_ximeta_driver_NDASPhysicalUnitDevice	*physicalDevice;
	bool										needLogin = TRUE;
	bool					bConnectionError = false;
	
	lsp_wrapper_context_ptr_t	lwcontext = NULL;
	bool	writable = 0;
	
	DbgIOLog(DEBUG_MASK_LS_TRACE, ("processManagingIOCommand: Entered.\n"));	
	
	physicalDevice = OSDynamicCast(com_ximeta_driver_NDASPhysicalUnitDevice, pManagingIOCommand->device);
	if(NULL == physicalDevice) {
		DbgIOLog(DEBUG_MASK_LS_ERROR, ("processManagingIOCommand: No Physical Device.\n"));
		
		return false;		
	}
			
	memcpy(&targetData, physicalDevice->TargetData(), sizeof(TARGET_DATA));
	
	switch(pManagingIOCommand->command) {
		case kNDASManagingIOReadSectors:
		{
			if ( physicalDevice->IsConnected() ) {
				needLogin = FALSE;
				lwcontext = physicalDevice->ConnectionData()->fLspWrapperContext;
			} else {

				needLogin = TRUE;
				writable = 0;
			}
		}
			break;
		case kNDASManagingIOWriteSectors:
		{
			if ( physicalDevice->IsConnected() 
				 && kNDASUnitStatusConnectedRW == physicalDevice->Status()) {
				needLogin = FALSE;
				lwcontext = physicalDevice->ConnectionData()->fLspWrapperContext;
			} else {

				needLogin = TRUE;
				writable = 1;
			}
		}
			break;
		default:
			break;
	}
	
	DbgIOLog(DEBUG_MASK_LS_TRACE, ("IOCommand : needLogin(%d), pManagingIOCommand->command(%d)\n",
		needLogin, pManagingIOCommand->command));
		
	if (needLogin) {
		
		bConnectionError = true;
		
		lwcontext = (lsp_wrapper_context_ptr_t)IOMalloc(lsp_wrapper_context_size());
		if(NULL == lwcontext)
		{
			DbgIOLog(DEBUG_MASK_LS_ERROR, ("[%s] IOMalloc for session buffer Failed.\n", __FUNCTION__));
			goto Out;
		}		

		error = lsp_wrapper_connect(lwcontext, fAddress, fInterfaceAddress, NORMAL_DATACONNECTION_TIMEOUT);
		if(error) {
			DbgIOLog(DEBUG_MASK_LS_ERROR, ("[%s] lsp_wrapper_connect Failed(%d)\n", __FUNCTION__, error));
			goto Out;
		}

		error = lsp_wrapper_login(lwcontext, userPassword(), 0, physicalDevice->unitNumber(), writable);
		if(error) {
			DbgIOLog(DEBUG_MASK_LS_ERROR, ("[%s] lsp_wrapper_login Failed(%d)\n", __FUNCTION__, error));
			goto Out;
		}

		bConnectionError = false;
	}
	
	if (physicalDevice->TargetData()->bPresent) {
		
		uint32_t	remainedSectors = pManagingIOCommand->sectors;
		uint32_t	offset = 0;
		
		while (0 < remainedSectors) {
		
			uint32_t	actualSectors;
			
			actualSectors = (remainedSectors > (MaxRequestBytes() / physicalDevice->blockSize())) ? (MaxRequestBytes() / physicalDevice->blockSize()) : remainedSectors;
			
			switch(pManagingIOCommand->command) {
				case kNDASManagingIOReadSectors:
				{
					error = lsp_wrapper_ide_read(
						lwcontext,
						pManagingIOCommand->Location + offset,
						actualSectors,
						(IOVirtualAddress)&pManagingIOCommand->buffer[offset * physicalDevice->blockSize()], //physicalDevice->DataBuffer(),
						actualSectors * physicalDevice->blockSize());
				}
					break;
				case kNDASManagingIOWriteSectors:
				{
//					memcpy(physicalDevice->DataBuffer(), &pManagingIOCommand->buffer[offset * SECTOR_SIZE], actualSectors * SECTOR_SIZE);

					error = lsp_wrapper_ide_write(
						lwcontext,
						pManagingIOCommand->Location + offset,
						actualSectors,
						&pManagingIOCommand->buffer[offset * physicalDevice->blockSize()], //physicalDevice->DataBuffer(),
						actualSectors * physicalDevice->blockSize());
				}
					break;
			}
			
			if(error)
			{
				DbgIOLog(DEBUG_MASK_DISK_WARNING, ("processManagingIOCommand: lsp_wrapper_ide_read/write Error! %d\n", error));
				
				bResult = false;
				
				if (-3 == error) {					
					bConnectionError = true;
				}
				break;
			} 
/*			
			switch(pManagingIOCommand->command) {
				case kNDASManagingIOReadSectors:
				{
					memcpy(&pManagingIOCommand->buffer[offset * SECTOR_SIZE], physicalDevice->DataBuffer(), actualSectors * SECTOR_SIZE);
				}
					break;
			}
*/			
			remainedSectors -= actualSectors;
			offset += actualSectors;
		}
		
	} else {
		DbgIOLog(DEBUG_MASK_LS_ERROR, ("processManagingIOCommand: Not Present.\n"));

		bResult = false;
	}

Out:
		
	if (needLogin) {
		if(lwcontext)
		{
			// Logout
			error = lsp_wrapper_logout(lwcontext, false);
			DbgIOLog(DEBUG_MASK_LS_TRACE, ("%d = lsp_wrapper_logout()\n", error));
			
			error = lsp_wrapper_disconnect(lwcontext);
			DbgIOLog(DEBUG_MASK_LS_TRACE, ("%d = lsp_wrapper_disconnect()\n", error));

			IOFree(lwcontext, lsp_wrapper_context_size());
			lwcontext = NULL;			
		}
	} else {
		
		if(bConnectionError && physicalDevice->IsConnected()) {
			
			DbgIOLog(DEBUG_MASK_DISK_WARNING, ("processManagingIOCommand: !!!!!! DISCONNECTED !!!!!\n"));
			
			//unmount(pScsiCommand->targetNo);
			cleanupDataConnection(physicalDevice, true);
			
		}

	}
	
	physicalDevice->updateStatus();
	
	return bResult;
}
