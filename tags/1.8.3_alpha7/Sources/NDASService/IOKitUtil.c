/*
 *  IOKitUtil.c
 *  NDASService
 *
 *  Created by Jung-gyun Ahn on 09. 07. 21.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#include <syslog.h>

#include "IOKitUtil.h"

#include <unistd.h>
#include "NDASBusEnumeratorUserClientTypes.h"

bool isNDASFamilyLoaded()
{
	kern_return_t			kernReturn;
	bool					bReturn = false;

	CFDictionaryRef			classToMatch = NULL;
	io_connect_t			masterPort;
	io_iterator_t			existing = 0;

	// Obtain the NDASBusEnumerator service.
	classToMatch = IOServiceMatching(DriversIOKitClassName);
	if(classToMatch == NULL) {
		syslog(LOG_ERR, "IOServiceMatching returned a NULL dictionary.");
		
		return false;
	}
	
	kernReturn = IOMasterPort(MACH_PORT_NULL, &masterPort);
	if(kernReturn != KERN_SUCCESS) {
		syslog(LOG_ERR, "can't obtain I/O Kit's master port. %d\n", kernReturn);
		
		goto out;
	}
	
	existing = IOServiceGetMatchingService(
										   masterPort,
										   classToMatch);  
	classToMatch = NULL;	// It will release by IOServiceGetMatchingServices	
	if(existing) {
		bReturn = true;
	}
	
out:
	
	if(masterPort)		IOObjectRelease(masterPort);
	if(classToMatch)	CFRelease(classToMatch);
	if(existing)		IOObjectRelease(existing);
	
	return bReturn;
}

//
// Add Notification observer from Device Driver.
//
bool createDeviceNotification(const char *className, const io_name_t interestName, IOServiceInterestCallback callback, io_object_t *notification)
{
	kern_return_t			kernReturn;
	bool					bResult = false;
	IONotificationPortRef	notificationObject = NULL;
	CFRunLoopSourceRef		notificationRunLoopSource = NULL;
	io_connect_t			dataPort;
	io_service_t			serviceObject;
	io_iterator_t			iterator = 0;
	CFDictionaryRef			classToMatch = NULL;
	
	// Obtain the device service.
	classToMatch = IOServiceMatching(className);
	if(classToMatch == NULL) {
		syslog(LOG_ERR, "IOServiceMatching returned a NULL dictionary.");
		
		return false;
	}
	
	notificationObject = IONotificationPortCreate(kIOMasterPortDefault);
	
	notificationRunLoopSource = IONotificationPortGetRunLoopSource(notificationObject);
	
	CFRetain(notificationRunLoopSource);
	
	CFRunLoopAddSource(
					   CFRunLoopGetCurrent(), 
					   notificationRunLoopSource,
					   kCFRunLoopCommonModes
					   );
	

	// Obtain the I/O Kit communication handle.
	kernReturn = IOMasterPort(MACH_PORT_NULL, &dataPort);
	if(kernReturn != KERN_SUCCESS) {
		syslog(LOG_ERR, "can't obtain I/O Kit's master port. %d\n", kernReturn);
		
		goto out;
	}
	
	kernReturn = IOServiceGetMatchingServices(dataPort, classToMatch, &iterator);
	classToMatch = NULL;	// It will release by IOServiceGetMatchingServices
	if(kernReturn != KERN_SUCCESS) {
		syslog(LOG_ERR, "IOServiceGetMatchingServices returned. %d\n", kernReturn);
		
		goto out;
	}
	
	serviceObject = IOIteratorNext(iterator);
/*	
	kernReturn = IOServiceAddInterestNotification(
												  notificationObject,
												  serviceObject,
												  kIOGeneralInterest,
												  callback,
												  NULL,
												  &notification
												  );
*/	
	kernReturn = IOServiceAddInterestNotification(
												  notificationObject,
												  serviceObject,
												  interestName,
												  callback,
												  NULL,
												  notification
												  );
	if(kernReturn != KERN_SUCCESS) {
		syslog(LOG_ERR, "IOServiceAddInterestNotification returned. %d\n", kernReturn);
		
		goto out;		
	}
	
	bResult = true;

out:
	if(iterator)					IOObjectRelease(iterator);
	if(notificationRunLoopSource)	CFRelease(notificationRunLoopSource);
	if(classToMatch)				CFRelease(classToMatch);
		
	return bResult;
}

void releaseDeviceNotification(io_object_t notification)
{
	if (notification) {
		IOObjectRelease(notification);
	}
}
