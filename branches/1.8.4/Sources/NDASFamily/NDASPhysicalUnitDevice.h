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
class  com_ximeta_driver_NDASSCSICommand;
class  com_ximeta_driver_NDASDevice;
class com_ximeta_driver_NDASPhysicalUnitDeviceController;

typedef struct _TargetInfo {
	
	struct LanScsiSession			fTargetDataSessions;
	lsp_wrapper_context_ptr_t		fLspWrapperContext;
	int								fTargetSenseKey;
	UInt8							fTargetASC;
	UInt8							fTargetASCQ;
	
	com_ximeta_driver_NDASSCSICommand*	fTargetPendingCommand;

} TargetInfo, *PTargetInfo;

class com_ximeta_driver_NDASPhysicalUnitDevice : public com_ximeta_driver_NDASUnitDevice
{
	OSDeclareDefaultStructors(com_ximeta_driver_NDASPhysicalUnitDevice)
	
private:
	
	int32_t		fSlotNumber;
	
	uint32_t	fUnitNumber;
		
	// IDE Infos.
	TARGET_DATA	fTargetData;
	
	// Connection Infos.
	TargetInfo	fTargetInfo;
	
	bool	fNeedRefresh;
	
	char	*fDataBuffer;
	
	int		fDataBufferSize;
			
	uint32_t	fDisconnectedTime;
	
	uint32_t	fStartTimeOfConnecting;
	
	bool	fIsPresent;
	
	bool	fWritable;
	
	uint32_t	fMaxRequestBytes;

public:

	void	setSlotNumber(int32_t slotNumber);
	int32_t	slotNumber();

	void	setUnitNumber(uint32_t unitNumber);
	uint32_t	unitNumber();
		
	void	setTargetData(TARGET_DATA *Target);
	TARGET_DATA	*TargetData();
	
	TargetInfo *ConnectionData();
	
	bool	IsNeedRefresh();
	void	setNeedRefresh(bool refresh);
	
	bool	createDataBuffer(int bufferSize);
	char	*DataBuffer();
	
	void setPendingCommand(com_ximeta_driver_NDASSCSICommand* command);
	com_ximeta_driver_NDASSCSICommand* PendingCommand();
	
	void setIsPresent(bool present);
	bool IsPresent();
	
	void setMaxRequestBytes(uint32_t MRB);
	uint32_t maxRequestBytes();
	
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
	
	void setWritable(bool writable);
	
	// NDASUnitDevice
	virtual uint32_t Type();
	
	virtual bool	IsWritable();
	
	virtual	bool readSectorsOnce(uint64_t location, uint32_t sectors, char *buffer);
	virtual bool writeSectorsOnce(uint64_t location, uint32_t sectors, char *buffer);
	virtual	bool processSRBCommand(com_ximeta_driver_NDASSCSICommand *command);
	virtual void completeCommand(com_ximeta_driver_NDASSCSICommand *command);
	virtual uint32_t	maxRequestSectors();
	virtual uint32_t	updateStatus();	
	virtual bool	sendNDASFamilyIOMessage(UInt32 type, void * argument = 0, vm_size_t argSize = 0 );
};
