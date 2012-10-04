/* UtilityController */

#import <Cocoa/Cocoa.h>
#import "LanScsiDiskInformationBlock.h"

#define DragDropSimplePboardType	@"RAIDBindSheetDragDropType"

@interface UtilityController : NSObject
{
	NSMutableDictionary  *NDASDevices;
	NSMutableDictionary  *RAIDDevices;
	id					selectedDevice;
	GUID				selectedLogicalDeviceLowerUnitID;
	
	// For Request.
	UInt32				IDForNotification;
	UInt32				recentRequest;
	BOOL				editingName;
	BOOL				sentRequest;
		
	NSLock				*updateLock;
	
	IBOutlet id backgroundInformationField;
    IBOutlet id progressIndicator;

	IBOutlet id tabView;

	// General Tab.
	IBOutlet id idField;
    IBOutlet id nameField;
    IBOutlet id statusField;
    IBOutlet id writeKeyField;
	IBOutlet id versionField;
	IBOutlet id maxRequestBlocksField;
	IBOutlet id MACAddressField;
	IBOutlet id writeKeyButton;
	IBOutlet id deviceNameTagField;
	IBOutlet id deviceIDTagField;

	// RAID Unit Tab.
	IBOutlet id RAIDUnitNameField;
	IBOutlet id RAIDUnitStatusField;
	IBOutlet id RAIDUnitConfigurationPopupButton;
	IBOutlet id RAIDUnitROAccessField;
	IBOutlet id RAIDUnitRWAccessField;
	IBOutlet id RAIDUnitDiskArrayTypeField;
	IBOutlet id RAIDUnitTypeField;
	IBOutlet id RAIDUnitCapacityField;
	IBOutlet id RAIDUnitImage;
	IBOutlet id RAIDUnitUnmountPopupItem;
	IBOutlet id RAIDUnitMountROPopupItem;
	IBOutlet id RAIDUnitMountRWPopupItem;
	IBOutlet id RAIDUnitRAIDStatusField;
	IBOutlet id RAIDUnitRecoveringStatusTagField;
	IBOutlet id RAIDUnitRecoveringStatusField;
	IBOutlet id RAIDUnitRecoveringStatusBar;

	// Unit Tab.
	IBOutlet id unitNumberField;
	IBOutlet id unitStatusField;
	IBOutlet id unitConfigurationPopupButton;
	IBOutlet id unitROAccessField;
	IBOutlet id unitRWAccessField;
	IBOutlet id unitDiskArrayTypeField;
	IBOutlet id unitTypeField;
	IBOutlet id unitModelField;
	IBOutlet id unitFirmwareField;
	IBOutlet id unitSNField;
	IBOutlet id unitCapacityField;
	IBOutlet id unitTranferModeField;
	IBOutlet id unitImage;
	IBOutlet id unitRAIDTypeTagField;
	IBOutlet id unmountPopupItem;
	IBOutlet id mountROPopupItem;
	IBOutlet id mountRWPopupItem;	
	
	// RAID Set Tab.
	IBOutlet id	RAIDSetNameField;
	IBOutlet id RAIDSetTypeField;
	IBOutlet id RAIDSetSizeField;
	IBOutlet id RAIDSetOutlineView;
	IBOutlet id RAIDSetUnbindButton;
	IBOutlet id RAIDSetMigrateActionItem;
	IBOutlet id RAIDSetRebindActionItem;
	
	IBOutlet id ndasOutlineView;
	
	// Register Sheet.
	IBOutlet id registerSheet;
	
	IBOutlet id registerNameField;
	IBOutlet id registerIDField1;
	IBOutlet id registerIDField2;
	IBOutlet id registerIDField3;
	IBOutlet id registerIDField4;
	IBOutlet id registerWritekeyField;
	IBOutlet id registerErrorString;
	IBOutlet id registerRegisterButton;

	IBOutlet id registerDeviceNameTagField;
	IBOutlet id registerDeviceIDTagField;

	// Write Key Sheet.
	IBOutlet id enterWriteKeySheet;
	
	IBOutlet id enterWriteKeyField;
	IBOutlet id enterWriteKeyButton;
	IBOutlet id enterWriteKeyErrorField;
	
	// RAID Bind Sheet.
	IBOutlet id RAIDBindSheet;
	
	IBOutlet id RAIDBindSheetNameField;
	IBOutlet id RAIDBindSheetTypePopupButton;
	IBOutlet id RAIDBindSheetSizeField;
	IBOutlet id RAIDBindSheetBindButton;
	IBOutlet id RAIDBindSheetCancelButton;
	IBOutlet id RAIDBindSheetUnitOutlineView;
	IBOutlet id RAIDBindSheetSubUnitOutlineView;
	IBOutlet id RAIDBindSheetRemoveButton;
	IBOutlet id RAIDBindSheetErrorMessageField;
	NSMutableArray		*RAIDBindSheetSubUnitDevices;
	
	// Export Registration Sheet.
	IBOutlet id ExportRegistrationSheet;
	
