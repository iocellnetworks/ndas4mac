#import "UIController.h"

#import <Security/Authorization.h>
#import <Security/AuthorizationTags.h>

NSString *ComponentPath[] = {

// netLPX.
@"/System/Library/Extensions/netlpx.kext",		// NETLPX_PATH
@"/System/Library/StartupItems/XiMetaNetLpx",	// NETLPX_START
@"/Library/Receipts/lpx_drv.pkg",				// NETLPX_RECEIPT1_0
@"/Library/Receipts/netlpx.pkg",				// NETLPX_RECEIPT1_2
@"/Library/Receipts/netlpx-10.4.pkg",			// NETLPX_RECEIPT1_3
@"/Library/Receipts/netlpx-10.3.pkg",			// NETLPX_RECEIPT1_4
@"/Library/Receipts/netlpx-1.pkg",				// NETLPX_RECEIPT1_8_1
@"/Library/Receipts/ximetanetlpx.pkg",			// NETLPX_RECEIPT1_8_1

// LanScsiBus.
@"/System/Library/Extensions/LanScsiBus.kext",	// LANSCSIBUS_PATH
@"/Library/Receipts/AutoBundle.pkg",			// LANSCSIBUS_RECEIPT1_0
@"/Library/Receipts/LanScsiBus.pkg",			// LANSCSIBUS_RECEIPT1_2

// NDAS Family.
@"/System/Library/Extensions/NDASFamily.kext",	// NDASFAMILY_PATH
@"/Library/Receipts/AutoBundle.pkg",			// NDASFAMILY_RECEIPT1_0
@"/Library/Receipts/NDASFamily.pkg",			// NDASFAMILY_RECEIPT1_2
@"/Library/Receipts/NDASFamily-10.4.pkg",		// NDASFAMILY_RECEIPT1_3
@"/Library/Receipts/NDASFamily-10.3.pkg",		// NDASFAMILY_RECEIPT1_4

// NDService.
@"/usr/sbin/NDService",							// NDSERVICE_PATH1_2
@"/System/Library/StartupItems/XiMetaNDService",// NDSERVICE_START1_2
@"/Library/Receipts/NDService.pkg",				// NDSERVICE_RECEIPT1_2

// NDASService.
@"/usr/sbin/NDASService",							// NDASSERVICE_PATH1_2
@"/System/Library/StartupItems/XiMetaNDASService",	// NDASSERVICE_START1_2
@"/Library/Receipts/NDASService.pkg",				// NDASSERVICE_RECEIPT1_2
@"/Library/Receipts/NDASService-10.4.pkg",			// NDASSERVICE_RECEIPT1_3
@"/Library/Receipts/NDASService-10.3.pkg",			// NDASSERVICE_RECEIPT1_4
@"/Library/Receipts/ximetandasservice.pkg",			// NDASSERVICE_RECEIPT1_8_1

// NDADmin.
@"/Applications/NDADmin.app",						// NDADMIN_PATH1_0
@"/Library/Receipts/ND_Admin.pkg",					// NDADMIN_RECEIPT1_0
@"/Applications/ND ADmin.app",						// NDADMIN_PATH1_2
@"/Library/Receipts/ND Admin.pkg",					// NDADMIN_RECEIPT1_2

// EOSEED Admin.
@"/Applications/Eoseed.app",							// EOSEEDADMIN_PATH1_0
@"/Library/Receipts/Eoseed_Admin.pkg",				// EOSEEDADMIN_RECEIPT1_0
@"/Applications/EOSEED ADmin.app",					// EOSEEDADMIN_PATH1_2
@"/Library/Receipts/EOSEED Admin.pkg",				// EOSEEDADMIN_RECEIPT1_2

// Logitec Admin.
@"/Applications/LHD-LU2.app",						// LOGITECADMIN_PATH1_0
@"/Library/Receipts/LHD-LU2_Admin.pkg",				// LOGITECADMIN_RECEIPT1_0
@"/Applications/LHD-LU2 ADmin.app",					// LOGITECADMIN_PATH1_2
@"/Library/Receipts/LHD-LU2 Admin.pkg",				// LOGITECADMIN_RECEIPT1_2

// IODATA Admin.
@"/Applications/HDH-UL Admin.app",					// IODATAADMIN_PATH1_2
@"/Library/Receipts/HDH-UL Admin.pkg",				// IODATAADMIN_RECEIPT1_2

// NDAS Admin.
@"/Applications/NDAS Admin.app",						// NDASADMIN_PATH1_2
@"/Library/Receipts/NDAS Admin.pkg",					// NDASADMIN_RECEIPT1_2

// GenDisk Admin.
@"/Applications/GenDisk Admin.app",					// GENDISKADMIN_PATH1_2
@"/Library/Receipts/GenDisk Admin.pkg",				// GENDISKADMIN_RECEIPT1_2

// NDAS Utility.
@"/Applications/Utilities/NDAS Utility.app",			// NDASUTILITY_PATH1_6
@"/Library/Receipts/NDAS Utility.pkg",				//NDASUITLITY_RECEIPT1_6

// ND Utility.
@"/Applications/Utilities/ND Utility.app",			// NDUTILITY_PATH1_6
@"/Library/Receipts/ND Utility.pkg",					// NDUITLITY_RECEIPT1_6

// Share Disk Utility.
@"/Applications/Utilities/ShareDisk Utility.app",	// SHAREDISKUTILITY_PATH1_6
@"/Library/Receipts/ShareDisk Utility.pkg",			// SHAREDISKUITLITY_RECEIPT1_6

// Freecomm NDAS Utility.
@"/Applications/Utilities/Freecom NDAS Utility.app",	// FREECOMNDASUTILITY_PATH1_6
@"/Library/Receipts/Freecom NDAS Utility.pkg",		// FREECOMNDASUITLITY_RECEIPT1_6

// NDAS Install.
@"/Library/Receipts/NDAS Install.pkg"		// 1.8.0 Alpha ver.

};

