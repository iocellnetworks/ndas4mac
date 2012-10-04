/*
 *  NDASChangeSettingCommand.h
 *  NDASFamily
 *
 *  Created by Jung-gyun Ahn on 09. 07. 07.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef _NDAS_CHANGE_SETTING_COMMAND_H_
#define _NDAS_CHANGE_SETTING_COMMAND_H_

#include "LanScsiProtocol.h"
#include "NDASCommand.h"

typedef struct _ChangeSettingCommand {
	
	uint32_t			fInterfaceArrayIndex;
	char			fAddress[6];
	uint64_t			fLinkSpeed;
	bool			fInterfaceChanged;		// For RAID device it means that device is presented.
	
} ChangeSettingCommand, *PChangeSettingCommand;

#define NDAS_CHANGE_SETTING_COMMAND_CLASS	"com_ximeta_driver_NDASChangeSettingCommand"

class com_ximeta_driver_NDASChangeSettingCommand : public com_ximeta_driver_NDASCommand {
	
	OSDeclareDefaultStructors(com_ximeta_driver_NDASChangeSettingCommand)
	
private:
	
	ChangeSettingCommand					fChangeSettingCommand;
	
public:
	
	virtual bool init();
	virtual void free(void);	
	
	void setCommand(uint32_t interfaceArrayIndex, char *address, uint64_t linkSpeed, bool interfaceChanged);
	PChangeSettingCommand changeSettingCommand();	
	
};

#endif