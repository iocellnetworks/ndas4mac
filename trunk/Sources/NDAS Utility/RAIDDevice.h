//
//  RAIDDevice.h
//  NDAS Utility
//
//  Created by ?? ? on 06. 01. 27.
//  Copyright 2006 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "Device.h"
#import "LanScsiDiskInformationBlock.h"

@interface RAIDDevice : Device {

	UInt32		index;

	UInt32		diskArrayType;
	
	UInt32		UnitDeviceType;
		
	BOOL		writable;
		
	UInt32		CountOfUnitDevices;
	UInt32		CountOfSpareDevices;
		
	UInt32		ROAccess;
	UInt32		RWAccess;
	
	UInt32		RAIDStatus;
	
	NSMutableDictionary	*subUnitDictionary;
	
	UInt64		RecoveringSector;
}

- (UInt32) index;
- (void) setIndex: (UInt32) newValue;

- (int) ROAccess;
- (void) setROAccess: (UInt32) newValue;
- (int) RWAccess;
- (void) setRWAccess: (UInt32) newValue;

- (BOOL) writable;
- (void) setWritable: (BOOL) newWritable;
- (UInt32) unitDeviceType;
- (void) setUnitDeviceType: (UInt32) newValue;
- (UInt32) diskArrayType;
- (void) setDiskArrayType: (UInt32) newValue;

- (UInt32) CountOfUnitDevices;
- (void) setCountOfUnitDevices: (UInt32) newValue;
- (UInt32) CountOfSpareDevices;
- (void) setCountOfSpareDevices: (UInt32) newValue;

- (void) addSubUnitDevice: (id) newUnit 
				   ForKey: (int) keyWithInt;
- (void) deleteSubUnitDeviceForKey: (int) keyWithInt;
- (id) subUnitAtIndex: (UInt32) subindex;
- (int) numberOfSubUnits;

- (UInt32) RAIDStatus;
- (void) setRAIDStatus: (UInt32) newStatus;

- (UInt64) RecoveringSector;
- (void) setRecoveringSector: (UInt64) newValue;
@end
