//
//  RAIDDevice.m
//  NDAS Utility
//
//  Created by ?? ? on 06. 01. 27.
//  Copyright 2006 __MyCompanyName__. All rights reserved.
//

#import "RAIDDevice.h"


@implementation RAIDDevice

- (id) init
{
	self = [super init];
	
	deviceType = kNDASDeviceTypeRAID;
	
	subUnitDictionary = [[NSMutableDictionary alloc] init];

	return self;
}

- (void) dealloc
{
	[subUnitDictionary release];

	[super dealloc];
}

- (UInt32) index { return index; }
- (void) setIndex: (UInt32) newValue { index = newValue; }

- (int) ROAccess { return ROAccess; }
- (void) setROAccess: (UInt32) newValue { ROAccess = newValue; }
- (int) RWAccess { return RWAccess; }
- (void) setRWAccess: (UInt32) newValue { RWAccess = newValue; }

- (BOOL) writable { return writable; }
- (void) setWritable: (BOOL) newWritable { writable = newWritable; }

- (UInt32) unitDeviceType { return UnitDeviceType; }
- (void) setUnitDeviceType: (UInt32) newValue { UnitDeviceType = newValue; }

- (UInt32) diskArrayType { return diskArrayType; }
- (void) setDiskArrayType: (UInt32) newValue { diskArrayType = newValue; }

- (UInt32) CountOfUnitDevices { return CountOfUnitDevices; }
- (void) setCountOfUnitDevices: (UInt32) newValue { CountOfUnitDevices = newValue; }
- (UInt32) CountOfSpareDevices { return CountOfSpareDevices; }
- (void) setCountOfSpareDevices: (UInt32) newValue { CountOfSpareDevices = newValue; }

- (void) addSubUnitDevice: (id) newUnit 
				   ForKey: (int) keyWithInt
{
	[subUnitDictionary setObject: newUnit forKey: [NSNumber numberWithInt: keyWithInt]];	
}

- (void) deleteSubUnitDeviceForKey: (int) keyWithInt
{
	[subUnitDictionary removeObjectForKey: [NSNumber numberWithInt: keyWithInt]];
}

- (id) subUnitAtIndex: (UInt32) subindex
{
	//NSLog(@"unitAtIndex: %d", index);
	
	return [subUnitDictionary objectForKey: [NSNumber numberWithInt: subindex]];
}

- (int) numberOfSubUnits
{
	//NSLog(@"numberOfUnits: %d", [unitDictionary count]);
	
	return [subUnitDictionary count];
}

- (UInt32) RAIDStatus { return RAIDStatus; }
- (void) setRAIDStatus: (UInt32) newStatus { RAIDStatus = newStatus; }

- (UInt64) RecoveringSector { return RecoveringSector; }
- (void) setRecoveringSector: (UInt64) newValue { RecoveringSector = newValue; }

@end
