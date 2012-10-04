#import "UtilityController.h"
#import "NDASDevice.h"
#import "UnitDevice.h"
#import "RAIDDevice.h"
#import "RAIDSubDevice.h"
#import "RAIDBindSheetController.h"

#import "ImageAndTextCell.h"

#import "NDASNotification.h"
#import "NDASBusEnumeratorUserClientTypes.h"
#import "NDASGetInformation.h"
#import "LanScsiDiskInformationBlock.h"

#import "serial.h"
#import "NDASID.h"

@implementation UtilityController

NSTabViewItem *generalTab;
NSTabViewItem *singleUnitTab;
NSTabViewItem *RAIDUnitTab;
NSTabViewItem *RAIDTab;

UtilityController *controller;
BOOL			isTabClosing;

static NSString* 	MyDocToolbarIdentifier			= @"My Document Toolbar Identifier";
static NSString*	RegisterToolbarItemIdentifier 	= @"Register NDAS Device Item Identifier";
static NSString*	UnregisterToolbarItemIdentifier = @"Unregister NDAS Device Item Identifier";
static NSString*	RefreshAllToolbarIdentifier		= @"Refresh All Toolbar Identifier";
static NSString*	BindToolbarItemIdentifier		= @"Bind NDAS Devices Item Identifier";
static NSString*	UnbindToolbarItemIdentifier 	= @"Unbind RAID Device Item Identifier";
static NSString*	ResyncToolbarItemIdentifier 	= @"Resync RAID Device Item Identifier";

- (id) init 
{
	self = [super init];
	NDASDevices = [[NSMutableDictionary alloc] init];
	RAIDDevices = [[NSMutableDictionary alloc] init];
	
	IDStringForNotification = [[NSString alloc] initWithString: [[NSProcessInfo processInfo] globallyUniqueString]];
		
	editingName = FALSE;
	sentRequest = FALSE;

	memset(&selectedLogicalDeviceLowerUnitID, 0, sizeof(GUID));
	
	updateLock = [[NSLock alloc] init];

	[NSApp setDelegate: self];
	
	return self;
}

- (void) dealloc 
{
	[[NSDistributedNotificationCenter defaultCenter] removeObserver:self];

	[updateLock release];
	
	[IDStringForNotification release];
	
	[NDASDevices release];
	[RAIDDevices release];

	[super dealloc];
}

- (void) refreshNDASDevices
{
	NDASPhysicalDeviceInformation	NDASInfo = { 0 };
	int								slotNumber, count;
	
//	NSLog(@"refreshNDASDevices\n");
	
	// For Update.
	NSEnumerator *enumerator = [NDASDevices objectEnumerator];
	NDASDevice *device;
  		
	// Mark removed all
	while ((device = [enumerator nextObject])) {
		[device setRemoved: YES];
	}
	
	NSMutableArray *indexArray;
	int	numberOfObjects;
	
	indexArray = (NSMutableArray *)NDASGetInformationCreateIndexArray(NDAS_DEVICE_CLASS_NAME);
	
	if(indexArray == NULL) {
		numberOfObjects = 0;
	} else {
		numberOfObjects = [indexArray count];
	}
		
	for (count = 0; count < numberOfObjects; count++) {
				
		NDASInfo.slotNumber = [[indexArray objectAtIndex:count] intValue];
		
		if (NDASInfo.slotNumber <= 0) {
			// Bypass internal use NDAS Devices.
			continue;
		}
		
//		NSLog(@"Slot Number %d\n", NDASInfo.slotNumber);
		
		if(NDASGetInformationBySlotNumber(&NDASInfo)) {
			
			NDASDevice	*entry;
			NSString	*IDForDisplay;
			NSString	*NameForDisplay;
			int			unitCount;
			
			if (NDASInfo.status == kNDASStatusNotPresent) {
				continue;
			}
			
			if ( !(entry = [NDASDevices objectForKey: [NSNumber numberWithInt: NDASInfo.slotNumber]] ) ) {
				// Create New Entry.
				entry = [[NDASDevice alloc] init];
			} else {
				[entry retain];
				[entry setRemoved: NO];
			}
			
			NameForDisplay = [NSString stringWithUTF8String:NDASInfo.name];
			IDForDisplay = [NSString stringWithCString:NDASInfo.deviceIDString];
			
			[entry setName: NameForDisplay];
			[entry setDeviceIDString: IDForDisplay];
			[entry setMACAddress: NDASInfo.address];
			
			[entry setConfiguration: NDASInfo.configuration];
			[entry setStatus: NDASInfo.status];
			[entry setSlotNo: NDASInfo.slotNumber];
			[entry setWritable: NDASInfo.writable];
			[entry setAutoRegister:NDASInfo.autoRegister];
						
			// Version.
			if( NDASInfo.hwType == 0 ) {
				switch( NDASInfo.hwVersion ) {
					case HW_VERSION_1_0:
						[entry setVersionString: @"1.0"];
						break;
					case HW_VERSION_1_1:
						[entry setVersionString: @"1.1"];
						break;
					case HW_VERSION_2_0:
						[entry setVersionString: @"2.0"];
						break;
					default:
						[entry setVersionString: NSLocalizedString(@"TAB_DEVICE : Undefined", @"The type or version of the selected NDAS device is not defined.")];
				}
				
			} else {
				[entry setVersionString:  NSLocalizedString(@"TAB_DEVICE : Undefined", @"The type or version of the selected NDAS device is not defined.")];
			}
			
			// Max Request Blocks.
			[entry setMaxRequestBlocks: NDASInfo.maxRequestBlocks];
						
			// Setting Units.
			for(unitCount = 0; unitCount < MAX_NR_OF_TARGETS_PER_DEVICE; unitCount++) {
				if ( kNDASUnitStatusNotPresent != NDASInfo.Units[unitCount].status ) {
					
					UnitDevice	*unit;
					NSString	*unitModel;
					NSString	*unitFirmware;
					NSString	*unitSerialNumber;
					NSString	*unitBSDName;
					
					if ( !( unit = [entry unitAtIndex: unitCount]) ) {
						// Create New Unit.
						unit = [[UnitDevice alloc] init];
					} else {
						[unit retain];
					}
					
					[unit setUnitNumber: unitCount];
					[unit setConfiguration: NDASInfo.Units[unitCount].configuration];
					[unit setStatus: NDASInfo.Units[unitCount].status];
					
					[unit setNdasId: entry];
					
					[unit setRWAccess: NDASInfo.Units[unitCount].RWHosts];
					
					[unit setROAccess: NDASInfo.Units[unitCount].ROHosts];
					
					unitModel = [NSString stringWithCString: NDASInfo.Units[unitCount].model];
					[unit setModel: [unitModel stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]]];
					
					unitFirmware = [NSString stringWithCString: NDASInfo.Units[unitCount].firmware];
					[unit setFirmware: [unitFirmware stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]]];
					
					unitSerialNumber = [NSString stringWithCString: NDASInfo.Units[unitCount].serialNumber];
					[unit setSerialNumber: [unitSerialNumber stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]]];
					
					[unit setTransferMode: NDASInfo.Units[unitCount].transferType];
					[unit setCapacity: NDASInfo.Units[unitCount].capacity];
					
					[unit setType: NDASInfo.Units[unitCount].type];
					
					[unit setDiskArrayType: NDASInfo.Units[unitCount].diskArrayType];
					
					[unit setDeviceID: NDASInfo.Units[unitCount].deviceID];
					
					//unitBSDName = [[NSString alloc] initWithCString: NDASInfo.Units[unitCount].BSDName];
					//[unit setBSDName: unitBSDName];
					
//					[unitModel autorelease];
//					[unitFirmware autorelease];
//					[unitSerialNumber autorelease];
					//[unitBSDName autorelease];
					
					[entry addUnitDevice: unit ForKey: [unit unitNumber]];
					
					[unit release];
					
				} else {
					[entry deleteUnitDeviceForKey: unitCount];
				}
			}
			
			//NSLog(@"Status %d", NDParam.ndsStatus);
			
			[NDASDevices setObject: entry forKey: [NSNumber numberWithInt: [entry slotNo]]];
			
			[entry release];
			
//			[IDForDisplay autorelease];				
//			[NameForDisplay autorelease];				
			
			// Refresh 
			//[ndasOutlineView reloadItem: entry reloadChildren: YES];
			
		}
	}			
	
	if (indexArray) {
		[indexArray removeAllObjects];
		[indexArray release];
	}
	
	// Delete Removed Entry.
	indexArray = [NSMutableArray arrayWithCapacity:0];
	enumerator = [NDASDevices objectEnumerator];
	
	while ((device = [enumerator nextObject])) {
		if ([device isRemoved]) {
			[indexArray addObject:[NSNumber numberWithInt: [device slotNo]]];
		}
	}

	enumerator = [indexArray objectEnumerator];
	
	id key;
	
	while((key = [enumerator nextObject])) {
		[NDASDevices removeObjectForKey: key];
	}	
	
	//
	// Get RAID Devices.
	//
	int index = 0;
	
	NDASRAIDDeviceInformation	RAIDInfo = { 0 };
	
	enumerator = [RAIDDevices objectEnumerator];
	RAIDDevice *raidDevice;
	
	// Mark removed all
	while ((raidDevice = [enumerator nextObject])) {
		[raidDevice setRemoved: YES];
	}
	
	indexArray = (NSMutableArray *)NDASGetInformationCreateIndexArray(NDAS_RAID_DEVICE_CLASS_NAME);
	
	if(indexArray == NULL) {
		numberOfObjects = 0;
	} else {
		numberOfObjects = [indexArray count];
	}
		
	for (count = 0; count < numberOfObjects; count++) {
		
		RAIDInfo.index = [[indexArray objectAtIndex:count] intValue];
	
		if(NDASGetRAIDDeviceInformationByIndex(&RAIDInfo)) {
			
			RAIDDevice	*entry;
			NSString	*NameForDisplay;

			if ( !(entry = [RAIDDevices objectForKey: [NSNumber numberWithInt: RAIDInfo.index]] ) ) {
				// Create New Entry.
				entry = [[RAIDDevice alloc] init];
			} else {
				[entry retain];
				[entry setRemoved: NO];
			}
			
			NameForDisplay = [NSString stringWithUTF8String:RAIDInfo.name];
			
			[entry setName: NameForDisplay];			
			[entry setConfiguration: RAIDInfo.configuration];
			[entry setStatus: RAIDInfo.status];
			[entry setIndex: RAIDInfo.index];
			[entry setWritable: RAIDInfo.writable];
			[entry setUnitDeviceType: RAIDInfo.unitDeviceType];
			[entry setDiskArrayType: RAIDInfo.diskArrayType];
			[entry setCapacity: RAIDInfo.capacity];
			[entry setRWAccess: RAIDInfo.RWHosts];
			[entry setROAccess: RAIDInfo.ROHosts];
			[entry setRAIDStatus : RAIDInfo.RAIDStatus];
			
			[entry setRecoveringSector : RAIDInfo.RecoveringSector];
			
			[entry setCountOfUnitDevices: RAIDInfo.CountOfUnitDevices];
			[entry setCountOfSpareDevices: RAIDInfo.CountOfSpareDevices];
			
			int subCount;
			
			for (subCount = 0; subCount < NDAS_MAX_UNITS_IN_V2; subCount++) {
				if (subCount < [entry CountOfUnitDevices] + [entry CountOfSpareDevices]) {
					RAIDSubDevice	*subUnit;
					
					if ( !( subUnit = [entry subUnitAtIndex: subCount]) ) {
						// Create New Unit.
						subUnit = [[RAIDSubDevice alloc] init];
					} else {
						[subUnit retain];
					}
					
					[subUnit setSubUnitNumber: subCount];
					[subUnit setDeviceID: RAIDInfo.unitDiskLocationOfSubUnits[subCount]];
					[subUnit setRAIDDeviceID: entry];
					[subUnit setSubUnitStatus: RAIDInfo.RAIDSubUnitStatus[subCount]];
					[subUnit setSubUnitType: RAIDInfo.RAIDSubUnitType[subCount]];
					
					[entry addSubUnitDevice: subUnit ForKey: [subUnit subUnitNumber]];
					
					[subUnit release];
				} else {
					[entry deleteSubUnitDeviceForKey: subCount];
				}
			}
			
			[RAIDDevices setObject: entry forKey: [NSNumber numberWithInt: [entry index]]];
			
			[entry release];
			
			//[NameForDisplay autorelease];
			
			// Refresh 
			//[ndasOutlineView reloadItem: entry reloadChildren: YES];
			
		}
	}

	if(indexArray) {
		[indexArray removeAllObjects];
		[indexArray release];
	}
	
	// Delete Removed Entry.
	enumerator = [RAIDDevices objectEnumerator];
	
	while ((raidDevice = [enumerator nextObject])) {
		if ([raidDevice isRemoved]) {
			[RAIDDevices removeObjectForKey: [NSNumber numberWithInt: [raidDevice index]]];
		}
	}
		
	[ndasOutlineView reloadData];

/*	
	// Expand All.
	for (index = 0; index < [[NDASDevices allValues] count]; index++) {
		[ndasOutlineView expandItem: [[NDASDevices allValues] objectAtIndex: index]];
	}	
 */
}