NSString *PropertyPath[] = {

// 1.6
@"/Library/Preferences/Netdisk Property.plist",								// PROPERTY_PATH 1.6

// 1.8
@"/Library/Preferences/com.ximeta.NDAS.service.register.plist",				// Property 1.8
@"/Library/Preferences/com.ximeta.NDAS.service.configuration.plist",			// Property 1.8
@"/Library/Preferences/com.ximeta.NDAS.service.RAID.Information.plist",		// Property 1.8

};

extern AuthorizationRef	myAuthorizationRef; 

@implementation UIController

- (void) awakeFromNib
{
	
	NSDictionary	*infoDic;
	NSString		*tempString;
	NSString		*format;	
	
	infoDic = [[NSBundle mainBundle] infoDictionary];
	if( nil == infoDic ) {
		NSLog(@"NDAdmin: Can't get infoDic");
	}
	
	tempString = [infoDic valueForKey:@"ModelName"];
	
	NSString	*info = NSLocalizedString(@"This will remove all files previously installed by the %@ installer.", @"Uninstall Info.");	
	
	[InfoTextField setStringValue: [NSString localizedStringWithFormat: info, tempString]];
	[InfoTextField displayIfNeeded];

	infoDic = [[NSBundle mainBundle] localizedInfoDictionary];
	if( nil == infoDic ) {
		NSLog(@"NDAdmin: Can't get infoDic");
	}
	
	// Change Menu Titles.
	tempString = [infoDic valueForKey:@"CFBundleName"];
	
	format = NSLocalizedString(@"About %@", @"About Menu");
	[AboutMenu setTitle: [NSString localizedStringWithFormat: format, tempString]];
	
	format = NSLocalizedString(@"Hide %@", @"Hide Menu");
	[HideMenu setTitle: [NSString localizedStringWithFormat: format, tempString]];
	
	format = NSLocalizedString(@"Quit %@", @"Quit Menu");
	[QuitMenu setTitle: [NSString localizedStringWithFormat: format, tempString]];

	[NSApp unhide];		
}

- (IBAction)quit:(id)sender
{
	[NSApp terminate: self];
}

