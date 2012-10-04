/***************************************************************************
 *
 *  NDASProtocolTransport.cpp
 *
 *    
 *
 *    Copyright (c) 2003 XiMeta, Inc. All rights reserved.
 *
 **************************************************************************/

//-----------------------------------------------------------------------------
//	Includes
//-----------------------------------------------------------------------------

#include "NDASProtocolTransport.h"

#include "NDASProtocolTransportNub.h"

//#include "NDASDeviceController.h"

#include "Utilities.h"

#include "DebugPrint.h"

//-----------------------------------------------------------------------------
//	Macros
//-----------------------------------------------------------------------------

#define DEBUG 										1
#define DEBUG_ASSERT_COMPONENT_NAME_STRING			"NDAS Controller"

#include "NDASProtocolTransportDebugging.h"

#define HTONLL(Data)    ( (((Data)&0x00000000000000FF) << 56) | (((Data)&0x000000000000FF00) << 40) \
                          | (((Data)&0x0000000000FF0000) << 24) | (((Data)&0x00000000FF000000) << 8)  \
                          | (((Data)&0x000000FF00000000) >> 8)  | (((Data)&0x0000FF0000000000) >> 24) \
                          | (((Data)&0x00FF000000000000) >> 40) | (((Data)&0xFF00000000000000) >> 56))

#define NTOHLL(Data)    ( (((Data)&0x00000000000000FF) << 56) | (((Data)&0x000000000000FF00) << 40) \
                          | (((Data)&0x0000000000FF0000) << 24) | (((Data)&0x00000000FF000000) << 8)  \
                          | (((Data)&0x000000FF00000000) >> 8)  | (((Data)&0x0000FF0000000000) >> 24) \
                          | (((Data)&0x00FF000000000000) >> 40) | (((Data)&0xFF00000000000000) >> 56))

#define super   IOSCSIProtocolServices
OSDefineMetaClassAndStructors( com_ximeta_driver_NDASProtocolTransport, IOSCSIProtocolServices );

//-----------------------------------------------------------------------------
//	Constants
//-----------------------------------------------------------------------------

#define NR_MAX_TARGET           2
#define MAX_DATA_BUFFER_SIZE    64 * 1024 // 128 * 1024
#define MAX_TRANSFER_SIZE       (64 * 1024) //(128 * 1024)
#define MAX_TRANSFER_BLOCKS      MAX_TRANSFER_SIZE / BLOCK_SIZE

#define SEC         (LONGLONG)(1000)
#define TIME_OUT    (SEC * 30)                  // 5 min.

//-----------------------------------------------------------------------------
//	LanScsiControllerDebugAssert				   [STATIC]
//-----------------------------------------------------------------------------

#if !DEBUG_ASSERT_PRODUCTION_CODE

void
IONDASProtocolTransportDebugAssert (
								const char * 	componentNameString,
								const char * 	assertionString, 
								const char * 	exceptionLabelString,
								const char * 	errorString,
								const char * 	fileName,
								long 			lineNumber,
								int 			errorCode 
								)
{
	
	IOLog ( "%s Assert failed: %s ", componentNameString, assertionString );
	
	if ( exceptionLabelString != NULL ) { IOLog ( "%s ", exceptionLabelString ); }
	
	if ( errorString != NULL ) { IOLog ( "%s ", errorString ); }
	
	if ( fileName != NULL ) { IOLog ( "file: %s ", fileName ); }
	
	if ( lineNumber != 0 ) { IOLog ( "line: %ld ", lineNumber ); }
	
	if ( ( long ) errorCode != 0 ) { IOLog ( "error: %ld ( 0x%08lx )", ( long ) errorCode, ( long ) errorCode  ); }
	
	IOLog ( "\n" );
	
}

#endif

#pragma mark -
#pragma mark == Public Methods ==
#pragma mark -

//-----------------------------------------------------------------------------
//	¥ init - Called by IOKit to initialize us.						   [PUBLIC]
//-----------------------------------------------------------------------------

