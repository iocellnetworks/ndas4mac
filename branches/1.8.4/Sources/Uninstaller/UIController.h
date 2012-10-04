/* UIController */
#import <Cocoa/Cocoa.h>

@interface UIController : NSObject
{
	IBOutlet id AboutMenu;
	IBOutlet id HideMenu;
	IBOutlet id QuitMenu;
	
	IBOutlet id ProgressBar;
	IBOutlet id WithPropertyCheckbox;
	
	IBOutlet id InfoTextField;

	IBOutlet id QuitButton;
	IBOutlet id RemoveButton;
	
	bool		bUninstallEnd;
}
- (void) awakeFromNib;
- (IBAction)quit:(id)sender;
- (IBAction)uninstall:(id)sender;
- (BOOL)RemoveComponent:(NSString *)path;

@end
