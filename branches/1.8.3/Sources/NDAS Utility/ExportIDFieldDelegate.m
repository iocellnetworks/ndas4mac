#import "ExportIDFieldDelegate.h"
#import "UtilityController.h"

#import "serial.h"
#import "NDASID.h"

@implementation ExportIDFieldDelegate

- (void)updateUI
{
	// Check ID.	
	if( 5 == [[controller ExportRegistrationIDField1] cStringLength] &&
		5 == [[controller ExportRegistrationIDField2] cStringLength] &&
		5 == [[controller ExportRegistrationIDField3] cStringLength] &&
		5 == [[controller ExportRegistrationIDField4] cStringLength] &&
		( 0 == [[[controller ExportRegistrationWritekeyField] stringValue] cStringLength] 
		  || 5 == [[[controller ExportRegistrationWritekeyField] stringValue] cStringLength])) {
		
		SERIAL_INFO	info;
		
		[[controller ExportRegistrationIDField1] getCString: info.ucSN[0] maxLength: 5];
		[[controller ExportRegistrationIDField2] getCString: info.ucSN[1] maxLength: 5];
		[[controller ExportRegistrationIDField3] getCString: info.ucSN[2] maxLength: 5];
		[[controller ExportRegistrationIDField4] getCString: info.ucSN[3] maxLength: 5];
		
		BOOL bHasWriteKey = FALSE;
		
		if( 5 == [[[controller ExportRegistrationWritekeyField] stringValue] cStringLength] ) {
			bHasWriteKey = TRUE;
			[[[controller ExportRegistrationWritekeyField] stringValue] getCString: info.ucWKey maxLength: 5];
		}
		
		memcpy(info.ucKey1, NDAS_ID_KEY_DEFAULT.Key1, 8);
		memcpy(info.ucKey2, NDAS_ID_KEY_DEFAULT.Key2, 8);
		
		if ( TRUE == DecryptSerial( &info ) ) {
			
			if( bHasWriteKey ) {
				if( info.bIsReadWrite ) {
					[[controller ExportRegistrationExportButton] setEnabled: TRUE];
					[[controller ExportRegistrationErrorString] setStringValue: @" "];
				} else {
					[[controller ExportRegistrationExportButton] setEnabled: FALSE];
					[[controller ExportRegistrationErrorString] setStringValue: NSLocalizedString(@"PANEL_EXPORT : Invalid NDAS device Write Key.", @"Typed wrong NDAS device write key (5 chars)")];
				}
			} else {
				
				if (0 == [[controller ExportRegistrationNotExportWriteKeyCheckbox] state]) {
					[[controller ExportRegistrationExportButton] setEnabled: FALSE];
					[[controller ExportRegistrationErrorString] setStringValue: NSLocalizedString(@"PANEL_EXPORT : Fill the Write Key.", @"Write key required to export registration file with write privilege")];
				} else {
					[[controller ExportRegistrationExportButton] setEnabled: TRUE];
					[[controller ExportRegistrationErrorString] setStringValue: @" "];
				}
			}
			
		} else {
			
			NSDictionary *infoDic = [[NSBundle mainBundle] infoDictionary];
			if( nil == infoDic ) {
				NSLog(@"NDAS Utility: Can't get infoDic");
			}
			
			[[controller ExportRegistrationExportButton] setEnabled: FALSE];
			[[controller ExportRegistrationErrorString] setStringValue: [NSString stringWithFormat: NSLocalizedString(@"PANEL_EXPORT : Invalid NDAS device ID.", @"Typed wrong NDAS device ID (20 chars)")]];
		}
	} else {
		[[controller ExportRegistrationExportButton] setEnabled: FALSE];
		[[controller ExportRegistrationErrorString] setStringValue: @" "];
	}
}

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
	
	[self updateUI];
}

- (IBAction)exportRegistrationInformationSwitchWriteKeyCheckBox:(id)sender
{
	[[controller ExportRegistrationWritekeyField] setStringValue: @""]; 
	
	if (0 == [[controller ExportRegistrationNotExportWriteKeyCheckbox] state]) {
		[[controller ExportRegistrationWritekeyField] setEnabled: YES];
	} else {
		[[controller ExportRegistrationWritekeyField] setEnabled: NO];
	}
	
	[self updateUI];
}

@end