bool 
com_ximeta_driver_NDASProtocolTransport::init(
						OSDictionary * propTable
						)
{
	DebugPrint(3, false, "LanScsiController:init Entered.\n");
	
	if ( super::init ( propTable ) == false )
	{
		return false;
	}
	
	fIsSleepMode = false;
	
	return true;
	
}

//-----------------------------------------------------------------------------
//	¥ start - Called by IOKit to start our services.				   [PUBLIC]
//-----------------------------------------------------------------------------

bool 
com_ximeta_driver_NDASProtocolTransport::start(
						 IOService * provider 
						 )
{
	
	IOReturn				status;
	Boolean					returnValue;
	Boolean 				openSucceeded;
	OSNumber * 				rto;
	OSNumber * 				wto;
	OSDictionary *			dict;
	bool					sucess;
	
	status				= kIOReturnSuccess;
	returnValue			= false;
	openSucceeded		= false;
	rto					= NULL;
	wto					= NULL;
	sucess				= false;
	dict				= NULL;
	
	DebugPrint(3, false, "%s:start Entered.\n", getName());
		
	// See if there is a read time out duration passed in the property dictionary - if not
	// set the default to 30 seconds.
	/*
	rto = OSDynamicCast ( OSNumber, getProperty ( kIOPropertyReadTimeOutDurationKey ) );
	if ( rto == NULL )
	{
		rto = OSNumber::withNumber ( kDefaultTimeOutValue, 32 );
		require ( rto, exit );
		
		sucess = setProperty ( kIOPropertyReadTimeOutDurationKey, rto );
		check ( sucess );
		rto->release();
		
		rto = OSDynamicCast ( OSNumber, getProperty ( kIOPropertyReadTimeOutDurationKey ) );
	}
	
	STATUS_LOG ( ( "%s: start read time out = %ld\n", getName (), rto->unsigned32BitValue ( ) ) );
	
	// See if there is a write time out duration passed in the property dictionary - if not
	// set the default to 30 seconds.
	
	wto = OSDynamicCast ( OSNumber,  getProperty ( kIOPropertyWriteTimeOutDurationKey ) );
	if ( wto == NULL )
	{
		wto = OSNumber::withNumber ( kDefaultTimeOutValue, 32 );
		require ( wto, exit );
		
		sucess = setProperty ( kIOPropertyWriteTimeOutDurationKey, wto );
		check ( sucess );
		wto->release();
		
		wto = OSDynamicCast ( OSNumber, getProperty ( kIOPropertyWriteTimeOutDurationKey ) );
	}
	
	STATUS_LOG ( ( "%s: start read time out = %ld\n", getName (), wto->unsigned32BitValue ( ) ) );
	*/
	
	fProvider = OSDynamicCast ( com_ximeta_driver_NDASProtocolTransportNub, provider );
	require ( fProvider, exit );
	//
	// Add a retain here so we can keep LanScsiDevice from doing garbage
	// collection on us when we are in the middle of our finalize method.
	
	fProvider->retain ( );
	
	openSucceeded = super::start ( provider );
	require ( openSucceeded, exit );
	
	openSucceeded = provider->open ( this );
	require ( openSucceeded, exit );
		
//	bTerminating = false;
	
	/*
	fUnit = fSBPTarget->getFireWireUnit ();
	require ( fUnit, exit );
	
	// Explicitly set the "enable retry on ack d" flag.
	
	fUnit->setNodeFlags ( kIOFWEnableRetryOnAckD );
	
	status = AllocateResources ();
	require_noerr ( status, exit );
	
	// Get us on the workloop so we can sleep the start thread.
	
	fCommandGate->runAction ( ConnectToDeviceStatic );
	
	if ( reserved->fLoginState == kLogginSucceededState )
	{
		registerService ();
	}
	
	STATUS_LOG ( ( "%s: start complete\n", getName () ) );
	
	returnValue = true;
	
	// Copy some values to the Protocol Characteristics Dictionary
	dict = OSDynamicCast ( OSDictionary, getProperty ( kIOPropertyProtocolCharacteristicsKey ) );
	if ( dict != NULL )
	{
		
		OSString *	string = NULL;
		
		string = OSString::withCString ( kFireWireGUIDKey );
		if ( string != NULL )
		{
			
			dict->setObject ( string, getProperty ( kFireWireGUIDKey, gIOServicePlane ) );
			string->release ( );
			
		}
		
		string = OSString::withCString ( kPreferredNameKey );
		if ( string != NULL )
		{
			
			dict->setObject ( kPreferredNameKey, getProperty ( kFireWireVendorNameKey, gIOServicePlane ) );
			string->release ( );
			
		}
		
	}
	*/

	fProvider->setCommandGate(fCommandGate);
	fProvider->setTransport(this);
	
	registerService ();
	
	InitializePowerManagement ( provider );
	
	returnValue = true;
	
exit:
		
		if ( returnValue == false )
		{
			
			DebugPrint(1, false, "%s: start failed.  status = %x\n", getName (), status);
			
			// Call the cleanUp method to clean up any allocated resources.
			cleanUp ();
		}
	
	return returnValue;
	
}