- (void) changeDisplayedInformationsAtIndex: (int)index
{
	id			device = nil;
	NDASDevice	*ndas = nil;
	UnitDevice	*unit = nil;
	RAIDDevice	*RAID = nil;
	
	device = [ ndasOutlineView itemAtRow:index ];
	
	if ( nil == device ) {
		return;
	}
		
	[writeKeyButton setEnabled: NO];
	[manageWriteKeyMenuItem setEnabled: NO];
	[RAIDMigrateMenuItem setEnabled: NO];
	[RAIDRebindMenuItem setEnabled: NO];
//	[nameField setEditable: NO];
	
	switch ([(Device *)device deviceType]) {
		case kNDASDeviceTypeNDAS:
		{
			ndas = (NDASDevice *)device;
			
			[RAIDUnbindMenuItem setEnabled: FALSE];
			[refreshMenuItem setEnabled: FALSE];
		}
			break;
		case kNDASDeviceTypeUnit:
		{
			unit = device;
			ndas = [unit ndasId];
			
			[RAIDUnbindMenuItem setEnabled: FALSE];
			[refreshMenuItem setEnabled: TRUE];
		}
			break;
		case kNDASDeviceTypeRAID:
		{
			RAID = device;			
		
			// Unbind Control
			if ( ![RAID isInRunningStatus]
				 && ( NMT_SINGLE == [RAID diskArrayType] ||
					  NMT_INVALID == [RAID diskArrayType])
				 && 0 == [RAID ROAccess]
				 && 0 == [RAID RWAccess]) {
				 
				[RAIDSetUnbindButton setEnabled: YES];
				[RAIDUnbindMenuItem setEnabled: YES];
			} else {
				[RAIDSetUnbindButton setEnabled: NO];
				[RAIDUnbindMenuItem setEnabled: NO];
			}
			
			// Migrate Control.
			switch ([RAID unitDeviceType]) {
				case NMT_MIRROR:
				case NMT_RAID1:
				case NMT_RAID1R2:
				{
					[RAIDMigrateMenuItem setEnabled: YES];
					[RAIDSetMigrateActionItem setEnabled: YES];
				}
					break;
				default:
				{
					[RAIDMigrateMenuItem setEnabled: NO];
					[RAIDSetMigrateActionItem setEnabled: NO];
				}
					break;
			}
			
			// Rebind Control.
			if ((kNDASRAIDStatusBadRAIDInformation == [RAID RAIDStatus]
				 || kNDASRAIDStatusGoodWithReplacedUnit == [RAID RAIDStatus]
				 || kNDASRAIDStatusReadyToRecoveryWithReplacedUnit == [RAID RAIDStatus])
				&& 0 == [RAID RWAccess]
				&& 0 == [RAID ROAccess]) {
				[RAIDRebindMenuItem setEnabled: YES];
				[RAIDSetRebindActionItem setEnabled: YES];
			} else {
				[RAIDRebindMenuItem setEnabled: NO];
				[RAIDSetRebindActionItem setEnabled: NO];
			}
			[refreshMenuItem setEnabled: TRUE];
		}
			break;
	}
	
	if (ndas) {
		
		if (![ndas autoRegister]) {
			[writeKeyButton setEnabled: YES];
			[manageWriteKeyMenuItem setEnabled: YES];
		}
		[nameField setEditable: YES];
		
		// Name
		[nameField setStringValue:[ndas name]];
		
		// Device ID String
		[idField setStringValue:[ndas deviceIDString]];
		
		// Write Key.
		if([ndas writable]) {
			[writeKeyField setStringValue:  NSLocalizedString(@"TAB_DEVICE_WRITE_KEY : Present", @"The NDAS device is registered with write key")];
			[writeKeyButton setTitle:  NSLocalizedString(@"TAB_DEVICE_WRITE_KEY_BUTTON : Remove Write Key", @"The text of the button to remove write key of the selected NDAS device")];
			[manageWriteKeyMenuItem setTitle:  NSLocalizedString(@"MENU_MANAGEMENT_WRITE_KEY : Remove Write Key", @"The text of the menu to remove write key of the selected NDAS device")];
		} else {
			[writeKeyField setStringValue:  NSLocalizedString(@"TAB_DEVICE_WRITE_KEY : Not Present", @"The NDAS device is registered without write key")];
			[writeKeyButton setTitle:  NSLocalizedString(@"TAB_DEVICE_WRITE_KEY_BUTTON : Add Write Key", @"The text of the button to add write key to the selected NDAS device")];
			[manageWriteKeyMenuItem setTitle:  NSLocalizedString(@"MENU_MANAGEMENT_WRITE_KEY : Add Write Key...", @"The text of the menu to add write key to the selected NDAS device")];
		}
		
		// Status.
		switch ([ndas status]) {
			case kNDASStatusOffline: 
			{
				[statusField setStringValue: NSLocalizedString(@"TAB_DEVICE_STATUS : Offline", @"The NDAS device is off or not connected via ethernet")];
				[statusField setTextColor: [NSColor blackColor]];
			}
				break;
			case kNDASStatusOnline:
			{
				[statusField setStringValue: NSLocalizedString(@"TAB_DEVICE_STATUS : Online", @"The NDAS device is on and connected via ethernet")];
				[statusField setTextColor: [NSColor blackColor]];
			}
				break;
			case kNDASStatusNoPermission:
			{
				[statusField setStringValue: NSLocalizedString(@"TAB_DEVICE_STATUS : No Permission", @"NDAS Status No Permission")];
				[statusField setTextColor: [NSColor redColor]];
			}
				break;	
			case kNDASStatusNeedRegisterKey:
			{
				[statusField setStringValue: NSLocalizedString(@"TAB_DEVICE_STATUS : NDAS Key is necessary", @"NDAS Status Need Register Key")];
				[statusField setTextColor: [NSColor redColor]];
			}
				break;	
			case kNDASStatusFailure:
			default:
			{
				[statusField setStringValue: NSLocalizedString(@"TAB_DEVICE_STATUS : Failure", @"Access failed to the NDAS device for some reason")];
				[statusField setTextColor: [NSColor redColor]];
			}
				break;
		}
		
		if ( kNDASStatusOnline == [ndas status] ) {
			// Version String.
			[versionField setStringValue: [ndas versionString]];
			
			// Max Request Block.
			[maxRequestBlocksField setStringValue: [NSString stringWithFormat: @"%d", [ndas maxRequestBlocks]]];
			
		} else {
			// empty string does not clear place holder text. use 1 char length text
			[versionField setStringValue: @" "];
			[maxRequestBlocksField setStringValue: @" "];
		}
		
		// MAC Address.
		[MACAddressField setStringValue: [NSString stringWithFormat: @"%02X:%02X:%02X:%02X:%02X:%02X", [ndas MACAddress][0], [ndas MACAddress][1]
			, [ndas MACAddress][2], [ndas MACAddress][3]
			, [ndas MACAddress][4], [ndas MACAddress][5]]];
		
		// Enable Menus
		if (unit == nil) {
			if(![ndas autoRegister]) {
				[unregisterMenuItem setEnabled: TRUE];
				[saveNDASRegistrationFileMenuItem setEnabled: YES];
				[registerKeyMenuItem setEnabled: NO];
			} else {
				[saveNDASRegistrationFileMenuItem setEnabled: NO];

				switch([ndas status]) {
					case kNDASStatusOffline:
					{
						[registerKeyMenuItem setEnabled: NO];					
						[unregisterMenuItem setEnabled: YES];
					}
						break;
					case kNDASStatusOnline:
					{
						if ( [ndas isIdle] ) {
							[registerKeyMenuItem setEnabled: YES];
						} else {
							[registerKeyMenuItem setEnabled: NO];			
						}
						[unregisterMenuItem setEnabled: NO];
					}
						break;
					case kNDASStatusNeedRegisterKey:
					{
						[registerKeyMenuItem setEnabled: YES];					
						[unregisterMenuItem setEnabled: YES];
					}
						break;
					default:
					{
						[registerKeyMenuItem setEnabled: NO];					
						[unregisterMenuItem setEnabled: NO];
					}
						break;
				}			
			}
		} else {
			[unregisterMenuItem setEnabled: NO];
			[saveNDASRegistrationFileMenuItem setEnabled: NO];
			[registerKeyMenuItem setEnabled: NO];			
		}
	}
	
	// Unit Tag.
	if ( nil != unit ) {
		
		// Manage Write Key Button.
		[writeKeyButton setEnabled: NO];
		[manageWriteKeyMenuItem setEnabled: NO];
//		[nameField setEditable: NO];
			
		// Unit Number.
		[unitNumberField setStringValue: [NSString stringWithFormat: @"%d", [unit unitNumber]]];
		
		// Configuration.
		[unitConfigurationPopupButton setEnabled: TRUE];
		[unitConfigurationPopupButton selectItemAtIndex: [device configuration]];
		
		// Status.
		switch( [unit status] ) {
			case kNDASUnitStatusDisconnected:
			{
				[unitStatusField setStringValue: NSLocalizedString(@"TAB_UNIT_DEVICE_STATUS : Not Mounted", @"The NDAS unit device is not mounted")];
			}
				break;
			case kNDASUnitStatusConnectedRO:
			{
				[unitStatusField setStringValue: NSLocalizedString(@"TAB_UNIT_DEVICE_STATUS : Mounted With Read only access", @"The NDAS unit device is mounted read only")];
			}
				break;
			case kNDASUnitStatusConnectedRW:
			{
				[unitStatusField setStringValue: NSLocalizedString(@"TAB_UNIT_DEVICE_STATUS : Mounted With Read & Write access", @"The NDAS unit device is mounted read write")];
			}
				break;
			case kNDASUnitStatusTryConnectRO:
			{
				[unitStatusField setStringValue: NSLocalizedString(@"TAB_UNIT_DEVICE_STATUS : Mounting With Read only access", @"The NDAS unit device is being mounted read only")];
			}
				break;
			case kNDASUnitStatusTryConnectRW:
			{
				[unitStatusField setStringValue: NSLocalizedString(@"TAB_UNIT_DEVICE_STATUS : Mounting With Read & Write access", @"The NDAS unit device is being mounted read write")];
			}
				break;
			case kNDASUnitStatusTryDisconnectRO:
			{
				[unitStatusField setStringValue: NSLocalizedString(@"TAB_UNIT_DEVICE_STATUS : Unmounting With Read only access", @"The NDAS unit device is being unmounted read only")];
			}
				break;
			case kNDASUnitStatusTryDisconnectRW:
			{
				[unitStatusField setStringValue: NSLocalizedString(@"TAB_UNIT_DEVICE_STATUS : Unmounting With Read & Write access", @"The NDAS unit device is being unmounted read write")];
			}
				break;
			case kNDASUnitStatusReconnectRO:
			{
				[unitStatusField setStringValue: NSLocalizedString(@"TAB_UNIT_DEVICE_STATUS : Reconnecting With Read only access", @"The NDAS unit device is being reconnected read only")];
			}
				break;
			case kNDASUnitStatusReconnectRW:
			{
				[unitStatusField setStringValue: NSLocalizedString(@"TAB_UNIT_DEVICE_STATUS : Reconnecting With Read & Write access", @"The NDAS unit device is being reconnected read write")];
			}
				break;				
			case kNDASUnitStatusNotPresent:				
			{
				[unitStatusField setStringValue: NSLocalizedString(@"TAB_UNIT_DEVICE_STATUS : Not Present", @"The NDAS unit device is not present in the NDAS device")];				
			}
				break;
			case kNDASUnitStatusFailure:
			default:
			{
				[unitStatusField setStringValue: NSLocalizedString(@"TAB_UNIT_DEVICE_STATUS : Failure", @"Failed to access the NDAS unit device")];
			}
				break;
		}
		
		// Menu Configuration.
		switch( [unit configuration] ) {
			case kNDASUnitConfigurationUnmount:
				[unmountMenuItem setState: NSOnState];
				[mountROMenuItem setState: NSOffState];
				[mountRWMenuItem setState: NSOffState];
				break;
			case kNDASUnitConfigurationMountRO:
				[unmountMenuItem setState: NSOffState];
				[mountROMenuItem setState: NSOnState];
				[mountRWMenuItem setState: NSOffState];
				break;
			case kNDASUnitConfigurationMountRW:
				[unmountMenuItem setState: NSOffState];
				[mountROMenuItem setState: NSOffState];
				[mountRWMenuItem setState: NSOnState];
				break;
		}

		// RO access.
		[unitROAccessField setStringValue: [NSString stringWithFormat: @"%d", [unit ROAccess]]];
		
		// RW access.
		[unitRWAccessField setStringValue: [NSString stringWithFormat: @"%d", [unit RWAccess]]];
				
		// Enable Menus for Unit.
	
		switch([unit configuration]) {
			case kNDASUnitConfigurationUnmount:
				
				[unmountMenuItem setEnabled: TRUE];
				[unmountPopupItem setEnabled: TRUE];
				
				[mountROMenuItem setEnabled: TRUE];			
				[mountROPopupItem setEnabled: TRUE];
				
				if ( [ndas writable] ) {
					[mountRWMenuItem setEnabled: TRUE];
					[mountRWPopupItem setEnabled: TRUE];
				} else {
					[mountRWMenuItem setEnabled: NO];
					[mountRWPopupItem setEnabled: NO];
				}
					break;
				
			case kNDASUnitConfigurationMountRO:
				
				[unmountMenuItem setEnabled: TRUE];
				[unmountPopupItem setEnabled: TRUE];
				
				[mountROMenuItem setEnabled: TRUE];			
				[mountROPopupItem setEnabled: TRUE];
				
				[mountRWMenuItem setEnabled: FALSE];
				[mountRWPopupItem setEnabled: FALSE];
				
				break;
			case kNDASUnitConfigurationMountRW:
				
				[unmountMenuItem setEnabled: TRUE];
				[unmountPopupItem setEnabled: TRUE];
				
				[mountROMenuItem setEnabled: FALSE];			
				[mountROPopupItem setEnabled: FALSE];
				
				[mountRWMenuItem setEnabled: TRUE];
				[mountRWPopupItem setEnabled: TRUE];
				
				break;
		}
		
		// Type.
		switch([unit type]) {
			case kNDASUnitMediaTypeHDD:

				[unitTypeField setStringValue:  NSLocalizedString(@"TAB_UNIT_DEVICE_TYPE : HDD", @"Hard Disk Drive")];
				[unitImage setImage: [NSImage imageNamed:@"Disk"]];
				
				// Capacity.
				[unitCapacityField setStringValue: [unit capacityToString]];
										
				// RAID Type.
				[unitDiskArrayTypeField setStringValue: printRAIDType([unit diskArrayType])];

				break;
			case kNDASUnitMediaTypeODD:
				
				[unitTypeField setStringValue:  NSLocalizedString(@"TAB_UNIT_DEVICE_TYPE : ODD", @"CD-ROM, DVD ...")];
				
				[unitImage setImage: [NSImage imageNamed:@"ODD"]];
				
				[unitCapacityField setStringValue: NSLocalizedString(@"TAB_UNIT_DEVICE_CAPACITY : N/A", @"Cannot get capacity of the unit device")];
				
				[unitDiskArrayTypeField setStringValue: NSLocalizedString(@"TAB_UNIT_DEVICE_RAID_TYPE : N/A", @"The unit device does not support RAID")];
				
				break;				
			default:
				[unitTypeField setStringValue:  NSLocalizedString(@"TAB_UNIT_DEVICE_TYPE : Unknown", @"Unknown type. May result of IDE_IDENTIFY but not now.")];
				
				[unitCapacityField setStringValue: NSLocalizedString(@"TAB_UNIT_DEVICE_CAPACITY : N/A", @"Cannot get capacity of the unit device")];
				
				[unitDiskArrayTypeField setStringValue: NSLocalizedString(@"TAB_UNIT_DEVICE_RAID_TYPE : N/A", @"The unit device does not support RAID")];
				
				break;				
		}
		
		// Model.
		[unitModelField setStringValue: [unit model]];
		
		// Firmware.
		[unitFirmwareField setStringValue: [unit firmware]];
		
		// Serial Number.
		[unitSNField setStringValue: [unit serialNumber]];
							
		switch ( [unit transferMode] >> 4 ) {
			case 0:
			{
				[unitTranferModeField setStringValue: [NSString stringWithFormat: NSLocalizedString(@"TAB_UNIT_DEVICE_TRANSFER_MODE : PIO %u(level)",@"Programmed I/O"), [unit transferMode] & 0x07]];
			}
				break;
			case 1:
			{
				[unitTranferModeField setStringValue: [NSString stringWithFormat: NSLocalizedString(@"TAB_UNIT_DEVICE_TRANSFER_MODE : Single-word DMA %u(level)", @"Single word DMA"), [unit transferMode] & 0x0f]];
			}
				break;
			case 2:
			{
				[unitTranferModeField setStringValue: [NSString stringWithFormat: NSLocalizedString(@"TAB_UNIT_DEVICE_TRANSFER_MODE : Multi-word DMA %u(level)", @"Multi word DMA"), [unit transferMode] & 0x0f]];
			}
				break;
			case 4:
			{
				[unitTranferModeField setStringValue: [NSString stringWithFormat: NSLocalizedString(@"TAB_UNIT_DEVICE_TRANSFER_MODE : Ultra-DMA %u(level)", @"Ultra DMA"), [unit transferMode] & 0x0f]];
			}
				break;
			default:
				[unitTranferModeField setStringValue: NSLocalizedString(@"TAB_UNIT_DEVICE_TRANSFER_MODE : Undefined", @"Undefined Transfer Mode")];
		}
		
		if (NMT_SINGLE != [unit diskArrayType] 
			&& NMT_CDROM != [unit diskArrayType]) {
			[unitConfigurationPopupButton setEnabled: NO];
		} else {
			[unitConfigurationPopupButton setEnabled: YES];
		}
			
//		[backgroundInformationField setStringValue: @""];
	}
		
// MARK: mark here
// TODO: RAID tab should be merged with single device tab later
// TODO: create method for size string - search this sprintf(tempBuffer, "%llu B", buffer);
// FIXME: mark here
// !!!: mark here
// ???: mark here
	if (RAID) {
		
		// RAID Unit Tab.
			
		// name.
		[RAIDUnitNameField setStringValue:[RAID name]];
		
		// Configuration.
		[RAIDUnitConfigurationPopupButton setEnabled: TRUE];
		[RAIDUnitConfigurationPopupButton selectItemAtIndex: [RAID configuration]];
		
		// Status.
		switch( [RAID status] ) {
			case kNDASUnitStatusDisconnected:
			{
				[RAIDUnitStatusField setStringValue: NSLocalizedString(@"TAB_RAID_UNIT_DEVICE_STATUS : Not Mounted", @"The NDAS RAID unit device is not mounted")];
			}
				break;
			case kNDASUnitStatusConnectedRO:
			{
				[RAIDUnitStatusField setStringValue: NSLocalizedString(@"TAB_RAID_UNIT_DEVICE_STATUS : Mounted With Read only access", @"The NDAS raid unit device is mounted read only")];
			}
				break;
			case kNDASUnitStatusConnectedRW:
			{
				[RAIDUnitStatusField setStringValue: NSLocalizedString(@"TAB_RAID_UNIT_DEVICE_STATUS : Mounted With Read & Write access", @"The NDAS raid unit device is mounted read write")];
			}
				break;
			case kNDASUnitStatusTryConnectRO:
			{
				[RAIDUnitStatusField setStringValue: NSLocalizedString(@"TAB_RAID_UNIT_DEVICE_STATUS : Mounting With Read only access", @"The NDAS raid unit device is being mounted read only")];
			}
				break;
			case kNDASUnitStatusTryConnectRW:
			{
				[RAIDUnitStatusField setStringValue: NSLocalizedString(@"TAB_RAID_UNIT_DEVICE_STATUS : Mounting With Read & Write access", @"The NDAS raid unit device is being mounted read write")];
			}
				break;
			case kNDASUnitStatusTryDisconnectRO:
			{
				[RAIDUnitStatusField setStringValue: NSLocalizedString(@"TAB_RAID_UNIT_DEVICE_STATUS : Unmounting With Read only access", @"The NDAS raid unit device is being unmounted read only")];
			}
				break;
			case kNDASUnitStatusTryDisconnectRW:
			{
				[RAIDUnitStatusField setStringValue: NSLocalizedString(@"TAB_RAID_UNIT_DEVICE_STATUS : Unmounting With Read & Write access", @"The NDAS raid unit device is being unmounted read write")];
			}
				break;
			case kNDASUnitStatusReconnectRO:
			{
				[RAIDUnitStatusField setStringValue: NSLocalizedString(@"TAB_RAID_UNIT_DEVICE_STATUS : Remounting With Read only access", @"The NDAS raid unit device is being reconnected read only")];
			}
				break;
			case kNDASUnitStatusReconnectRW:
			{
				[RAIDUnitStatusField setStringValue: NSLocalizedString(@"TAB_RAID_UNIT_DEVICE_STATUS : Remounting With Read & Write access", @"The NDAS raid unit device is being reconnected read write")];
			}
				break;				
			case kNDASUnitStatusNotPresent:				
			{
				[RAIDUnitStatusField setStringValue: NSLocalizedString(@"TAB_RAID_UNIT_DEVICE_STATUS : Not Present", @"The NDAS raid unit device is not present in the NDAS device")];				
			}
				break;
			case kNDASUnitStatusFailure:
			default:
			{
				[RAIDUnitStatusField setStringValue: NSLocalizedString(@"TAB_RAID_UNIT_DEVICE_STATUS : Failure", @"Failed to access the NDAS raid unit device")];
			}
				break;
		}
		
		// Menu Configuration.
		switch( [RAID configuration] ) {
			case kNDASUnitConfigurationUnmount:
				[unmountMenuItem setState: NSOnState];
				[mountROMenuItem setState: NSOffState];
				[mountRWMenuItem setState: NSOffState];
				break;
			case kNDASUnitConfigurationMountRO:
				[unmountMenuItem setState: NSOffState];
				[mountROMenuItem setState: NSOnState];
				[mountRWMenuItem setState: NSOffState];
				break;
			case kNDASUnitConfigurationMountRW:
				[unmountMenuItem setState: NSOffState];
				[mountROMenuItem setState: NSOffState];
				[mountRWMenuItem setState: NSOnState];
				break;
		}

		// RO access.
		[RAIDUnitROAccessField setStringValue: [NSString stringWithFormat: @"%d", [RAID ROAccess]]];
		
		// RW access.
		[RAIDUnitRWAccessField setStringValue: [NSString stringWithFormat: @"%d", [RAID RWAccess]]];
				
		// Enable Menus for Unit.
		switch([RAID status]) {
			case kNDASUnitStatusNotPresent:
			case kNDASUnitStatusFailure:
			{
				if (kNDASUnitConfigurationUnmount == [RAID configuration]) { 
					[unmountMenuItem setEnabled: FALSE];
					[RAIDUnitUnmountPopupItem setEnabled: FALSE];
				} else {
					if (NMT_RAID1R3 == [RAID unitDeviceType]) {
						[unmountMenuItem setEnabled: TRUE];
						[RAIDUnitUnmountPopupItem setEnabled: TRUE];
					} else {
						[unmountMenuItem setEnabled: FALSE];
						[RAIDUnitUnmountPopupItem setEnabled: FALSE];
					}
				}

				[mountROMenuItem setEnabled: FALSE];			
				[RAIDUnitMountROPopupItem setEnabled: FALSE];
				
				[mountRWMenuItem setEnabled: FALSE];
				[RAIDUnitMountRWPopupItem setEnabled: FALSE];							
			}
				break;
			default:
			{
				switch([RAID configuration]) {
					case kNDASUnitConfigurationUnmount:
						
						[unmountMenuItem setEnabled: TRUE];
						[RAIDUnitUnmountPopupItem setEnabled: TRUE];
						
						[mountROMenuItem setEnabled: TRUE];			
						[RAIDUnitMountROPopupItem setEnabled: TRUE];
						
						if ( [RAID writable] ) {
							[mountRWMenuItem setEnabled: TRUE];
							[RAIDUnitMountRWPopupItem setEnabled: TRUE];
						} else {
							[mountRWMenuItem setEnabled: FALSE];
							[RAIDUnitMountRWPopupItem setEnabled: FALSE];
						}
							break;
						
					case kNDASUnitConfigurationMountRO:
						
						[unmountMenuItem setEnabled: TRUE];
						[RAIDUnitUnmountPopupItem setEnabled: TRUE];
						
						[mountROMenuItem setEnabled: TRUE];			
						[RAIDUnitMountROPopupItem setEnabled: TRUE];
						
						[mountRWMenuItem setEnabled: FALSE];
						[RAIDUnitMountRWPopupItem setEnabled: FALSE];
						
						break;
					case kNDASUnitConfigurationMountRW:
						
						[unmountMenuItem setEnabled: TRUE];
						[RAIDUnitUnmountPopupItem setEnabled: TRUE];
						
						[mountROMenuItem setEnabled: FALSE];			
						[RAIDUnitMountROPopupItem setEnabled: FALSE];
						
						[mountRWMenuItem setEnabled: TRUE];
						[RAIDUnitMountRWPopupItem setEnabled: TRUE];
						
						break;
				}
			}
		}
				
		[RAIDUnitDiskArrayTypeField setStringValue: printRAIDType([RAID diskArrayType])];
		
		// Unit Device Type.
		[RAIDUnitTypeField setStringValue: printRAIDType([RAID unitDeviceType])];
		
		[RAIDUnitImage setImage: [NSImage imageNamed:@"RAID"]];
		
		// Capacity.
		[RAIDUnitCapacityField setStringValue: [RAID capacityToString]];

		if (NMT_SINGLE != [RAID diskArrayType] ) {
			[unitConfigurationPopupButton setEnabled: NO];
		} else {
			[unitConfigurationPopupButton setEnabled: YES];
		}

		[RAIDSetOutlineView reloadData];

		
		// Name
		[RAIDSetNameField setStringValue:[RAID name]];

		// RAID Type.
		[RAIDSetTypeField setStringValue: printRAIDType([RAID unitDeviceType])];

		// Set Size.
		[RAIDSetSizeField setStringValue: [RAID capacityToString]];
		
		// RAID Status.
		[RAIDUnitRAIDStatusField setTextColor: [NSColor blackColor]];
		switch([RAID RAIDStatus]) {
			case kNDASRAIDStatusNotReady:
				[RAIDUnitRAIDStatusField setStringValue: NSLocalizedString(@"TAB_RAID_UNIT_DEVICE_STATUS : Not Ready", @"Not Ready RAID Status")];
				break;
			case kNDASRAIDStatusBadRAIDInformation:
			case kNDASRAIDStatusReadyToRecoveryWithReplacedUnit:
				[RAIDUnitRAIDStatusField setStringValue: NSLocalizedString(@"TAB_RAID_UNIT_DEVICE_STATUS : Bad RAID Information", @"Bad RAID Information RAID Status")];
				[RAIDUnitRAIDStatusField setTextColor: [NSColor redColor]];
				break;				
			case kNDASRAIDStatusBroken:
				[RAIDUnitRAIDStatusField setStringValue: NSLocalizedString(@"TAB_RAID_UNIT_DEVICE_STATUS : Broken", @"Broken RAID Status")];
				[RAIDUnitRAIDStatusField setTextColor: [NSColor redColor]];
				break;
			case kNDASRAIDStatusReadyToRecovery:
			{
				if (0 < [RAID RWAccess]
					&& kNDASUnitStatusConnectedRW != [RAID status]) {
					[RAIDUnitRAIDStatusField setStringValue: NSLocalizedString(@"TAB_RAID_UNIT_DEVICE_STATUS : Recovering by Another Host", @"Recovering by Another Host RAID Status")];
				} else {
					[RAIDUnitRAIDStatusField setStringValue: NSLocalizedString(@"TAB_RAID_UNIT_DEVICE_STATUS : Ready To Recovery", @"Ready To Recovery RAID Status")];
				}
				
				[RAIDUnitRAIDStatusField setTextColor: [NSColor redColor]];
			}
				break;
			case kNDASRAIDStatusRecovering:
				[RAIDUnitRAIDStatusField setStringValue: NSLocalizedString(@"TAB_RAID_UNIT_DEVICE_STATUS : Recovering", @"Recovering RAID Status")];
				[RAIDUnitRAIDStatusField setTextColor: [NSColor redColor]];
				break;
			case kNDASRAIDStatusBrokenButWorkable:
				[RAIDUnitRAIDStatusField setStringValue: NSLocalizedString(@"TAB_RAID_UNIT_DEVICE_STATUS : Broken But Workable", @"Broken But Workable RAID Status")];
				break;
			case kNDASRAIDStatusGoodButPartiallyFailed:
				[RAIDUnitRAIDStatusField setStringValue: NSLocalizedString(@"TAB_RAID_UNIT_DEVICE_STATUS : Good But Partially Failed", @"Good But Partially Failed RAID Status")];
				break;
			case kNDASRAIDStatusGood:
				[RAIDUnitRAIDStatusField setStringValue: NSLocalizedString(@"TAB_RAID_UNIT_DEVICE_STATUS : Good", @"Good RAID Status")];
				break;
			case kNDASRAIDStatusGoodWithReplacedUnit:
				[RAIDUnitRAIDStatusField setStringValue: NSLocalizedString(@"TAB_RAID_UNIT_DEVICE_STATUS : Invalid RAID Information", @"Invalid RAID Information RAID Status")];
				break;				
			case kNDASRAIDStatusUnsupported:
				[RAIDUnitRAIDStatusField setStringValue: NSLocalizedString(@"TAB_RAID_UNIT_DEVICE_STATUS : Unsupported", @"Unsupported RAID Set")];
				break;				
			default:
				[RAIDUnitRAIDStatusField setStringValue: NSLocalizedString(@"TAB_RAID_UNIT_DEVICE_STATUS : Undefined", @"Undefined RAID Status")];
				break;
		}
		
		// Recovering Status
		if (kNDASUnitStatusConnectedRW == [RAID status]
			&& kNDASRAIDStatusRecovering == [RAID RAIDStatus]) {
			
			UInt32	percentage = ([RAID RecoveringSector] * 100 / ([RAID capacity] / DISK_BLOCK_SIZE));
			
			[RAIDUnitRecoveringStatusTagField setHidden: FALSE];
			[RAIDUnitRecoveringStatusField setStringValue: [NSString stringWithFormat: @"%d%% (%lld of %lld)", percentage, [RAID RecoveringSector], ([RAID capacity] / DISK_BLOCK_SIZE)]];
			if (percentage == 0) {
				[RAIDUnitRecoveringStatusBar setDoubleValue: 0.1];
			} else {
				[RAIDUnitRecoveringStatusBar setDoubleValue: percentage];
			}
			[RAIDUnitRecoveringStatusBar startAnimation: self];
		} else {
			[RAIDUnitRecoveringStatusTagField setHidden: TRUE];
			[RAIDUnitRecoveringStatusField setStringValue: @""];
			[RAIDUnitRecoveringStatusBar setDoubleValue: 0];
			[RAIDUnitRecoveringStatusBar stopAnimation: self];
		}
				
	}
}
// TODO: backgroundInformationField should be create on runtime. It interferes with other resource tabs
- (void) updateInformation
{	
	// Update tabView.
	int index = [ndasOutlineView selectedRow];
	
//	NSLog(@"\n\n\n!!!!!!!!!!!! index %d !!!!!!!!!\n\n\n", index);
				
	if( 0 > index ||
		([RAIDDevices count] > 0 && nil == [ndasOutlineView itemAtRow: index])) {
		//Unselected or Select separator.
		if( !isTabClosing ) {
			// device is not selected. remove all tabs and show help message
//			NSDictionary *infoDic = [[NSBundle mainBundle] infoDictionary];
			
			while([tabView numberOfTabViewItems] > 0) {
				[tabView removeTabViewItem: [tabView tabViewItemAtIndex:0]];
			}
			
//			[tabView removeTabViewItem: generalTab];
//			[tabView removeTabViewItem: singleUnitTab];
			isTabClosing = TRUE;
			
			[backgroundInformationField setFrame: NSMakeRect(262, 197, 370, 45)];
			[backgroundInformationField setStringValue: NSLocalizedString(@"TAB_BACKGROUND : Select a NDAS device, or register a new one.", @"Instruction message. This text appears in the place of tab view when no deivce selected like 'Disk Utility'")];
		}
		
		// Disable Menus
		[unregisterMenuItem setEnabled: FALSE];
		[registerKeyMenuItem setEnabled: NO];
		[saveNDASRegistrationFileMenuItem setEnabled: NO];
		[unmountMenuItem setEnabled: FALSE];
		[mountROMenuItem setEnabled: FALSE];
		[mountRWMenuItem setEnabled: FALSE];
		[manageWriteKeyMenuItem setEnabled: FALSE];
		[RAIDUnbindMenuItem setEnabled: FALSE];
		[RAIDMigrateMenuItem setEnabled: FALSE];
		[RAIDRebindMenuItem setEnabled: FALSE];
		[refreshMenuItem setEnabled: FALSE];
		
		[unmountMenuItem setState: NSOffState];
		[mountROMenuItem setState: NSOffState];
		[mountRWMenuItem setState: NSOffState];
		
	} else {
		
		id	device;
		
		device = [ndasOutlineView itemAtRow: index];
				
		// Change Values.
		[self changeDisplayedInformationsAtIndex: index];
		
		// Display tabView.
		if( isTabClosing ) {
			[backgroundInformationField setStringValue: @""];
			
			switch([(UnitDevice *)device deviceType]) {
				case kNDASDeviceTypeNDAS:
				{
					[tabView insertTabViewItem: generalTab atIndex: 0];
					[tabView selectTabViewItemAtIndex: 0];

				}
					break;
				case kNDASDeviceTypeUnit:
				{
					[tabView insertTabViewItem: singleUnitTab atIndex: 0];
					[tabView selectTabViewItemAtIndex: 0];
				}
					break;
				case kNDASDeviceTypeRAID:
				{
					[tabView insertTabViewItem: RAIDUnitTab atIndex: 0];
					[tabView insertTabViewItem: RAIDTab atIndex: 1];
					[tabView selectTabViewItemAtIndex: 0];
				}
					break;
			}
			
			isTabClosing = FALSE;
		}
		
		[refreshMenuItem setEnabled: YES];

	}
		
	[RAIDBindSheetUnitOutlineView reloadData];
	[RAIDBindSheetSubUnitOutlineView reloadData];
	[(RAIDBindSheetController *)[RAIDBindSheet delegate] updateUI];
		
}

