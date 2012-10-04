/*
 *  NDASPhysicalUnitDeviceController.h
 *  NDASFamily
 *
 *  Created by Jung-gyun Ahn on 09. 07. 02.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#include <IOKit/IOService.h>

#include "Utilities.h"

class com_ximeta_driver_NDASPhysicalUnitDevice;

class com_ximeta_driver_NDASPhysicalUnitDeviceController : public IOService {
	
	OSDeclareDefaultStructors(com_ximeta_driver_NDASPhysicalUnitDeviceController)
	
private:
		
	com_ximeta_driver_NDASPhysicalUnitDevice	*fProvider;
	
	unsigned long		fCurrentPowerState;
	
protected:
	
public:
	
	virtual bool init(OSDictionary *dictionary = 0);
	virtual void free(void);
	virtual IOService *probe(IOService *provider, SInt32 *score);
	virtual bool start(IOService *provider);
	virtual bool finalize ( IOOptionBits options );
	virtual void stop(IOService *provider);
//	virtual IOReturn message ( UInt32 type, IOService * provider, void * argument = 0 );	
	virtual IOReturn setPowerState (
									unsigned long   powerStateOrdinal,
									IOService*		whatDevice
									);
	
};

