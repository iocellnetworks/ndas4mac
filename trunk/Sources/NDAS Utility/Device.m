//
//  Device.m
//  NDAS Utility
//
//  Created by 정균 안 on 05. 03. 28.
//  Copyright 2005 __MyCompanyName__. All rights reserved.
//

#import "Device.h"
#import "NDASBusEnumeratorUserClientTypes.h"


@implementation Device

- (id) init {
	self = [super init];
	[self setName: @"" ];
	
	return self;
}

- (void) dealloc {
	[name release];
	
	[super dealloc];
}

- (UInt32) configuration { return configuration; }
- (void) setConfiguration: (UInt32) newConfig { configuration = newConfig; }

- (UInt32) status { return status; }
- (void) setStatus: (UInt32) newStatus { status = newStatus; }

- (UInt32) deviceType { return deviceType; }

- (UInt64) capacity { return capacity; }
- (void) setCapacity: (UInt64) newValue { capacity = newValue; };
- (NSString *) capacityToString 
{
	return [Device capacityToString: capacity];
}

- (PGUID) deviceID { return &deviceID; }
- (void) setDeviceID: (GUID) newValue { memcpy(&deviceID, &newValue, sizeof(GUID)); }

- (NSString *) name 
{
	if (0 == [name length]) {
		return  NSLocalizedString(@"INTERFACE_DEVICE : Untitled", @"This is the default name for the device which is no name given yet.");
	} else {
		return name;
	}
}

- (void) setName: (NSString *)valueDisplay
{
	if (valueDisplay != name) {
		[name release];
		name = [valueDisplay retain];
	}
}

- (BOOL)isRemoved
{
	return removed;
}

- (void)setRemoved: (BOOL) newValue
{
	removed = newValue;
}

- (BOOL) isInRunningStatus
{
	if (kNDASUnitConfigurationUnmount == [self configuration]) {
		return NO;
	}
	
	switch([self status]) {
		case kNDASUnitStatusFailure:
		case kNDASUnitStatusDisconnected:
		case kNDASUnitStatusNotPresent:
			return NO;
	}
	
	return YES;
}

+ (NSString *) capacityToString: (UInt64) capacity 
{
	double	tempCapa = capacity;
	char	tempBuffer[256];
	
	if( tempCapa < 1024 ) {
		if (tempCapa < 100) {
			sprintf(tempBuffer, "%3.2f B", tempCapa);
		} else {
			sprintf(tempBuffer, "%3.1f B", tempCapa);
		}			
	} else if ( (tempCapa /= 1024) < 1024 ) {
		if (tempCapa < 100) {
			sprintf(tempBuffer, "%3.2f KB", tempCapa);
		} else {
			sprintf(tempBuffer, "%3.1f KB", tempCapa);
		}			
	} else if ( (tempCapa /= 1024) < 1024 ) {
		if (tempCapa < 100) {
			sprintf(tempBuffer, "%3.2f MB", tempCapa);
		} else {
			sprintf(tempBuffer, "%3.1f MB", tempCapa);
		}			
	} else if ( (tempCapa /= 1024) < 1024 ) {
				
		if (tempCapa < 100) {
			sprintf(tempBuffer, "%3.2f GB", tempCapa);
		} else {
			sprintf(tempBuffer, "%3.1f GB", tempCapa);
		}			
	} else {
		tempCapa /= 1024;
		if (tempCapa < 100) {
			sprintf(tempBuffer, "%3.2f TB", tempCapa);
		} else {
			sprintf(tempBuffer, "%3.1f TB", tempCapa);
		}			
	}
	
	return [NSString stringWithCString: tempBuffer];
}

@end
