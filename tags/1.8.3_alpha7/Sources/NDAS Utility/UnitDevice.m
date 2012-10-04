//
//  UnitDevice.m
//  NDAS Utility
//
//  Created by 정균 안 on 05. 03. 28.
//  Copyright 2005 __MyCompanyName__. All rights reserved.
//

#import "UnitDevice.h"


@implementation UnitDevice

- (id) init
{
	self = [super init];

	deviceType = kNDASDeviceTypeUnit;

	[self setModel: @"" ];
	[self setFirmware: @"" ];
	[self setSerialNumber: @"" ];
	[self setBSDName: @"" ];
		
	return self;
}

- (void) dealloc
{
	[model release];
	[firmware release];
	[serialNumber release];
	[BSDName release];
	
	[super dealloc];
}

- (int) unitNumber { return unitNumber; }
- (void) setUnitNumber: (UInt32) newUnitNumber { unitNumber = newUnitNumber; }

- (id) ndasId { return ndasId; }
- (void) setNdasId: (id) newId { ndasId = newId; }

- (int) ROAccess { return ROAccess; }
- (void) setROAccess: (UInt32) newValue { ROAccess = newValue; }
- (int) RWAccess { return RWAccess; }
- (void) setRWAccess: (UInt32) newValue { RWAccess = newValue; }

- (NSString *) model { return model; }
- (void) setModel: (NSString *)valueDisplay
{
	if( nil != valueDisplay ) {
		[model autorelease];
		
		model = [valueDisplay copy];	
	} else {		
		[self setModel: @""];
	}
}
- (NSString *) firmware { return firmware; }
- (void) setFirmware: (NSString *)valueDisplay
{
	if( nil != valueDisplay ) {
		[firmware autorelease];
		
		firmware = [valueDisplay copy];	
	} else {		
		[self setFirmware: @""];
	}
}
- (NSString *) serialNumber { return serialNumber; }
- (void) setSerialNumber: (NSString *)valueDisplay
{
	if( nil != valueDisplay ) {
		[serialNumber autorelease];
		
		serialNumber = [valueDisplay copy];	
	} else {		
		[self setSerialNumber: @""];
	}	
}

- (UInt8) transferMode { return transferMode; }
- (void) setTransferMode: (UInt8) newValue { transferMode = newValue; }

- (UInt8) type { return type; }
- (void) setType: (UInt8) newValue { type = newValue; }

- (UInt32) diskArrayType { return diskArrayType; }
- (void) setDiskArrayType: (UInt32) newValue { diskArrayType = newValue; }

- (NSString *) BSDName { return BSDName; }
- (void) setBSDName: (NSString *)valueDisplay
{
	if( nil != valueDisplay ) {
		[BSDName autorelease];
		
		BSDName = [valueDisplay copy];	
	} else {		
		[self setBSDName: @""];
	}
}

@end