- (void) notificationSelector: (NSNotification *)notification
{
	NSDictionary	*reply = nil;
	NSString		*nssSenderID = nil;
	NSNumber		*nsnOperation = nil;
	NSNumber		*nsnErrorCode = nil;
	uint32_t		operation = 0;
	uint32_t		errorCode = 0;
	
//	NSLog(@"notification");
	
	reply = [notification userInfo];
	
	if ( nil != reply ) {
		
		nssSenderID = [reply objectForKey:[NSString stringWithCString: NDAS_NOTIFICATION_REPLY_SENDER_ID_KEY_STRING]];
		
		nsnOperation = [reply objectForKey:[NSString stringWithCString: NDAS_NOTIFICATION_REPLY_OPERATION_KEY_STING]];
		if ( nil != nsnOperation ) {
			operation = [nsnOperation unsignedIntValue];
		}
		
		nsnErrorCode = [reply objectForKey:[NSString stringWithCString: NDAS_NOTIFICATION_REPLY_ERROR_CODE_KEY_STRING]];
		if ( nil != nsnErrorCode ) {
			errorCode = [nsnErrorCode unsignedIntValue];
		}
		
		if ( nssSenderID && NSOrderedSame == [nssSenderID compare: IDStringForNotification] ) {
			// Reply for My Request.
	
			// Delay if the command is unregister.
			if (NDAS_NOTIFICATION_REQUEST_UNREGISTER == operation) {
				// Sleep 1 sec.
				[NSThread sleepUntilDate: [NSDate dateWithTimeIntervalSinceNow: 1]];
			}
			
			// Stop Busy Indicator.
			[progressIndicator stopAnimation: self];
							
			sentRequest = FALSE;
			
			if ( operation == recentRequest ) {
								
				switch ( operation ) {
					case NDAS_NOTIFICATION_REQUEST_REGISTER:
					{
						switch( errorCode ) {
							case NDAS_NOTIFICATION_ERROR_SUCCESS:
							{
								break;
							}
							case NDAS_NOTIFICATION_ERROR_ALREADY_REGISTERED:
							{
								NSRunAlertPanel(
												NSLocalizedString(@"PANEL-TITLE : This NDAS device was registered Already.", @"Info Already Registered"),
												NSLocalizedString(@"PANEL-MSG : If you want to see the ID of registered NDAS device, Select the NDAS device at the NDAS device list.", @"Info Already Registered"),
												NSLocalizedString(@"PANEL-BUTTON : OK", @"Text for button in panel"),
												nil,
												nil
												);
							}
							default:
								break;
						}
					}
						break;
					case NDAS_NOTIFICATION_REQUEST_SURRENDER_ACCESS:
					{
						switch ( errorCode ) {
							case NDAS_NOTIFICATION_ERROR_SUCCESS:
							{
								// If we got a good result, It doesn't mean We can use this one.
								
								/*
								// Search Logical Device.
								NDASLogicalDeviceInformation	LDInfo = { 0 };
								
								LDInfo.LowerUnitDeviceID = selectedLogicalDeviceLowerUnitID;
								
								if (NDASGetLogicalDeviceInformationByLowerUnitDeviceID(&LDInfo)) {
									
									[self _sndNotification : 0
												unitNumber : 0
													 index : LDInfo.index
												eNdCommand : NDAS_NOTIFICATION_REQUEST_CONFIGURATE_LOGICAL_DEVICE
													  Name : nil
													IDSeg1 : nil
													IDSeg2 : nil
													IDSeg3 : nil
													IDSeg4 : nil
													 WrKey : nil
												 eNdConfig : kNDASUnitConfigurationMountRW
												  eOptions : 0
											logicalDevices : nil
												  RAIDType : 0
													  Sync : false]; 
								}
								*/
								
							}
								break;
							default:
							{
								
								NSRunInformationalAlertPanel(
															 [NSString stringWithFormat: NSLocalizedString(@"PANEL-TITLE : '%@'(NDAS device name) cannot be mounted", @"Failed to mount the selected NDAS device for some reason."), [selectedDevice name]],
															 [NSString stringWithFormat: NSLocalizedString(@"PANEL-MSG : The computer that currently hold Read & Write access has denied your request or is not responding.\nThere may be another computer in the network running a different Operating System than Mac OS X that has mounted '%@' with Read & Write access. Please check and try again.", @"The reason why mounting failed."), [selectedDevice name]],
															 NSLocalizedString(@"PANEL-BUTTON : OK", @"Text for button in panel"),
															 nil,
															 nil
															 );
								 
							}
								break;
						}
					}
						break;
					default:
						break;
				}
			}
		}
	}
		
	if ( ! editingName ) { //&& ! sentRequest ) {		
		[self refreshAndUpdateOutlineView];
	}
}

- (void) periodicStatusChecker: (NSNotification *)notification
{
	BOOL		needRefresh = false;
	id			device = nil;
	RAIDDevice	*RAID = nil;
	
//	NSLog(@"periodicStatusChecker");
	
	if ( ! editingName && ! sentRequest ) {

		device = [ ndasOutlineView itemAtRow: [ndasOutlineView selectedRow] ];
		
		if ( nil == device ) {
			return;
		}
				
		switch ([(Device *)device deviceType]) {
			case kNDASDeviceTypeNDAS:
			{
			}
				break;
			case kNDASDeviceTypeUnit:
			{
			}
				break;
			case kNDASDeviceTypeRAID:
			{
				RAID = device;			
				
				if (kNDASRAIDStatusRecovering == [RAID RAIDStatus]) {
					needRefresh = true;
				}
			}
				break;
		}
	}

	 if(needRefresh) {
		[self refreshAndUpdateOutlineView];	
	}
	
	return;
}

- (void) refreshAndUpdateOutlineView
{
	id selectedItem;
	
	selectedItem = [ndasOutlineView itemAtRow: [ndasOutlineView selectedRow]];
	
	[self refreshNDASDevices];
	[self updateInformation];
	
	int row = [ndasOutlineView rowForItem: selectedItem];
	
	if ( -1 != row) {
		[ndasOutlineView selectRowIndexes: [NSIndexSet indexSetWithIndex: row] byExtendingSelection: NO];
	} else {
		[ndasOutlineView deselectAll: self];
	}	
}

- (void) outlineViewSelectionDidChanged: (NSNotification *)notification
{
//	NSLog(@"outlineViewSelectionDidChanged %d", [ndasOutlineView selectedRow]);
		
	[self refreshNDASDevices];
	[self updateInformation];
	
	if ( ! isTabClosing ) {
		
		// Close All tabs.
		while([tabView numberOfTabViewItems] > 0) {
			[tabView removeTabViewItem: [tabView tabViewItemAtIndex:0]];
		}
		
		isTabClosing = TRUE;
		
		switch ( [[ndasOutlineView itemAtRow: [ndasOutlineView selectedRow]] deviceType] ) {
			case kNDASDeviceTypeNDAS:
			{
				[tabView insertTabViewItem: generalTab atIndex: 0];
				[tabView selectTabViewItemAtIndex: 0];

				isTabClosing = FALSE;
			}
				break;
			case kNDASDeviceTypeUnit:
			{
				[tabView insertTabViewItem: singleUnitTab atIndex: 0];
				[tabView selectTabViewItemAtIndex: 0];
				
				isTabClosing = FALSE;
			}
				break;
			case kNDASDeviceTypeRAID:
			{
				[tabView insertTabViewItem: RAIDUnitTab atIndex: 0];
				[tabView insertTabViewItem: RAIDTab atIndex: 1];
				[tabView selectTabViewItemAtIndex: 0];
				
				isTabClosing = FALSE;
			}
				break;
		}		
	}
}

- (void) raidSheetOutlineViewSelectionDidChanged: (NSNotification *)notification
{
	NSLog(@"outlineViewSelectionDidChanged %d", [RAIDBindSheetUnitOutlineView selectedRow]);
	
	if ([RAIDBindSheetUnitOutlineView selectedRow] < 0) {
		[RAIDBindSheetErrorMessageField setStringValue: @" "];		
	} else {
		
		switch ( [[RAIDBindSheetUnitOutlineView itemAtRow: [RAIDBindSheetUnitOutlineView selectedRow]] deviceType] ) {
			case kNDASDeviceTypeNDAS:
			{
				//NDASDevice *device = [RAIDBindSheetUnitOutlineView itemAtRow: [RAIDBindSheetUnitOutlineView selectedRow]];
				
				[RAIDBindSheetErrorMessageField setStringValue: @" "];
			}
				break;
			case kNDASDeviceTypeUnit:
			{
				UnitDevice *unit = [RAIDBindSheetUnitOutlineView itemAtRow: [RAIDBindSheetUnitOutlineView selectedRow]];

				switch ([unit diskArrayType]) {
					case NMT_SINGLE:
					{
						if (kNDASUnitConfigurationUnmount != [unit configuration]) {
							[RAIDBindSheetErrorMessageField setStringValue: NSLocalizedString(@"PANEL_RAID_BIND : This is currently mounted on your system.", @"Not available as RAID device unit : Selected a mounted NDAS unit device on this system.")];	
						} else {
							if (0 < [unit ROAccess] || 0 < [unit RWAccess]) {
								[RAIDBindSheetErrorMessageField setStringValue: NSLocalizedString(@"PANEL_RAID_BIND : There is another computer in the network that has mounted this Device.", @"Not available as RAID device unit : Selected a mounted NDAS unit device on other system")];
							}
						}
					}
						break;
					case NMT_CDROM:
					{
						[RAIDBindSheetErrorMessageField setStringValue: NSLocalizedString(@"PANEL_RAID_BIND : This is a ODD Device.", @"Not available as RAID device unit : ODD Device")];
					}
						break;
					default:
					{
						[RAIDBindSheetErrorMessageField setStringValue: NSLocalizedString(@"PANEL_RAID_BIND : This is a member of another RAID set.", @"Not available as RAID device unit : Member of another RAID Set")];
					}
				}				
			}
				break;
			case kNDASDeviceTypeRAID:
			{
				[RAIDBindSheetErrorMessageField setStringValue: NSLocalizedString(@"PANEL_RAID_BIND : This is a RAID Device.", @"Not available as RAID device unit : RAID Device")];
			}
				break;
		}		
	}
}

// TODO: We don't need many lines of this method as we will use NDAS Utility only.	
- (void)awakeFromNib
{
	controller = self;

	NSDictionary *infoDic = [[NSBundle mainBundle] localizedInfoDictionary];
	if( nil == infoDic ) {
		NSLog(@"NDAS Utility: Can't get infoDic");
	}
		
	// Change Menu Titles.
	//NSString *tempString = [infoDic valueForKey:@"CFBundleName"];
	
	// Disable TabView
	generalTab = [tabView tabViewItemAtIndex: 0];
	[generalTab retain];
	singleUnitTab = [tabView tabViewItemAtIndex: 1];
	[singleUnitTab retain];
	RAIDUnitTab = [tabView tabViewItemAtIndex: 2];
	[RAIDUnitTab retain];
	RAIDTab = [tabView tabViewItemAtIndex: 3];
	[RAIDTab retain];
	
	[tabView removeTabViewItem: generalTab];
	[tabView removeTabViewItem: singleUnitTab];
	[tabView removeTabViewItem: RAIDUnitTab];
	[tabView removeTabViewItem: RAIDTab];
	isTabClosing = TRUE;

	// Get Model Name.
//	infoDic = [[NSBundle mainBundle] infoDictionary];
	
	NSRect tabViewSize = [tabView frame];
	
	[backgroundInformationField setFrame: NSMakeRect(tabViewSize.origin.x + 10, tabViewSize.origin.y + tabViewSize.size.height / 2 - 22.5, tabViewSize.size.width - 20, 45)];
	[backgroundInformationField setStringValue: NSLocalizedString(@"TAB_BACKGROUND : Select a NDAS device, or register a new one.", @"Instruction message. This text appears in the place of tab view when no deivce selected like 'Disk Utility'")];
	
	// Insert Custom cell types into the table view.
	NSTableColumn		*tableColumn = nil;
	ImageAndTextCell	*imageAndTextCell = nil;

	// NDAS List.
	imageAndTextCell = [[ImageAndTextCell alloc] init];
	[imageAndTextCell setEditable: YES];
	tableColumn = [ndasOutlineView tableColumnWithIdentifier: @"FirstColumn"];
	[tableColumn setDataCell: imageAndTextCell];
	[imageAndTextCell release];

	// RAID set List.
	imageAndTextCell = [[ImageAndTextCell alloc] init];
	[imageAndTextCell setEditable: YES];
	tableColumn = [RAIDSetOutlineView tableColumnWithIdentifier: @"RAIDIndex"];
	[tableColumn setDataCell: imageAndTextCell];
	[imageAndTextCell release];

	imageAndTextCell = [[ImageAndTextCell alloc] init];
	[imageAndTextCell setEditable: YES];
	tableColumn = [RAIDSetOutlineView tableColumnWithIdentifier: @"SubUnit"];
	[tableColumn setDataCell: imageAndTextCell];
	[imageAndTextCell release];

	// RAID Bind Sheet Unit List.
	imageAndTextCell = [[ImageAndTextCell alloc] init];
	[imageAndTextCell setEditable: YES];
	tableColumn = [RAIDBindSheetUnitOutlineView tableColumnWithIdentifier: @"FirstColumn"];
	[tableColumn setDataCell: imageAndTextCell];
	[imageAndTextCell release];

	imageAndTextCell = [[ImageAndTextCell alloc] init];
	[imageAndTextCell setEditable: YES];
	tableColumn = [RAIDBindSheetSubUnitOutlineView tableColumnWithIdentifier: @"SubUnit"];
	[tableColumn setDataCell: imageAndTextCell];
	[imageAndTextCell release];
	
	// Set to support D&D
	[RAIDBindSheetSubUnitOutlineView registerForDraggedTypes: [NSArray arrayWithObjects: DragDropSimplePboardType, NSStringPboardType, nil]];
/*	
	// For Tiger
	if ( [RAIDBindSheetSubUnitOutlineView respondsToSelector: @selector(setDraggingSourceOperationMask: forLocal:)] ) {
		[RAIDBindSheetSubUnitOutlineView setDraggingSourceOperationMask: NSDragOperationEvery forLocal: YES];
		[RAIDBindSheetSubUnitOutlineView setDraggingSourceOperationMask: NSDragOperationAll_Obsolete forLocal: YES];
	}
*/	
	// Unit Device Configuration.
	[mountROPopupItem setTitle: NSLocalizedString(@"TAB_UNIT_DEVICE_CONFIGURATION : Mount with Read only", @"Unit Configuration Mount RO")];
	[mountRWPopupItem setTitle: NSLocalizedString(@"TAB_UNIT_DEVICE_CONFIGURATION : Mount with Read & Write", @"Unit Configuration Mount RW")];
	
	// Menu Items.
	[[saveNDASRegistrationFileMenuItem menu] setAutoenablesItems: NO];
	[[registerMenuItem menu] setAutoenablesItems: NO];
	[[unregisterMenuItem menu] setAutoenablesItems: NO];
	[[registerKeyMenuItem menu] setAutoenablesItems: NO];
	[[unmountMenuItem menu] setAutoenablesItems: NO];
	[[mountROMenuItem menu] setAutoenablesItems: NO];
	[[mountRWMenuItem menu] setAutoenablesItems: NO];
	[[manageWriteKeyMenuItem menu] setAutoenablesItems: NO];
	[[refreshMenuItem menu] setAutoenablesItems: NO];
	[[RAIDMigrateMenuItem menu] setAutoenablesItems: NO];
	[[RAIDRebindMenuItem menu] setAutoenablesItems: NO];
	[[RAIDUnbindMenuItem menu] setAutoenablesItems: NO];	
	
	// Popup Menu Items.
	[[unmountPopupItem menu] setAutoenablesItems: NO];
	[[mountROPopupItem menu] setAutoenablesItems: NO];
	[[mountRWPopupItem menu] setAutoenablesItems: NO];
	
	// Catch user event from outline view.
	[[NSNotificationCenter defaultCenter] addObserver:self
											 selector:@selector(outlineViewSelectionDidChanged:)
												 name:NSOutlineViewSelectionDidChangeNotification
											   object:ndasOutlineView];

	// Catch event from NDAS Service.
	[[NSDistributedNotificationCenter defaultCenter] addObserver: self 
														selector: @selector(notificationSelector:) 
															name: @NDAS_UI_OBJECT_ID 
														  object: @NDAS_NOTIFICATION
											  suspensionBehavior: NSNotificationSuspensionBehaviorDeliverImmediately];

	// Catch user event from outline view of Bind sheet.
	[[NSNotificationCenter defaultCenter] addObserver:self
											 selector:@selector(raidSheetOutlineViewSelectionDidChanged:)
												 name:NSOutlineViewSelectionDidChangeNotification
											   object:RAIDBindSheetUnitOutlineView];
	
	// Create Timer.
	[NSTimer scheduledTimerWithTimeInterval: 1
									 target: self
								   selector: @selector(periodicStatusChecker:)
								   userInfo:nil
									repeats: YES];
	
	// call refreshNDASDevices twice to make sure items sorted
	[self refreshNDASDevices];
	
	[self updateInformation];
}

- (IBAction)manageWriteKey:(id)sender
{
	if ( sentRequest ) { 
		return;
	}
	
	int selectedRow = [ndasOutlineView selectedRow];
	
	if(-1 < selectedRow) {
		
		NDASPhysicalDeviceInformation	NDParam;
		NDASDevice						*ndasDevice;
		id								device;
		
		device = [ndasOutlineView itemAtRow: selectedRow];
		
		if( nil == device ||
			kNDASDeviceTypeNDAS != [(Device *)device deviceType] ) {
			return;
		}
		
		ndasDevice = (NDASDevice *)device;
		
		if ( [ndasDevice writable] ) {
			
			// Check Unit Device.
			int count;
			BOOL writeEnabled = FALSE;
			
			for(count = 0; count < MAX_NR_OF_TARGETS_PER_DEVICE; count++) {
				UnitDevice *unitDevice = [ndasDevice unitAtIndex: count];
				
				if ( nil != unitDevice &&
					 kNDASUnitConfigurationMountRW == [unitDevice configuration] ) {
				
					writeEnabled = TRUE;
					
					break;
				}
			}
			
			if ( writeEnabled ) {
				
				NSRunInformationalAlertPanel(
								[NSString stringWithFormat: NSLocalizedString(@"PANEL-TITLE : The write key of '%@'(NDAS device name) could not be removed", @"Failed to remove write key for some reason."), [ndasDevice name]],
								[NSString stringWithFormat: NSLocalizedString(@"PANEL-MSG : '%@'(NDAS device name) is currently mounted in Read & Write mode on your system. Please unmount the NDAS device and try again.", @"The reason why removing write key failed."), [ndasDevice name]],
								NSLocalizedString(@"PANEL-BUTTON : OK", @"Text for button in panel"),
								nil,
								nil
								);
				return;
			}
			
			// Delete Write Key.
			int answer = NSRunCriticalAlertPanel(
												 [NSString stringWithFormat: NSLocalizedString(@"PANEL-TITLE : Are you sure you want to remove write key of '%@'(NDAS device name)?", @"Confirm removing write key"), [ndasDevice name]],
												 NSLocalizedString(@"PANEL-MSG : You may add the write key again at a later time.", @"Confirm removing write key"),
											 NSLocalizedString(@"PANEL-BUTTON : Remove Write Key", @"Text for button in panel"),
											 NSLocalizedString(@"PANEL-BUTTON : Cancel", @"Text for button in panel"),
											 nil
											 );
			if( NSAlertDefaultReturn == answer ) {
				
				[self _sndNotification : [ndasDevice slotNo]
							unitNumber : 0								 
								 index : 0
							eNdCommand : NDAS_NOTIFICATION_REQUEST_EDIT_NDAS_WRITEKEY 
								  Name : nil
								IDSeg1 : nil
								IDSeg2 : nil
								IDSeg3 : nil
								IDSeg4 : nil
								 WrKey : nil
							 eNdConfig : 0
							  eOptions : 0
						logicalDevices : nil
							  RAIDType : 0
								  Sync : false]; 
				
			} 
						
		} else {
			// Enter Write Key.
			selectedDevice = ndasDevice;
			
			// Disable Enter Button.
			[enterWriteKeyButton setEnabled: false];
			
			// Cleanup.
			[enterWriteKeyField setStringValue:@""];
			[enterWriteKeyErrorField setStringValue:@" "];
			
			// Forcus Name field.
			[enterWriteKeyField selectText: self];
			
			[NSApp beginSheet: enterWriteKeySheet 
			   modalForWindow: [NSApp keyWindow]
				modalDelegate: self
			   didEndSelector: nil
				  contextInfo: nil];			
		}
	}
	
}

- (IBAction)close:(id)sender
{
	[NSApp terminate: self];
}

- (IBAction)registerDevice:(id)sender
{
	if ( sentRequest ) { 
		return;
	}
	
	// Disable Register Button.
	[registerRegisterButton setEnabled: false];
	[registerMenuItem setEnabled: NO];

	// Cleanup.
	[registerErrorString setStringValue:@" "];
	[registerIDField1 setStringValue:@""];
	[registerIDField2 setStringValue:@""];
	[registerIDField3 setStringValue:@""];
	[registerIDField4 setStringValue:@""];
	[registerWritekeyField setStringValue:@""];
	
	// Enter Default Name.
	NDASPhysicalDeviceInformation	NDASInfo;
	int slotNumber = 0;

	// Find Empty Slot Number.
	do {
		NDASInfo.slotNumber = ++slotNumber;
		if(!NDASGetInformationBySlotNumber(&NDASInfo)) {
			break;
		}
	} while(TRUE);
		
	[registerNameField setStringValue:[NSString stringWithFormat:@"NDAS %d", slotNumber]];
	
	[registerNameField setEnabled: YES];
	[registerIDField1 setEnabled: YES];
	[registerIDField2 setEnabled: YES];
	[registerIDField3 setEnabled: YES];
	
	// Forcus Name field.
	[registerNameField selectText: self];
	
	[registerWritekeyInfoField setStringValue: NSLocalizedString(@"PANEL_REGISTER : (Optional)", @"Text for Writekey Information for Device")];

	registerWriteKeyRequired = NO;
	
	[NSApp beginSheet: registerSheet 
	   modalForWindow: [NSApp keyWindow]
		modalDelegate: self
	   didEndSelector: @selector(registerDeviceSheetDidEnd: returnCode: contextInfo:)
		  contextInfo: nil];
}

- (void)registerDeviceXML_ndasDevice_legacy:(CFXMLTreeRef)xmlTreeNode_ndasDevice
{
	int childCount = CFTreeGetChildCount(xmlTreeNode_ndasDevice);
	int childCountSub;
	int i;
	NSString *stringNode, *stringNodeSub, *stringName = NULL, *stringID = NULL, *stringWriteKey = NULL, *stringDescription = NULL;
	CFXMLTreeRef xmlTreeNode, xmlTreeNodeSub;
	CFXMLNodeRef xmlNode, xmlNodeSub;
	CFXMLNodeTypeCode typeCode;
	
	//name, id, writeKey, description
	for(i = 0; i < childCount; i++)
	{
		xmlTreeNode = CFTreeGetChildAtIndex(xmlTreeNode_ndasDevice, i);
		xmlNode = CFXMLTreeGetNode(xmlTreeNode);
		stringNode = (NSString *)CFXMLNodeGetString(xmlNode);

		// retrieve CDATA or text
		childCountSub = CFTreeGetChildCount(xmlTreeNode);
		if(1 != childCountSub)
			continue;
			
		xmlTreeNodeSub = CFTreeGetChildAtIndex(xmlTreeNode, 0);
		xmlNodeSub = CFXMLTreeGetNode(xmlTreeNodeSub);
		stringNodeSub = (NSString *)CFXMLNodeGetString(xmlNodeSub);
		
		if([stringNode caseInsensitiveCompare:@"name"] == NSOrderedSame)
		{
			stringName = stringNodeSub;
		}
		else if([stringNode caseInsensitiveCompare:@"id"] == NSOrderedSame)
		{
			stringID = stringNodeSub;
		}
		else if([stringNode caseInsensitiveCompare:@"writeKey"] == NSOrderedSame)
		{
			stringWriteKey = stringNodeSub;
		}
		else if([stringNode caseInsensitiveCompare:@"description"] == NSOrderedSame)
		{
//			stringDescription = stringNodeSub;
		}
	}
	
	if(NULL == stringName || NULL == stringID)
	{
		return;
	}

	// Process Register.
	NSRange rangeIDSeg1 = {0, 5}, rangeIDSeg2 = {5, 5}, rangeIDSeg3 = {10, 5}, rangeIDSeg4 = {15, 5};
	[self _sndNotification : 0	// Auto select.
				unitNumber : 0
					 index : 0
				eNdCommand : NDAS_NOTIFICATION_REQUEST_REGISTER 
					  Name : stringName
					IDSeg1 : [stringID substringWithRange:rangeIDSeg1]
					IDSeg2 : [stringID substringWithRange:rangeIDSeg2]
					IDSeg3 : [stringID substringWithRange:rangeIDSeg3]
					IDSeg4 : [stringID substringWithRange:rangeIDSeg4]
					 WrKey : stringWriteKey
				 eNdConfig : 0
				  eOptions : 0
			logicalDevices : nil
				  RAIDType : 0
					  Sync : false];
}

