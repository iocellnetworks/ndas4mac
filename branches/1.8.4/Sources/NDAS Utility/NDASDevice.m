//
//  NDASDevice.m
//  NDAS Utility
//
//  Created by 정균 안 on 05. 03. 25.
//  Copyright 2005 __MyCompanyName__. All rights reserved.
//

#import "NDASDevice.h"
#import "UnitDevice.h"

#import "NDASBusEnumeratorUserClientTypes.h"

@implementation NDASDevice

- (id) init {
	self = [super init];
	[self setDeviceIDString: @"" ];
	[self setVersionString: @"" ];
	
	int count;
	
	deviceType = kNDASDeviceTypeNDAS;

	unitDictionary = [[NSMutableDictionary alloc] init];

	return self;
}

- (void) dealloc {
	[deviceIDString release];
	[versionString release];
	
	[unitDictionary release];
	
	[super dealloc];
}

- (NSString *) deviceIDString { return deviceIDString; }
- (void) setDeviceIDString: (NSString *)newDeviceIDString
{
	[deviceIDString autorelease];
	
	deviceIDString = [newDeviceIDString copy];
}

- (NSString *) versionString { return versionString; }
- (void) setVersionString: (NSString *)newVersionString
{
	[versionString autorelease];
	
	versionString = [newVersionString copy];	
}

- (SInt32) slotNo { return slotNo; }
- (void) setSlotNo: (SInt32) newSlotNo { slotNo = newSlotNo; }

- (int) maxRequestBlocks { return maxRequestBlocks; }
- (void) setMaxRequestBlocks: (UInt32) newMRB { maxRequestBlocks = newMRB; }

- (BOOL) writable { return writable; }
- (void) setWritable: (BOOL) newWritable { writable = newWritable; }

- (UInt8 *) MACAddress { return MACAddress; }
- (void) setMACAddress: (UInt8 *) newAddress 
{
	if (newAddress) {
		memcpy(MACAddress, newAddress, 6); 
	}
}

- (BOOL) autoRegister { return autoRegister; }
- (void) setAutoRegister: (BOOL) newAutoRegister
{
	autoRegister = newAutoRegister;
}

- (void) addUnitDevice: (id) newUnit 
				ForKey: (int) keyWithInt
{
	[unitDictionary setObject: newUnit forKey: [NSNumber numberWithInt: keyWithInt]];	
}

- (void) deleteUnitDeviceForKey: (int) keyWithInt
{
	[unitDictionary removeObjectForKey: [NSNumber numberWithInt: keyWithInt]];
}

- (id) unitAtIndex: (UInt32) index
{
	//NSLog(@"unitAtIndex: %d", index);
	
	return [unitDictionary objectForKey: [NSNumber numberWithInt: index]];
}

- (int) numberOfUnits
{
	//NSLog(@"numberOfUnits: %d", [unitDictionary count]);
	
	return [unitDictionary count];
}

- (NSMutableDictionary *) unitDictionary
{
	return unitDictionary;
}

- (BOOL) isIdle
{
	if ( kNDASStatusOnline == status ) {
		
		NSEnumerator *enumerator = [unitDictionary objectEnumerator];
		
		UnitDevice *unit;
		while ((unit = (UnitDevice *)[enumerator nextObject]) != nil)
		{
			if ( [unit configuration] != kNDASUnitConfigurationUnmount ) {
				return NO;
			} 
			
			if ( [unit status] != (UInt32)kNDASUnitStatusDisconnected ) {
				return NO;
			}
		}
		
		return YES;
		
	} else {
		return NO;
	}
}

@end
