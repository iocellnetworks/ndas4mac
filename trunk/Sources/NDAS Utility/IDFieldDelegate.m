#import "IDFieldDelegate.h"
#import "UtilityController.h"

#import "serial.h"
#import "NDASID.h"

@implementation IDFieldDelegate

- (BOOL)goodCharacter:(unichar)inputChar
{
	if (! [[NSCharacterSet uppercaseLetterCharacterSet] characterIsMember: inputChar] 
		&& ! [[NSCharacterSet decimalDigitCharacterSet] characterIsMember: inputChar] ) {
		return NO;
	}
	
	return YES;
}

- (void)controlTextDidChange:(NSNotification *)aNotification
{
	NSString *aString;
	
	// to Uppercase.
	aString = [[[aNotification object] stringValue] uppercaseString];

	// Delete non roman char.
	switch ([aString length]) {
		case 0:
		{
			return;
		}
			break;
		case 1:
		{
			if (![self goodCharacter: [aString characterAtIndex: 0]]) {
				aString = @"";
			}
		}
			break;
		default:
		{
			if (![self goodCharacter: [aString characterAtIndex: 0]]) {
				aString = @"";
			} else {
				
				int i = 0;
				for (i = 1; i < [aString length]; i++) {
					
					if (![self goodCharacter: [aString characterAtIndex: i]]) {
						aString = [aString substringToIndex: i];
					}					
				}	
			}
		}
	}
	
	// Update
	[[aNotification object] setStringValue: aString];
	
	if( 5 <= [aString length] ) {
		
		aString = [aString substringToIndex:5];
		
		// Advenced to next text field.
		[[[aNotification object] window] selectKeyViewFollowingView: [aNotification object]];
	}
	
	// Check ID.	
	if( 5 == [[controller registerIDField1] cStringLength] &&
		5 == [[controller registerIDField2] cStringLength] &&
		5 == [[controller registerIDField3] cStringLength] &&
		5 == [[controller registerIDField4] cStringLength] &&
		( 0 == [[controller registerWritekeyField] cStringLength] || 5 == [[controller registerWritekeyField] cStringLength])) {
	
		SERIAL_INFO	info;
		
		[[controller registerIDField1] getCString: info.ucSN[0] maxLength: 5];
		[[controller registerIDField2] getCString: info.ucSN[1] maxLength: 5];
		[[controller registerIDField3] getCString: info.ucSN[2] maxLength: 5];
		[[controller registerIDField4] getCString: info.ucSN[3] maxLength: 5];
	
		BOOL bHasWriteKey = FALSE;
				
		if( 5 == [[controller registerWritekeyField] cStringLength] ) {
			bHasWriteKey = TRUE;
			[[controller registerWritekeyField] getCString: info.ucWKey maxLength: 5];
		}
		
		memcpy(info.ucKey1, NDAS_ID_KEY_DEFAULT.Key1, 8);
		memcpy(info.ucKey2, NDAS_ID_KEY_DEFAULT.Key2, 8);
		
		if ( TRUE == DecryptSerial( &info ) ) {
			
			if( bHasWriteKey ) {
				if( info.bIsReadWrite ) {
					[[controller registerRegisterButton] setEnabled: TRUE];
					[[controller registerErrorString] setStringValue: @" "];
				} else {
					[[controller registerRegisterButton] setEnabled: FALSE];
					[[controller registerErrorString] setStringValue: NSLocalizedString(@"PANEL_REGISTER : Invalid NDAS device write key.", @"Typed wrong NDAS device ID (5 chars)")];
				}
			} else {
				[[controller registerRegisterButton] setEnabled: TRUE];
				[[controller registerErrorString] setStringValue: @" "];
			}
			
		} else {
			
			NSDictionary *infoDic = [[NSBundle mainBundle] infoDictionary];
			if( nil == infoDic ) {
				NSLog(@"NDAS Utility: Can't get infoDic");
			}
			
			[[controller registerRegisterButton] setEnabled: FALSE];
			[[controller registerErrorString] setStringValue: NSLocalizedString(@"PANEL_REGISTER : Invalid NDAS device ID.", @"Typed wrong NDAS device ID (20 chars)")];
		}
	} else {
		[[controller registerRegisterButton] setEnabled: FALSE];
		[[controller registerErrorString] setStringValue: @" "];
	}
}

@end