- (void)registerDeviceXML_nif_legacy:(CFXMLTreeRef)xmlTreeNode_nif
{
	int childCount = CFTreeGetChildCount(xmlTreeNode_nif);
	int i;
	
	for(i = 0; i < childCount; i++)
	{
		// process "<ndasDevice/>"
		CFXMLTreeRef xmlTreeNode_ndasDevice = CFTreeGetChildAtIndex(xmlTreeNode_nif, i);
		CFXMLNodeRef xmlNode = CFXMLTreeGetNode(xmlTreeNode_ndasDevice);
		if([(NSString *)CFXMLNodeGetString(xmlNode) caseInsensitiveCompare:@"ndasDevice"] != NSOrderedSame)
		{
			continue;			
		}

		[self registerDeviceXML_ndasDevice_legacy:xmlTreeNode_ndasDevice];
	}
}
/*
- (void)registerDeviceXML_ndasDevice:(NSXMLElement *)xmlNdasDevice
{
	NSString *stringNode, *stringNodeSub, *stringName = NULL, *stringID = NULL, *stringWriteKey = NULL, *stringDescription = NULL;
	NSArray *elementArray;
	
	if ( (elementArray = [xmlNdasDevice elementsForName: @"name"]) 
		 && (1 <= [elementArray count])) {
		stringName = [(NSXMLElement *)[elementArray objectAtIndex: 0] stringValue];
	}
	
	if ( (elementArray = [xmlNdasDevice elementsForName: @"id"]) 
		 && (1 <= [elementArray count]) ) {
		stringID = [(NSXMLElement *)[elementArray objectAtIndex: 0] stringValue];
	}
	
	if ( (elementArray = [xmlNdasDevice elementsForName: @"writeKey"]) 
		 && (1 <= [elementArray count])) {
		stringWriteKey = [(NSXMLElement *)[elementArray objectAtIndex: 0] stringValue];
	}
	
	if ( (elementArray = [xmlNdasDevice elementsForName: @"description"]) 
		 && (1 <= [elementArray count])) {
		stringDescription = [(NSXMLElement *)[elementArray objectAtIndex: 0] stringValue];
	}

	if(NULL == stringName || NULL == stringID)
	{
		return;
	}
	
	// Process Register.
	NSRange rangeIDSeg1 = {0, 5}, rangeIDSeg2 = {5, 5}, rangeIDSeg3 = {10, 5}, rangeIDSeg4 = {15, 5};
	[self _sndNotification : 0	// Auto select.
				unitNumber : 0
					 index : 0
				eNdCommand : NDAS_NOTIFICATION_REQUEST_REGISTER 
					  Name : stringName
					IDSeg1 : [stringID substringWithRange:rangeIDSeg1]
					IDSeg2 : [stringID substringWithRange:rangeIDSeg2]
					IDSeg3 : [stringID substringWithRange:rangeIDSeg3]
					IDSeg4 : [stringID substringWithRange:rangeIDSeg4]
					 WrKey : stringWriteKey
				 eNdConfig : 0
				  eOptions : 0
			logicalDevices : nil
				  RAIDType : 0
					  Sync : false];
}

- (void)registerDeviceXML_nif:(NSXMLElement *)rootElement
{
	int i;
	
	NSArray *deviceArray = [rootElement elementsForName: @"ndasDevice"];
	
	for (i = 0; i < [deviceArray count]; i++) {
		[self registerDeviceXML_ndasDevice: (NSXMLElement *)[deviceArray objectAtIndex: i]];
	}	
}
*/
- (IBAction)registerDeviceXML:(id)sender
{
	if ( sentRequest ) { 
		return;
	}
	
	// open panel for *.ndas, *.nda 
	NSOpenPanel *opanel = [NSOpenPanel openPanel];
	[opanel setAllowsMultipleSelection:TRUE];
	[opanel runModalForTypes:[NSArray arrayWithObjects:@"ndas", @"nda", nil]];
	
	NSArray *aFilenames = [opanel filenames];
	int cnt = [aFilenames count];
	int i;
/*	
	for(i = 0; i < cnt; i++)
	{
		NSURL		*fileURL = [NSURL fileURLWithPath: [aFilenames objectAtIndex:i]];

		// open ndas XML file
		NSXMLDocument *xmlDoc;
		NSError *err = nil;
		
		xmlDoc = [[NSXMLDocument alloc] initWithContentsOfURL: fileURL 
													  options: (NSXMLNodePreserveWhitespace | NSXMLNodePreserveCDATA)
														error: &err];
		if (!xmlDoc) {
			continue;
		}
		
		if ([xmlDoc validateAndReturnError: &err]) {
			
			[self registerDeviceXML_nif: [xmlDoc rootElement]];
		}		

	}
*/	
	for(i = 0; i < cnt; i++)
	{		
		NSString *filename = [aFilenames objectAtIndex:i];

		// open ndas XML file
		CFXMLTreeRef cfXMLTree;
		
		CFURLRef dataSource = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, (CFStringRef)filename, kCFURLPOSIXPathStyle, FALSE);
		cfXMLTree = CFXMLTreeCreateWithDataFromURL(kCFAllocatorDefault, dataSource, kCFXMLParserSkipWhitespace, kCFXMLNodeCurrentVersion);
		CFRelease(dataSource);
		
		if(!cfXMLTree)
		{
			continue;
		}
		
		int childCount = CFTreeGetChildCount(cfXMLTree);
		int index;
		
		for(index = 0; index < childCount; index++)
		{
			// process "<nif/>"
			CFXMLTreeRef xmlTreeNode = CFTreeGetChildAtIndex(cfXMLTree, index);
			CFXMLNodeRef xmlNode = CFXMLTreeGetNode(xmlTreeNode);
			if([(NSString *)CFXMLNodeGetString(xmlNode) caseInsensitiveCompare:@"nif"] != NSOrderedSame)
			{
				continue;
			}
			
			[self registerDeviceXML_nif_legacy:xmlTreeNode];
		}
		
		CFRelease(cfXMLTree);
	}
}

- (void)application:(NSApplication *)sender openFiles:(NSArray *)aFilenames
{
	int cnt = [aFilenames count];
	int i;
/*	
	for(i = 0; i < cnt; i++)
	{
		NSURL		*fileURL = [NSURL fileURLWithPath: [aFilenames objectAtIndex:i]];
		
		// open ndas XML file
		NSXMLDocument *xmlDoc;
		NSError *err = nil;
		
		xmlDoc = [[NSXMLDocument alloc] initWithContentsOfURL: fileURL 
													  options: (NSXMLNodePreserveWhitespace | NSXMLNodePreserveCDATA)
														error: &err];
		if (!xmlDoc) {
			continue;
		}
		
		if ([xmlDoc validateAndReturnError: &err]) {
			
			[self registerDeviceXML_nif: [xmlDoc rootElement]];
		}		
		
	}	
*/	
	for(i = 0; i < cnt; i++)
	{		
		NSString *filename = [aFilenames objectAtIndex:i];
		
		// open ndas XML file
		CFXMLTreeRef cfXMLTree;
		CFURLRef dataSource = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, (CFStringRef)filename, kCFURLPOSIXPathStyle, FALSE);
		cfXMLTree = CFXMLTreeCreateWithDataFromURL(kCFAllocatorDefault, dataSource, kCFXMLParserSkipWhitespace, kCFXMLNodeCurrentVersion);
		CFRelease(dataSource);
		
		if(!cfXMLTree)
		{
			continue;
		}
		
		int childCount = CFTreeGetChildCount(cfXMLTree);
		int index;
		
		for(index = 0; index < childCount; index++)
		{
			// process "<nif/>"
			CFXMLTreeRef xmlTreeNode = CFTreeGetChildAtIndex(cfXMLTree, index);
			CFXMLNodeRef xmlNode = CFXMLTreeGetNode(xmlTreeNode);
			if([(NSString *)CFXMLNodeGetString(xmlNode) caseInsensitiveCompare:@"nif"] != NSOrderedSame)
			{
				continue;
			}
			
			[self registerDeviceXML_nif_legacy:xmlTreeNode];
		}
		
		CFRelease(cfXMLTree);
	}
}

- (IBAction)unregisterDevice:(id)sender
{
//	NSLog(@"unregisterDevice: Start");

	if ( sentRequest ) { 
		return;
	}
	
	int selectedRow;
		
	selectedRow = [ndasOutlineView selectedRow];
	
	if(-1 < selectedRow) {
		
		NDASPhysicalDeviceInformation NDParam;
		NDASDevice				*ndEntry;
		id						device;
		
		device = [ndasOutlineView itemAtRow: selectedRow];
		if( nil == device ||
			kNDASDeviceTypeNDAS != [(Device *)device deviceType] ) {
			return;
		}
		
		ndEntry = (NDASDevice *)device;
		
		NDParam.slotNumber = [ndEntry slotNo];
		
		if ( NDASGetInformationBySlotNumber( &NDParam ) ) {
			
			int count;
			
			if ( kNDASStatusOnline == NDParam.status ) {
				// BUG BUG BUG
				
				// Is Active?
				for(count = 0; count < MAX_NR_OF_TARGETS_PER_DEVICE; count++) {
					
					if( kNDASUnitConfigurationMountRW == NDParam.Units[count].configuration 
						|| kNDASUnitConfigurationMountRO == NDParam.Units[count].configuration ) {
						
						NSRunInformationalAlertPanel(
													 [NSString stringWithFormat: NSLocalizedString(@"PANEL-TITLE : '%@'(NDAS device name) could not be unregistered because it is still mounted on your system.", @"Failed to unregister for some reason."), [ndEntry name]],
													 [NSString stringWithFormat: NSLocalizedString(@"PANEL-MSG : Try again after having unmounted the NDAS unit device %d(NDAS unit device number)." , @"The reason why unregistering failed and the solution."), count],
													 NSLocalizedString(@"PANEL-BUTTON : OK", @"Text for button in panel"),
													 nil,
													 nil
													 );
						return;
					}
				}
			}
							
			int answer;
			
			answer = NSRunCriticalAlertPanel(
											 [NSString stringWithFormat: NSLocalizedString(@"PANEL-TITLE : Are you sure you want to unregister the '%@'(NDAS device name)?", @"Confirm unregistering"), [ndEntry name]],
											 @"",
											 NSLocalizedString(@"PANEL-BUTTON : Cancel", @"Text for button in panel"),
											 NSLocalizedString(@"PANEL-BUTTON : Unregister", @"Text for button in panel"),
											 nil
											 );
			if( NSAlertAlternateReturn == answer ) {
				
				[self _sndNotification : [ndEntry slotNo]
							unitNumber : 0
								 index : 0
							eNdCommand : NDAS_NOTIFICATION_REQUEST_UNREGISTER
								  Name : nil
								IDSeg1 : nil
								IDSeg2 : nil
								IDSeg3 : nil
								IDSeg4 : nil
								 WrKey : nil
							 eNdConfig : 0
							  eOptions : 0
						logicalDevices : nil
							  RAIDType : 0
								  Sync : false]; 
				
				// Refresh 
				[ndasOutlineView deselectAll: self];
			} 
		}
	}
	
//	[self refreshNDASDevices];
	
//	[self updateInformation];
	
//	NSLog(@"unregisterDevice: End.");

}

- (IBAction)registerKey:(id)sender
{
	//	NSLog(@"unregisterDevice: Start");
	
	if ( sentRequest ) { 
		return;
	}
	
	int selectedRow;
	
	selectedRow = [ndasOutlineView selectedRow];
	
	if(-1 < selectedRow) {
		
		NDASPhysicalDeviceInformation NDParam;
		NDASDevice				*ndEntry;
		id						device;
		
		device = [ndasOutlineView itemAtRow: selectedRow];
		if( nil == device ||
		   kNDASDeviceTypeNDAS != [(Device *)device deviceType] ) {
			return;
		}
		
		ndEntry = (NDASDevice *)device;
		
		// Check status.
		if ( !([ndEntry isIdle] || kNDASStatusNeedRegisterKey == [ndEntry status]) ) {
/*
			NSRunInformationalAlertPanel(
										 [NSString stringWithFormat: NSLocalizedString(@"PANEL-TITLE : '%@'(NDAS device name) could not be unregistered because it is still mounted on your system.", @"Failed to unregister for some reason."), [ndEntry name]],
										 [NSString stringWithFormat: NSLocalizedString(@"PANEL-MSG : Try again after having unmounted the NDAS unit device %d(NDAS unit device number)." , @"The reason why unregistering failed and the solution."), count],
										 NSLocalizedString(@"PANEL-BUTTON : OK", @"Text for button in panel"),
										 nil,
										 nil
										 );
 */
			return;			
			
		}
		
		int answer = NSAlertAlternateReturn;
		
		if (kNDASStatusOnline == [ndEntry status]) {
			
			answer = NSRunCriticalAlertPanel(
											 [NSString stringWithFormat: NSLocalizedString(@"PANEL-TITLE : Do you want to register the key of '%@'(NDAS unit device name)?", @"Confirm register Key"), [ndEntry name]],
											 [NSString stringWithFormat: NSLocalizedString(@"PANEL-MSG : If you want to register '%@'(NDAS device name) to another computer after the key was registered, You should enter the NDAS ID manually.", @"Register key Note"), [ndEntry name]],
											 NSLocalizedString(@"PANEL-BUTTON : Cancel", @"Text for button in panel"),
											 NSLocalizedString(@"PANEL-BUTTON : Register", @"Text for button in panel"),
											 nil
											 );
		}
		
		if( NSAlertAlternateReturn == answer ) {
			
			// Ask NDAS ID.
			
			// Disable Register Button.
			[registerKeyMenuItem setEnabled: NO];
			
			[registerRegisterButton setEnabled: NO];
			[registerMenuItem setEnabled: NO];
			
			// Cleanup.
			[registerErrorString setStringValue:@" "];
			
			// Init Values.
			[registerNameField setStringValue: [ndEntry name]];
			[registerIDField1 setStringValue: [[ndEntry deviceIDString] substringWithRange: NSMakeRange(0, 5)]];
			[registerIDField2 setStringValue: [[ndEntry deviceIDString] substringWithRange: NSMakeRange(6, 5)]];
			[registerIDField3 setStringValue: [[ndEntry deviceIDString] substringWithRange: NSMakeRange(12, 5)]];
			[registerIDField4 setStringValue: @""];
			[registerWritekeyField setStringValue:@""];
			
			// Forcus Name field.
			[registerIDField4 selectText: self];
			
			// Disable edit name.
			[registerNameField setEnabled: NO];

			[registerIDField1 setEnabled: NO];
			[registerIDField2 setEnabled: NO];
			[registerIDField3 setEnabled: NO];

			if ( kNDASStatusNeedRegisterKey == [ndEntry status]) {
				[registerWritekeyInfoField setStringValue: NSLocalizedString(@"PANEL_REGISTER : (Optional)", @"Text for Writekey Information for Key")];
				registerWriteKeyRequired = NO;
			} else {
				[registerWritekeyInfoField setStringValue: NSLocalizedString(@"PANEL_REGISTER : (Required)", @"Text for Writekey Information for Key")];
				registerWriteKeyRequired = YES;
			}
			
			[NSApp beginSheet: registerSheet 
			   modalForWindow: [NSApp keyWindow]
				modalDelegate: self
			   didEndSelector: @selector(registerKeySheetDidEnd: returnCode: contextInfo:)
				  contextInfo: ndEntry];
		}
	}
}

// TODO: Merge mountRW/mountRO as one. Apply this to all RW, RO
// TODO: Merge NDAS Unit device tab & NDAS RAID unit device tab

- (IBAction)mountRW:(id)sender
{
	if ( sentRequest ) { 
		return;
	}
	
	int selectedRow;
	
	//NSLog(@"mountRW");
		
	selectedRow = [ndasOutlineView selectedRow];
	
	if(-1 < selectedRow ) {
		
		id								device;
		
		UInt32							configuration;
		GUID							deviceID;
		BOOL							writable;
		UInt32							slotNumber;
		UInt32							unitNumber;
		UInt32							RWHosts;
		UInt32							type;
		UInt32							diskArrayType;
		NSString						*nameString = NULL;
		Device							*tempDevice = NULL;
		
		device = [ndasOutlineView itemAtRow: selectedRow];
		if( nil == device ||
			kNDASDeviceTypeNDAS == [(Device *)device deviceType] ) {
			return;
		}
		
		switch([(Device *)device deviceType]) {
			case kNDASDeviceTypeUnit:
			{
				NDASPhysicalDeviceInformation	NDParam = { 0 };
				NDASDevice						*ndEntry;
				UnitDevice						*unitDevice;

				ndEntry = [(UnitDevice *)device ndasId];
				unitDevice = (UnitDevice *)device;
				nameString = [ndEntry name];
				tempDevice = ndEntry;
				
				// Get Unit Device Information.
				NDParam.slotNumber = [ndEntry slotNo];

				if ( NDASGetInformationBySlotNumber(&NDParam) ) {

					slotNumber = NDParam.slotNumber;
					unitNumber = [unitDevice unitNumber];
					type = NDParam.Units[unitNumber].type; 
					diskArrayType = NDParam.Units[unitNumber].diskArrayType;
					configuration = NDParam.Units[unitNumber].configuration;
					deviceID = NDParam.Units[unitNumber].deviceID;
					writable = NDParam.writable;
					RWHosts = NDParam.Units[unitNumber].RWHosts;
					
				} else {
					return;
				}
			}
				break;
			case kNDASDeviceTypeRAID:
			{
				NDASRAIDDeviceInformation	NDParam = { 0 };
				RAIDDevice					*raidDevice;

				raidDevice = (RAIDDevice *)device;
				nameString = [raidDevice name];
				tempDevice = raidDevice;
				
				// Get RAID Unit Device information.
				NDParam.index = [raidDevice index];
				
				if ( NDASGetRAIDDeviceInformationByIndex(&NDParam)) {
					
					UNIT_DISK_LOCATION				firstUnit;
					NDASPhysicalDeviceInformation	firstUnitInfo = { 0 };
					
					memcpy(firstUnitInfo.address, &NDParam.unitDiskLocation, 6); 
					
					if ( NDASGetInformationByAddress(&firstUnitInfo) ) {
						slotNumber = firstUnitInfo.slotNumber;
						unitNumber = ((PUNIT_DISK_LOCATION)(&NDParam.unitDiskLocation))->UnitNumber;
					}
				
					type = NDParam.unitDeviceType; 
					diskArrayType = NDParam.diskArrayType;
					configuration = NDParam.configuration;
					deviceID = NDParam.deviceID;
					writable = NDParam.writable;
					RWHosts = NDParam.RWHosts;

					NDASPhysicalDeviceInformation	unitParam = { 0 };
					
					memcpy(&unitParam.address, ((UNIT_DISK_LOCATION *)&deviceID)->MACAddr, 6);
					if ( NDASGetInformationByAddress(&unitParam) ) {
						slotNumber = unitParam.slotNumber;
						unitNumber = ((UNIT_DISK_LOCATION *)&deviceID)->UnitNumber;
					} else {
						return;
					}
					
				} else {
					return;
				}
			}
				break;
				
			default:
				return;
		}
		
		if ( kNDASUnitConfigurationMountRW != configuration ) {
			
			// Check RAID Type.
			if ( kNDASUnitMediaTypeODD != type && 
				 NMT_SINGLE != diskArrayType ) {
				NSRunInformationalAlertPanel(
											 [NSString stringWithFormat: NSLocalizedString(@"PANEL-TITLE : '%@'(NDAS device name) cannot be mounted", @"Failed to mount the selected NDAS device for some reason."), nameString],
											 [NSString stringWithFormat: NSLocalizedString(@"PANEL-MSG : '%@'(NDAS device name) is a member of RAID.", @"The reason why mounting failed."), nameString],
											 NSLocalizedString(@"PANEL-BUTTON : OK", @"Text for button in panel"),
											 nil,
											 nil
											 );
				
				return;
			}
			
			// Search Logical Device.
			NDASLogicalDeviceInformation	LDInfo = { 0 };
			
			LDInfo.LowerUnitDeviceID = deviceID;
			
			if (!NDASGetLogicalDeviceInformationByLowerUnitDeviceID(&LDInfo)) {
				NSRunInformationalAlertPanel(
											 [NSString stringWithFormat: NSLocalizedString(@"PANEL-TITLE : '%@'(NDAS device name) cannot be mounted", @"Failed to mount the selected NDAS device for some reason."), nameString],
											 [NSString stringWithFormat: NSLocalizedString(@"PANEL-MSG : '%@'(NDAS device name) is not ready to be mounted.", @"The reason why mounting failed."), nameString],
											 NSLocalizedString(@"PANEL-BUTTON : OK", @"Text for button in panel"),
											 nil,
											 nil
											 );
				
				return;				
			}
			
			if ((TRUE == writable)) {
				
				if ( 0 < RWHosts ) {
					// Another Host Use this ndas device with RW access.
					int answer;
					
					answer = NSRunAlertPanel(
											 [NSString stringWithFormat: NSLocalizedString(@"PANEL-TITLE : '%@'(NDAS device name) cannot be mounted", @"Failed to mount the selected NDAS device for some reason."), nameString],
											 [NSString stringWithFormat: NSLocalizedString(@"PANEL-MSG : There is another computer in the network that has mounted '%@' with Read & Write access. To avoid data corruption or loss, please unmount '%@' on that computer and try again. To mount '%@'in Read & Write mode anyway, click Send. It is possible that the user denies your request for Read & Write access.", @"The reason why mounting failed and suggestion"), nameString, nameString, nameString],
											 NSLocalizedString(@"PANEL-BUTTON : Send", @"Text for button in panel"),
											 NSLocalizedString(@"PANEL-BUTTON : Cancel", @"Text for button in panel"),
											 nil
											 );
					if( NSAlertDefaultReturn == answer ) {
				
						selectedLogicalDeviceLowerUnitID = deviceID;
						
						[self _sndNotification : slotNumber
									unitNumber : unitNumber
										 index : 0
									eNdCommand : NDAS_NOTIFICATION_REQUEST_SURRENDER_ACCESS
										  Name : nil
										IDSeg1 : nil
										IDSeg2 : nil
										IDSeg3 : nil
										IDSeg4 : nil
										 WrKey : nil
									 eNdConfig : kNDASUnitConfigurationMountRW
									  eOptions : 0
								logicalDevices : nil
									  RAIDType : 0
										  Sync : false];
						
						selectedDevice = tempDevice;
						
					}
					
				} else {
					
					[self _sndNotification : 0
								unitNumber : 0
									 index : LDInfo.index
								eNdCommand : NDAS_NOTIFICATION_REQUEST_CONFIGURATE_LOGICAL_DEVICE
									  Name : nil
									IDSeg1 : nil
									IDSeg2 : nil
									IDSeg3 : nil
									IDSeg4 : nil
									 WrKey : nil
								 eNdConfig : kNDASUnitConfigurationMountRW
								  eOptions : 0
							logicalDevices : nil
								  RAIDType : 0
									  Sync : false]; 
				}
			} 
		} 
		
	} else { // if(-1 < selectedRow)
			 // Display warning message onto Main Window 
	} // if(-1 < selectedRow)
	
	[self refreshNDASDevices];
	
	[self updateInformation];
}


