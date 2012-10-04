/***************************************************************************
 *
 *  NDASProtocolTransport.h
 *
 *    LanScsiController Class and data structure definitions 
 *
 *    Copyright (c) 2003 XiMeta, Inc. All rights reserved.
 *
 **************************************************************************/

#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOService.h>
#include <IOKit/IOSyncer.h>
#include <IOKit/IOCommandPool.h>

#include <IOKit/scsi/IOSCSIProtocolServices.h>

#include "NDASCommand.h"

#define USE_ELG   0                 // for debugging
#define kEvLogSize  (4096*16)       // 16 pages = 64K = 4096 events

#if USE_ELG /* (( */
#define ELG(A,B,ASCI,STRING)    EvLog( (UInt32)(A), (UInt32)(B), (UInt32)(ASCI), STRING )
#define PAUSE(A,B,ASCI,STRING)  Pause( (UInt32)(A), (UInt32)(B), (UInt32)(ASCI), STRING )
#else /* ) not USE_ELG: (   */
#define ELG(A,B,ASCI,S)
#define PAUSE(A,B,ASCI,STRING)  IOLog( "LanScsiController: %8x %8x " STRING "\n", (unsigned int)(A), (unsigned int)(B) )
#endif /* USE_ELG )) */

#define require_nonzero( obj, exceptionLabel ) \
	require( ( 0 != obj ), exceptionLabel )

class com_ximeta_driver_NDASProtocolTransportNub;
class com_ximeta_driver_NDASCommand;

enum {
	kNDASProtocolTransportRequestSendCommand,
	kNDASProtocolTransportRequestCompleteCommand,
	kNDASNumberOfPrptocolTransportRequest
};

class com_ximeta_driver_NDASProtocolTransport : public IOSCSIProtocolServices
{
    OSDeclareDefaultStructors( com_ximeta_driver_NDASProtocolTransport )   /* Constructor & Destructor stuff   */

private:
	
	com_ximeta_driver_NDASProtocolTransportNub	*fProvider;									

	bool										fIsSleepMode;
/*	
	bool					bTerminating;
	
	IOService				* TerminationProvider;
	IOOptionBits			TerminationOptions;
*/	
		
protected:
    
	virtual IOReturn
	message ( UInt32 type, IOService * provider, void * argument = 0 );

	/*!
		@function SendSCSICommand
		@abstract Prepare and send a SCSI command to the device.
		@discussion	The incoming SCSITaskIdentifier gets turned into a IOFireWireSBP2ORB
		and is submitted to the SBP2 layer. See IOSCSIProtocolServices.h for more details
		regarding SendSCSICommand. Also see IOFireWireSBP2Lib.h for details regarding the
		IOFireWireSBP2ORB structure and the submitORB method.
		@result If the command was sent to the device and is pending completion, the
		subclass should return true and return back the kSCSIServiceResponse_Request_In_Process response.
		If the command completes immediately with an error, the subclass will return true
		and return back the appropriate status. If the subclass is currently processing all the
		commands it can, the subclass will return false and the command will be resent next time
		CommandCompleted is called.
	*/

	virtual bool 
	SendSCSICommand (	SCSITaskIdentifier 		request,
						SCSIServiceResponse *	serviceResponse,
						SCSITaskStatus *		taskStatus );


	/*!
		@function AbortSCSICommand
		@abstract This method is intended to abort an in progress SCSI Task.
		@discussion	Currently not implemented in super class. This is a stub method for adding
		the abort command in the near future.
		@result See SCSITask.h for SCSIServiceResponse codes.
	*/
	
	virtual SCSIServiceResponse 
	AbortSCSICommand ( SCSITaskIdentifier request );

	// The IsProtocolServiceSupported method will return true if the protocol
	// layer supports the specified feature.
	virtual bool IsProtocolServiceSupported ( SCSIProtocolFeature feature, void * serviceValue );
	
	// The HandleProtocolServiceFeature method will return true if the protocol
	// layer properly handled the specified feature.
	virtual bool HandleProtocolServiceFeature ( SCSIProtocolFeature feature, void * serviceValue );
			
	virtual IOReturn	HandlePowerOff ( void );
	
	virtual IOReturn	HandlePowerOn ( void );
	
public:

//	struct lanScsiAdapter   *fLanScsiAdapter;
	
	/*!
		@function init
		@abstract See IOService for discussion.
		@discussion	Setup and prime class into known state.
	*/
	
