//
//  RAIDBindSheetController.h
//  NDAS Utility
//
//  Created by 정균 안 on 06. 02. 06.
//  Copyright 2006 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>


@interface RAIDBindSheetController : NSObject {

}

- (void)updateUI;
- (IBAction)RAIDRemoveSubUnit:(id)sender;

- (void) RAIDTypeDidChanged: (NSNotification *)notification;
- (void) SubUintOutlineViewSelectionDidChanged: (NSNotification *)notification;
@end
