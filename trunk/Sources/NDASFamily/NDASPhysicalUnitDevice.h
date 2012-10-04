/*
 *  NDASPhysicalUnitDevice.h
 *  NDASFamily
 *
 *  Created by Junggyun Ahn on 05. 11. 29.
 *  Copyright 2005 XIMETA, Inc. All rights reserved.
 *
 */

#include <IOKit/IOService.h>

#define NDAS_PHYSICAL_UNIT_DEVICE_CLASS	"com_ximeta_driver_NDASPhysicalUnitDevice"

#include "NDASUnitDevice.h"

#include "LanScsiProtocol.h"
#include "lsp_wrapper.h"

class  com_ximeta_driver_NDASCommand;
class  com_ximeta_driver_NDASDevice;

typedef struct _TargetInfo {
	
	struct LanScsiSession			fTargetDataSessions;
	lsp_wrapper_context_ptr_t		fLspWrapperContext;
	int								fTargetSenseKey;
	UInt8							fTargetASC;
	UInt8							fTargetASCQ;
	
	com_ximeta_driver_NDASCommand*	fTargetPendingCommand;

} TargetInfo, *PTargetInfo;

class com_ximeta_driver_NDASPhysicalUnitDevice : public com_ximeta_driver_NDASUnitDevice
{
	OSDeclareDefaultStructors(com_ximeta_driver_NDASPhysicalUnitDevice)
	
private:
	
	UInt32	fSlotNumber;
	
	UInt32	fUnitNumber;
		
	// IDE Infos.
	TARGET_DATA	fTargetData;
	
	// Connection Infos.
	TargetInfo	fTargetInfo;
	
	bool	fNeedRefresh;
	
	char	*fDataBuffer;
	
	int		fDataBufferSize;
	
	com_ximeta_driver_NDASDevice	*fDevice;
	
	UInt32	fDisconnectedTime;
	
	UInt32	fStartTimeOfConnecting;
	
	bool	fIsPresent;

public:

	void	setDevice(com_ximeta_driver_NDASDevice * device);
	void	setSlotNumber(UInt32 slotNumber);
	UInt32	slotNumber();

	void	setUnitNumber(UInt32 unitNumber);
	UInt32	unitNumber();
		
	void	setTargetData(TARGET_DATA *Target);
	TARGET_DATA	*TargetData();
	
	TargetInfo *ConnectionData();
	
	bool	IsNeedRefresh();
	void	setNeedRefresh(bool refresh);
	
	bool	createDataBuffer(int bufferSize);
	char	*DataBuffer();
	
	void setPendingCommand(com_ximeta_driver_NDASCommand* command);
	com_ximeta_driver_NDASCommand* PendingCommand();
	
	void setIsPresent(bool present);
	bool IsPresent();
	
	// IOService
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
	
	// NDASUnitDevice
	virtual UInt32 Type();
	
	virtual bool	IsWritable();
	
	virtual	bool readSectorsOnce(UInt64 location, UInt32 sectors, char *buffer);
	virtual bool writeSectorsOnce(UInt64 location, UInt32 sectors, char *buffer);
	virtual	bool processSRBCommand(com_ximeta_driver_NDASCommand *command);
	virtual void completeCommand(com_ximeta_driver_NDASCommand *command);
	virtual UInt32	maxRequestSectors();
	virtual UInt32	updateStatus();	
	virtual UInt32	blocksize();
	virtual bool	sendNDASFamilyIOMessage(UInt32 type, void * argument = 0, vm_size_t argSize = 0 );
};
