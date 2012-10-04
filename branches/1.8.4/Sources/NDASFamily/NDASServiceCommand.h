/*
 *  NDASServiceCommand.h
 *  NDASFamily
 *
 *  Created by Jung-gyun Ahn on 09. 07. 07.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef _NDAS_SERVICE_COMMAND_H_
#define _NDAS_SERVICE_COMMAND_H_

#include "LanScsiProtocol.h"
#include "NDASCommand.h"

#define NDAS_SERVICE_COMMAND_CLASS	"com_ximeta_driver_NDASServiceCommand"

enum {
	kNDASServiceCommandChangeUserPassword,
	kNDASNumberOfServiceCommands
};

typedef struct _ServiceCommand {
	
	uint32_t	serviceType;
	char		password[8];
	uint8_t		vid;
	bool		bWritable;
	
} ServiceCommand, *PServiceCommand;

class com_ximeta_driver_NDASServiceCommand : public com_ximeta_driver_NDASCommand {
	
	OSDeclareDefaultStructors(com_ximeta_driver_NDASServiceCommand)
	
private:
	
	ServiceCommand	fServiceCommand;
		
public:
	
	virtual bool init();
	virtual void free(void);	
		
	void setCommand(uint32_t serviceType, char *password, uint8_t vid, bool bWritable);
	PServiceCommand serviceCommand();

};

#endif