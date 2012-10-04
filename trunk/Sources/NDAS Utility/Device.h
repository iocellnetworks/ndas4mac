//
//  Device.h
//  NDAS Utility
//
//  Created by 정균 안 on 05. 03. 28.
//  Copyright 2005 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "LanScsiDiskInformationBlock.h"

enum {
	kNDASDeviceTypeNDAS,
	kNDASDeviceTypeUnit,
	kNDASDeviceTypeRAID
};

@interface Device : NSObject {

	UInt32		configuration;
    UInt32		status;	

	UInt32		deviceType;
	
	UInt64		capacity;
	
//	UInt64		unitDiskLocation;
	GUID	deviceID;
		
	NSString	*name;

	BOOL				removed;
}

- (UInt32) configuration;
- (void) setConfiguration: (UInt32) newConfig;
- (UInt32) status;
- (void) setStatus: (UInt32) newStatus;

- (UInt32) deviceType;

- (UInt64) capacity;
- (void) setCapacity: (UInt64) newValue;
- (NSString *) capacityToString;

- (PGUID) deviceID;
- (void) setDeviceID: (GUID) newValue;

- (NSString *) name;
- (void) setName: (NSString *)valueDisplay;

- (BOOL)isRemoved;
- (void)setRemoved: (BOOL) newValue;

- (BOOL) isInRunningStatus;

+ (NSString *) capacityToString: (UInt64) capacity; 

@end
