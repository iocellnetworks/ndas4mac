#import "NameFieldDelegate.h"

@implementation NameFieldDelegate

- (void)controlTextDidChange:(NSNotification *)aNotification
{
	NSString *aString;
		
	// Get String.
	aString = [[aNotification object] stringValue];

	if(128 <= [aString length]) {
		
		aString = [aString substringToIndex: 128];		
	}
	
	// Update
	[[aNotification object] setStringValue: aString];
}

@end