//-----------------------------------------------------------------------------
//	¥ cleanUp - Called to deallocate any resources.					   [PUBLIC]
//-----------------------------------------------------------------------------

void 
com_ximeta_driver_NDASProtocolTransport::cleanUp ( void )
{
	
    DbgIOLog(DEBUG_MASK_DISK_TRACE, ("cleanUp called\n"));
	
	if ( fProvider != NULL )
	{
		
		if ( fProvider->isOpen ( this ) )
		{
			fProvider->setTransport(NULL);
			
			fProvider->setCommandGate(NULL);
			
			fProvider->close ( this );
		}
	}
	
}

//-----------------------------------------------------------------------------
// ¥ finalize - Terminates all power management.					   [PUBLIC]
//-----------------------------------------------------------------------------

bool
com_ximeta_driver_NDASProtocolTransport::finalize (
							 IOOptionBits options 
							 )
{
	
    DbgIOLog(DEBUG_MASK_DISK_TRACE, ("finalize\n"));
	
//	DeallocateResources ();
	
	// Release the retain we took to keep IOFireWireSBP2LUN from doing garbage
	// collection on us when we are in the middle of DeallocateResources.
/*	
	if ( fProvider )
	{
		
		fProvider->setTransport(NULL);
		
		fProvider->setCommandGate(NULL);
		
		fProvider->release ( );
		
		fProvider = NULL;
	}
*/	
	return super::finalize ( options );
	
}

//-----------------------------------------------------------------------------
// ¥ stop - Called by IOKit to stot our services.					   [PUBLIC]
//-----------------------------------------------------------------------------

void 
com_ximeta_driver_NDASProtocolTransport::stop(
						IOService * provider
						)
{
    DbgIOLog(DEBUG_MASK_DISK_TRACE, ("stop\n"));
		
	if ( fProvider )
	{
				
		fProvider->release ( );
		
		fProvider = NULL;
	}
	
	super::stop(provider);
	
    return;
}

//-----------------------------------------------------------------------------
//	¥ free - Called to deallocate ExpansionData.					   [PUBLIC]
//-----------------------------------------------------------------------------

void com_ximeta_driver_NDASProtocolTransport::free ( void )
{
	DebugPrint(3, false, "%s:free Entered.\n", getName());
/*	
	if ( reserved != NULL )
	{
		
		if ( reserved->fCommandPool != NULL )
		{
			
			reserved->fCommandPool->release ();
			reserved->fCommandPool = NULL;
			
		}
		
		IOFree ( reserved, sizeof ( ExpansionData ) );
		reserved = NULL;
		
	}
*/	
	super::free ( );
	
}
/*
bool 
com_ximeta_driver_NDASProtocolTransport::willTerminate (
			   IOService * provider,
			   IOOptionBits options
			   )
{
	DebugPrint(1, false, "%s:willTerminate Entered.\n", getName());
	
	bTerminating = true;
	
	return true;
	
}

bool 
com_ximeta_driver_NDASProtocolTransport::didTerminate (
			  IOService * provider,
			  IOOptionBits options,
			  bool * defer
			  )
{
	DebugPrint(1, false, "%s:didTerminate Entered.\n", getName());

	// Wait last Operation complete.
	if( NULL != fLanScsiAdapter->Cmd_ptr ) {
	
		DebugPrint(1, false, "%s:didTerminate Op not completed.\n", getName());
		
		*defer = true;
		
		TerminationProvider = provider;
		TerminationOptions = options;
		
	} else {
		
		DebugPrint(1, false, "%s:didTerminate No peding Op.\n", getName());

		*defer = false;
	}
	
	return true;
}
*/

