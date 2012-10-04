/***************************************************************************
*
*  Utilities.c
*
*    Utilities for Debugging.
*
*    Copyright (c) 2004 XiMeta, Inc. All rights reserved.
*
**************************************************************************/

#include "Utilities.h"
#include <sys/socket.h>
#include <sys/random.h>
#ifdef __KPI_SOCKET__	// BUG BUG BUG!!!
#include <uuid/uuid.h>
#endif

UInt32	debugLevel = 0x33333333; //DEBUG_MASK_DISK_ERROR; //DEBUG_MASK_NDAS_ERROR; //0x33333333;

void NDAS_clock_get_uptime(UInt64 *time)
{
#ifdef __KPI_SOCKET__	// BUG BUG BUG!!!
	UInt64	absoluteTime;
	
	clock_get_uptime(&absoluteTime);
	
	absolutetime_to_nanoseconds(absoluteTime, time);
#else 
	AbsoluteTime	absoluteTime;
	
	clock_get_uptime(&absoluteTime);
	
	//AbsoluteTime_to_scalar((AbsoluteTime*)time);
	
	absolutetime_to_nanoseconds(absoluteTime, time);
#endif
}
	
void NDAS_clock_get_system_value(UInt32 *time)
{
#ifdef __KPI_SOCKET__	// BUG BUG BUG!!!
	uint32_t microsecs;	
	
	clock_get_system_microtime((uint32_t*)time, &microsecs);
#else 
	mach_timespec_t	time_t;
	
	time_t = clock_get_system_value();
	
	*time = time_t.tv_sec;
#endif
	
}

#ifndef __KPI_SOCKET__	// BUG BUG BUG!!!
void
NDAS_uuid_generate_random(NDAS_uuid_t out)
{
	read_random(out, sizeof(NDAS_uuid_t));
	
	out[6] = (out[6] & 0x0F) | 0x40;
	out[8] = (out[8] & 0x3F) | 0x80;
}
#endif

void
NDAS_uuid_generate(NDAS_uuid_t out)
{
#ifdef __KPI_SOCKET__	// BUG BUG BUG!!!
	uuid_generate_random(out);
#else
	NDAS_uuid_generate_random(out);
#endif
}
