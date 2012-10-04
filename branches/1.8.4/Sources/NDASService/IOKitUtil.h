/*
 *  IOKitUtil.h
 *  NDASService
 *
 *  Created by Jung-gyun Ahn on 09. 07. 21.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef _IOKIT_UTIL_H_
#define _IOKIT_UTIL_H_

#include <IOKit/IOKitLib.h>

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus
	
bool isNDASFamilyLoaded();
bool createDeviceNotification(const char *className, const io_name_t interestName, IOServiceInterestCallback callback, io_object_t *notification);
void releaseDeviceNotification(io_object_t notification);

#ifdef __cplusplus
}
#endif // __cplusplus
		
#endif