- (BOOL)RemoveComponent:(NSString *)path
{	
	OSStatus	myStatus;
	char		CPath[512];

	sprintf(CPath, "%s", [path UTF8String]);

	if ( [[NSFileManager defaultManager] fileExistsAtPath: path] ) {
		
		char	*rmArguments[] = { "-r", CPath, NULL };
				
		myStatus = AuthorizationExecuteWithPrivileges(
													  myAuthorizationRef,
													  "/bin/rm",
													  kAuthorizationFlagDefaults,
													  rmArguments,
													  NULL
													  );
		
		if(errAuthorizationSuccess != myStatus) { 
			return NO;
		}
	}
	
	return YES;
}

- (IBAction)uninstall:(id)sender
{
	
	// Get Device Mode Name.
	NSDictionary	*infoDic;
	NSString		*tempString;
	NSString		*info;
	int				i;
	int				ComponnentsToRemove;
	int				PropertiesToRemove;
	int				totalToRemove;
	
	infoDic = [[NSBundle mainBundle] infoDictionary];
	if( nil == infoDic ) {
		NSLog(@"NDAdmin: Can't get infoDic");
	}
	
	[WithPropertyCheckbox setEnabled: NO];
	[WithPropertyCheckbox displayIfNeeded];

	[RemoveButton setEnabled: NO];
	[RemoveButton displayIfNeeded];
	[QuitButton setEnabled: NO];
	[QuitButton displayIfNeeded];
	
	tempString = [infoDic valueForKey:@"ModelName"];
		
	[InfoTextField setStringValue: [NSString localizedStringWithFormat: 
									NSLocalizedString(@"Now Uninstalling...", @"Uninstall start")]];
	[InfoTextField displayIfNeeded];
	
	ComponnentsToRemove = sizeof(ComponentPath) / sizeof(NSString);
	PropertiesToRemove = sizeof(PropertyPath) / sizeof(NSString);
	
	totalToRemove = ComponnentsToRemove;
	if (NSOnState == [WithPropertyCheckbox state]) {
		totalToRemove += PropertiesToRemove;
	}
	
	for (i = 0; i < ComponnentsToRemove; i++) {

//		NSLog(@"[%d] %@\n", i, ComponentPath[i]);
		
		double progress = 100.0 * (i + 1) / (double)(totalToRemove);

		[self RemoveComponent: ComponentPath[i]];
			
		[ProgressBar setDoubleValue: progress];
		[ProgressBar displayIfNeeded];
	}

	if (NSOnState == [WithPropertyCheckbox state]) {
		for (i = 0; i < PropertiesToRemove; i++) {

//			NSLog(@"[%d] %@\n", i + ComponnentsToRemove, PropertyPath[i]);

			double progress = 100.0 * (i + 1 + ComponnentsToRemove) / (double)(totalToRemove);
			
			[self RemoveComponent: PropertyPath[i]];
			
			[ProgressBar setDoubleValue: progress];
			[ProgressBar displayIfNeeded];
		}
	}

	[ProgressBar setHidden: YES];

	[InfoTextField setStringValue: [NSString localizedStringWithFormat: 
							  NSLocalizedString(@"The uninstaller has successfully uninstalled the %@ Software.", @"Uninstall success"),
							  tempString]];
	[InfoTextField displayIfNeeded];

	[QuitButton setEnabled: YES];
}

- (BOOL) windowShouldClose: (NSWindow *) sender
{
	[NSApp terminate: self];
	
	return true;
}

- (void)windowDidBecomeMain:(NSNotification *)aNotification
{
	NSDictionary	*infoDic;
	NSString		*tempString;
		
	// Get the application's bundle and read it's get info string.
	infoDic = [[NSBundle mainBundle] localizedInfoDictionary];
	if( nil == infoDic ) {
		NSLog(@"NDAdmin: Can't get infoDic");
	}
	
	tempString = [infoDic valueForKey:@"CFBundleDisplayName"];
	
	// Change Main window title.
	[[aNotification object] setTitle:tempString]; 
}

@end