void com_ximeta_driver_NDASProtocolTransport::completeCommand(com_ximeta_driver_NDASCommand *command)
{
	fCommandGate->runAction(CriticalCompleteStatic, command);
}

#pragma mark -
#pragma mark == Protected Methods ==
#pragma mark -

//-----------------------------------------------------------------------------
//	¥ message - Called by IOKit to deliver messages.				[PROTECTED]
//-----------------------------------------------------------------------------

IOReturn
com_ximeta_driver_NDASProtocolTransport::message (
							UInt32 		type,
							IOService *	nub,
							void * 		arg 
							)
{
	DebugPrint(3, false, "%s:message Entered.\n", getName());
	
	IOReturn				status		= kIOReturnSuccess;

	switch ( type )
	{
		
		case kIOMessageServiceIsSuspended:
			DbgIOLog(DEBUG_MASK_DISK_INFO, ("kIOMessageServiceIsSuspended\n"));
			// bus reset started - set flag to stop submitting orbs.
			//fIsSleepMode = true;
			
			break;
			
		case kIOMessageServiceIsResumed:
			DbgIOLog(DEBUG_MASK_DISK_INFO, ("kIOMessageServiceIsResumed\n"));
			// bus reset finished - if we have failed to log in previously, try again.
			//fIsSleepMode = false;
			
			break;
/*			
		case kIOMessageFWSBP2ReconnectComplete:
			
			// As of this writing FireWireSBP2LUN will message all multi-LUN instances with this
			// message. So we qualify this message with our instance variable fLogin and ignore others.
			
			if( ( ( FWSBP2ReconnectParams* ) arg )->login == fLogin )
			{
				STATUS_LOG ( ( "%s: kIOMessageFWSBP2ReconnectComplete\n", getName () ) );
				
				fLoggedIn = true;
				
				clientData = ( SBP2ClientOrbData * ) fORB->getRefCon ();
				if ( clientData != NULL )
				{
					
					if ( ( clientData->scsiTask != NULL ) && ( fReconnectCount < kMaxReconnectCount ) )
					{
						
						STATUS_LOG ( ( "%s: resubmit orb \n", getName () ) );
						fReconnectCount++;
						fLogin->submitORB ( fORB );
						
					}
					
					else
					{
						// We are unable to recover from bus reset storm
						// We have exhausted the fReconnectCount - punt...
						
						clientData->serviceResponse	= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
						clientData->taskStatus		= kSCSITaskStatus_No_Status;
						CompleteSCSITask ( fORB );
					}
				}
			}
			
			break;
			
		case kIOMessageFWSBP2ReconnectFailed:
			
			// As of this writing FireWireSBP2LUN will message all multi-LUN instances with this
			// message. So we qualify this message with our instance variable fLogin and ignore others.
			
			if( ( ( FWSBP2ReconnectParams* ) arg )->login == fLogin )
			{
				STATUS_LOG ( ( "%s: kIOMessageFWSBP2ReconnectFailed\n", getName () ) );
				
				// Try to reestablish log in.
				fLoginRetryCount = 0;
				login ();
			}
			break;
		*/	
		case kIOMessageServiceIsRequestingClose:
			
			DbgIOLog(DEBUG_MASK_DISK_INFO, ("kIOMessageServiceIsRequestingClose\n"));
			
			// tell our super to message it's clients that the device is gone
			SendNotification_DeviceRemoved ();
/*			
			if ( fLanScsiAdapter->Cmd_ptr != NULL )
			{
				// We are unable to recover from bus reset storm
				
				fLanScsiAdapter->Cmd_ptr->CmdData.serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
				fLanScsiAdapter->Cmd_ptr->CmdData.taskStatus = kSCSITaskStatus_No_Status;
				CompleteSCSITask ( fLanScsiAdapter->Cmd_ptr );
				
			}
*/		
				cleanUp();

				break;
			
		case kIOMessageServiceIsTerminated:
			DbgIOLog(DEBUG_MASK_DISK_INFO, ("kIOMessageServiceIsTerminated\n"));
			
			break;
			
		default:

			status = IOService::message (type, nub, arg);
			break;
			
	}
	
	return status;
	
}


