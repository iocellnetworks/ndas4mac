//
//  main.m
//  «PROJECTNAME»
//
//  Created by «FULLUSERNAME» on «DATE».
//  Copyright (c) «YEAR» «ORGANIZATIONNAME». All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import <Security/Authorization.h>
#import <Security/AuthorizationTags.h>

AuthorizationRef	myAuthorizationRef; 

int main(int argc, const char *argv[])
{
	
	OSStatus			myStatus;
	int					result;

    AuthorizationFlags  myFlags = kAuthorizationFlagDefaults; 
    
    myStatus = AuthorizationCreate(NULL, kAuthorizationEmptyEnvironment, 
								   myFlags, &myAuthorizationRef); 
	
    if (myStatus != errAuthorizationSuccess)
        return myStatus;
	
	AuthorizationItem myItems = {kAuthorizationRightExecute, 0, NULL, 0}; 
	
	AuthorizationRights myRights = {1, &myItems}; 
	
	
	myFlags = kAuthorizationFlagDefaults | 
		kAuthorizationFlagInteractionAllowed | 
		kAuthorizationFlagPreAuthorize | 
		kAuthorizationFlagExtendRights; 
	
	myStatus = AuthorizationCopyRights (myAuthorizationRef, &myRights, NULL, myFlags, NULL ); 
	
	if (myStatus != errAuthorizationSuccess) {
		return myStatus;
	}
	
	// Start Application.
    result = NSApplicationMain(argc, argv);
	
	AuthorizationFree (myAuthorizationRef, kAuthorizationFlagDefaults); 
	
	return result;
}
