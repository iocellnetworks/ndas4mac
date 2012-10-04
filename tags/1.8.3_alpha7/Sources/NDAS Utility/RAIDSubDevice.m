//
//  RAIDSubDevice.m
//  NDAS Utility
//
//  Created by 정균 안 on 06. 02. 04.
//  Copyright 2006 __MyCompanyName__. All rights reserved.
//

#import "RAIDSubDevice.h"


@implementation RAIDSubDevice

- (id) init
{
	self = [super init];
	
	RAIDDeviceID = nil;
	
	return self;
}

- (void) dealloc
{	
	[super dealloc];
}

- (id) RAIDDeviceID { return RAIDDeviceID; }
- (void) setRAIDDeviceID: (id) newId { RAIDDeviceID = newId; }

- (int) subUnitNumber { return subUnitNumber; }
- (void) setSubUnitNumber: (UInt32) newUnitNumber { subUnitNumber = newUnitNumber; }

- (UInt64) deviceID { return deviceID; }
- (void) setDeviceID: (UInt64) newValue { deviceID = newValue; }

- (UInt32) subUnitStatus { return subUnitStatus; }
- (void) setSubUnitStatus: (UInt32) newStatus { subUnitStatus = newStatus; }

- (UInt32) subUnitType { return subUnitType; }
- (void) setSubUnitType: (UInt32) newType { subUnitType = newType; }

@end