//-----------------------------------------------------------------------------
//	¥ SendSCSICommand - Converts a SCSITask to an ORB.				[PROTECTED]
//-----------------------------------------------------------------------------

bool 
com_ximeta_driver_NDASProtocolTransport::SendSCSICommand (
														  SCSITaskIdentifier 	request,
														  SCSIServiceResponse * serviceResponse,
														  SCSITaskStatus * 		taskStatus 
														  )
{	
	UInt8							commandLength		= 0;
	bool							commandProcessed	= true;
	com_ximeta_driver_NDASCommand	*NDCmd_ptr			= NULL;
	SCSICommand						scsiCommand;
	
	DbgIOLog(DEBUG_MASK_DISK_TRACE, ("SendSCSICommand: Entered. Request 0x%x\n", request));
	
	*serviceResponse	= kSCSIServiceResponse_Request_In_Process;
	*taskStatus			= kSCSITaskStatus_No_Status;
	
	commandLength = GetCommandDescriptorBlockSize ( request );
	
#if 0
	SCSICommandDescriptorBlock		cdb					= { 0 };
	
	DbgIOLog(DEBUG_MASK_DISK_WARNING, ("SendSCSICommand: Data Transfer Request %lld\n", GetRequestedDataTransferCount( request )));
	
	GetCommandDescriptorBlock ( request, &cdb );
	
	if ( commandLength == kSCSICDBSize_6Byte )
	{
		
		DbgIOLog(DEBUG_MASK_DISK_WARNING, ("cdb = %02x:%02x:%02x:%02x:%02x:%02x\n", cdb[0], cdb[1],
										  cdb[2], cdb[3], cdb[4], cdb[5] ));
		
	}
	
	else if ( commandLength == kSCSICDBSize_10Byte )
	{
		
		DbgIOLog(DEBUG_MASK_DISK_WARNING, ("cdb = %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n", cdb[0],
										  cdb[1], cdb[2], cdb[3], cdb[4], cdb[5], cdb[6], cdb[7], cdb[8],
										  cdb[9] ));
		
	}
	
	else if ( commandLength == kSCSICDBSize_12Byte )
	{
		
		DbgIOLog(DEBUG_MASK_DISK_WARNING, ("cdb = %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n", cdb[0],
										  cdb[1], cdb[2], cdb[3], cdb[4], cdb[5], cdb[6], cdb[7], cdb[8],
										  cdb[9], cdb[10], cdb[11] ));
		
	}
#endif
	
	if ( isInactive ())
	{
		DbgIOLog(DEBUG_MASK_DISK_WARNING, ("SendSCSICommand: Device is inactive.\n"));
		
		// device is disconnected - we can not service command request
		*serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
		commandProcessed = false;
		
		goto exit;
	}
	
	// Check Workable.
	if (!fProvider 
		|| !fProvider->isWorkable()) {
		
		DbgIOLog(DEBUG_MASK_DISK_WARNING, ("SendSCSICommand: Device is not workable.\n"));

		// device is disconnected - we can not service command request
//		*serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
		commandProcessed = false;
		
		goto exit;		
	}
	
	// Check Busy.
	if (fProvider->isBusy() )
	{
		DbgIOLog(DEBUG_MASK_DISK_INFO, ("SendSCSICommand: Busy!!!\n"));
		
		commandProcessed = false;
		goto exit;
	}
	
	// Allocate Command.
	NDCmd_ptr = OSDynamicCast(com_ximeta_driver_NDASCommand, 
							  OSMetaClass::allocClassWithName(NDAS_COMMAND_CLASS));
    
    if(NDCmd_ptr == NULL || !NDCmd_ptr->init()) {
        DbgIOLog(DEBUG_MASK_DISK_ERROR, ("SendSCSICommand: failed to alloc command class\n"));
		
		if (NDCmd_ptr) {
			NDCmd_ptr->release();
		}
		
		*serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
		commandProcessed = true;
		goto exit;
    }
	
	memset(&scsiCommand, 0, sizeof(SCSICommand));
	
	scsiCommand.scsiTask = request;
//	scsiCommand.targetNo = fProvider->targetNo();
	scsiCommand.LogicalUnitNumber = GetLogicalUnitNumber( request );
	GetCommandDescriptorBlock ( request, &scsiCommand.cdb );
	scsiCommand.cdbSize = commandLength;
	scsiCommand.MemoryDescriptor_ptr = GetDataBuffer( request );
	scsiCommand.BufferOffset = GetDataBufferOffset( request );
	scsiCommand.RequestedDataTransferCount = GetRequestedDataTransferCount( request );
	scsiCommand.DataTransferDirection = GetDataTransferDirection( request );
//	scsiCommand.BlockSize = fProvider->blocksize();
		
	NDCmd_ptr->setCommand(&scsiCommand);
	
	request->retain();
	
	//	CriticalCommandSubmission(NDCmd_ptr, &commandProcessed);
	
	fCommandGate->runAction ( CriticalCommandSubmissionStatic, NDCmd_ptr);
	
	
	//fXiCommandGate->runCommand((void *)kNDASProtocolTransportRequestSendCommand, NDCmd_ptr, &commandProcessed);
	
exit:
		
		if ( kSCSIServiceResponse_Request_In_Process != *serviceResponse 
			 && NULL != NDCmd_ptr ) {
			
			DbgIOLog(DEBUG_MASK_DISK_WARNING, ("SendSCSICommand: ERROR Service Response = %x, task = 0x%x\n", *serviceResponse, request));
			
			request->release();
			
			NDCmd_ptr->release();
		}
	
	DbgIOLog(DEBUG_MASK_DISK_INFO,("SendSCSICommand: exit, Service Response = %x, task = 0x%x\n", *serviceResponse, request));
	
	return commandProcessed;
	
}

