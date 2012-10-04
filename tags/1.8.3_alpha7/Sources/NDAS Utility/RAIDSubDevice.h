//
//  RAIDSubDevice.h
//  NDAS Utility
//
//  Created by 정균 안 on 06. 02. 04.
//  Copyright 2006 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface RAIDSubDevice : NSObject {
	id			RAIDDeviceID;
	UInt32		subUnitNumber;
	UInt64		deviceID;
	
	UInt32		subUnitStatus;
	UInt32		subUnitType;
}

- (id) RAIDDeviceID;
- (void) setRAIDDeviceID: (id) newId;

- (int) subUnitNumber;
- (void) setSubUnitNumber: (UInt32) newUnitNumber;

- (UInt64) deviceID;
- (void) setDeviceID: (UInt64) newValue;

- (UInt32) subUnitStatus;
- (void) setSubUnitStatus: (UInt32) newStatus;

- (UInt32) subUnitType;
- (void) setSubUnitType: (UInt32) newType;

@end
