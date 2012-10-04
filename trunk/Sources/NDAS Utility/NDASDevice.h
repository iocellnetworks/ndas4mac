//
//  NDASDevice.h
//  NDAS Utility
//
//  Created by 정균 안 on 05. 03. 25.
//  Copyright 2005 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "Device.h"

@class UnitDevice;

@interface NDASDevice : Device {
	
	UInt32				slotNo;
	
	NSString			*deviceIDString;
		
	NSString			*versionString;
	
	UInt32				maxRequestBlocks;
	
	BOOL				writable;
	
	NSMutableDictionary	*unitDictionary;
	
	UInt8				MACAddress[6];
	
}

- (NSString *) deviceIDString;
- (void) setDeviceIDString: (NSString *)deviceIDString;
- (NSString *) versionString;
- (void) setVersionString: (NSString *)newVersionString;
- (int) slotNo;
- (void) setSlotNo: (UInt32) newSlotNo;
- (BOOL) writable;
- (void) setWritable: (BOOL) newWritable;
- (int) maxRequestBlocks;
- (void) setMaxRequestBlocks: (UInt32) newMRB;
- (UInt8 *) MACAddress;
- (void) setMACAddress: (UInt8 *) newAddress;

- (void) addUnitDevice: (id) newUnit ForKey: (int) keyWithInt;
- (void) deleteUnitDeviceForKey: (int) keyWithInt;
- (id) unitAtIndex: (UInt32) index;
- (int) numberOfUnits;

- (NSMutableDictionary *) unitDictionary;

@end