//-----------------------------------------------------------------------------
//	¥ CompleteSCSITask - Complets a task.							[PROTECTED]
//-----------------------------------------------------------------------------

void
com_ximeta_driver_NDASProtocolTransport::CompleteSCSITask (
														   com_ximeta_driver_NDASCommand  *NDCmd_ptr
														   )
{
	SCSITaskIdentifier		scsiTask		= NULL;
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskStatus			taskStatus		= kSCSITaskStatus_No_Status;
	IOByteCount				bytesTransfered = 0;
	PSCSICommand			scsiCommand_Ptr = NULL;
	
	DbgIOLog(DEBUG_MASK_DISK_TRACE, ("CompleteSCSITask: Entered. 0x%x\n", NDCmd_ptr));
	
	scsiCommand_Ptr = NDCmd_ptr->scsiCommand();
	
	if ( scsiCommand_Ptr->CmdData.taskStatus  == kSCSITaskStatus_GOOD )
	{
		
		bytesTransfered = scsiCommand_Ptr->CmdData.xferCount;
	}
	
	DbgIOLog(DEBUG_MASK_DISK_INFO, ("CompleteSCSITask: task 0x%x xferCount %u result 0x%x\n", scsiCommand_Ptr->scsiTask, bytesTransfered, scsiCommand_Ptr->CmdData.results));
	
	SetRealizedDataTransferCount ( scsiCommand_Ptr->scsiTask, bytesTransfered );
	
	// re-entrancy protection
	scsiTask				= scsiCommand_Ptr->scsiTask;
	
	if(scsiCommand_Ptr->CmdData.results == kIOReturnSuccess) {
		serviceResponse	= kSCSIServiceResponse_TASK_COMPLETE;
	} else {
		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	}
	
	taskStatus				= scsiCommand_Ptr->CmdData.taskStatus;
	scsiCommand_Ptr->scsiTask	= NULL;
	
	//	fProvider->clearCommand();
		
	CommandCompleted ( scsiTask, serviceResponse, taskStatus );
	
	scsiTask->release();

	DbgIOLog(DEBUG_MASK_DISK_INFO, ("CompleteSCSITask: End task 0x%x task status 0x%x\n", scsiTask, taskStatus));
	
	NDCmd_ptr->release();
	
	/*	
		if ( true == bTerminating ) {
			
			DebugPrint(1, false, "%s: CompleteSCSITask call didTerminate\n", getName () );
			
			bool	defer = false;
			
			IOService::didTerminate(TerminationProvider, TerminationOptions, &defer);
		}
	 */
	return;
}