- (IBAction)mountRO:(id)sender
{
	if ( sentRequest ) { 
		return;
	}
	
	int selectedRow;
	
	//NSLog(@"mountRO");
	
	selectedRow = [ndasOutlineView selectedRow];
	
	if(-1 < selectedRow) {
		
		id							device;
		UInt32						configuration;
		UInt32						type;
		UInt32						diskArrayType;
		GUID						deviceID;
		NSString					*nameString;
		
		device = [ndasOutlineView itemAtRow: selectedRow];
		if( nil == device ||
			kNDASDeviceTypeNDAS == [(Device *)device deviceType] ) {
			return;
		}
		
		switch ([(Device *)device deviceType]) {
			case kNDASDeviceTypeUnit:
			{
				NDASPhysicalDeviceInformation	NDParam;
				NDASDevice					*ndEntry;
				UnitDevice					*unitDevice;

				ndEntry = [(UnitDevice *)device ndasId];
				unitDevice = (UnitDevice *)device;
		
				nameString = [ndEntry name];
				
				NDParam.slotNumber = [ndEntry slotNo];
				if (NDASGetInformationBySlotNumber(&NDParam) ) {
					configuration = NDParam.Units[[unitDevice unitNumber]].configuration;
					type = NDParam.Units[[unitDevice unitNumber]].type;
					diskArrayType = NDParam.Units[[unitDevice unitNumber]].diskArrayType;
					deviceID = NDParam.Units[[unitDevice unitNumber]].deviceID;
				} else {
					return;
				}
			}
				break;
			case kNDASDeviceTypeRAID:
			{
				NDASRAIDDeviceInformation	NDParam;
				RAIDDevice					*raidDevice;
				
				raidDevice = (RAIDDevice *)device;
				
				nameString = [raidDevice name];
				
				NDParam.index = [raidDevice index];
				if (NDASGetRAIDDeviceInformationByIndex(&NDParam) ) {
					configuration = NDParam.configuration;
					type = NDParam.unitDeviceType;
					diskArrayType = NDParam.diskArrayType;
					deviceID = NDParam.deviceID;
				} else {
					return;
				}				
			}
				break;
			default:
				return;
		}
		
		if (kNDASUnitConfigurationMountRO != configuration ) {
	
			// Check RAID Type.
			if (  kNDASUnitMediaTypeODD != type &&
				  NMT_SINGLE != diskArrayType ) {
				NSRunInformationalAlertPanel(
											 [NSString stringWithFormat: NSLocalizedString(@"PANEL-TITLE : '%@'(NDAS device name) cannot be mounted", @"Failed to mount the selected NDAS device for some reason."), nameString],
											 [NSString stringWithFormat: NSLocalizedString(@"PANEL-MSG : '%@'(NDAS device name) is a member of RAID.", @"The reason why mounting failed."), nameString],
											 NSLocalizedString(@"PANEL-BUTTON : OK", @"Text for button in panel"),
											 nil,
											 nil
											 );
				
				return;
			}
			
			// Search Logical Device.
			NDASLogicalDeviceInformation	LDInfo = { 0 };
			
			LDInfo.LowerUnitDeviceID = deviceID;
			
			if (!NDASGetLogicalDeviceInformationByLowerUnitDeviceID(&LDInfo)) {
				NSRunInformationalAlertPanel(
											 [NSString stringWithFormat: NSLocalizedString(@"PANEL-TITLE : '%@'(NDAS device name) cannot be mounted", @"Failed to mount the selected NDAS device for some reason."), nameString],
											 [NSString stringWithFormat: NSLocalizedString(@"PANEL-MSG : '%@'(NDAS device name) is not ready to be mounted.", @"The reason why mounting failed."), nameString],
											 NSLocalizedString(@"PANEL-BUTTON : OK", @"Text for button in panel"),
											 nil,
											 nil
											 );
				
				return;				
			}
			
			[self _sndNotification : 0
						unitNumber : 0
							 index : LDInfo.index
						eNdCommand : NDAS_NOTIFICATION_REQUEST_CONFIGURATE_LOGICAL_DEVICE
							  Name : nil
							IDSeg1 : nil
							IDSeg2 : nil
							IDSeg3 : nil
							IDSeg4 : nil
							 WrKey : nil
						 eNdConfig : kNDASUnitConfigurationMountRO
						  eOptions : 0
					logicalDevices : nil
						  RAIDType : 0
							  Sync : false]; 
		}
		
	} else {
		// Display warning message onto Main Window 
	}
	
	[self refreshNDASDevices];
	
	[self updateInformation];
}

#include <sys/param.h>
#include <sys/ucred.h>
#include <sys/mount.h>
#include <err.h>

static struct statfs *mntbuf;
static int mntsize;

typedef enum { MNTON, MNTFROM } mntwhat;

bool
IsMounted(const char *name, mntwhat what)
{
	int i;
	
	if ((mntsize = getmntinfo(&mntbuf, MNT_NOWAIT)) == 0) {
		warn("getmntinfo");
		return false;
	}
	
	char            deviceName[255];
	
    sprintf(deviceName, "/dev/%s", name);
	
	for (i = 0; i < mntsize; i++) {
		if ((what == MNTON) && !strncmp(mntbuf[i].f_mntfromname, deviceName, 10)) {
			return true;
		}
	}
	return false;
}

- (IBAction)unmount:(id)sender
{
	if ( sentRequest ) { 
		return;
	}
	
	int selectedRow;
	
	//NSLog(@"unmount");
		
	selectedRow = [ndasOutlineView selectedRow];
	
	if(-1 < selectedRow) {
		
		id							device;
		UInt32						configuration;
		UInt32						status;
		UInt32						type;
		UInt32						diskArrayType;
		GUID						deviceID;
		NSString					*nameString;
		
		device = [ndasOutlineView itemAtRow: selectedRow];
		if( nil == device ||
			kNDASDeviceTypeNDAS == [(Device *)device deviceType] ) {
			return;
		}
		
		switch([(Device *)device deviceType]) {
			case kNDASDeviceTypeUnit:
			{
				NDASPhysicalDeviceInformation	NDParam;
				NDASDevice						*ndEntry;
				UnitDevice						*unitDevice;
				
				ndEntry = [(UnitDevice *)device ndasId];
				unitDevice = (UnitDevice *)device;				
				
				nameString = [ndEntry name];
				
				NDParam.slotNumber = [ndEntry slotNo];
				if ( NDASGetInformationBySlotNumber(&NDParam) ) {
					type = NDParam.Units[[unitDevice unitNumber]].type;
					diskArrayType = NDParam.Units[[unitDevice unitNumber]].diskArrayType;
					configuration = NDParam.Units[[unitDevice unitNumber]].configuration;
					status = NDParam.Units[[unitDevice unitNumber]].status;
					deviceID = NDParam.Units[[unitDevice unitNumber]].deviceID;
				} else {
					return;
				}
			}
				break;
			case  kNDASDeviceTypeRAID:
			{
				NDASRAIDDeviceInformation	NDParam = { 0 };
				RAIDDevice *raidDevice;
				
				raidDevice = (RAIDDevice *)device;
				
				nameString = [raidDevice name];
				
				NDParam.index = [raidDevice index];
				if ( NDASGetRAIDDeviceInformationByIndex(&NDParam) ) {
					type = kNDASUnitMediaTypeHDD; // NDParam.unitDeviceType;
					diskArrayType = NDParam.diskArrayType;
					configuration = NDParam.configuration;
					status = NDParam.status;
					deviceID = NDParam.deviceID;
				} else {
					return;
				}
			}
				break;
			default:
				return;
		}
		
		[nameString retain];
/*
		// Check RAID Type.
		if ( kNDASUnitMediaTypeODD != type && 
			 NMT_SINGLE != diskArrayType ) {
			NSRunInformationalAlertPanel(
										 [NSString stringWithFormat: NSLocalizedString(@"PANEL-TITLE : '%@'(NDAS device name) cannot be unmounted", @"Failed to unmount the selected NDAS device for some reason."), nameString],
										 [NSString stringWithFormat: NSLocalizedString(@"PANEL-MSG : The NDAS unit device %d is a member of RAID. Try again after change the NDAS unit device to single disk type.", @"The reason why unmounting failed"), [unitDevice unitNumber]],
										 NSLocalizedString(@"PANEL-BUTTON : OK", @"Text for button in panel"),
										 nil,
										 nil
										 );
			
			[nameString release];

			return;
		}
*/		
		if ( kNDASUnitConfigurationUnmount != configuration ) {
			
			bool	bDisable;
			
			// Is Active?
			if( kNDASUnitStatusConnectedRW == status || 
				kNDASUnitStatusConnectedRO == status ) {
				int answer;
				
				answer = NSRunAlertPanel(
										 [NSString stringWithFormat: NSLocalizedString(@"PANEL-TITLE : Are you sure you want to unmount '%@'(NDAS device name)?", @"Confirm unmounting"), nameString],
										 [NSString stringWithFormat: NSLocalizedString(@"PANEL-MSG : You may lose or corrupt your data if you do not Eject your '%@'(NDAS device name) first. Please eject before unmounting it.", @"Unmount Note"), nameString],
										 NSLocalizedString(@"PANEL-BUTTON : Cancel", @"Text for button in panel"),
										 NSLocalizedString(@"PANEL-BUTTON : Unmount", @"Text for button in panel"),
										 nil
										 );
				if( NSAlertAlternateReturn == answer ) {
					bDisable = true;
				} else {
					bDisable = false;
				}
			} else {
				bDisable = true;
			}
			
			if( true == bDisable) {

				// Search Logical Device.
				NDASLogicalDeviceInformation	LDInfo = { 0 };
				
				LDInfo.LowerUnitDeviceID = deviceID;
				
				if (!NDASGetLogicalDeviceInformationByLowerUnitDeviceID(&LDInfo)) {
					NSRunInformationalAlertPanel(
												 [NSString stringWithFormat: NSLocalizedString(@"PANEL-TITLE : '%@'(NDAS device name) cannot be unmounted", @"Failed to unmount the selected NDAS device for some reason."), nameString],
												 [NSString stringWithFormat: NSLocalizedString(@"PANEL-MSG : '%@'(NDAS device name) is not ready to be unmounted.", @"The reason why unmounting failed."), nameString],
												 NSLocalizedString(@"PANEL-BUTTON : OK", @"Text for button in panel"),
												 nil,
												 nil
												 );

					[nameString release];

					return;				
				}
				
				int terminationStatusOfEject;
				
				if ( kNDASUnitMediaTypeHDD == type
					 && (kNDASUnitStatusConnectedRO == status || kNDASUnitStatusConnectedRW == status))  {
					
					NSTask		*disktoolTask;
					
					// Check dev file.
					if ( [[NSFileManager defaultManager] fileExistsAtPath: [NSString stringWithFormat:@"/dev/%s", LDInfo.BSDName]] ) {
						
						// Start Busy Indicator.
						[progressIndicator startAnimation: self];
						
						// Eject using disktool program.				
						disktoolTask = [NSTask launchedTaskWithLaunchPath: @"/usr/sbin/diskutil"
																arguments: [NSArray arrayWithObjects:@"eject", 
																	[NSString stringWithCString: LDInfo.BSDName], 
																	nil]];
						
						
						[disktoolTask waitUntilExit];
						terminationStatusOfEject = [disktoolTask terminationStatus];
						
					}
					
					// Stop Busy Indicator.
					[progressIndicator stopAnimation: self];
				}
				
				// Check eject.
				if (IsMounted(LDInfo.BSDName, MNTON)) {
					
					int answer = NSRunCriticalAlertPanel(
														 [NSString stringWithFormat: NSLocalizedString(@"PANEL-TITLE : '%@'(NDAS device name) cannot be Ejected. Unmount Anyway?", @"Confirm ejecting"), nameString],
														 [NSString stringWithFormat: NSLocalizedString(@"PANEL-MSG : You may lose or corrupt your data if you do not Eject your '%@'(NDAS device name) first. Please eject before unmounting it.", @"Unmount Note"), nameString],
														 NSLocalizedString(@"PANEL-BUTTON : Cancel", @"Text for button in panel"),
														 NSLocalizedString(@"PANEL-BUTTON : Unmount", @"Text for button in panel"),
														 nil
														 );
					if( NSAlertAlternateReturn == answer ) {
						status = kNDASUnitStatusDisconnected;
					}
				} else {
					status = kNDASUnitStatusDisconnected;
				}
				
				if (status == kNDASUnitStatusDisconnected || status == kNDASUnitStatusNotPresent) {
					
					[self _sndNotification : 0
								unitNumber : 0
									 index : LDInfo.index
								eNdCommand : NDAS_NOTIFICATION_REQUEST_CONFIGURATE_LOGICAL_DEVICE
									  Name : nil
									IDSeg1 : nil
									IDSeg2 : nil
									IDSeg3 : nil
									IDSeg4 : nil
									 WrKey : nil
								 eNdConfig : kNDASUnitConfigurationUnmount
								  eOptions : 0
							logicalDevices : nil
								  RAIDType : 0
									  Sync : false]; 
				}
			}
		}
		
		[nameString release];

	} else {
		/* Display warning message onto Main Window */
	}
	
	[self refreshNDASDevices];
	
	[self updateInformation];
}

- (bool)sendRefresh:(id)device 
			   wait:(bool)wait
{
	while( sentRequest ) { 
		if (wait) {
			// Sleep 0.1 sec.
			[NSThread sleepUntilDate: [NSDate dateWithTimeIntervalSinceNow: 0.01]];
		} else {
			return false;
		}
	}
	
	switch([(Device *)device deviceType]) {
		case kNDASDeviceTypeUnit:
		{
			NDASPhysicalDeviceInformation	NDParam;
			NDASDevice						*ndEntry;
			UnitDevice						*unitDevice;
			
			ndEntry = [(UnitDevice *)device ndasId];
			unitDevice = (UnitDevice *)device;				
			
			NDParam.slotNumber = [ndEntry slotNo];
			if ( NDASGetInformationBySlotNumber(&NDParam) ) {
				
				[self _sndNotification : [ndEntry slotNo]	// Auto select.
							unitNumber : [unitDevice unitNumber]
								 index : 0
							eNdCommand : NDAS_NOTIFICATION_REQUEST_REFRESH 
								  Name : nil
								IDSeg1 : nil
								IDSeg2 : nil
								IDSeg3 : nil
								IDSeg4 : nil
								 WrKey : nil
							 eNdConfig : 0
							  eOptions : NDAS_NOTIFICATION_OPTION_FOR_PHYSICAL_UNIT
						logicalDevices : nil
							  RAIDType : 0
								  Sync : false];					
			} else {
				return false;
			}
		}
			break;
		case  kNDASDeviceTypeRAID:
		{
			NDASRAIDDeviceInformation	NDParam = { 0 };
			RAIDDevice *raidDevice;
			
			raidDevice = (RAIDDevice *)device;
			
			NDParam.index = [raidDevice index];
			if ( NDASGetRAIDDeviceInformationByIndex(&NDParam) ) {
				
				[self _sndNotification : 0
							unitNumber : 0
								 index : [raidDevice index]
							eNdCommand : NDAS_NOTIFICATION_REQUEST_REFRESH 
								  Name : nil
								IDSeg1 : nil
								IDSeg2 : nil
								IDSeg3 : nil
								IDSeg4 : nil
								 WrKey : nil
							 eNdConfig : 0
							  eOptions : NDAS_NOTIFICATION_OPTION_FOR_RAID_UNIT
						logicalDevices : nil
							  RAIDType : 0
								  Sync : false];
			} else {
				return false;
			}
		}
			break;
		default:
			return false;
	}	
	
	return true;
}

- (void)refreshAllThreadRoutine:(id)param
{
	id value;
	
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
		
	// NDAS device.
	NSEnumerator *objectEnumerator = [NDASDevices objectEnumerator];
	while (value = [objectEnumerator nextObject]) {
		
		NSEnumerator *unitEnumerator = [[(NDASDevice *)value unitDictionary] objectEnumerator];
		
		UnitDevice *unit;
		
		while(unit = (UnitDevice *)[unitEnumerator nextObject]) {
			[self sendRefresh: unit wait: YES];
			
		}		
	}
		
	objectEnumerator = [RAIDDevices objectEnumerator];
	while (value = [objectEnumerator nextObject]) {
		[self sendRefresh: (Device *)value wait: YES];
	}	
	
	[pool release];
}

- (IBAction)refreshAll:(id)sender
{
	if ( sentRequest ) { 
		return;
	}

	// Detach the new thread.
	[NSThread detachNewThreadSelector:@selector(refreshAllThreadRoutine:) toTarget:self withObject:nil];	
}

- (IBAction)refresh:(id)sender
{
	if ( sentRequest ) { 
		return;
	}
	
	int selectedRow;
	
	//NSLog(@"refresh");
		
	selectedRow = [ndasOutlineView selectedRow];
	
	if(-1 < selectedRow) {
		
		id							device;
		
		device = [ndasOutlineView itemAtRow: selectedRow];
		if( nil == device ||
			kNDASDeviceTypeNDAS == [(Device *)device deviceType] ) {
			return;
		}
		
		[self sendRefresh: device wait: NO];
	}
	
//	[self refreshNDASDevices];
	
//	[self updateInformation];
}

- (void)registerDeviceSheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
	NSLog(@"registerDeviceSheetDidEnd : return Code %d\n", returnCode);
	
	[registerSheet close];	

	if (1 == returnCode) {
		// Press register button.
		
		// Process Register.
		[self _sndNotification : 0	// Auto select.
					unitNumber : 0
						 index : 0
					eNdCommand : NDAS_NOTIFICATION_REQUEST_REGISTER 
						  Name : [registerNameField stringValue]
						IDSeg1 : [registerIDField1 stringValue]
						IDSeg2 : [registerIDField2 stringValue]
						IDSeg3 : [registerIDField3 stringValue]
						IDSeg4 : [registerIDField4 stringValue]
						 WrKey : [registerWritekeyField stringValue]
					 eNdConfig : 0
					  eOptions : 0
				logicalDevices : nil
					  RAIDType : 0
						  Sync : false];
		
	}
	
	[registerKeyMenuItem setEnabled: YES];

	[registerMenuItem setEnabled: YES];
	
	[self updateInformation];	
}

- (void)registerKeySheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
	NSLog(@"registerKeySheetDidEnd : return Code %d\n", returnCode);
	
	[registerSheet close];	

	NDASDevice *ndas = (NDASDevice *)contextInfo;
	
	if (1 == returnCode && ndas != nil) {
		// Press register button.
		
		// Process Register Key.
		[self _sndNotification : [ndas slotNo]	
					unitNumber : 0
						 index : 0
					eNdCommand : NDAS_NOTIFICATION_REQUEST_REGISTER_KEY 
						  Name : [registerNameField stringValue]
						IDSeg1 : [registerIDField1 stringValue]
						IDSeg2 : [registerIDField2 stringValue]
						IDSeg3 : [registerIDField3 stringValue]
						IDSeg4 : [registerIDField4 stringValue]
						 WrKey : [registerWritekeyField stringValue]
					 eNdConfig : 0
					  eOptions : 0
				logicalDevices : nil
					  RAIDType : 0
						  Sync : false];
	}
	
	[registerKeyMenuItem setEnabled: YES];
	
	[registerMenuItem setEnabled: YES];

	[self updateInformation];	
}

- (IBAction)registerSheetRegister:(id)sender
{
	[NSApp endSheet: registerSheet returnCode: 1];
}

- (IBAction)registerSheetCancel:(id)sender
{
	[NSApp endSheet: registerSheet returnCode: 0];	
}

- (IBAction)enterWriteKeySheetEnter:(id)sender
{
	[NSApp endSheet: enterWriteKeySheet];
	[enterWriteKeySheet close];
	
	// Process Register.
	[self _sndNotification : [selectedDevice slotNo]
				unitNumber : 0
					 index : 0
				eNdCommand : NDAS_NOTIFICATION_REQUEST_EDIT_NDAS_WRITEKEY 
					  Name : nil
					IDSeg1 : nil
					IDSeg2 : nil
					IDSeg3 : nil
					IDSeg4 : nil
					 WrKey : [enterWriteKeyField stringValue]
				 eNdConfig : 0
				  eOptions : 0
			logicalDevices : nil
				  RAIDType : 0
					  Sync : false];
		
//	[self refreshNDASDevices];
	
//	[self updateInformation];	
	
}

- (IBAction)enterWriteKeySheetCancel:(id)sender
{
	NSLog(@"enterWriteKeySheetCancel");

	[NSApp endSheet: enterWriteKeySheet];
	[enterWriteKeySheet close];
}

- (NSMutableDictionary *) NDASDevices 
{ 
	return  NDASDevices;
}

- (NSMutableDictionary *) RAIDDevices
{
	return RAIDDevices;
}

- (NSString *)registerIDField1
{
	return [registerIDField1 stringValue];
}

- (NSString *)registerIDField2
{
	return [registerIDField2 stringValue];
}

- (NSString *)registerIDField3
{
	return [registerIDField3 stringValue];
}

- (NSString *)registerIDField4
{
	return [registerIDField4 stringValue];
}

- (NSString *)registerWritekeyField
{
	return [registerWritekeyField stringValue];
}

- (id)registerRegisterButton
{
	return registerRegisterButton;
}

- (id)registerErrorString
{
	return registerErrorString;
}

- (BOOL)registerWriteKeyRequired 
{
	return registerWriteKeyRequired;
}

- (NSString *)ExportRegistrationIDField1
{
	return [ExportRegistrationIDField1 stringValue];
}

- (NSString *)ExportRegistrationIDField2
{
	return [ExportRegistrationIDField2 stringValue];
}

- (NSString *)ExportRegistrationIDField3
{
	return [ExportRegistrationIDField3 stringValue];
}

- (NSString *)ExportRegistrationIDField4
{
	return [ExportRegistrationIDField4 stringValue];
}

- (id)ExportRegistrationWritekeyField
{
	return ExportRegistrationWritekeyField;
}

- (id)ExportRegistrationExportButton
{
	return ExportRegistrationExportButton;
}

- (id)ExportRegistrationErrorString
{
	return ExportRegistrationErrorString;
}

- (id)ExportRegistrationNotExportWriteKeyCheckbox
{
	return ExportRegistrationNotExportWriteKeyCheckbox;
}

- (id)selectedNDASDevice
{
	return selectedDevice;
}

- (NSString *)enterWriteKeyField
{
	return [enterWriteKeyField stringValue];
}

- (id)enterWriteKeyButton
{
	return enterWriteKeyButton;
}

- (id)enterWriteKeyErrorField
{
	return enterWriteKeyErrorField;
}

- (id)RAIDBindSheetUnitOutlineView
{
	return RAIDBindSheetUnitOutlineView;
}

- (id)RAIDBindSheetSubUnitOutlineView
{
	return RAIDBindSheetSubUnitOutlineView;
}

- (NSMutableArray *)RAIDBindSheetSubUnitDevices
{
	return RAIDBindSheetSubUnitDevices;
}

- (id)RAIDBindSheetRemoveButton
{
	return RAIDBindSheetRemoveButton;
}

- (id)RAIDBindSheetTypePopupButton
{
	return RAIDBindSheetTypePopupButton;
}

- (id)RAIDBindSheetSizeField
{
	return RAIDBindSheetSizeField;
}

