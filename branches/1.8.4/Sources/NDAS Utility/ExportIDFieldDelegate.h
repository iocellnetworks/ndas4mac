/* ExportIDFieldDelegate */

#import <Cocoa/Cocoa.h>

@interface ExportIDFieldDelegate : NSObject
{
}

- (void)controlTextDidChange:(NSNotification *)aNotification;
- (IBAction)exportRegistrationInformationSwitchWriteKeyCheckBox:(id)sender;

@end
