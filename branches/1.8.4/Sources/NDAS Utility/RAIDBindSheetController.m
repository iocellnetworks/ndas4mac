//
//  RAIDBindSheetController.m
//  NDAS Utility
//
//  Created by 정균 안 on 06. 02. 06.
//  Copyright 2006 __MyCompanyName__. All rights reserved.
//

#import "RAIDBindSheetController.h"
#import "UtilityController.h"
#import "NDASDevice.h"
#import "UnitDevice.h"
#import "RAIDDevice.h"
#import "RAIDSubDevice.h"
#import "ImageAndTextCell.h"

//#import "NDASNotification.h"
#import "NDASBusEnumeratorUserClientTypes.h"
//#import "NDASGetInformation.h"
//#import "LanScsiDiskInformationBlock.h"

@implementation RAIDBindSheetController

- (void)awakeFromNib
{
	// Catch user event from outline view.
	[[NSNotificationCenter defaultCenter] addObserver:self
											 selector:@selector(SubUintOutlineViewSelectionDidChanged:)
												 name:NSOutlineViewSelectionDidChangeNotification
											   object:[controller RAIDBindSheetSubUnitOutlineView]];
	
	// Catch user event from outline view.
	[[NSNotificationCenter defaultCenter] addObserver:self
											 selector:@selector(RAIDTypeDidChanged:)
												 name:NSMenuDidChangeItemNotification
											   object:[[controller RAIDBindSheetTypePopupButton] menu]];
	
	
}

//
// Notification.
//

- (void) RAIDTypeDidChanged: (NSNotification *)notification
{
	if (2 == [[controller RAIDBindSheetTypePopupButton] indexOfSelectedItem]) {
		// RAID 1
		while (3 < [[controller RAIDBindSheetSubUnitDevices] count]) {
			[[controller RAIDBindSheetSubUnitDevices] removeLastObject];
		}
	}
	
	[[controller RAIDBindSheetSubUnitOutlineView] reloadData];
	
	[self updateUI];
}

- (void) SubUintOutlineViewSelectionDidChanged: (NSNotification *)notification
{
//	NSLog(@"SubUintOutlineViewSelectionDidChanged %d", [[controller RAIDBindSheetSubUnitOutlineView] selectedRow]);

	if ( -1 == [[controller RAIDBindSheetSubUnitOutlineView] selectedRow] ) {
		[[controller RAIDBindSheetRemoveButton] setEnabled: false];
	} else {
		[[controller RAIDBindSheetRemoveButton] setEnabled: true];
	}
	
	[[controller RAIDBindSheetSubUnitOutlineView] reloadData];
	
	[self updateUI];
}

//
// OutlineView
//

