/*
 *  NDASManagingIOCommand.h
 *  NDASFamily
 *
 *  Created by Jung-gyun Ahn on 09. 07. 07.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef _NDAS_MANAGING_IO_COMMAND_H_
#define _NDAS_MANAGING_IO_COMMAND_H_

#include "LanScsiProtocol.h"
#include "NDASCommand.h"

enum {
	kNDASManagingIOReadSectors,
	kNDASManagingIOWriteSectors,
	kNDASNumberOfManagingIOs,
};

typedef struct _ManagingIOCommand {
	
	com_ximeta_driver_NDASUnitDevice	*device;
	
	uint32_t					command;
	
	uint64_t					Location;
	uint32_t					sectors;
	
	char					*buffer;
	
	bool					IsSuccess;
	
} ManagingIOCommand, *PManagingIOCommand;

#define NDAS_MANAGING_IO_COMMAND_CLASS	"com_ximeta_driver_NDASManagingIOCommand"

class com_ximeta_driver_NDASManagingIOCommand : public com_ximeta_driver_NDASCommand {
	
	OSDeclareDefaultStructors(com_ximeta_driver_NDASManagingIOCommand)
	
private:

	ManagingIOCommand			fManagingIOCommand;

public:
	
	virtual bool init();
	virtual void free(void);	

	void setCommand(PManagingIOCommand command);
	PManagingIOCommand managingIOCommand();

};

#endif