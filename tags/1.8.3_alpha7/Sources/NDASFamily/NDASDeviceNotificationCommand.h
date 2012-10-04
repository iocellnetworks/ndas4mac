/*
 *  NDASDeviceNotificationCommand.h
 *  NDASFamily
 *
 *  Created by Jung-gyun Ahn on 09. 07. 07.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef _NDAS_DEVICE_NOTIFICATION_COMMAND_H_
#define _NDAS_DEVICE_NOTIFICATION_COMMAND_H_

#include "LanScsiProtocol.h"
#include "NDASCommand.h"

typedef enum {
	kNDASDeviceNotificationPresent,
	kNDASDeviceNotificationNotPresent,
	kNDASDeviceNotificationCheckBind,
//	kNDASDeviceNotificationDisconnect,
	kNDASNumberOfDeviceNotifications
} kNDASDeviceNotificationType;

#define NDAS_DEVICE_NOTIFICATION_COMMAND_CLASS	"com_ximeta_driver_NDASDeviceNotificationCommand"

class com_ximeta_driver_NDASDeviceNotificationCommand : public com_ximeta_driver_NDASCommand {
	
	OSDeclareDefaultStructors(com_ximeta_driver_NDASDeviceNotificationCommand)
	
private:

	kNDASDeviceNotificationType			fNotificationType;
	com_ximeta_driver_NDASUnitDevice	*fDevice;

public:
	
	virtual bool init();
	virtual void free(void);	

	// Setter.
	void setCommand(kNDASDeviceNotificationType notificationType, com_ximeta_driver_NDASUnitDevice *device);

	// Getter.
	kNDASDeviceNotificationType notificationType();
	com_ximeta_driver_NDASUnitDevice *device();

};

#endif