- (id)outlineView:(NSOutlineView *)outlineView child:(NSInteger)index ofItem:(id)item;
{
//	NSLog(@"child index %d 0x%x\n", index, item);
	
	if ([outlineView isEqual: [controller RAIDBindSheetUnitOutlineView]]) {
		
		if ( nil == item ) {
			
			if ( [[controller NDASDevices] count] > index ) {
				return [[[controller NDASDevices] allValues] objectAtIndex: index];		
			} else {
				return [[[controller RAIDDevices] allValues] objectAtIndex: (index - [[controller NDASDevices] count])];				
			}
			
		} else if ( kNDASDeviceTypeNDAS == [(Device *)item deviceType] ) {
			return [(NDASDevice *)item unitAtIndex: index];
		} else 	{
			return nil;
		}
	} else {
		if ( nil == item ) {
			return [[controller RAIDBindSheetSubUnitDevices] objectAtIndex: index];
		}		
	}
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item;
{
//	NSLog(@"isItemExpendable 0x%x\n", item);

	if ([outlineView isEqual: [controller RAIDBindSheetUnitOutlineView]]) {
		
		if ( nil == item ) {
			if ( 0 < [[controller NDASDevices] count] + [[controller RAIDDevices] count] ) {
				return TRUE;
			} else {
				return FALSE;
			}
		} else if ( kNDASDeviceTypeNDAS == [(Device *)item deviceType] ) {
			if ( 0 < [(NDASDevice *)item numberOfUnits] ) {
				return TRUE;
			} else {
				return FALSE;
			}
		} else 	{
			return FALSE;
		}
	} else {
		return FALSE;
	}
}

- (NSInteger)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item;
{
//	NSLog(@"numberOfChildrenOfItem 0x%x %d\n", item, [[controller NDASDevices] count] + [[controller RAIDDevices] count]);
	
	if ([outlineView isEqual: [controller RAIDBindSheetUnitOutlineView]]) {

		if( nil == item ) {		
			return [[controller NDASDevices] count] + [[controller RAIDDevices] count];
		} else if ( kNDASDeviceTypeNDAS == [(Device *)item deviceType] ){
			return [(NDASDevice *)item numberOfUnits];
		} else {
			return 0;
		}
	} else {
		
		if ( nil == item) {
			return [[controller RAIDBindSheetSubUnitDevices] count];
		} else {
			return 0;
		}
	}
}

- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item;
{
	//	NSLog(@"objectValueForTableColumn 0x%x %@\n", item, [tableColumn identifier]);

	id objectValue = nil;
	NSString	*tempString;

	if ([outlineView isEqual: [controller RAIDBindSheetUnitOutlineView]]) {
		
		if( [[tableColumn identifier] isEqualToString: @"FirstColumn"] ) {
			
			switch ( [(Device *)item deviceType] ) {
				case kNDASDeviceTypeNDAS:
				{
					objectValue = [(NDASDevice *)item name];					
				}
					break;
				case kNDASDeviceTypeUnit:
				{					
					if(kNDASUnitMediaTypeHDD == [(UnitDevice *)item type]) {
						tempString = [NSString stringWithFormat: @"(%@) %@", [(UnitDevice *)item capacityToString], [(UnitDevice *)item model]];
						
					} else {
						tempString = [NSString stringWithString: [(UnitDevice *)item model]];
					}
					
					if([tempString length] > 20) {
						tempString = [NSString stringWithString: [tempString substringToIndex: 20]];
					}
					
					objectValue = tempString;
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
	} else {
		
		if( [[tableColumn identifier] isEqualToString: @"Index"] ) {

			if (2 == [[controller RAIDBindSheetTypePopupButton] indexOfSelectedItem]
				&& 2 == [[controller RAIDBindSheetSubUnitDevices] indexOfObject: item]) {
				tempString = [NSString stringWithFormat: @" S"];
			} else {
				tempString = [NSString stringWithFormat: @"%2d", [[controller RAIDBindSheetSubUnitDevices] indexOfObject: item]];
			}
			objectValue = tempString;

		} else if ( [[tableColumn identifier] isEqualToString: @"SubUnit"] ) {
						
			switch([(Device *)item deviceType]) {
				case kNDASDeviceTypeUnit:
				{
					NDASDevice *entry = nil;
					UnitDevice *unitDevice = nil;
					
					unitDevice = (UnitDevice *)item;
					entry = [unitDevice ndasId];

					tempString = [NSString stringWithFormat: @"(%@) %@(%d) %@", [unitDevice capacityToString], [entry name], [unitDevice unitNumber], [unitDevice model]];
				}
					break;
				case kNDASDeviceTypeRAID:
				{
					RAIDDevice *raidDevice = nil;
					
					raidDevice = (RAIDDevice *)item;

					tempString = [NSString stringWithFormat: @"(%@) %@", [raidDevice capacityToString], [raidDevice name]];
					
				}
					break;
				default:
				{
					tempString = @"Unknown";
				}
			}
			
			if([tempString length] > 60) {
				tempString = [NSString stringWithString: [tempString substringToIndex: 60]];
			}
			
			objectValue = tempString;			
		}	
	}
	
	return objectValue;
}

//
// OutlineView Delegate methods
//
- (void)outlineView:(NSOutlineView *)outlineView willDisplayCell:(id)cell forTableColumn:(NSTableColumn *)tableColumn item:(id)item
{
//	NSLog(@"willDisplayCell\n");
	
	NSImage *image;
	NSSize	imageSize;

	if ([outlineView isEqual: [controller RAIDBindSheetUnitOutlineView]]) {
		
		if ( [[tableColumn identifier] isEqualToString: @"FirstColumn"] ) {
						
			imageSize.height = 16;
			imageSize.width = 16;
			
			switch ( [(Device *)item deviceType] ) {
				case kNDASDeviceTypeNDAS:
				{

					image = [NSImage imageNamed:@"External"];
					
					[image setScalesWhenResized: TRUE];
					[image setSize: imageSize];
					
					// Set Image.
					[(ImageAndTextCell *) cell setImage: image];
					if ( kNDASStatusOnline == [(NDASDevice *)item status] ) { 
						[(ImageAndTextCell *) cell setTextColor: [NSColor blackColor] ];
						
					} else {
						[(ImageAndTextCell *) cell setTextColor: [NSColor grayColor] ];
					}
				}
					break;
				case kNDASDeviceTypeUnit:
				{
					switch([(UnitDevice *)item type]) {
						case kNDASUnitMediaTypeODD:
							image = [NSImage imageNamed:@"ODD"];							
							[cell setTextColor: [NSColor grayColor]];
							
							break;					
						case kNDASUnitMediaTypeHDD:
						{
							image = [NSImage imageNamed:@"Disk"];
							
							if ( kNDASUnitConfigurationUnmount == [(UnitDevice *)item configuration]
								 && kNDASUnitStatusDisconnected == [(UnitDevice *)item status]
								 && NMT_SINGLE == [(UnitDevice *)item diskArrayType]
								 && 0 == [(UnitDevice *)item RWAccess]
								 && 0 == [(UnitDevice *)item ROAccess]) {
								[cell setTextColor: [NSColor blackColor]];
							} else {
								[cell setTextColor: [NSColor grayColor]];
							}
						}	
							break;
						default:
							image = [NSImage imageNamed:@"External"];
							[cell setTextColor: [NSColor blackColor]];

							break;					
					}
					
					[image setScalesWhenResized: TRUE];
					[image setSize: imageSize];
					
					// Set Image.
					[(ImageAndTextCell *) cell setImage: image];
				}
					break;
				case kNDASDeviceTypeRAID:
				{					
					image = [NSImage imageNamed:@"RAID"];
					
					[image setScalesWhenResized: TRUE];
					[image setSize: imageSize];

					// Set Image.
					[(ImageAndTextCell *) cell setImage: image];					
					
					if ( kNDASUnitConfigurationUnmount == [(RAIDDevice *)item configuration]
						 && kNDASUnitStatusDisconnected == [(RAIDDevice *)item status]
						 && NMT_SINGLE == [(RAIDDevice *)item diskArrayType]
						 && 0 == [(RAIDDevice *)item RWAccess]
						 && 0 == [(RAIDDevice *)item ROAccess]) {
						[cell setTextColor: [NSColor blackColor]];
					} else {
						[cell setTextColor: [NSColor grayColor]];
					}
					
				}
					break;
			}
		}
	} else {
		
		if( [[tableColumn identifier] isEqualToString: @"SubUnit"] ) {

			imageSize.height = 16;
			imageSize.width = 16;
			
			switch ([(Device*)item deviceType]) {
				case kNDASDeviceTypeUnit:
				{
					image = [NSImage imageNamed:@"Disk"];
					
					if ( kNDASUnitConfigurationUnmount == [(UnitDevice *)item configuration]
						 && kNDASUnitStatusDisconnected == [(UnitDevice *)item status]
						 && NMT_SINGLE == [(UnitDevice *)item diskArrayType]
						 && 0 == [(UnitDevice *)item RWAccess]
						 && 0 == [(UnitDevice *)item ROAccess]) {
						[cell setTextColor: [NSColor blackColor]];
					} else {
						[cell setTextColor: [NSColor grayColor]];
					}
					
				}
					break;
				case kNDASDeviceTypeRAID:
				{
					image = [NSImage imageNamed:@"RAID"];				
					
					if ( kNDASUnitConfigurationUnmount == [(RAIDDevice *)item configuration]
						 && kNDASUnitStatusDisconnected == [(RAIDDevice *)item status]
						 && NMT_SINGLE == [(RAIDDevice *)item diskArrayType]
						 && 0 == [(RAIDDevice *)item RWAccess]
						 && 0 == [(RAIDDevice *)item ROAccess]) {
						[cell setTextColor: [NSColor blackColor]];
					} else {
						[cell setTextColor: [NSColor grayColor]];
					}
					
				}
					break;
				default:
				{
					image = [NSImage imageNamed:@"Disk"];						// BUG BUG
				}
			}
			
			[image setScalesWhenResized: TRUE];
			[image setSize: imageSize];
			
			// Set Image.
			[(ImageAndTextCell *) cell setImage: image];
		}
	}		
}
/*
- (BOOL)outlineView:(NSOutlineView *)outlineView shouldEditTableColumn:(NSTableColumn *)tableColumn item:(id)item;
{
	return NO;
}
*/

- (BOOL)outlineView:(NSOutlineView *)outlineView shouldCollapseItem:(id)item
{
	return FALSE;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView writeItems:(NSArray*)items toPasteboard:(NSPasteboard*)pboard {
	
//	NSLog(@"writeItems\n");
	
	id device;
	
	if ([outlineView isEqual: [controller RAIDBindSheetUnitOutlineView]]) {
		
//		GUID	deviceID;
		
		if ([items count] != 1) {
			return NO;
		}
		
		device = [items objectAtIndex: 0];
		
		if (device) {
			
			switch ( [(Device *)device deviceType] ) {
				case kNDASDeviceTypeNDAS:
				{
					return NO;
				}
					break;
				case kNDASDeviceTypeUnit:
				{
					// Check ODD.
					if (kNDASUnitMediaTypeHDD != [(UnitDevice *)device type] ) {
						return NO;
					}
						
					if (NMT_SINGLE != [(UnitDevice *)device diskArrayType]) {
						return NO;
					} else {
						if(kNDASUnitConfigurationUnmount == [(UnitDevice *)device configuration]
						   && kNDASUnitStatusDisconnected == [(UnitDevice *)device status]
						   && 0 == [(UnitDevice *)device RWAccess]
						   && 0 == [(UnitDevice *)device ROAccess]) {
//							deviceID = [(UnitDevice *)device deviceID];
						} else {
							return NO;
						}
					}
				}
					break;
				case kNDASDeviceTypeRAID:
				{
					/*
					if (NMT_SINGLE != [(RAIDDevice *)device diskArrayType]) {
						return NO;
					} else {
						if(kNDASUnitConfigurationUnmount == [(RAIDDevice *)device configuration]
						   && kNDASUnitStatusDisconnected == [(RAIDDevice *)device status]
						   && 0 == [(RAIDDevice *)device RWAccess]
						   && 0 == [(RAIDDevice *)device ROAccess]) {
							deviceID = *[(RAIDDevice *)device UnitDeviceID];
						} else {
							return NO;
						}
					}
					 */
					return NO;
				}
					break;
			}
			
		}
		
		// Already in the RAID set?
		if ( NSNotFound != [[controller RAIDBindSheetSubUnitDevices] indexOfObject: device] ) {
			return NO;
		}
		
		//	draggedNodes = items; // Don't retain since this is just holding temporaral drag information, and it is only used during a drag!  We could put this in the pboard actually.
		
		// Provide data for our custom type, and simple NSStrings.
		[pboard declareTypes:[NSArray arrayWithObjects: DragDropSimplePboardType, NSStringPboardType, nil] owner:self];
		
		// the actual data doesn't matter since DragDropSimplePboardType drags aren't recognized by anyone but us!.
		[pboard setData:[NSData dataWithBytes:&device length: sizeof(id)] forType:DragDropSimplePboardType]; 
		
		// Put string data on the pboard... notice you candrag into TextEdit!
		[pboard setString: [items description] forType: NSStringPboardType];
	} else {
	
		if ([items count] != 1) {
			return NO;
		}
		
		device = [items objectAtIndex: 0];
		
		if (device) {
			
			// Provide data for our custom type, and simple NSStrings.
			[pboard declareTypes:[NSArray arrayWithObjects: DragDropSimplePboardType, NSStringPboardType, nil] owner:self];
			
			// the actual data doesn't matter since DragDropSimplePboardType drags aren't recognized by anyone but us!.
			[pboard setData:[NSData dataWithBytes:&device length: sizeof(id)] forType:DragDropSimplePboardType]; 
			
			// Put string data on the pboard... notice you candrag into TextEdit!
			[pboard setString: [items description] forType: NSStringPboardType];	
		}
	}
	
    return YES;
}

- (void)updateUI
{

	//NSLog(@"updateUI\n");
	
	UInt64	buffer = 0;
	int index;

	if (nil == [controller RAIDBindSheetSubUnitDevices]) {
		return;
	}
	
	BOOL	badSubUnit = NO;
	
	// Check for Bad Subunit.
	for (index = 0; index < [[controller RAIDBindSheetSubUnitDevices] count]; index++) {
		Device *subUnit;
		
		subUnit = (Device *)[[controller RAIDBindSheetSubUnitDevices] objectAtIndex: index];
		
		switch ( [(Device *)subUnit deviceType] ) {
			case kNDASDeviceTypeNDAS:
			{
				badSubUnit = YES;
			}
				break;
			case kNDASDeviceTypeUnit:
			{
				// Check ODD.
				if (kNDASUnitMediaTypeHDD != [(UnitDevice *)subUnit type] ) {
					badSubUnit = YES;
				}
				
				if (NMT_SINGLE != [(UnitDevice *)subUnit diskArrayType]) {
					badSubUnit = YES;
				} else {
					if(kNDASUnitConfigurationUnmount == [(UnitDevice *)subUnit configuration]
					   && kNDASUnitStatusDisconnected == [(UnitDevice *)subUnit status]
					   && 0 == [(UnitDevice *)subUnit RWAccess]
					   && 0 == [(UnitDevice *)subUnit ROAccess]) {
						//							deviceID = [(UnitDevice *)device deviceID];
					} else {
						badSubUnit = YES;
					}
				}
			}
				break;
			case kNDASDeviceTypeRAID:
			{
				badSubUnit = YES;
			}
				break;
		}
		
		if (badSubUnit) {
			[[controller RAIDBindSheetBindButton] setEnabled: false];
			
			[[controller RAIDBindSheetSizeField] setStringValue: @""];		
			
			return;
		}
	}
	
	// Update Data.
	switch ([[controller RAIDBindSheetTypePopupButton] indexOfSelectedItem]) {
		case 0:
		{
			// Aggregation.
			
			if (1 < [[controller RAIDBindSheetSubUnitDevices] count] ) {
				[[controller RAIDBindSheetBindButton] setEnabled: true];
								
				// Capacity.
				for (index = 0; index < [[controller RAIDBindSheetSubUnitDevices] count]; index++) {
					buffer += [(Device *)[[controller RAIDBindSheetSubUnitDevices] objectAtIndex: index] capacity];
				}

				[[controller RAIDBindSheetSizeField] setStringValue: [Device capacityToString: buffer]];
				
			} else {
				[[controller RAIDBindSheetBindButton] setEnabled: false];
				
				[[controller RAIDBindSheetSizeField] setStringValue: @""];
			}
		}
			break;
		case 1:
		{
			// RAID 0
			
			if (1 < [[controller RAIDBindSheetSubUnitDevices] count] ) {
				[[controller RAIDBindSheetBindButton] setEnabled: true];
				
				// Capacity.
				for (index = 0; index < [[controller RAIDBindSheetSubUnitDevices] count]; index++) {
					if(buffer == 0) {
						buffer = [(Device *)[[controller RAIDBindSheetSubUnitDevices] objectAtIndex: index] capacity];
					} else if ( buffer > [(Device *)[[controller RAIDBindSheetSubUnitDevices] objectAtIndex: index] capacity]) {
						buffer = [(Device *)[[controller RAIDBindSheetSubUnitDevices] objectAtIndex: index] capacity];					
					}
				}
				
				buffer *= [[controller RAIDBindSheetSubUnitDevices] count];

				[[controller RAIDBindSheetSizeField] setStringValue: [Device capacityToString: buffer]];

				
			} else {
				[[controller RAIDBindSheetBindButton] setEnabled: false];
				
				[[controller RAIDBindSheetSizeField] setStringValue: @""];
			}
			
		}
			break;
		case 2:
		{
			// RAID 1

			if (2 <= [[controller RAIDBindSheetSubUnitDevices] count]
				&& 3 >= [[controller RAIDBindSheetSubUnitDevices] count]) {
				[[controller RAIDBindSheetBindButton] setEnabled: true];
								
				// Capacity.
				for (index = 0; index < [[controller RAIDBindSheetSubUnitDevices] count]; index++) {
					if(buffer == 0) {
						buffer = [(Device *)[[controller RAIDBindSheetSubUnitDevices] objectAtIndex: index] capacity];
					} else if ( buffer > [(Device *)[[controller RAIDBindSheetSubUnitDevices] objectAtIndex: index] capacity]) {
						buffer = [(Device *)[[controller RAIDBindSheetSubUnitDevices] objectAtIndex: index] capacity];					
					}
				}

				[[controller RAIDBindSheetSizeField] setStringValue: [Device capacityToString: buffer]];

			} else {
				[[controller RAIDBindSheetBindButton] setEnabled: false];
				
				[[controller RAIDBindSheetSizeField] setStringValue: @""];
			}
			
		}
			break;
	}	
}

- (BOOL)outlineView:(NSOutlineView*)olv acceptDrop:(id <NSDraggingInfo>)info item:(id)item childIndex:(NSInteger)index
{
//	NSLog(@"acceptDrop index %d\n", index);
	
	NSPasteboard	*pboard;
	UInt64			deviceID;
	id				*device;
	int				indexOfReplaceID;
	
	pboard = [info draggingPasteboard];
	
	device = (id *)[[pboard dataForType: DragDropSimplePboardType] bytes];
	
//	indexOfReplaceID = [[controller RAIDBindSheetSubUnitDevices] indexOfObject: item];

//	NSLog(@"Replace ID 0x%x\n", *device);

	if (index == -1) {
		if ([[info draggingSource] isEqual: [controller RAIDBindSheetUnitOutlineView]]) { 	
			// Drag a unit device from left list to right list.
			[[controller RAIDBindSheetSubUnitDevices] insertObject: *device atIndex: [[controller RAIDBindSheetSubUnitDevices] count]];
		} else {
			// Ignore.
		}
	} else {
		
		if ([[info draggingSource] isEqual: [controller RAIDBindSheetUnitOutlineView]]) { 			
			[[controller RAIDBindSheetSubUnitDevices] insertObject: *device atIndex: index];
		} else {
			id	replaceID;
//			int lastIndex = [[controller RAIDBindSheetSubUnitDevices] count] - 1;
			
			replaceID = *device;
			indexOfReplaceID = [[controller RAIDBindSheetSubUnitDevices] indexOfObject: replaceID];

//			NSLog(@"acceptDrop index of Replace ID %d\n", indexOfReplaceID);

			if ( (index > indexOfReplaceID) && (abs(index - indexOfReplaceID) > 1)) {
				
				[[controller RAIDBindSheetSubUnitDevices] removeObject: replaceID];
				[[controller RAIDBindSheetSubUnitDevices] insertObject: replaceID atIndex: index - 1];
				
			} else if ( (index < indexOfReplaceID) && (abs(index - indexOfReplaceID) > 0)) {
				[[controller RAIDBindSheetSubUnitDevices] removeObject: replaceID];
				[[controller RAIDBindSheetSubUnitDevices] insertObject: replaceID atIndex: index];				
			}
		}
	}
	
	[[controller RAIDBindSheetSubUnitOutlineView] reloadData];
					
	[self updateUI];
	
	return YES;
}

- (NSDragOperation)outlineView:(NSOutlineView*)olv validateDrop:(id <NSDraggingInfo>)info proposedItem:(id)item proposedChildIndex:(NSInteger)index
{	
//	NSLog(@"validateDrop index %d item %@\n", index, item);

    // This method validates whether or not the proposal is a valid one. Returns NO if the drop should not be allowed.
//    SimpleTreeNode *targetNode = item;
    BOOL targetNodeIsValid = YES;
	
	if (item) {
		return NSDragOperationNone;
	}
	
	if ([[info draggingSource] isEqual: [controller RAIDBindSheetUnitOutlineView]]) { 	
		
		if (2 == [[controller RAIDBindSheetTypePopupButton] indexOfSelectedItem]) {
			// RAID 1
			if (3 > [[controller RAIDBindSheetSubUnitDevices] count]) {
				return NSDragOperationGeneric;
			} else {
				return NSDragOperationNone;
			}
		}
		
		return NSDragOperationGeneric;
	} else {		
		NSPasteboard	*pboard;
		id				*device;
		int				indexOfReplaceID;
		
		pboard = [info draggingPasteboard];
		
		device = (id *)[[pboard dataForType: DragDropSimplePboardType] bytes];
		
		indexOfReplaceID = [[controller RAIDBindSheetSubUnitDevices] indexOfObject: *device];
		
		if ( (index > indexOfReplaceID) && (abs(index - indexOfReplaceID) <= 1)) {
			return NSDragOperationNone;
		} else if ( (index < indexOfReplaceID) && (abs(index - indexOfReplaceID) <= 0)) {
			return NSDragOperationNone;
		} else if ( index == indexOfReplaceID ) {
			return NSDragOperationNone;
		}
	}
	
	/*
    if ([self onlyAcceptDropOnRoot]) {
		targetNode = nil;
		childIndex = NSOutlineViewDropOnItemIndex;
    } else {
		BOOL isOnDropTypeProposal = childIndex==NSOutlineViewDropOnItemIndex;
		
		// Refuse if: dropping "on" the view itself unless we have no data in the view.
		if (targetNode==nil && childIndex==NSOutlineViewDropOnItemIndex && [treeData numberOfChildren]!=0) 
			targetNodeIsValid = NO;
		
		if (targetNode==nil && childIndex==NSOutlineViewDropOnItemIndex && [self allowOnDropOnLeaf]==NO)
			targetNodeIsValid = NO;
		
		// Refuse if: we are trying to do something which is not allowed as specified by the UI check boxes.
		if ((targetNodeIsValid && isOnDropTypeProposal==NO && [self allowBetweenDrop]==NO) ||
			([NODE_DATA(targetNode) isGroup] && isOnDropTypeProposal==YES && [self allowOnDropOnGroup]==NO) ||
			([NODE_DATA(targetNode) isLeaf ] && isOnDropTypeProposal==YES && [self allowOnDropOnLeaf]==NO))
			targetNodeIsValid = NO;
	    
		// Check to make sure we don't allow a node to be inserted into one of its descendants!
		if (targetNodeIsValid && ([info draggingSource]==outlineView) && [[info draggingPasteboard] availableTypeFromArray:[NSArray arrayWithObject: DragDropSimplePboardType]] != nil) {
			NSArray *_draggedNodes = [[[info draggingSource] dataSource] draggedNodes];
			targetNodeIsValid = ![targetNode isDescendantOfNodeInArray: _draggedNodes];
		}
    }
    */
    // Set the item and child index in case we computed a retargeted one.
//    [outlineView setDropItem:targetNode dropChildIndex:childIndex];
    
    return targetNodeIsValid ? NSDragOperationGeneric : NSDragOperationNone;
}

- (IBAction)RAIDRemoveSubUnit:(id)sender
{
	if ( -1 != [[controller RAIDBindSheetSubUnitOutlineView] selectedRow] ) {
		[[controller RAIDBindSheetSubUnitDevices] removeObjectAtIndex: [[controller RAIDBindSheetSubUnitOutlineView] selectedRow]];
		
		[[controller RAIDBindSheetSubUnitOutlineView] reloadData];
		
		[self updateUI];
	}
}

@end