	bool init ( OSDictionary * propTable );

	/*! 
		@function start
		@discussion See IOService for discussion.
		@result Return true if the start was successful, false otherwise ( which will
		cause the instance to be detached and usually freed ).
	*/

	virtual bool start ( IOService * provider );
		
	/*!
		@function cleanUp
		@abstract cleanUp is called to tear down IOFireWireSerialBusProtocolTransport.
		@discussion	cleanUp is called when we receive a kIOFWMessageServiceIsRequestingClose
		message or if we fail our initialization.
	*/
	
	virtual void cleanUp ( void );
	
	/*!
		@function finalize
		@abstract See IOService for discussion.
		@result Returns true.
	*/
	
	virtual bool finalize ( IOOptionBits options );

	/*!
		@function stop
		@abstract See IOService for discussion.
		@result none.
	 */

	virtual void stop( IOService * provider );

	/*! 
		@function free
		@discussion See IOService for discussion.
		@result none.
	*/
	
	virtual void free ( void );
	
	/*!
		@function CompleteSCSITask
	 @abstract This qualifies and sets appropriate data then calls CommandCompleted.
	 @discussion	See IOSCSIProtocolServices.h for more details
	 regarding CommandCompleted.
	 */
	
	virtual void CompleteSCSITask (
							  com_ximeta_driver_NDASCommand  *NDCmd_ptr
							  );
	
/*
	virtual bool willTerminate (
								IOService * provider,
								IOOptionBits options
								);
	
	virtual bool didTerminate (
							   IOService * provider,
							   IOOptionBits options,
							   bool * defer
							   );
	
*/
	/*!
		@function CriticalCommandSubmission
	 @abstract xxx.
	 @discussion	xxx.
	 @result none.
	 */
	
	void CriticalCommandSubmission (
							   com_ximeta_driver_NDASCommand	*NDCmd_ptr
							   );
	
	/*!
		@function CriticalOrbSubmission
	 @abstract xxx.
	 @discussion	xxx.
	 @result none.
	 */
	void
		CriticalComplete (
						  com_ximeta_driver_NDASCommand	*NDCmd_ptr
						  );
	
	void completeCommand(com_ximeta_driver_NDASCommand *command);
		
private:
	
    OSMetaClassDeclareReservedUnused ( com_ximeta_driver_NDASProtocolTransport, 1 );
    OSMetaClassDeclareReservedUnused ( com_ximeta_driver_NDASProtocolTransport, 2 );
    OSMetaClassDeclareReservedUnused ( com_ximeta_driver_NDASProtocolTransport, 3 );
	OSMetaClassDeclareReservedUnused ( com_ximeta_driver_NDASProtocolTransport, 4 );
	OSMetaClassDeclareReservedUnused ( com_ximeta_driver_NDASProtocolTransport, 5 );
	OSMetaClassDeclareReservedUnused ( com_ximeta_driver_NDASProtocolTransport, 6 );
	OSMetaClassDeclareReservedUnused ( com_ximeta_driver_NDASProtocolTransport, 7 );
	OSMetaClassDeclareReservedUnused ( com_ximeta_driver_NDASProtocolTransport, 8 );
	OSMetaClassDeclareReservedUnused ( com_ximeta_driver_NDASProtocolTransport, 9 );
	OSMetaClassDeclareReservedUnused ( com_ximeta_driver_NDASProtocolTransport, 10 );
	OSMetaClassDeclareReservedUnused ( com_ximeta_driver_NDASProtocolTransport, 11 );
	OSMetaClassDeclareReservedUnused ( com_ximeta_driver_NDASProtocolTransport, 12 );
	OSMetaClassDeclareReservedUnused ( com_ximeta_driver_NDASProtocolTransport, 13 );
	OSMetaClassDeclareReservedUnused ( com_ximeta_driver_NDASProtocolTransport, 14 );
	OSMetaClassDeclareReservedUnused ( com_ximeta_driver_NDASProtocolTransport, 15 );
	OSMetaClassDeclareReservedUnused ( com_ximeta_driver_NDASProtocolTransport, 16 );
	
};

IOReturn
CriticalCommandSubmissionStatic ( 
								  OSObject *	refCon, 
								  void *		val1,
								  void *		val2,
								  void *		val3,
								  void *		val4 
								  );
IOReturn
CriticalCompleteStatic ( 
						 OSObject * refCon, 
						 void * val1,
						 void * val2,
						 void * val3,
						 void * val4 );	
