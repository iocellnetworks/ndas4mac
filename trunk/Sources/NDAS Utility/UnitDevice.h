//
//  UnitDevice.h
//  NDAS Utility
//
//  Created by Ï†ïÍ∑† Ïïà on 05. 03. 28.
//  Copyright 2005 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "Device.h"

@interface UnitDevice : Device {
	
	UInt32	unitNumber;
	id		ndasId;
	
	UInt32		ROAccess;
	UInt32		RWAccess;
	
	NSString	*model;
	NSString	*firmware;
	NSString	*serialNumber;
	
	UInt8		transferMode;
	
	UInt8		type;
	
	UInt32		diskArrayType;
	
	NSString	*BSDName;	
}

- (int) unitNumber;
- (void) setUnitNumber: (UInt32) newUnitNumber;
- (id) ndasId;
- (void) setNdasId: (id) newId;

- (int) ROAccess;
- (void) setROAccess: (UInt32) newValue;
- (int) RWAccess;
- (void) setRWAccess: (UInt32) newValue;

- (NSString *) model;
- (void) setModel: (NSString *)valueDisplay;
- (NSString *) firmware;
- (void) setFirmware: (NSString *)valueDisplay;
- (NSString *) serialNumber;
- (void) setSerialNumber: (NSString *)valueDisplay;

- (UInt8) transferMode;
- (void) setTransferMode: (UInt8) newValue;
- (UInt8) type;
- (void) setType: (UInt8) newValue;
- (UInt32) diskArrayType;
- (void) setDiskArrayType: (UInt32) newValue;

- (NSString *) BSDName;
- (void) setBSDName: (NSString *)valueDisplay;

@end