//-----------------------------------------------------------------------------
//	¥ AbortSCSICommand - Aborts an outstanding I/O.					[PROTECTED]
//-----------------------------------------------------------------------------

SCSIServiceResponse
com_ximeta_driver_NDASProtocolTransport::AbortSCSICommand (
									 SCSITaskIdentifier request 
									 )
{
	DEBUG_UNUSED ( request );
	
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_FUNCTION_REJECTED;

	DbgIOLog(DEBUG_MASK_DISK_WARNING, ("AbortSCSICommand: Entered.\n"));
	
	return serviceResponse;
	
}

//-----------------------------------------------------------------------------
//	¥ IsProtocolServiceSupported -	Checks for valid protocol services
//									supported by this device.		[PROTECTED]
//-----------------------------------------------------------------------------

bool
com_ximeta_driver_NDASProtocolTransport::IsProtocolServiceSupported (
											   SCSIProtocolFeature	feature,
											   void *				serviceValue 
											   )
{
	
	bool	isSupported = false;
	
	DbgIOLog(DEBUG_MASK_DISK_TRACE, ("IsProtocolServiceSuppoerted: Entered. feature %d\n", feature));
	
	switch ( feature )
	{
		/*
		case kSCSIProtocolFeature_MaximumReadBlockTransferCount:
		case kSCSIProtocolFeature_MaximumWriteBlockTransferCount:
		{
			
			*(( UInt32 * ) serviceValue) = fProvider->maxRequestSectors();
			isSupported = true;
		}	
			break;
		*/
		case kSCSIProtocolFeature_MaximumReadTransferByteCount:
		case kSCSIProtocolFeature_MaximumWriteTransferByteCount:
		{	
			*(( UInt64 * )serviceValue) = fProvider->maxRequestSectors() * SECTOR_SIZE;
			
			isSupported = true;			
		}	
			break;
					
		default:
			break;
			
	}
	
	return isSupported;
	
}


//-----------------------------------------------------------------------------
//	¥ HandleProtocolServiceFeature - Handles protocol service features.
//																	[PROTECTED]
//-----------------------------------------------------------------------------

bool
com_ximeta_driver_NDASProtocolTransport::HandleProtocolServiceFeature (
												 SCSIProtocolFeature	feature, 
												 void *				serviceValue 
												 )
{
	
	DEBUG_UNUSED ( feature );
	DEBUG_UNUSED ( serviceValue );
	
	DbgIOLog(DEBUG_MASK_DISK_TRACE, ("HandleProtocolServiceFeature: Entered. feature %d\n", feature));
	
	return false;
	
}

//-----------------------------------------------------------------------------
//	¥ CriticalCommandSubmissionStatic - C->C++ glue.	
//-----------------------------------------------------------------------------

IOReturn
CriticalCommandSubmissionStatic ( 
						 OSObject *	refCon, 
						 void *		val1,
						 void *		val2,
						 void *		val3,
						 void *		val4 
						 )
{
	DEBUG_UNUSED ( val2 );
	DEBUG_UNUSED ( val3 );
	DEBUG_UNUSED ( val4 );
	
	( ( com_ximeta_driver_NDASProtocolTransport * ) refCon )->CriticalCommandSubmission ( (com_ximeta_driver_NDASCommand *) val1 );
	
	return kIOReturnSuccess;
	
}

//-----------------------------------------------------------------------------
//	¥ CriticalCommandSubmission - submitORB on workloop.			[PROTECTED]
//-----------------------------------------------------------------------------

