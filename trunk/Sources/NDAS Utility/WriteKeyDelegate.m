//
//  WriteKeyDelegate.m
//  NDAS Utility
//
//  Created by 정균 안 on 05. 03. 29.
//  Copyright 2005 __MyCompanyName__. All rights reserved.
//

#import "WriteKeyDelegate.h"
#import "UtilityController.h"
#import "NDASDevice.h"

#import "serial.h"
#import "NDASID.h"
#import "NDASGetInformation.h"

@implementation WriteKeyDelegate

- (void)controlTextDidChange:(NSNotification *)aNotification
{	
	NSString *aString;
	
	[[controller enterWriteKeyErrorField] setStringValue: @" "];
	
	// to Uppercase.
	aString = [[[aNotification object] stringValue] uppercaseString];
	
	if( 5 <= [aString length] ) {
		
		aString = [aString substringToIndex:5];
		
		// Advenced to next toextx field.
		[[NSApp mainWindow] selectKeyViewFollowingView: [aNotification object]];
	}
	
	// Update
	[[aNotification object] setStringValue: aString];
	
	// Check WriteKey.	
	if( 5 == [[controller enterWriteKeyField] cStringLength] ) {
		
		SERIAL_INFO					info;
		NDASPhysicalDeviceInformation	NDParam;
		
		// Get Address.
		NDParam.slotNumber = [(NDASDevice *)[controller selectedNDASDevice] slotNo];
		NDASGetInformationBySlotNumber(&NDParam);
		
		// Encrypt Serial
		memcpy(info.ucAddr, NDParam.address, 6);
		info.ucVid = 0x01;
		info.reserved[0] = 0xFF;
		info.reserved[1] = 0xFF;
		info.ucRandom = 0xCD;
		
		memcpy(info.ucKey1, NDAS_ID_KEY_DEFAULT.Key1, 8);
		memcpy(info.ucKey2, NDAS_ID_KEY_DEFAULT.Key2, 8);
		
		EncryptSerial(&info);
		
//		NSLog(@"%s %s", info.ucWKey, [[[aNotification object] stringValue] cString]);

		if ( 0 == strncmp((char *)info.ucWKey, [[[aNotification object] stringValue] cString], 5) ) {
			[[controller enterWriteKeyButton] setEnabled: TRUE];
		} else {
			[[controller enterWriteKeyButton] setEnabled: FALSE];
			[[controller enterWriteKeyErrorField] setStringValue: NSLocalizedString(@"PANEL_WRITE_KEY : Invalid NDAS device write key.", @"Typed wrong NDAS device write key (5 chars)")];
		}
	} else {
		[[controller enterWriteKeyButton] setEnabled: FALSE];
	}
}

@end
