/*
 *  NDASUnitDeviceList.h
 *  NDASFamily
 *
 *  Created by 정균 안 on 06. 11. 28.
 *  Copyright 2006 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef _NDAS_UNIT_DEVICE_LIST_H_
#define _NDAS_UNIT_DEVICE_LIST_H_

#include <IOKit/IOService.h>
#include "LanScsiDiskInformationBlock.h"
 
#define NDAS_DEVICE_LIST_CLASS	"com_ximeta_driver_NDASUnitDeviceList"

class  com_ximeta_driver_NDASUnitDevice;

class com_ximeta_driver_NDASUnitDeviceList : public OSObject
{
	OSDeclareDefaultStructors(com_ximeta_driver_NDASUnitDeviceList)

	OSDictionary	*list;
	IOLock			*lock;
	
public:

	virtual bool init();
	virtual void free(void);
	
	SInt insertUnitDevice(com_ximeta_driver_NDASUnitDevice *device);
	bool insertUnitDevice(SInt	index, com_ximeta_driver_NDASUnitDevice *device);
	bool removeUnitDevice(SInt	index);
	
	com_ximeta_driver_NDASUnitDevice *getFirstUnitDevice();
	
	com_ximeta_driver_NDASUnitDevice *findUnitDeviceByIndex(SInt index);
	com_ximeta_driver_NDASUnitDevice *findUnitDeviceByGUID(PGUID guid);
	unsigned int getCount();
};

#endif
