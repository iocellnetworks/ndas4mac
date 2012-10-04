/***************************************************************************
*
*   NDASProtocolTransportNub.h
*
*    NDAS Logical Device
*
*    Copyright (c) 2004 XiMeta, Inc. All rights reserved.
*
**************************************************************************/

#include <IOKit/IOService.h>

#define NDAS_PROTOCOL_TRANSPORT_NUB_CLASS	"com_ximeta_driver_NDASProtocolTransportNub"

class com_ximeta_driver_NDASLogicalDevice;
class com_ximeta_driver_NDASSCSICommand;
class com_ximeta_driver_NDASProtocolTransport;

typedef struct _SCSICommand *PSCSICommand;

class com_ximeta_driver_NDASProtocolTransportNub : public IOService
{
	OSDeclareDefaultStructors(com_ximeta_driver_NDASProtocolTransportNub)
	
	com_ximeta_driver_NDASSCSICommand	*fCommand;
	
	uint32_t								fMaxRequestSectors;
	uint32_t								fBlockSize;
		
public:
						
	OSString *bsdName();
	
	bool	isBusy();
	bool	isWorkable();
	
	bool	sendCommand(com_ximeta_driver_NDASSCSICommand *command);
	void	completeCommand(com_ximeta_driver_NDASSCSICommand *command);
		
	void	clearCommand();
	
	virtual bool init(OSDictionary *dictionary = 0);
	virtual void free(void);
	virtual IOService *probe(IOService *provider, SInt32 *score);
	virtual bool start(IOService *provider);
	virtual void stop(IOService *provider);
	virtual IOReturn message( 
							  UInt32	mess, 
							  IOService	* provider,
							  void		* argument 
							  );
	
	virtual bool willTerminate(
							   IOService * provider,
							   IOOptionBits options ); 
	
	// setter
	void setMaxRequestSectors(uint32_t sectors);
	void setBlockSize(uint32_t size);
	
	// getter
	uint32_t	maxRequestSectors();
	uint32_t	blocksize();
		
};