/*
 *  NDASPnpCommand.h
 *  NDASFamily
 *
 *  Created by Jung-gyun Ahn on 09. 07. 07.
 *  Copyright 2009 XIMETA, Inc. All rights reserved.
 *
 */

#ifndef _NDAS_PNP_COMMAND_H_
#define _NDAS_PNP_COMMAND_H_

#include "LanScsiProtocol.h"
#include "NDASCommand.h"

typedef struct _PNPCommand {
	
	uint32_t			fReceivedTime;	
	PNP_MESSAGE		fMessage;	
	uint32_t			fInterfaceArrayIndex;
	char			fAddress[6];
	
} PNPCommand, *PPNPCommand;

#define NDAS_PNP_COMMAND_CLASS	"com_ximeta_driver_NDASPnpCommand"

class com_ximeta_driver_NDASPnpCommand : public com_ximeta_driver_NDASCommand {
	
	OSDeclareDefaultStructors(com_ximeta_driver_NDASPnpCommand)
	
private:
	
	PNPCommand					fPnpCommand;
	
public:
	
	virtual bool init();
	virtual void free(void);
	void setCommand(uint32_t interfaceArrayIndex, char *address, uint32_t receivedTime, PPNP_MESSAGE message);
	PPNPCommand pnpCommand();				
};

#endif