	IBOutlet id ExportRegistrationNameField;
	IBOutlet id ExportRegistrationIDField1;
	IBOutlet id ExportRegistrationIDField2;
	IBOutlet id ExportRegistrationIDField3;
	IBOutlet id ExportRegistrationIDField4;
	IBOutlet id ExportRegistrationWritekeyField;
	IBOutlet id ExportRegistrationErrorString;
	IBOutlet id ExportRegistrationExportButton;
	IBOutlet id ExportRegistrationNotExportWriteKeyCheckbox;
	IBOutlet id ExportRegistrationDescriptionTextView;
	
	IBOutlet id ExportRegistrationDeviceNameTagField;
	IBOutlet id ExportRegistrationDeviceIDTagField;

	
	// Menu.
	IBOutlet id aboutMenuItem;
	IBOutlet id hideMenuItem;
	IBOutlet id quitMenuItem;
	
	IBOutlet id openNDASRegistrationFileMenuItem;
	IBOutlet id saveNDASRegistrationFileMenuItem;
	
	IBOutlet id registerMenuItem;	
	IBOutlet id unregisterMenuItem;

	IBOutlet id unmountMenuItem;
	IBOutlet id mountROMenuItem;
	IBOutlet id mountRWMenuItem;
	IBOutlet id manageWriteKeyMenuItem;

	IBOutlet id refreshMenuItem;
	
	IBOutlet id RAIDBindMenuItem;
	IBOutlet id RAIDUnbindMenuItem;
	IBOutlet id RAIDMigrateMenuItem;
	IBOutlet id RAIDRebindMenuItem;
}

- (id) init;
- (void) dealloc;
- (void) refreshNDASDevices;
- (void) awakeFromNib;
- (void) outlineViewSelectionDidChanged: (NSNotification *)notification;
- (void) raidSheetOutlineViewSelectionDidChanged: (NSNotification *)notification;

- (IBAction)close:(id)sender;
- (IBAction)manageWriteKey:(id)sender;
- (IBAction)registerDevice:(id)sender;
- (IBAction)registerDeviceXML:(id)sender;
- (IBAction)unregisterDevice:(id)sender;
- (IBAction)mountRW:(id)sender;
- (IBAction)mountRO:(id)sender;
- (IBAction)unmount:(id)sender;
- (IBAction)refreshAll:(id)sender;
- (IBAction)refresh:(id)sender;

- (IBAction)registerSheetRegister:(id)sender;
- (IBAction)registerSheetCancel:(id)sender;

- (IBAction)enterWriteKeySheetEnter:(id)sender;
- (IBAction)enterWriteKeySheetCancel:(id)sender;

- (NSMutableDictionary *) NDASDevices;
- (NSMutableDictionary *) RAIDDevices;

- (NSString *)ExportRegistrationIDField1;
- (NSString *)ExportRegistrationIDField2;
- (NSString *)ExportRegistrationIDField3;
- (NSString *)ExportRegistrationIDField4;
- (id)ExportRegistrationWritekeyField;
- (id)ExportRegistrationExportButton;
- (id)ExportRegistrationErrorString;
- (id)ExportRegistrationNotExportWriteKeyCheckbox;

- (NSString *)registerIDField1;
- (NSString *)registerIDField2;
- (NSString *)registerIDField3;
- (NSString *)registerIDField4;
- (NSString *)registerWritekeyField;
- (id)registerRegisterButton;
- (id)registerErrorString;

- (id)selectedNDASDevice;
- (NSString *)enterWriteKeyField;
- (id)enterWriteKeyButton;
- (id)enterWriteKeyErrorField;
- (id)RAIDBindSheetUnitOutlineView;
- (id)RAIDBindSheetSubUnitOutlineView;
- (NSMutableArray *)RAIDBindSheetSubUnitDevices;
- (id)RAIDBindSheetRemoveButton;
- (id)RAIDBindSheetTypePopupButton;
- (id)RAIDBindSheetSizeField;
- (id)RAIDBindSheetBindButton;

- (void) _sndNotification : (UInt32) slotNumber
			   unitNumber : (UInt32) unitNumber
					index : (UInt32) index
			   eNdCommand : (UInt32) request
					 Name : (NSString *)Name
				   IDSeg1 : (NSString *)IDSeg1
				   IDSeg2 : (NSString *)IDSeg2
				   IDSeg3 : (NSString *)IDSeg3
				   IDSeg4 : (NSString *)IDSeg4
					WrKey : (NSString *)WrKey
				eNdConfig : (UInt32) configuration
				 eOptions : (UInt32) options
		   logicalDevices : (NSArray *)logicalDevices
				 RAIDType : (UInt32) RAIDType
					 Sync : (bool) Sync;

- (void)controlTextDidChange:(NSNotification *)aNotification;
- (void)controlTextDidBeginEditing:(NSNotification *)aNotification;
- (void)controlTextDidEndEditing:(NSNotification *)aNotification;

- (IBAction)RAIDBind:(id)sender;
- (IBAction)RAIDBindSheetBind:(id)sender;
- (IBAction)RAIDBindSheetCancel:(id)sender;
- (IBAction)RAIDUnbind:(id)sender;
- (IBAction)RAIDMigrate:(id)sender;
- (IBAction)RAIDRebind:(id)sender;
- (IBAction)RAIDResync:(id)sender;

- (IBAction)exportRegistrationInformation:(id)sender;

- (bool)sendRefresh:(id)device wait:(bool)wait;

NSString *printRAIDType(UInt32 type);

@end

extern UtilityController *controller;