- (id)RAIDBindSheetBindButton
{
	return RAIDBindSheetBindButton;
}

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
					 Sync : (bool) sync
{
	NSMutableDictionary *mdCommand;
	
	mdCommand = [[NSMutableDictionary alloc] init];

	[mdCommand setObject: IDStringForNotification forKey: @NDAS_NOTIFICATION_SENDER_ID_KEY_STRING];
	[mdCommand setObject: [NSNumber numberWithInt:slotNumber] forKey: @NDAS_NOTIFICATION_SLOTNUMBER_KEY_STRING];
	[mdCommand setObject: [NSNumber numberWithInt:unitNumber] forKey: @NDAS_NOTIFICATION_UNITNUMBER_KEY_STRING]; 
	[mdCommand setObject: [NSNumber numberWithInt:index] forKey: @NDAS_NOTIFICATION_INDEX_KEY_STRING]; 
	[mdCommand setObject: [NSNumber numberWithInt:request] forKey: @NDAS_NOTIFICATION_REQUEST_KEY_STRING]; 
	[mdCommand setObject: [NSNumber numberWithInt:configuration] forKey: @NDAS_NOTIFICATION_CONFIGURATION_KEY_STRING]; 
	[mdCommand setObject: [NSNumber numberWithInt:options] forKey: @NDAS_NOTIFICATION_OPTIONS_KEY_STRING];
	
	if( Name ) {
		[mdCommand setObject: Name forKey: @NDAS_NOTIFICATION_NAME_STRING]; 
	}
	
	if( IDSeg1 && IDSeg2 && IDSeg3 && IDSeg4 ) {
		[mdCommand setObject: IDSeg1 forKey: @NDAS_NOTIFICATION_ID_1_KEY_STRING]; 
		[mdCommand setObject: IDSeg2 forKey: @NDAS_NOTIFICATION_ID_2_KEY_STRING]; 
		[mdCommand setObject: IDSeg3 forKey: @NDAS_NOTIFICATION_ID_3_KEY_STRING]; 
		[mdCommand setObject: IDSeg4 forKey: @NDAS_NOTIFICATION_ID_4_KEY_STRING]; 
	}	
	
	if ( WrKey ) {
		[mdCommand setObject: WrKey forKey: @NDAS_NOTIFICATION_WRITEKEY_KEY_STRING]; 
	}
	
	[mdCommand setObject: [NSNumber numberWithInt: RAIDType]
				  forKey: @NDAS_NOTIFICATION_RAID_TYPE_KEY_STRING];

	if ( logicalDevices ) {
		
		[mdCommand setObject: [NSNumber numberWithInt: [logicalDevices count]]
					  forKey: @NDAS_NOTIFICATION_NUMBER_OF_LOGICAL_DEVICES_KEY_STRING];
			
		for (index = 0; index < [logicalDevices count]; index++) {
			[mdCommand setObject: [logicalDevices objectAtIndex: index] 
						  forKey: [NSString stringWithFormat: @NDAS_NOTIFICATION_LOGICAL_DEVICE_INDEX_KEY_STRING, index]]; 
		}
	}
	
	// Sync for RAID 1
	[mdCommand setObject: [NSNumber numberWithBool: sync]
				  forKey: @NDAS_NOTIFICATION_RAID_SYNC_FLAG_STRING];
	
	// Start Busy Indicator.
	[progressIndicator startAnimation: self];
	
	sentRequest = TRUE;
	
	// Send Request to Service.
	if ( [[NSDistributedNotificationCenter defaultCenter] respondsToSelector: @selector(postNotificationName: object: userInfo: options:)] ) {
		
		[[NSDistributedNotificationCenter defaultCenter] postNotificationName: @NDAS_NOTIFICATION
																	   object: @NDAS_SERVICE_OBJECT_ID
																	 userInfo: mdCommand
																	  options: NSNotificationPostToAllSessions];
		
	} else {
		
		[[NSDistributedNotificationCenter defaultCenter] postNotificationName: @NDAS_NOTIFICATION
																	   object: @NDAS_SERVICE_OBJECT_ID
																	 userInfo: mdCommand
														   deliverImmediately: FALSE];
		
	}
	
	recentRequest = request;
	
	[mdCommand release];
	
}

//
// OutlineView
//
#pragma mark -
#pragma mark == OutlineView Data access methods

