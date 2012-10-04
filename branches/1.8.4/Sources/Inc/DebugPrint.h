/*
 *  DebugPrint.h
 *  NDService
 *
 *  Created by jgahn on Wed Apr 21 2004.
 *  Copyright (c) 2004 XIMETA, Inc. All rights reserved.
 *
 */

#ifdef KERNEL
#include <IOKit/IOLib.h>
#else
#include <Carbon/Carbon.h>
#endif

#ifndef _DEBUG_PRINT_H_
#define _DEBUG_PRINT_H_

#ifdef __cplusplus
extern "C"
{
#endif
	
extern unsigned int UserToolDebugLevel;

void DebugPrint(
				unsigned int DebugPrintLevel,
				bool ErrorPrint,
				const char *DebugMessage,
				...
				);


#ifdef __cplusplus
}
#endif
	
#endif