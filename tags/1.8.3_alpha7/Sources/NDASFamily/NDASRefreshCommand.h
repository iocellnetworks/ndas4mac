/*
 *  NDASRefreshCommand.h
 *  NDASFamily
 *
 *  Created by Jung-gyun Ahn on 09. 07. 07.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef _NDAS_REFRESH_COMMAND_H_
#define _NDAS_REFRESH_COMMAND_H_

#include "LanScsiProtocol.h"
#include "NDASCommand.h"

typedef struct _RefreshCommand {
	
	uint32_t	fTargetNumber;
	
} RefreshCommand, *PRefreshCommand;

#define NDAS_REFRESH_COMMAND_CLASS	"com_ximeta_driver_NDASRefreshCommand"

class com_ximeta_driver_NDASRefreshCommand : public com_ximeta_driver_NDASCommand {
	
	OSDeclareDefaultStructors(com_ximeta_driver_NDASRefreshCommand)
	
private:
	
	RefreshCommand				fRefreshCommand;

public:
	
	virtual bool init();
	virtual void free(void);	
	
	void setCommand(uint32_t TargetNumber);
	PRefreshCommand refreshCommand();

};

#endif