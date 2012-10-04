/*
 *  DebugPrint.c
 *  NDService
 *
 *  Created by Á¤±Õ ¾È on Wed Apr 21 2004.
 *  Copyright (c) 2004 __MyCompanyName__. All rights reserved.
 *
 */

#include "DebugPrint.h"
#include <stdarg.h>

#define DEBUG_BUFFER_LENGTH 256

unsigned int UserToolDebugLevel = 1;
char UserToolBuffer[DEBUG_BUFFER_LENGTH+1];

#ifndef KERNEL

/***************************************************************************
*
*  DebugPrint
*
**************************************************************************/

 void DebugPrint(
				unsigned long DebugPrintLevel,
				bool ErrorPrint,
				char *DebugMessage,
				...
				)
{
    UserToolBuffer[DEBUG_BUFFER_LENGTH] = 0;
    
    if (DebugPrintLevel <= UserToolDebugLevel) {
		
        va_list ap;
		va_start(ap, DebugMessage);
		
		if(ErrorPrint)
            perror(NULL);
		
        vsnprintf(UserToolBuffer, DEBUG_BUFFER_LENGTH, DebugMessage, ap);
		
        fprintf(stderr, UserToolBuffer);
        fflush(stderr);
		
		va_end(ap);
    }
} 

#else

void DebugPrint(
				unsigned long DebugPrintLevel,
				bool ErrorPrint,
				char *DebugMessage,
				...
				)
{
    UserToolBuffer[DEBUG_BUFFER_LENGTH] = 0;
    
    if (DebugPrintLevel <= UserToolDebugLevel) {
	
        va_list ap;
		va_start(ap, DebugMessage);

		if(ErrorPrint) {
            //perror(NULL);
		}
		
		vsnprintf(UserToolBuffer, DEBUG_BUFFER_LENGTH, DebugMessage, ap);
		
        IOLog(UserToolBuffer);
		
		va_end(ap);
    }
	
	return;
} 

#endif
