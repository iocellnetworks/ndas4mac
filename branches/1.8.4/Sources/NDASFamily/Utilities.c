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

uint32_t	debugLevel = 0x33333333; //DEBUG_MASK_DISK_ERROR; //DEBUG_MASK_NDAS_ERROR; //0x33333333;

inline void NDAS_clock_get_uptime(uint64_t *time)
{
#ifdef __KPI_SOCKET__	// BUG BUG BUG!!!
	/*
	uint64_t	absoluteTime;
	
	clock_get_uptime(&absoluteTime);
	
	absolutetime_to_nanoseconds(absoluteTime, time);
	*/
	
	uint32_t	secpart, nsecpart;
	
	clock_get_system_nanotime(&secpart, &nsecpart);
	*time = nsecpart + (1000000000ULL * secpart); //convert seconds to  nanoseconds.
	
#else 
	AbsoluteTime	absoluteTime;
	
	clock_get_uptime(&absoluteTime);
	
	//AbsoluteTime_to_scalar((AbsoluteTime*)time);
	
	absolutetime_to_nanoseconds(absoluteTime, time);
#endif
}

inline void NDAS_clock_get_system_value(uint32_t *time)
{
#ifdef __KPI_SOCKET__	// BUG BUG BUG!!!
	uint32_t microsecs;	
	
#if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ <= 1050
	clock_get_system_microtime(time, &microsecs);
#else
	clock_get_system_microtime((clock_sec_t *)time, &microsecs);	
#endif
	
#else 
	mach_timespec_t	time_t;
	
	time_t = clock_get_system_value();
	
	*time = time_t.tv_sec;
#endif
	
}

#ifndef __KPI_SOCKET__	// BUG BUG BUG!!!
inline void
NDAS_uuid_generate_random(NDAS_uuid_t out)
{
	read_random(out, sizeof(NDAS_uuid_t));
	
	out[6] = (out[6] & 0x0F) | 0x40;
	out[8] = (out[8] & 0x3F) | 0x80;
}
#endif

inline void
NDAS_uuid_generate(NDAS_uuid_t out)
{
#ifdef __KPI_SOCKET__	// BUG BUG BUG!!!
	uuid_generate_random(out);
#else
	NDAS_uuid_generate_random(out);
#endif
}

// sprintf
inline int mySnprintf(
					  char *buf,
					  size_t	size,
					  const char *format,
					  ...
					  )
{	
	int ret;
	
	va_list ap;
	va_start(ap, format);
	
	ret = vsnprintf(buf, size, format, ap);
	
	va_end(ap);
	
	return ret;
} 

#if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= 1040

inline kern_return_t	myCreateThread(
									   THREAD_FUNCTION	continuation,
									   void*				parameter,
									   THREAD_T*			new_thread
									   )
{
	return kernel_thread_start(continuation, parameter, new_thread);
}


inline void myExitThread(THREAD_T *thread)
{
	THREAD_T	thread_ptr = *thread;
	
	*thread = NULL;
	
	thread_deallocate(thread_ptr);
	thread_terminate(thread_ptr);
}

#else

inline kern_return_t	myCreateThread(
									   THREAD_FUNCTION	continuation,
									   void*				parameter,
									   THREAD_T*			new_thread
									   )
{
	*new_thread = IOCreateThread((IOThreadFunc)continuation, parameter);
	if(!*new_thread) {
		
		return KERN_FAILURE;
	}
	
	return KERN_SUCCESS;
}

inline void myExitThread(THREAD_T *thread)
{
	IOExitThread();
}

#endif