- (id)outlineView:(NSOutlineView *)outlineView child:(NSInteger)index ofItem:(id)item;
{
//	NSLog(@"child index %p %d\n", item, index);
	
	if ([outlineView isEqual: ndasOutlineView]) {
		
		if ( nil == item ) {
			
			if ( ([[controller NDASDevices] count] + [[controller RAIDDevices] count] ) < index) {
				return nil;
			}
			
			if ( ([[controller NDASDevices] count] == index )) {
				// Separator.
				return nil;
			}
			
			if ( [[controller NDASDevices] count] > index ) {
				NSArray *keys = [[[controller NDASDevices] allKeys] sortedArrayUsingSelector:@selector(compare:)];

//				NSLog(@"child index root!!! %p %d, return %p\n", item, index, [[controller NDASDevices] objectForKey: [keys objectAtIndex:index]]);

				return [[controller NDASDevices] objectForKey: [keys objectAtIndex:index]];				
			} else {
				return [[[controller RAIDDevices] allValues] objectAtIndex: (index - [[controller NDASDevices] count] - 1)];				
			}
			
		} else if ( kNDASDeviceTypeNDAS == [(Device *)item deviceType] ) {
//			NSLog(@"child index NDAS DEVICE!!! %p %d, return %p\n", item, index, [(NDASDevice *)item unitAtIndex: index]);
						
			return [(NDASDevice *)item unitAtIndex: index];
		} else 	{
//			NSLog(@"child index Other!!! %p %d\n", item, index);
			return nil;
		}
	} else 	if ([outlineView isEqual: RAIDSetOutlineView]) {
		int selectedRow = [ndasOutlineView selectedRow];
		
		if(-1 < selectedRow ) {
			
			id								device;
			
			device = [ndasOutlineView itemAtRow: selectedRow];
			
			if (kNDASDeviceTypeRAID == [(Device *)device deviceType]) {
				RAIDDevice	*raidDevice = (RAIDDevice *)device;
				
				return [raidDevice subUnitAtIndex: index];
			}
		}
		
		return nil;
	}
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item;
{
//	NSLog(@"isItemExpendable 0x%x\n", item);
	return YES;
#if 0
	if ([outlineView isEqual: ndasOutlineView]) {
		
		if ( nil == item ) {
			return TRUE;
/*			
			if ( 0 < [[controller NDASDevices] count] + [[controller RAIDDevices] count] ) {
				return TRUE;
			} else {
				return FALSE;
			}
*/			
		} else if ( kNDASDeviceTypeNDAS == [(Device *)item deviceType] ) {
			// To display status button.
			return TRUE;
		} else if ( kNDASDeviceTypeRAID == [(Device *)item deviceType] ) {
			return TRUE;
		}
	} else 	if ([outlineView isEqual: RAIDSetOutlineView]) {
		return FALSE;
	}
	
	return FALSE;
#endif
}

- (NSInteger)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item;
{
//	NSLog(@"numberOfChildrenOfItem %p %d\n", item, [[controller NDASDevices] count] + [[controller RAIDDevices] count]);

	if ([outlineView isEqual: ndasOutlineView]) {
		
		if( nil == item ) {		
			if ([[controller RAIDDevices] count] > 0 ) {
				return [[controller NDASDevices] count] + [[controller RAIDDevices] count] + 1; // with Separator.
			} else {
//				NSLog(@"numberOfChildrenOfItem root %p %d\n", item, [[controller NDASDevices] count]);

				return [[controller NDASDevices] count];
			}
		} else if ( kNDASDeviceTypeNDAS == [(Device *)item deviceType] ){
//			NSLog(@"numberOfChildrenOfItem Device %p %d\n", item, [(NDASDevice *)item numberOfUnits]);
			
			return [(NDASDevice *)item numberOfUnits];
		} else {
//			NSLog(@"numberOfChildrenOfItem Other. %p, 0\n", item);

			return 0;
		}
	} else 	if ([outlineView isEqual: RAIDSetOutlineView]) {
		
		int selectedRow = [ndasOutlineView selectedRow];
		
		if(-1 < selectedRow ) {
			
			id								device;
						
			device = [ndasOutlineView itemAtRow: selectedRow];
			
			if (kNDASDeviceTypeRAID == [(Device *)device deviceType]) {
				RAIDDevice	*raidDevice = (RAIDDevice *)device;
				
				return [raidDevice CountOfUnitDevices] + [raidDevice CountOfSpareDevices];
			}
		}
		
		return 0;
	}
	
}

- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item;
{
//	NSLog(@"objectValueForTableColumn 0x%x %@\n", item, [tableColumn identifier]);

	id objectValue = nil;
	
	if ([outlineView isEqual: ndasOutlineView]) {
		
		NSString	*tempString;
		
		if( [[tableColumn identifier] isEqualToString: @"FirstColumn"] ) {
			
			if (item == nil) {
				return nil;
			} else {
				
				switch ( [(Device *)item deviceType] ) {
					case kNDASDeviceTypeNDAS:
					{
						objectValue = [(NDASDevice *)item name];
						
						if ( NO ==[outlineView isItemExpanded: item] ) {
							[outlineView performSelector:@selector(expandItem:) withObject: item afterDelay:0];
						}
						
					}
						break;
					case kNDASDeviceTypeUnit:
					{
												
						if(kNDASUnitMediaTypeHDD == [(UnitDevice *)item type]) {
							tempString = [NSString stringWithFormat: @"(%@) %@", [(UnitDevice *)item capacityToString], [(UnitDevice *)item model]];
							
						} else {
							tempString = [NSString stringWithString: [(UnitDevice *)item model]];
						}
						
						if([tempString length] > 21) {
							tempString = [NSString stringWithString: [tempString substringToIndex: 21]];
						}
						
						objectValue = tempString;
						
						if ( NO ==[outlineView isItemExpanded: item] ) {
							[outlineView performSelector:@selector(expandItem:) withObject: item afterDelay:0];
						}
						
						
					}
						break;
					case kNDASDeviceTypeRAID:
					{
						tempString = [NSString stringWithFormat: @"(%@) %@", [(RAIDDevice *)item capacityToString], [(RAIDDevice *)item name]];
						
						if([tempString length] > 21) {
							tempString = [NSString stringWithString: [tempString substringToIndex: 21]];
						}
						
						objectValue = tempString;
					}
						break;
				}
			}
		}
	} else 	if ([outlineView isEqual: RAIDSetOutlineView]) {

		RAIDSubDevice	*raidDevice = (RAIDSubDevice *)item;
		NSString	*tempString;
				
		if( [[tableColumn identifier] isEqualToString: @"RAIDIndex"] ) {

			switch([raidDevice subUnitType]) {
				case kNDASRAIDSubUnitTypeSpare:
				{
					tempString = @" S";
				}
					break;
				case kNDASRAIDSubUnitTypeReplacedBySpare:
				{
					tempString = @" R";
				}
					break;
				default:
				{
					if ([raidDevice subUnitNumber] >= [[raidDevice RAIDDeviceID] CountOfUnitDevices]) {
						tempString = @" ?";
					} else {
						tempString = [NSString stringWithFormat: @"%2d", [raidDevice subUnitNumber]];
					}
				}
					break;
			}
						
			objectValue = tempString;
		} else if ( [[tableColumn identifier] isEqualToString: @"SubUnit"] ) {

			UInt64				locationInt = [raidDevice deviceID];
			PUNIT_DISK_LOCATION	location = (PUNIT_DISK_LOCATION)&locationInt;
			
			// Physical Device.
			NDASPhysicalDeviceInformation	NDASInfo = { 0 };
			
			memcpy(NDASInfo.address, location->MACAddr, 6);
			
			if (NDASGetInformationByAddress(&NDASInfo)) {
				
				NDASDevice *entry = [NDASDevices objectForKey: [NSNumber numberWithInt: NDASInfo.slotNumber]];
				UnitDevice *unitDevice = [entry unitAtIndex: location->UnitNumber];
				
				switch([entry status]) {
					case kNDASStatusOnline:
					{
						if ( kNDASUnitMediaTypeHDD == [unitDevice type] ) {
							
							if ( [[raidDevice RAIDDeviceID] unitDeviceType] == [unitDevice diskArrayType] ) {
								tempString = [NSString stringWithFormat: @"(%@) %@(%d) %@", [unitDevice capacityToString], [entry name], location->UnitNumber, [unitDevice model]];
							} else {
								tempString = [NSString stringWithFormat: @"Unknown Device"];									
							}
						} else {
							tempString = [NSString stringWithFormat: @"(ODD) %@(%d)", [entry name], location->UnitNumber];
						}
					}
						break;
					case kNDASStatusOffline:
					case kNDASStatusNoPermission:
					case kNDASStatusFailure:
					default:
					{
						tempString = [NSString stringWithFormat: @"%@(%d)", [entry name], location->UnitNumber];
					}
						break;
				}
				
			} else {
				tempString = [NSString stringWithFormat: @"Unknown Device"];
			}
			
			if([tempString length] > 60) {
				objectValue = [NSString stringWithString: [tempString substringToIndex: 60]];
			} else {
				objectValue = tempString;			
			}
		}
	}
	
	return objectValue;
}

#pragma mark -

#pragma mark -== OutlineView Delegate methods.

//
// OutlineView Delegate methods
//
- (void)outlineView:(NSOutlineView *)outlineView willDisplayCell:(id)cell forTableColumn:(NSTableColumn *)tableColumn item:(id)item
{
//	NSLog(@"willDisplayCell\n");
	
	NSImage *image;
	NSSize	imageSize;

	if ([outlineView isEqual: ndasOutlineView]) {
		
		if ( [[tableColumn identifier] isEqualToString: @"FirstColumn"] ) {
						
			if (item == nil) {
				// Separator
				imageSize.height = 2;
				imageSize.width = 180;

				image = [NSImage imageNamed:@"Separator"];

				[image setScalesWhenResized: TRUE];
				[image setSize: imageSize];
				[(ImageAndTextCell *) cell setImage: image];
								
				return;
			} else {

				imageSize.height = 16;
				imageSize.width = 16;
				
				switch ( [(Device *)item deviceType] ) {
					case kNDASDeviceTypeNDAS:
					{
						NDASDevice *device = (NDASDevice *)item;
						
						if([device autoRegister]) {
							image = [NSImage imageNamed:@"External_Auto"];
						} else {
							image = [NSImage imageNamed:@"External"];
						}

						[image setScalesWhenResized: TRUE];
						[image setSize: imageSize];
						
						// Set Image.
						[(ImageAndTextCell *) cell setImage: image];
						
						// Set Text Color.
						switch ([device status]) {
							case kNDASStatusOnline:
							{
								[(ImageAndTextCell *) cell setTextColor: [NSColor blackColor] ];
							}
								break;
							case kNDASStatusOffline:
							case kNDASStatusFailure:
							case kNDASStatusNoPermission:
							{
								[(ImageAndTextCell *) cell setTextColor: [NSColor grayColor] ];
							}
								break;								
						}						
					}
						break;
					case kNDASDeviceTypeUnit:
					{
						switch([(UnitDevice *)item type]) {
							case kNDASUnitMediaTypeODD:
								image = [NSImage imageNamed:@"ODD"];
								
								break;					
							case kNDASUnitMediaTypeHDD:
								image = [NSImage imageNamed:@"Disk"];
								
								break;
							default:
								image = [NSImage imageNamed:@"External"];
								
								break;					
						}
						
						[image setScalesWhenResized: TRUE];
						[image setSize: imageSize];
						
						// Set Image.
						[(ImageAndTextCell *) cell setImage: image];					
						
						// Set Text Color.
						[(ImageAndTextCell *) cell setTextColor: [NSColor blackColor] ];
						
					}
						break;
					case kNDASDeviceTypeRAID:
					{					
						image = [NSImage imageNamed:@"RAID"];
						
						[image setScalesWhenResized: TRUE];
						[image setSize: imageSize];
						
						// Set Image.
						[(ImageAndTextCell *) cell setImage: image];
						
						// Set Color.
						switch ([(RAIDDevice *)item RAIDStatus]) {
							case kNDASRAIDStatusGood:
							{
								[(ImageAndTextCell *) cell setTextColor: [NSColor blackColor] ];
							}
								break;
							default:
							{
								[(ImageAndTextCell *) cell setTextColor: [NSColor blackColor] ];
							}
								break;								
						}
						
						
					}
						break;
				}
			}
			//[NSImage autorelease];
		}
	} else 	if ([outlineView isEqual: RAIDSetOutlineView]) {

		RAIDSubDevice	*raidDevice = (RAIDSubDevice *)item;
		NSString	*tempString;
		UNIT_DISK_LOCATION	location;
		
		imageSize.height = 16;
		imageSize.width = 16;

		*((UInt64 *)&location) = [raidDevice deviceID];
		
		if( [[tableColumn identifier] isEqualToString: @"RAIDIndex"] ) {
			
			NDASPhysicalDeviceInformation	NDASInfo = { 0 };
			
			memcpy(NDASInfo.address, location.MACAddr, 6);
			
			if (NDASGetInformationByAddress(&NDASInfo)) {
				
				NDASDevice	*entry = [NDASDevices objectForKey: [NSNumber numberWithInt: NDASInfo.slotNumber]];
				UnitDevice	*unitDevice = [entry unitAtIndex: location.UnitNumber];
				
				switch ([entry status]) {
					case kNDASStatusOnline:
					{
						// Invalid Sub Unit.
						if ( kNDASUnitMediaTypeHDD != [unitDevice type] ) {
							image = [NSImage imageNamed:@"red"];
						} else {
							if ([[raidDevice RAIDDeviceID] unitDeviceType] == [unitDevice diskArrayType] ) {
								if ([unitDevice isInRunningStatus]) {
									
									if (kNDASRAIDSubUnitStatusGood == [raidDevice subUnitStatus]) {
										image = [NSImage imageNamed:@"green"];							
									} else {
										image = [NSImage imageNamed:@"yellow"];
									}
								} else {
									switch([raidDevice subUnitStatus]) {
										case kNDASRAIDSubUnitStatusGood:
											image = [NSImage imageNamed:@"gray"];												
											break;
										case kNDASRAIDSubUnitStatusFailure:
											image = [NSImage imageNamed:@"red"];												
											break;
										case kNDASRAIDSubUnitStatusNeedRecovery:
										case kNDASRAIDSubUnitStatusRecovering:
										case kNDASRAIDSubUnitStatusNotPresent:												
										default:
											image = [NSImage imageNamed:@"gray"];												
									}
								}	
							} else {
								image = [NSImage imageNamed:@"red"];												
							}
						}
					}
						break;
					case kNDASStatusOffline:
					case kNDASStatusNoPermission:
					case kNDASStatusFailure:
					default:
					{
						image = [NSImage imageNamed:@"red"];
					}
						break;
				}
				
				[image setScalesWhenResized: TRUE];
				[image setSize: imageSize];
				
				// Set Image.
				[(ImageAndTextCell *) cell setImage: image];
				
			} else {
				image = [NSImage imageNamed:@"red"];				
				
				[image setScalesWhenResized: TRUE];
				[image setSize: imageSize];
				
				// Set Image.
				[(ImageAndTextCell *) cell setImage: image];					
			}
			
			
		} else if ( [[tableColumn identifier] isEqualToString: @"SubUnit"] ) {
			
			NDASPhysicalDeviceInformation	NDASInfo = { 0 };
			
			memcpy(NDASInfo.address, location.MACAddr, 6);
			
			if (NDASGetInformationByAddress(&NDASInfo)) {
				
				NDASDevice	*entry = [NDASDevices objectForKey: [NSNumber numberWithInt: NDASInfo.slotNumber]];
				UnitDevice	*unitDevice = [entry unitAtIndex: location.UnitNumber];
				
				if ( kNDASUnitMediaTypeHDD == [unitDevice type] ) {
					if ([[raidDevice RAIDDeviceID] unitDeviceType] == [unitDevice diskArrayType]
						&& (kNDASRAIDSubUnitStatusGood == [raidDevice subUnitStatus] || kNDASRAIDSubUnitStatusNeedRecovery == [raidDevice subUnitStatus] || kNDASRAIDSubUnitStatusRecovering == [raidDevice subUnitStatus])) {
						image = [NSImage imageNamed:@"Disk"];
					} else {
						image = [NSImage imageNamed:@"Caution"];
					}
				} else {
					image = [NSImage imageNamed:@"ODD"];
				}
			} else {
				image = [NSImage imageNamed:@"Caution"];
			}
			
			[image setScalesWhenResized: TRUE];
			[image setSize: imageSize];
			
			// Set Image.
			[(ImageAndTextCell *) cell setImage: image];
			/*
			NSBox *horizontalSeparator=[[NSBox alloc] initWithFrame:NSMakeRect(15.0,250.0,250.0,1.0)];
			[horizontalSeparator setBoxType:NSBoxSeparator];
			
			[(NSCell *) cell setObjectValue: horizontalSeparator];
			 */
		}
		
	}
}

- (void)outlineView:(NSOutlineView *)outlineView willDisplayOutlineCell:(id)cell forTableColumn:(NSTableColumn *)tableColumn item:(id)item
{
//	NSLog(@"willDisplayOutlineCell\n");
	
	if ([outlineView isEqual: ndasOutlineView]) {
		
		if ( [[tableColumn identifier] isEqualToString: @"FirstColumn"] ) {
			
			if ( kNDASDeviceTypeNDAS == [(Device *)item deviceType] ) {
				
				NDASDevice		*device = (NDASDevice *)item;
				
				switch ([device status]) {
					case kNDASStatusOnline:
						[cell setImage: [NSImage imageNamed:@"gray"]];
						[cell setAlternateImage: [NSImage imageNamed:@"gray"]];
						
						break;
					case kNDASStatusOffline:
						[cell setImage: [NSImage imageNamed:@"blank"]];
						[cell setAlternateImage: [NSImage imageNamed:@"blank"]];
						
						break;
					case kNDASStatusNoPermission:
					case kNDASStatusFailure:
					default:
						[cell setImage: [NSImage imageNamed:@"red"]];
						[cell setAlternateImage: [NSImage imageNamed:@"red"]];
						
						break;						
				}
			} else if ( kNDASDeviceTypeUnit == [(Device *)item deviceType] ) {
				
				UnitDevice *device = (UnitDevice *)item;
				
				switch( [device status] ) {											
					case kNDASUnitStatusConnectedRW:
						[cell setImage: [NSImage imageNamed:@"blue"]];
						[cell setAlternateImage: [NSImage imageNamed:@"blue"]];
						
						break;
					case kNDASUnitStatusConnectedRO:
						[cell setImage: [NSImage imageNamed:@"green"]];
						[cell setAlternateImage: [NSImage imageNamed:@"green"]];
						
						break;
					case kNDASUnitStatusTryConnectRO:
					case kNDASUnitStatusTryConnectRW:
					case kNDASUnitStatusTryDisconnectRO:
					case kNDASUnitStatusTryDisconnectRW:
					case kNDASUnitStatusReconnectRO:
					case kNDASUnitStatusReconnectRW:
						[cell setImage: [NSImage imageNamed:@"yellow"]];
						[cell setAlternateImage: [NSImage imageNamed:@"yellow"]];
						
						break;
					case kNDASUnitStatusFailure:
						[cell setImage: [NSImage imageNamed:@"red"]];
						[cell setAlternateImage: [NSImage imageNamed:@"red"]];
						
						break;
					default:
						[cell setImage: [NSImage imageNamed:@"gray"]];
						[cell setAlternateImage: [NSImage imageNamed:@"gray"]];

						break;
				}
				

			} else if ( kNDASDeviceTypeRAID == [(Device *)item deviceType] ) {
				
				RAIDDevice		*device = (RAIDDevice *)item;
				
				switch ([device RAIDStatus]) {
					case kNDASRAIDStatusGood:
						
						switch([device status]) {
							case kNDASUnitStatusConnectedRO:
							case kNDASUnitStatusConnectedRW:
								[cell setImage: [NSImage imageNamed:@"green"]];
								[cell setAlternateImage: [NSImage imageNamed:@"green"]];
								
								break;
							case kNDASUnitStatusReconnectRO:
							case kNDASUnitStatusReconnectRW:
								[cell setImage: [NSImage imageNamed:@"yellow"]];
								[cell setAlternateImage: [NSImage imageNamed:@"yellow"]];
																
								break;
							case kNDASUnitStatusFailure:
								[cell setImage: [NSImage imageNamed:@"red"]];
								[cell setAlternateImage: [NSImage imageNamed:@"red"]];
																
								break;
							default:
								[cell setImage: [NSImage imageNamed:@"gray"]];
								[cell setAlternateImage: [NSImage imageNamed:@"gray"]];
								
								break;
						}
													
						break;						
					case kNDASRAIDStatusGoodButPartiallyFailed:
					case kNDASRAIDStatusBrokenButWorkable:
						[cell setImage: [NSImage imageNamed:@"yellow"]];
						[cell setAlternateImage: [NSImage imageNamed:@"yellow"]];
						
						break;
					case kNDASRAIDStatusRecovering:
					case kNDASRAIDStatusReadyToRecovery:
						[cell setImage: [NSImage imageNamed:@"yellow"]];
						[cell setAlternateImage: [NSImage imageNamed:@"yellow"]];
						
						break;						
					case kNDASRAIDStatusNotReady:
					case kNDASRAIDStatusBroken:
					case kNDASRAIDStatusReadyToRecoveryWithReplacedUnit:
					default:
						[cell setImage: [NSImage imageNamed:@"red"]];
						[cell setAlternateImage: [NSImage imageNamed:@"red"]];
						
						break;						
				}
							
			} else {
				NSLog(@"willDisplayOutlineCell: Unknown Type\n");
			}
		} else {
			NSLog(@"willDisplayOutlineCell: Not First Column\n");
		}
	} else {
		NSLog(@"willDisplayOutlineCell: Not Outline View\n");
	}
}

/*
- (BOOL)outlineView:(NSOutlineView *)outlineView shouldEditTableColumn:(NSTableColumn *)tableColumn item:(id)item;
{
	return NO;
}
*/
/*
- (BOOL)outlineView:(NSOutlineView *)outlineView shouldExpandItem:(id)item
{		  
	return TRUE;
}
*/

- (BOOL)outlineView:(NSOutlineView *)outlineView shouldCollapseItem:(id)item
{
	return FALSE;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView shouldSelectItem:(id)item
{
	if ([outlineView isEqual: ndasOutlineView]) {
		// Prevent selection of separate bar.
		if ( nil == item ) {
			return NO;
		}
	} else if ([outlineView isEqual: RAIDSetOutlineView]) {
		return NO;
	}
	
	return YES;
}

//
// Toolbar.
//
- (void) setupToolbar {
    // Create a new toolbar instance, and attach it to our document window 
    NSToolbar *toolbar = [[NSToolbar alloc] initWithIdentifier: MyDocToolbarIdentifier];
    
    // Set up toolbar properties: Allow customization, give a default display mode, and remember state in user defaults 
    [toolbar setAllowsUserCustomization: YES];
    [toolbar setAutosavesConfiguration: YES];
    [toolbar setDisplayMode: NSToolbarDisplayModeIconOnly];
    
    // We are the delegate
    [toolbar setDelegate: self];
    
    // Attach the toolbar to the main window 
	[[NSApp mainWindow] setToolbar: toolbar];
	[toolbar release];
}

- (NSToolbarItem *) toolbar: (NSToolbar *)toolbar itemForItemIdentifier: (NSString *) itemIdent willBeInsertedIntoToolbar:(BOOL) willBeInserted {
    // Required delegate method:  Given an item identifier, this method returns an item 
    // The toolbar will use this method to obtain toolbar items that can be displayed in the customization sheet, or in the toolbar itself 
    NSToolbarItem *toolbarItem = [[[NSToolbarItem alloc] initWithItemIdentifier: itemIdent] autorelease];
    
	// Get Model Name.
//	NSDictionary *infoDic = [[NSBundle mainBundle] infoDictionary];

// FIXME: toolbar text is not appear.
    if ([itemIdent isEqual: RegisterToolbarItemIdentifier]) {
		// Set the text label to be displayed in the toolbar and customization palette 
		[toolbarItem setLabel:  NSLocalizedString(@"TOOLBAR_REGISTER : Register", @"Register tool bar label")];
		[toolbarItem setPaletteLabel: NSLocalizedString(@"TOOLBAR_REGISTER : Register", @"Register tool bar label")];
		
		// Set up a reasonable tooltip, and image   Note, these aren't localized, but you will likely want to localize many of the item's properties 
		[toolbarItem setToolTip: NSLocalizedString(@"TOOLBAR_REGISTER_TOOLTIP : Register new NDAS device", @"Register tool tip")];
		[toolbarItem setImage: [NSImage imageNamed: @"Register"]];
		
		// Tell the item what message to send when it is clicked 
		[toolbarItem setTarget: self];
		[toolbarItem setAction: @selector(registerDevice:)];
    } else if([itemIdent isEqual: UnregisterToolbarItemIdentifier]) {
		NSMenu *submenu = nil;
		NSMenuItem *submenuItem = nil, *menuFormRep = nil;
		
		// Set up the standard properties 
		[toolbarItem setLabel: NSLocalizedString(@"TOOLBAR_UNREGISTER : Unregister", @"Unregister")];
		[toolbarItem setPaletteLabel: NSLocalizedString(@"TOOLBAR_UNREGISTER : Unregister", @"Unregister")];
		[toolbarItem setToolTip: NSLocalizedString(@"TOOLBAR_UNREGISTER_TOOLTIP : Unregister the selected NDAS device", @"Unregister tool tip")];			
		
		[toolbarItem setImage: [NSImage imageNamed: @"Unregister"]];
		
		// Tell the item what message to send when it is clicked 
		[toolbarItem setTarget: self];
		[toolbarItem setAction: @selector(unregisterDevice:)];
    } else if ([itemIdent isEqual: RefreshAllToolbarIdentifier]) {
		// Set the text label to be displayed in the toolbar and customization palette 
		[toolbarItem setLabel:  NSLocalizedString(@"TOOLBAR_REFRESH_ALL : Refresh All", @"Refresh tool bar label")];
		[toolbarItem setPaletteLabel: NSLocalizedString(@"TOOLBAR_REFRESH_ALL : Refresh All", @"Refresh tool bar label")];
		
		// Set up a reasonable tooltip, and image   Note, these aren't localized, but you will likely want to localize many of the item's properties 
		[toolbarItem setToolTip: NSLocalizedString(@"TOOLBAR_REFRESH_ALL_TOOLTIP : Refresh information of all devices", @"Refresh all tool tip")];
		[toolbarItem setImage: [NSImage imageNamed: @"reload"]];
		
		// Tell the item what message to send when it is clicked 
		[toolbarItem setTarget: self];
		[toolbarItem setAction: @selector(refreshAll:)];
/*	} else if ([itemIdent isEqual: BindToolbarItemIdentifier]) {
		// Set the text label to be displayed in the toolbar and customization palette 
		[toolbarItem setLabel:  NSLocalizedString(@"TOOLBAR_BIND : Bind", @"Bind")];
		[toolbarItem setPaletteLabel: NSLocalizedString(@"TOOLBAR_BIND : Bind", @"Bind")];
		
		// Set up a reasonable tooltip, and image   Note, these aren't localized, but you will likely want to localize many of the item's properties 
		[toolbarItem setToolTip: NSLocalizedString(@"TOOLBAR_BIND_TOOLTIP : Create the new RAID device", @"Bind tool tip")];
		[toolbarItem setImage: [NSImage imageNamed: @"RAID"]];
		
		// Tell the item what message to send when it is clicked 
		[toolbarItem setTarget: self];
		[toolbarItem setAction: @selector(RAIDBind:)];
*/	} else {
		// itemIdent refered to a toolbar item that is not provide or supported by us or cocoa 
		// Returning nil will inform the toolbar this kind of item is not supported 
		toolbarItem = nil;
    }
    return toolbarItem;
}

- (NSArray *) toolbarDefaultItemIdentifiers: (NSToolbar *) toolbar {
    // Required delegate method:  Returns the ordered list of items to be shown in the toolbar by default    
    // If during the toolbar's initialization, no overriding values are found in the user defaults, or if the
    // user chooses to revert to the default items this set will be used 
    return [NSArray arrayWithObjects: RegisterToolbarItemIdentifier, UnregisterToolbarItemIdentifier, NSToolbarSeparatorItemIdentifier,
		RefreshAllToolbarIdentifier, /*NSToolbarSeparatorItemIdentifier,*/
		/*BindToolbarItemIdentifier, */NSToolbarFlexibleSpaceItemIdentifier, NSToolbarSpaceItemIdentifier, nil];
}

- (NSArray *) toolbarAllowedItemIdentifiers: (NSToolbar *) toolbar {
    // Required delegate method:  Returns the list of all allowed items by identifier.  By default, the toolbar 
    // does not assume any items are allowed, even the separator.  So, every allowed item must be explicitly listed   
    // The set of allowed items is used to construct the customization palette 
    return [NSArray arrayWithObjects: RegisterToolbarItemIdentifier, UnregisterToolbarItemIdentifier, 
		/*BindToolbarItemIdentifier, */RefreshAllToolbarIdentifier,
		NSToolbarCustomizeToolbarItemIdentifier,
		NSToolbarFlexibleSpaceItemIdentifier, NSToolbarSpaceItemIdentifier, NSToolbarSeparatorItemIdentifier, nil];
}

- (BOOL) validateToolbarItem: (NSToolbarItem *) toolbarItem {
    // Optional method:  This message is sent to us since we are the target of some toolbar item actions 
    // (for example:  of the save items action) 
    BOOL enable = NO;
	
	if ( sentRequest ) {
		return enable;
	}
	
    if ([[toolbarItem itemIdentifier] isEqual: RegisterToolbarItemIdentifier]) {
		// Always.
		enable = YES;
    } else if ([[toolbarItem itemIdentifier] isEqual: UnregisterToolbarItemIdentifier]) {
		
		if ( 0 <= [ndasOutlineView selectedRow] ) {
			Device *device = [ndasOutlineView itemAtRow: [ndasOutlineView selectedRow]];
			
			if ( kNDASDeviceTypeNDAS == [(Device *)device deviceType] ) {
				NDASDevice *ndas = (NDASDevice *)device;
				
				if ( ![ndas autoRegister] || (kNDASStatusOffline == [ndas status] || kNDASStatusNeedRegisterKey == [ndas status]) ) {
					enable = YES;
				} else {
					enable = NO;
				}
			} else {
				enable = NO;
			}
		}
	} else	if ([[toolbarItem itemIdentifier] isEqual: RefreshAllToolbarIdentifier]) {

		if (0 < [NDASDevices count]) {
			return YES;
		}
		
/*    } else if ([[toolbarItem itemIdentifier] isEqual: BindToolbarItemIdentifier]) {
		//Always.
		enable = YES;
*/	}
	
    return enable;
}

//
// Main Window delegate.
//

- (void)windowDidBecomeMain:(NSNotification *)aNotification
{
	NSDictionary	*infoDic;
	
	// Get the application's bundle and read it's get info string.
	infoDic = [[NSBundle mainBundle] localizedInfoDictionary];
	if( nil == infoDic ) {
		NSLog(@"NDAdmin: Can't get infoDic");
	}
		
	// Change Main window title.
	[[aNotification object] setTitle: [infoDic valueForKey:@"CFBundleName"]]; 

	// Create Toolbar.
	[self setupToolbar];
}

- (BOOL) windowShouldClose: (NSWindow *) sender
{
	[NSApp terminate: self];
}

//
// MenuItem delegate.
//
- (BOOL)validateMenuItem:(NSMenuItem*)anItem {
	// Required Function. Do not remove.
}

//
// Name field delegate.
//
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

- (void)controlTextDidBeginEditing:(NSNotification *)aNotification
{
	editingName = TRUE;	
}

- (void)controlTextDidEndEditing:(NSNotification *)aNotification
{
//	NSLog(@"controlTextDidEndEditing");
	
	editingName = FALSE;
	// Find Device.
	int selectedRow;
	
	selectedRow = [ndasOutlineView selectedRow];
	
	if(-1 < selectedRow ) {
		
		id							device;
		
		device = [ndasOutlineView itemAtRow: selectedRow];
		if( nil == device ) {
			return;
		} else {
			
			switch ([(Device *)device deviceType]) {
				case kNDASDeviceTypeNDAS:
				{
					NDASDevice					*ndasDevice;
					
					ndasDevice = (NDASDevice *)device;
					
					if ( ![[aNotification object] isEqual: nameField] ) {
						return;
					}
					
					// Process Register.
					[self _sndNotification : [ndasDevice slotNo]
								unitNumber : 0
									 index : 0
								eNdCommand : NDAS_NOTIFICATION_REQUEST_EDIT_NDAS_NAME
									  Name : [nameField stringValue]
									IDSeg1 : nil
									IDSeg2 : nil
									IDSeg3 : nil
									IDSeg4 : nil
									 WrKey : nil
								 eNdConfig : 0
								  eOptions : 0
							logicalDevices : nil
								  RAIDType : 0
									  Sync : false];
					
				}
					break;
				case kNDASDeviceTypeRAID:
				{
					RAIDDevice					*raidDevice;
					
					raidDevice = (RAIDDevice *)device;
					
					if ( ![[aNotification object] isEqual: RAIDSetNameField] ) {
						return;
					}
					
					// Process Register.
					[self _sndNotification : 0
								unitNumber : 0
									 index : [raidDevice index]
								eNdCommand : NDAS_NOTIFICATION_REQUEST_BIND_EDIT
									  Name : [RAIDSetNameField stringValue]
									IDSeg1 : nil
									IDSeg2 : nil
									IDSeg3 : nil
									IDSeg4 : nil
									 WrKey : nil
								 eNdConfig : 0
								  eOptions : 0
							logicalDevices : nil
								  RAIDType : 0
									  Sync : false];
				}
					break;
			}
		}
	}
	
	
//	[self refreshNDASDevices];
	
//	[self updateInformation];
}

- (IBAction)RAIDBind:(id)sender
{
	if ( sentRequest ) { 
		return;
	}

	
	RAIDBindSheetSubUnitDevices = [[NSMutableArray alloc] init];
	
	[RAIDBindSheetSubUnitOutlineView reloadData];

	[self refreshNDASDevices];
	[RAIDBindSheetUnitOutlineView reloadData];
	
	// Expand All.
	int index;
	
	for (index = 0; index < [[NDASDevices allValues] count]; index++) {
		[RAIDBindSheetUnitOutlineView expandItem: [[NDASDevices allValues] objectAtIndex: index]];
	}
	
	// Disable Register Button.
	[RAIDBindSheetBindButton setEnabled: false];
	[RAIDBindSheetRemoveButton setEnabled: false];
	
	// Cleanup.
	
	// Enter Default Name.
	NDASRAIDDeviceInformation	NDASInfo;
	index = 0;
	
	// Find Empty Index.
	do {
		NDASInfo.index = index;
		if(!NDASGetRAIDDeviceInformationByIndex(&NDASInfo)) {
			break;
		}
		index++;
	} while(TRUE);
	
	[RAIDBindSheetNameField setStringValue:[NSString stringWithFormat:NSLocalizedString(@"PANEL_RAID_BIND : RAID Set %d", @"Default RAID set name. Referenced Disk Utility... But this feature is meaningless up to NDAS 2.0 as the name is not stored in the device"), index]];
	[RAIDBindSheetErrorMessageField setStringValue: @" "];
	
	// Forcus Name field.
	[RAIDBindSheetNameField selectText: self];
	
	[NSApp beginSheet: RAIDBindSheet 
	   modalForWindow: [NSApp keyWindow]
		modalDelegate: self
	   didEndSelector: nil
		  contextInfo: nil];	
}

- (IBAction)RAIDBindSheetBind:(id)sender
{
	[NSApp endSheet: RAIDBindSheet];
	[RAIDBindSheet close];
	
	// Create Logical Device Array.
	NSMutableArray *logicalDevices = nil;
	int index;
	int RAIDType;
	bool	bSync = false;
	
	logicalDevices = [[NSMutableArray alloc] init];
		
	for ( index = 0; index < [RAIDBindSheetSubUnitDevices count]; index++) {

		NDASLogicalDeviceInformation	LDInfo = { 0 };

		LDInfo.LowerUnitDeviceID = *[(Device *)[RAIDBindSheetSubUnitDevices objectAtIndex: index] deviceID];
		
		if (NDASGetLogicalDeviceInformationByLowerUnitDeviceID(&LDInfo)) {
			[logicalDevices insertObject: [NSNumber numberWithInt: LDInfo.index] atIndex: index];
		} else {
			[logicalDevices release];
			return;
		}
	}
	
	switch([RAIDBindSheetTypePopupButton indexOfSelectedItem]) {
		case 0:
		{
			RAIDType = NMT_AGGREGATE;
		}
			break;
		case 1:
		{
			RAIDType = NMT_RAID0R2;
		}
			break;
		case 2:
		{
			RAIDType = NMT_RAID1R3;
		}
			break;
		default:
			[logicalDevices release];
			return;
	}
	
	// RAID 1 can sync.
	if (NMT_RAID1R3 == RAIDType) {
		
		NSString *tempString;
		
		switch([(Device *)[RAIDBindSheetSubUnitDevices objectAtIndex: 0] deviceType]) {
			case kNDASDeviceTypeUnit:
			{
				NDASDevice *entry = nil;
				UnitDevice *unitDevice = nil;
				
				unitDevice = (UnitDevice *)[RAIDBindSheetSubUnitDevices objectAtIndex: 0];
				entry = [unitDevice ndasId];
								
				tempString = [NSString stringWithFormat: @"%@(%d) %@", [entry name], [unitDevice unitNumber], [unitDevice model]];
				
			}
				break;
			case kNDASDeviceTypeRAID:
			{
				RAIDDevice *raidDevice = nil;
				
				raidDevice = (RAIDDevice *)[RAIDBindSheetSubUnitDevices objectAtIndex: 0];
								
				tempString = [NSString stringWithFormat: @"%@", [raidDevice name]];
				
			}
				break;
			default:
				tempString = @"Unknown";
		}
		
		int result = NSRunCriticalAlertPanel(
											 [NSString stringWithFormat: NSLocalizedString(@"PANEL-TITLE : Do you want to bind the disks or synchronize the disks with the data of '%@' ?", @"Bind Question of RAID1"), tempString],
											 NSLocalizedString(@"PANEL-MSG : If you choose 'Bind', all of the data will be lost once the disks are bound.", @"Bind Info of RAID1"),
											 NSLocalizedString(@"PANEL-BUTTON : Cancel", @"Text for button in panel"),
											 NSLocalizedString(@"PANEL-BUTTON : Synchronize", @"Text for button in panel"),
											 NSLocalizedString(@"PANEL-BUTTON : Bind", @"Text for button in panel")
											 );
		
		switch(result) {
			case NSAlertDefaultReturn:
				[logicalDevices release];
				
				return;
			case NSAlertAlternateReturn:
			{
				NSString	*subUnitNames[2];
				UInt64		subUnitCapacities[2];
				int count;
				
				for (count = 0; count < 2; count++) {
					switch([(Device *)[RAIDBindSheetSubUnitDevices objectAtIndex: count] deviceType]) {
						case kNDASDeviceTypeUnit:
						{						
							UnitDevice *unitDevice = (UnitDevice *)[RAIDBindSheetSubUnitDevices objectAtIndex: count];
							NDASDevice *entry = [unitDevice ndasId];
							
							subUnitNames[count] = [NSString stringWithFormat: @"%@(%d)", [entry name], [unitDevice unitNumber]];
							
							subUnitCapacities[count] = [unitDevice capacity];
						}
							break;
						case kNDASDeviceTypeRAID:
						{
							RAIDDevice *raidDevice = nil;
							
							raidDevice = (RAIDDevice *)[RAIDBindSheetSubUnitDevices objectAtIndex: 0];
							
							subUnitNames[count] = [NSString stringWithFormat: @"%@", [raidDevice name]];

							subUnitCapacities[count] = [raidDevice capacity];

						}
							break;
					}
					
				}
				
				// Check Size.
				if ( subUnitCapacities[0] > subUnitCapacities[1]) {
					
					NSRunCriticalAlertPanel(
											[NSString stringWithFormat: NSLocalizedString(@"PANEL-TITLE : You can't Synchronize '%@'(NDAS unit device name) and '%@'(NDAS unit device name).", @"Bind and Sync Error."), 
												subUnitNames[0],
												subUnitNames[1]],
											[NSString stringWithFormat: NSLocalizedString(@"PANEL-MSG : The size of '%@'(NDAS unit device name) is bigger than the size of new RAID Set.", @"Bind and Sync Error Info"), 
												subUnitNames[0]],
											NSLocalizedString(@"PANEL-BUTTON : Cancel", @"Text for button in panel"),	
											nil,
											nil
											);
					
					[logicalDevices release];

					return;
					
				} else {
					bSync = true;
				}
				
			}
				break;
			case NSAlertOtherReturn:
			{
				int result = NSRunCriticalAlertPanel(
													 NSLocalizedString(@"PANEL-TITLE : Are you sure you want to bind the disks?", @"Confirm binding"), 
													 NSLocalizedString(@"PANEL-MSG : All of the data will be lost once the disks are bound.", @"Confirm binding"),
													 NSLocalizedString(@"PANEL-BUTTON : Cancel", @"Text for button in panel"),
													 NSLocalizedString(@"PANEL-BUTTON : Bind", @"Text for button in panel"),
													 nil
													 );
				
				switch(result) {
					case NSAlertDefaultReturn:
						[logicalDevices release];
						return;
					case NSAlertAlternateReturn:
						break;
					default:
						[logicalDevices release];
						return;
				}				
			}
				break;
			default:
				[logicalDevices release];
				return;
		}
		
	} else {

		int result = NSRunCriticalAlertPanel(
											 NSLocalizedString(@"PANEL-TITLE : Are you sure you want to bind the disks?", @"Confirm binding"), 
											 NSLocalizedString(@"PANEL-MSG : All of the data will be lost once the disks are bound.", @"Confirm binding"),
											 NSLocalizedString(@"PANEL-BUTTON : Cancel", @"Text for button in panel"),
											 NSLocalizedString(@"PANEL-BUTTON : Bind", @"Text for button in panel"),
											 nil
											 );
		
		switch(result) {
			case NSAlertDefaultReturn:
				[logicalDevices release];
				
				return;
			case NSAlertAlternateReturn:
				break;
			default:
				[logicalDevices release];
				return;
		}
		
	}
	
	// Process Bind.
	[self _sndNotification : 0	// Auto select.
				unitNumber : 0
					 index : 0
				eNdCommand : NDAS_NOTIFICATION_REQUEST_BIND 
					  Name : [RAIDBindSheetNameField stringValue]
					IDSeg1 : nil
					IDSeg2 : nil
					IDSeg3 : nil
					IDSeg4 : nil
					 WrKey : nil
				 eNdConfig : 0
				  eOptions : 0
			logicalDevices : logicalDevices
				  RAIDType : RAIDType
					  Sync : bSync];
	
	[logicalDevices release];
	[RAIDBindSheetSubUnitDevices release];
	RAIDBindSheetSubUnitDevices = nil;
	//	[self refreshNDASDevices];
	
	[self updateInformation];	
}

- (IBAction)RAIDBindSheetCancel:(id)sender
{
	[NSApp endSheet: RAIDBindSheet];
	[RAIDBindSheet close];
		
	[RAIDBindSheetSubUnitDevices release];
	RAIDBindSheetSubUnitDevices = nil;
	
	[self updateInformation];	
}

- (IBAction)RAIDUnbind:(id)sender
{
	if ( sentRequest ) { 
		return;
	}
	
	int selectedRow;
		
	selectedRow = [ndasOutlineView selectedRow];
	
	if(-1 < selectedRow) {
		
		id							device;
		UInt32						configuration;
		UInt32						status;
		UInt32						type;
		UInt32						diskArrayType;
		GUID						deviceID;
		NSString					*nameString;
		
		device = [ndasOutlineView itemAtRow: selectedRow];
		if( nil == device ||
			kNDASDeviceTypeRAID != [(Device *)device deviceType] ) {
			return;
		}
		
		NDASRAIDDeviceInformation	NDParam = { 0 };
		RAIDDevice *raidDevice;
		
		raidDevice = (RAIDDevice *)device;
		
		nameString = [raidDevice name];
		
		NDParam.index = [raidDevice index];
		if ( NDASGetRAIDDeviceInformationByIndex(&NDParam) ) {
			type = NDParam.unitDeviceType;
			diskArrayType = NDParam.diskArrayType;
			configuration = NDParam.configuration;
			status = NDParam.status;
			deviceID = NDParam.deviceID;
		} else {
			return;
		}
		
		if ( kNDASUnitConfigurationUnmount == configuration
			 && ( kNDASUnitStatusConnectedRW != status || 
				  kNDASUnitStatusConnectedRO != status )) {
			
			int answer;
			
			answer = NSRunCriticalAlertPanel(
									 [NSString stringWithFormat: NSLocalizedString(@"PANEL-TITLE : Are you sure you want to unbind '%@'?", @"Confirm unbinding"), nameString],
									 NSLocalizedString(@"PANEL-MSG : You may lose or corrupt your data.", @"Confirm unbinding"),
									 NSLocalizedString(@"PANEL-BUTTON : Cancel", @"Text for button in panel"),
									 NSLocalizedString(@"PANEL-BUTTON : Unbind", @"Text for button in panel"),
									 nil
									 );
			if( NSAlertAlternateReturn == answer ) {
				
				[self _sndNotification : 0
							unitNumber : 0
								 index : NDParam.index
							eNdCommand : NDAS_NOTIFICATION_REQUEST_UNBIND
								  Name : nil
								IDSeg1 : nil
								IDSeg2 : nil
								IDSeg3 : nil
								IDSeg4 : nil
								 WrKey : nil
							 eNdConfig : 0
							  eOptions : 0
						logicalDevices : nil
							  RAIDType : 0
								  Sync : false]; 
				
			}
		}
	}
}

- (IBAction)RAIDMigrate:(id)sender
{
	if ( sentRequest ) { 
		return;
	}
	
	int selectedRow;
	
	selectedRow = [ndasOutlineView selectedRow];
	
	if(-1 < selectedRow) {
		
		id							device;
		UInt32						configuration;
		UInt32						status;
		UInt32						type;
		UInt32						diskArrayType;
		GUID						deviceID;
		NSString					*nameString;
		
		device = [ndasOutlineView itemAtRow: selectedRow];
		if( nil == device ||
			kNDASDeviceTypeRAID != [(Device *)device deviceType] ) {
			return;
		}
		
		NDASRAIDDeviceInformation	NDParam = { 0 };
		RAIDDevice *raidDevice;
		
		raidDevice = (RAIDDevice *)device;
		
		nameString = [raidDevice name];
		
		NDParam.index = [raidDevice index];
		if ( NDASGetRAIDDeviceInformationByIndex(&NDParam) ) {
			type = NDParam.unitDeviceType;
			diskArrayType = NDParam.diskArrayType;
			configuration = NDParam.configuration;
			status = NDParam.status;
			deviceID = NDParam.deviceID;
		} else {
			return;
		}
		
		if ( kNDASUnitConfigurationUnmount == configuration
			 && ( kNDASUnitStatusConnectedRW != status || 
				  kNDASUnitStatusConnectedRO != status )) {
			
			int answer;
			
			switch([raidDevice unitDeviceType]) {
				case NMT_MIRROR:
				case NMT_RAID1:
				case NMT_RAID1R2:
				{
					
					
					answer = NSRunAlertPanel(
											 [NSString stringWithFormat: NSLocalizedString(@"PANEL-TITLE : Do you want to migrate The RAID Device '%@'(NDAS device name)?", @"Confirm migration"), nameString],
											 NSLocalizedString(@"PANEL-MSG : If you want to use the RAID device that was configured to old type, you should migrate that RAID Device.", @"Confirm migration"),
											 NSLocalizedString(@"PANEL-BUTTON : Cancel", @"Text for button in panel"),
											 NSLocalizedString(@"PANEL-BUTTON : Migrate", @"Text for button in panel"),
											 nil
											 );
					if( NSAlertAlternateReturn == answer ) {
						
						UInt32	newRAIDType;
						
						switch([raidDevice unitDeviceType]) {
							case NMT_MIRROR:
							case NMT_RAID1:
							case NMT_RAID1R2:
								newRAIDType = NMT_RAID1R3;
								break;
							case NMT_RAID4:
								newRAIDType = NMT_RAID4R3;
								break;
							default:
								return;
						}
						
						[self _sndNotification : 0
									unitNumber : 0
										 index : NDParam.index
									eNdCommand : NDAS_NOTIFICATION_REQUEST_BIND_EDIT
										  Name : nil
										IDSeg1 : nil
										IDSeg2 : nil
										IDSeg3 : nil
										IDSeg4 : nil
										 WrKey : nil
									 eNdConfig : 0
									  eOptions : 0
								logicalDevices : nil
									  RAIDType : newRAIDType
										  Sync : false]; 
						
					}
				}
					break;
				default:
					break;
			}
		}
	}	
}

- (IBAction)RAIDRebind:(id)sender
{		
	if ( sentRequest ) { 
		return;
	}
	
	int selectedRow;
	
	selectedRow = [ndasOutlineView selectedRow];
	
	if(-1 < selectedRow) {
		
		id							device;
		UInt32						configuration;
		UInt32						status;
		UInt32						type;
		UInt32						diskArrayType;
		GUID						deviceID;
		NSString					*nameString;
		UInt32						rwHosts;
		UInt32						roHosts;
		
		device = [ndasOutlineView itemAtRow: selectedRow];
		if( nil == device ||
			kNDASDeviceTypeRAID != [(Device *)device deviceType] ) {
			return;
		}
		
		NDASRAIDDeviceInformation	NDParam = { 0 };
		RAIDDevice *raidDevice;
		
		raidDevice = (RAIDDevice *)device;
		
		nameString = [raidDevice name];
		
		NDParam.index = [raidDevice index];
		if ( NDASGetRAIDDeviceInformationByIndex(&NDParam) ) {
			type = NDParam.unitDeviceType;
			diskArrayType = NDParam.diskArrayType;
			configuration = NDParam.configuration;
			status = NDParam.status;
			deviceID = NDParam.deviceID;
			rwHosts = NDParam.RWHosts;
			roHosts = NDParam.ROHosts;
		} else {
			return;
		}
		
		if ( kNDASUnitConfigurationUnmount == configuration
			 && kNDASUnitStatusConnectedRW != status
			 && kNDASUnitStatusConnectedRO != status
			 && 0 == rwHosts
			 && 0 == roHosts
			 ) {
			
			int answer;
			
			answer = NSRunCriticalAlertPanel(
											 [NSString stringWithFormat: NSLocalizedString(@"PANEL-TITLE : Are you sure you want to rebind '%@'?", @"Confirm rebinding"), nameString],
											 @"",
											 NSLocalizedString(@"PANEL-BUTTON : Cancel", @"Text for button in panel"),
											 NSLocalizedString(@"PANEL-BUTTON : Rebind", @"Text for button in panel"),
											 nil
											 );
			if( NSAlertAlternateReturn == answer ) {
				
				[self _sndNotification : 0
							unitNumber : 0
								 index : NDParam.index
							eNdCommand : NDAS_NOTIFICATION_REQUEST_REBIND
								  Name : nil
								IDSeg1 : nil
								IDSeg2 : nil
								IDSeg3 : nil
								IDSeg4 : nil
								 WrKey : nil
							 eNdConfig : 0
							  eOptions : 0
						logicalDevices : nil
							  RAIDType : 0
								  Sync : false]; 
				
			}
		}
	}
}

- (IBAction)RAIDResync:(id)sender
{
}

- (IBAction)exportRegistrationInformation:(id)sender
{
	if ( sentRequest ) { 
		return;
	}
	
	int selectedRow;
	
	//NSLog(@"unmount");
	
	selectedRow = [ndasOutlineView selectedRow];
	
	if(-1 < selectedRow) {
		
		id							device;
		UInt32						configuration;
		UInt32						status;
		UInt32						type;
		UInt32						diskArrayType;
		GUID						deviceID;
		NSString					*nameString;
		NDASDevice					*ndasDevice;
		UnitDevice					*unitDevice;
		
		device = [ndasOutlineView itemAtRow: selectedRow];
		if( nil == device ) {
			return;
		}

		switch ([(Device *)device deviceType]) {
			case kNDASDeviceTypeNDAS:

				ndasDevice = (NDASDevice *)device;

				break;
			case kNDASDeviceTypeUnit:
				
				unitDevice = (UnitDevice *)device; 
				ndasDevice = [unitDevice ndasId];
				
				break;
			default:
				return;
		}
		
		// Disable Export Button.
		[ExportRegistrationExportButton setEnabled: false];
		
		// init Values.
		NSRange rangeIDSeg1 = {0, 5}, rangeIDSeg2 = {6, 5}, rangeIDSeg3 = {12, 5};

		[ExportRegistrationErrorString setStringValue:@" "];
		[ExportRegistrationIDField1 setStringValue: [[ndasDevice deviceIDString] substringWithRange: rangeIDSeg1]];
		[ExportRegistrationIDField2 setStringValue: [[ndasDevice deviceIDString] substringWithRange: rangeIDSeg2]];
		[ExportRegistrationIDField3 setStringValue: [[ndasDevice deviceIDString] substringWithRange: rangeIDSeg3]];
		[ExportRegistrationIDField4 setStringValue:@""];
		[ExportRegistrationWritekeyField setStringValue:@""];
		
		[ExportRegistrationNameField setStringValue: [ndasDevice name]];
		
		if ([ndasDevice writable]) {
			[ExportRegistrationNotExportWriteKeyCheckbox setState: 0];
			[ExportRegistrationNotExportWriteKeyCheckbox setEnabled: YES];
			[ExportRegistrationWritekeyField setEnabled: YES];
		} else {
			[ExportRegistrationNotExportWriteKeyCheckbox setState: 1];
			[ExportRegistrationNotExportWriteKeyCheckbox setEnabled: NO];
			[ExportRegistrationWritekeyField setEnabled: NO];
		}
		
		// Disable first three ID fields.
		[ExportRegistrationNameField setEnabled: NO];

		[ExportRegistrationIDField1 setEnabled: NO];
		[ExportRegistrationIDField2 setEnabled: NO];
		[ExportRegistrationIDField3 setEnabled: NO];

		// Forcus ID 4 field.
		[ExportRegistrationIDField4 selectText: self];
		
		[NSApp beginSheet: ExportRegistrationSheet 
		   modalForWindow: [NSApp keyWindow]
			modalDelegate: nil
		   didEndSelector: nil
			  contextInfo: nil];
		
	}
	
}

- (void)savePanelDidEnd:(NSSavePanel *)sheet returnCode:(int)returnCode  contextInfo:(void  *)contextInfo
{
	NSData *xmlData = (NSData *)contextInfo;
	
	if (NSFileHandlingPanelOKButton == returnCode) {
		if (![xmlData writeToFile: [sheet filename] atomically:YES]) {
			NSBeep();
		}
	}	
	
	[xmlData release];

}

- (IBAction)exportRegistrationSheetExport:(id)sender
{
	[NSApp endSheet: ExportRegistrationSheet];
	[ExportRegistrationSheet close];
	
	/*
	 //
	 // XML routine for Tiger(10.4)
	 //
	NSXMLElement *root = (NSXMLElement *)[NSXMLNode elementWithName: @"nif"];
	NSXMLElement *deviceElement = (NSXMLElement *)[NSXMLNode elementWithName: @"ndasDevice"];

	[root addNamespace: [NSXMLNode namespaceWithName: @"" stringValue:@"http://schemas.ximeta.com/ndas/2005/01/nif"]];
	[root addChild: deviceElement];

	NSXMLDocument *xmlDoc = [[NSXMLDocument alloc] initWithRootElement: root];
	
	[xmlDoc setVersion:@"1.0"];
	[xmlDoc setCharacterEncoding:@"UTF-8"];
	
	[deviceElement addChild:[NSXMLNode elementWithName: @"name" stringValue: [ExportRegistrationNameField stringValue]]];
	
	NSString *idString = [NSString stringWithFormat: @"%@%@%@%@", 
		[ExportRegistrationIDField1 stringValue], 
		[ExportRegistrationIDField2 stringValue], 
		[ExportRegistrationIDField3 stringValue], 
		[ExportRegistrationIDField4 stringValue]];
	
	[deviceElement addChild:[NSXMLNode elementWithName: @"id" stringValue: idString]];
	
	[idString release];
	
	if (0 == [ExportRegistrationNotExportWriteKeyCheckbox state]) {
		[deviceElement addChild:[NSXMLNode elementWithName: @"writeKey" stringValue: [ExportRegistrationWritekeyField stringValue]]];
	}
	
	if (0 < [[ExportRegistrationDescriptionTextView string] length]) {
		[deviceElement addChild:[NSXMLNode elementWithName: @"description" stringValue: [ExportRegistrationDescriptionTextView string]]];
	}
		
	NSData *xmlData = [xmlDoc XMLDataWithOptions: NSXMLNodePrettyPrint];
	*/
	
	//
	// Create docuement.
	//
	CFXMLTreeRef		xmlDoc;
	
	CFXMLDocumentInfo	documentInfo;
	
	documentInfo.sourceURL = NULL;
	documentInfo.encoding = kCFStringEncodingUTF8;
	
	CFXMLNodeRef	docNode = CFXMLNodeCreate(
											  kCFAllocatorDefault,
											  kCFXMLNodeTypeDocument,
											  CFSTR(""),
											  &documentInfo,
											  kCFXMLNodeCurrentVersion
											  );
	
	xmlDoc = CFXMLTreeCreateWithNode(kCFAllocatorDefault, docNode);
	CFRelease(docNode);
	
	CFXMLProcessingInstructionInfo	instructionInfo;
	
	instructionInfo.dataString = CFSTR("version=\"1.0\" encoding=\"utf-8\"");
	
	CFXMLNodeRef	instructionNode = CFXMLNodeCreate(
													  NULL,
													  kCFXMLNodeTypeProcessingInstruction,
													  CFSTR("xml"),
													  &instructionInfo,
													  kCFXMLNodeCurrentVersion
													  );
	CFXMLTreeRef	instructionTree = CFXMLTreeCreateWithNode(kCFAllocatorDefault, instructionNode);
	CFRelease(instructionNode);
	CFTreeAppendChild(xmlDoc, instructionTree);
	CFRelease(instructionTree);
	
	CFXMLElementInfo	entryInfo;
	entryInfo.attributes = NULL;
	entryInfo.attributeOrder = NULL;
	entryInfo.isEmpty = NO;
	
	// Create nif (name space)
	NSDictionary *attributeDictionary = [NSDictionary
	   dictionaryWithObject:@"http://schemas.ximeta.com/ndas/2005/01/nif" forKey:@"xmlns"];
	NSArray *attributeArray = [NSArray arrayWithObject:@"xmlns"];
	
	CFXMLElementInfo nameSpaceInfo;
	nameSpaceInfo.attributes = (CFDictionaryRef) attributeDictionary;
	nameSpaceInfo.attributeOrder = (CFArrayRef) attributeArray;
	nameSpaceInfo.isEmpty = NO;	
	CFXMLNodeRef nameSpaceNode = CFXMLNodeCreate (
												kCFAllocatorDefault,
												kCFXMLNodeTypeElement,
												(CFStringRef)@"nif",
												&nameSpaceInfo,
												kCFXMLNodeCurrentVersion);	
	CFXMLTreeRef nameSpaceTree = CFXMLTreeCreateWithNode(kCFAllocatorDefault, nameSpaceNode);
	CFRelease(nameSpaceNode);
	CFTreeAppendChild(xmlDoc, nameSpaceTree);
	
	// Create real entries.
	entryInfo.attributes = NULL;
	entryInfo.attributeOrder = NULL;
	entryInfo.isEmpty = NO;
	
	// Create ndasdDevice Element.	
	CFXMLNodeRef	ndasDeviceNode = CFXMLNodeCreate(
													 kCFAllocatorDefault,
													 kCFXMLNodeTypeElement,
													 CFSTR("ndasDevice"),
													 &entryInfo,
													 kCFXMLNodeCurrentVersion
													 );
	CFXMLTreeRef	ndasDeviceTree = CFXMLTreeCreateWithNode(kCFAllocatorDefault, ndasDeviceNode);
	CFRelease(ndasDeviceNode);
	CFTreeAppendChild(nameSpaceTree, ndasDeviceTree);

	// ID	
	CFXMLNodeRef	idNode = CFXMLNodeCreate(
											 kCFAllocatorDefault,
											 kCFXMLNodeTypeElement,
											 CFSTR("id"),
											 &entryInfo,
											 kCFXMLNodeCurrentVersion
											 );
	CFXMLTreeRef	idTree = CFXMLTreeCreateWithNode(kCFAllocatorDefault, idNode);
	CFRelease(idNode);
	CFTreeAppendChild(ndasDeviceTree, idTree);
	
	NSString *idString = [NSString stringWithFormat: @"%@%@%@%@", 
		[ExportRegistrationIDField1 stringValue], 
		[ExportRegistrationIDField2 stringValue], 
		[ExportRegistrationIDField3 stringValue], 
		[ExportRegistrationIDField4 stringValue]];
			
	CFXMLNodeRef	idTextNode = CFXMLNodeCreate(
											 kCFAllocatorDefault,
											 kCFXMLNodeTypeText,
											 (CFStringRef)idString,
											 &entryInfo,
											 kCFXMLNodeCurrentVersion
											 );

	CFXMLTreeRef	idTextTree = CFXMLTreeCreateWithNode(kCFAllocatorDefault, idTextNode);
	CFRelease(idTextNode);
	CFTreeAppendChild(idTree, idTextTree);
	CFRelease(idTextTree);	
	
	CFRelease(idTree);
	
	// name	
	CFXMLNodeRef	nameNode = CFXMLNodeCreate(
											 kCFAllocatorDefault,
											 kCFXMLNodeTypeElement,
											 CFSTR("name"),
											 &entryInfo,
											 kCFXMLNodeCurrentVersion
											 );
	CFXMLTreeRef	nameTree = CFXMLTreeCreateWithNode(kCFAllocatorDefault, nameNode);
	CFRelease(nameNode);
	CFTreeAppendChild(ndasDeviceTree, nameTree);
		
	CFXMLNodeRef	nameTextNode = CFXMLNodeCreate(
												 kCFAllocatorDefault,
												 kCFXMLNodeTypeText,
												 (CFStringRef)[ExportRegistrationNameField stringValue],
												 &entryInfo,
												 kCFXMLNodeCurrentVersion
												 );
		
	CFXMLTreeRef	nameTextTree = CFXMLTreeCreateWithNode(kCFAllocatorDefault, nameTextNode);
	CFRelease(nameTextNode);
	CFTreeAppendChild(nameTree, nameTextTree);
	CFRelease(nameTextTree);

	CFRelease(nameTree);

	// writeKey	
	if (0 == [ExportRegistrationNotExportWriteKeyCheckbox state]) {
		
		CFXMLNodeRef	writeKeyNode = CFXMLNodeCreate(
													   kCFAllocatorDefault,
													   kCFXMLNodeTypeElement,
													   CFSTR("writeKey"),
													   &entryInfo,
													   kCFXMLNodeCurrentVersion
													   );
		CFXMLTreeRef	writeKeyTree = CFXMLTreeCreateWithNode(kCFAllocatorDefault, writeKeyNode);
		CFRelease(writeKeyNode);
		CFTreeAppendChild(ndasDeviceTree, writeKeyTree);
		
		CFXMLNodeRef	writeKeyTextNode = CFXMLNodeCreate(
														   kCFAllocatorDefault,
														   kCFXMLNodeTypeText,
														   (CFStringRef)[ExportRegistrationWritekeyField stringValue],
														   &entryInfo,
														   kCFXMLNodeCurrentVersion
														   );
		
		CFXMLTreeRef	writeKeyTextTree = CFXMLTreeCreateWithNode(kCFAllocatorDefault, writeKeyTextNode);
		CFRelease(writeKeyTextNode);
		CFTreeAppendChild(writeKeyTree, writeKeyTextTree);
		CFRelease(writeKeyTextTree);
		
		CFRelease(writeKeyTree);

	}	
	// Description	
	if (0 < [[ExportRegistrationDescriptionTextView string] length]) {
		
		CFXMLNodeRef	descriptionNode = CFXMLNodeCreate(
												 kCFAllocatorDefault,
												 kCFXMLNodeTypeElement,
												 CFSTR("description"),
												 &entryInfo,
												 kCFXMLNodeCurrentVersion
												 );
		CFXMLTreeRef	descriptionTree = CFXMLTreeCreateWithNode(kCFAllocatorDefault, descriptionNode);
		CFRelease(descriptionNode);
		CFTreeAppendChild(ndasDeviceTree, descriptionTree);
		
		CFXMLNodeRef	descriptionTextNode = CFXMLNodeCreate(
													 kCFAllocatorDefault,
													 kCFXMLNodeTypeText,
													 (CFStringRef)[ExportRegistrationDescriptionTextView string],
													 &entryInfo,
													 kCFXMLNodeCurrentVersion
													 );
				
		CFXMLTreeRef	descriptionTextTree = CFXMLTreeCreateWithNode(kCFAllocatorDefault, descriptionTextNode);
		CFRelease(descriptionTextNode);
		CFTreeAppendChild(descriptionTree, descriptionTextTree);
		CFRelease(descriptionTextTree);
		
		CFRelease(descriptionTree);
	}
	
	CFRelease(ndasDeviceTree);
	
	// Convert to CFData.
	CFDataRef	xmlData = CFXMLTreeCreateXMLData(kCFAllocatorDefault, xmlDoc);
	
	CFRelease(xmlDoc);
	
	// open panel for *.ndas, *.nda 
	NSSavePanel *savePanel = [NSSavePanel savePanel];

	[savePanel setCanSelectHiddenExtension: YES];
	[savePanel setRequiredFileType:@"ndas"];
	
	[savePanel beginSheetForDirectory: nil 
								 file: [ExportRegistrationNameField stringValue]
					   modalForWindow: [NSApp keyWindow] 
						modalDelegate: self 
					   didEndSelector: @selector(savePanelDidEnd:returnCode:contextInfo:) 
						  contextInfo: [(NSData *)xmlData retain]];
	
	CFRelease(nameSpaceTree);
	CFRelease(xmlData);
	
	[self updateInformation];	
}

- (IBAction)exportRegistrationSheetCancel:(id)sender
{
	[NSApp endSheet: ExportRegistrationSheet];
	[ExportRegistrationSheet close];
			
	[self updateInformation];	
}

NSString *printRAIDType(UInt32 type) 
{	
	// RAID Type.
	switch ( type ) {
		case NMT_SINGLE:
			return NSLocalizedString(@"RAID-TYPE : Single", @"Single Disk");
		case NMT_MIRROR:
			return NSLocalizedString(@"RAID-TYPE : Mirror", @"Mirrored Disk - Recovery not supported");
		case NMT_AGGREGATE:
			return NSLocalizedString(@"RAID-TYPE : Aggregate", @"Aggregate or Spanning");
		case NMT_RAID0:
			return NSLocalizedString(@"RAID-TYPE : RAID 0 (Unsupported)", @"RAID 0. not supported in Mac OS X");
		case NMT_RAID0R2:
			return NSLocalizedString(@"RAID-TYPE : RAID 0 R2", @"RAID 0 Rev 2");
		case NMT_RAID1:
			return NSLocalizedString(@"RAID-TYPE : RAID 1 (Old)", @"RAID 1");
		case NMT_RAID1R2:
			return NSLocalizedString(@"RAID-TYPE : RAID 1 R2 (Old)", @"RAID 1 Rev 2");
		case NMT_RAID1R3:
			return NSLocalizedString(@"RAID-TYPE : RAID 1 R3", @"RAID 1 Rev 3");
		case NMT_RAID4:
			return NSLocalizedString(@"RAID-TYPE : RAID 4 (Old)", @"RAID 4");
		case NMT_RAID4R3:
			return NSLocalizedString(@"RAID-TYPE : RAID 4 R3", @"RAID 4 Rev 3");
		case NMT_INVALID:
		default:
			return NSLocalizedString(@"RAID-TYPE : Undefined", @"Unknown or not supported RAID type. May created by future version");
		}
}

@end
