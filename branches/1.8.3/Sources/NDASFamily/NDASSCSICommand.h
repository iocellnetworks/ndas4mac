/*
 *  NDASSCSICommand.h
 *  NDASFamily
 *
 *  Created by Jung-gyun Ahn on 09. 07. 07.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef _NDAS_SCSI_COMMAND_H_
#define _NDAS_SCSI_COMMAND_H_

#include "LanScsiProtocol.h"
#include "NDASCommand.h"

typedef struct PrivCmdData
	{
		uint32_t					xferCount;
		IOReturn				results;
		SCSIServiceResponse 	serviceResponse;
		SCSITaskStatus			taskStatus;
	} PrivCmdData;

typedef struct _SCSICommand {
	
	com_ximeta_driver_NDASUnitDevice	*device;
	
	SCSITaskIdentifier			scsiTask;
	
	uint32_t						targetNo;
	
	SInt8						LogicalUnitNumber;
	
	SCSICommandDescriptorBlock  cdb;
	UInt8						cdbSize;
	
	// Buffer and Data Transfer size.
	IOMemoryDescriptor		*MemoryDescriptor_ptr;
	uint64_t					BufferOffset;
	uint64_t					RequestedDataTransferCount;
	UInt8					DataTransferDirection;
	
	//	uint32_t					BlockSize;
	PrivCmdData				CmdData;
	
	bool					completeWhenFailed;
	
	uint32_t					SubUnitIndex;
	
} SCSICommand, *PSCSICommand;

#define NDAS_SCSI_COMMAND_CLASS	"com_ximeta_driver_NDASSCSICommand"

class com_ximeta_driver_NDASSCSICommand : public com_ximeta_driver_NDASCommand {
	
	OSDeclareDefaultStructors(com_ximeta_driver_NDASSCSICommand)
	
private:

	SCSICommand					fScsiCommand;

public:
	
	virtual bool init();
	virtual void free(void);	
	
	void setCommand(PSCSICommand command);
	PSCSICommand scsiCommand();
	
	void	convertSRBToSRBCompletedCommand();
	void	convertSRBCompletedToSRBCommand();

};

#endif