void
com_ximeta_driver_NDASProtocolTransport::CriticalCommandSubmission (
																	com_ximeta_driver_NDASCommand	*NDCmd_ptr
																	)
{
	
	DbgIOLog(DEBUG_MASK_DISK_TRACE, ("CriticalCommandSubmission: Entered. 0x%x\n", NDCmd_ptr));
	
	fProvider->sendCommand(NDCmd_ptr);
				
	return;
	
}

//-----------------------------------------------------------------------------
//	¥ CriticalCompleteStatic - C->C++ glue.			 		        [PROTECTED]
//-----------------------------------------------------------------------------

IOReturn
CriticalCompleteStatic ( 
						 OSObject *	refCon, 
						 void *		val1,
						 void *		val2,
						 void *		val3,
						 void *		val4 
						 )
{
	DEBUG_UNUSED ( val2 );
	DEBUG_UNUSED ( val3 );
	DEBUG_UNUSED ( val4 );
	
	( ( com_ximeta_driver_NDASProtocolTransport * ) refCon )->CriticalComplete ( ( com_ximeta_driver_NDASCommand *) val1 );
	
	return kIOReturnSuccess;
	
}

//-----------------------------------------------------------------------------
//	¥ CriticalComplete - submitORB on workloop.				        [PROTECTED]
//-----------------------------------------------------------------------------

void
com_ximeta_driver_NDASProtocolTransport::CriticalComplete (
														   com_ximeta_driver_NDASCommand	*NDCmd_ptr
														   )
{
	DbgIOLog(DEBUG_MASK_DISK_TRACE, ("CriticalComplete: Entered. 0x%x\n", NDCmd_ptr));

	CompleteSCSITask(NDCmd_ptr);

	return;
}

IOReturn
com_ximeta_driver_NDASProtocolTransport::HandlePowerOff() {
	
	DbgIOLog(DEBUG_MASK_POWER_TRACE, ("HandlePowerOff: Entered.\n"));
	
	fIsSleepMode = true;
	
	return IOPMAckImplied;
}

IOReturn
com_ximeta_driver_NDASProtocolTransport::HandlePowerOn() {
	
	DbgIOLog(DEBUG_MASK_POWER_TRACE, ("HandlePowerOn: Entered.\n"));
	
	fIsSleepMode = false;
	
	return IOPMAckImplied;
}


//-----------------------------------------------------------------------------

#pragma mark -
#pragma mark == VTable Padding ==
#pragma mark -

// binary compatibility reserved method space
OSMetaClassDefineReservedUnused ( com_ximeta_driver_NDASProtocolTransport,  1 );
OSMetaClassDefineReservedUnused ( com_ximeta_driver_NDASProtocolTransport,  2 );
OSMetaClassDefineReservedUnused ( com_ximeta_driver_NDASProtocolTransport,  3 );
OSMetaClassDefineReservedUnused ( com_ximeta_driver_NDASProtocolTransport,  4 );
OSMetaClassDefineReservedUnused ( com_ximeta_driver_NDASProtocolTransport,  5 );
OSMetaClassDefineReservedUnused ( com_ximeta_driver_NDASProtocolTransport,  6 );
OSMetaClassDefineReservedUnused ( com_ximeta_driver_NDASProtocolTransport,  7 );
OSMetaClassDefineReservedUnused ( com_ximeta_driver_NDASProtocolTransport,  8 );
OSMetaClassDefineReservedUnused ( com_ximeta_driver_NDASProtocolTransport,  9 );
OSMetaClassDefineReservedUnused ( com_ximeta_driver_NDASProtocolTransport, 10 );
OSMetaClassDefineReservedUnused ( com_ximeta_driver_NDASProtocolTransport, 11 );
OSMetaClassDefineReservedUnused ( com_ximeta_driver_NDASProtocolTransport, 12 );
OSMetaClassDefineReservedUnused ( com_ximeta_driver_NDASProtocolTransport, 13 );
OSMetaClassDefineReservedUnused ( com_ximeta_driver_NDASProtocolTransport, 14 );
OSMetaClassDefineReservedUnused ( com_ximeta_driver_NDASProtocolTransport, 15 );
OSMetaClassDefineReservedUnused ( com_ximeta_driver_NDASProtocolTransport, 16 );

//-----------------------------------------------------------------------------
