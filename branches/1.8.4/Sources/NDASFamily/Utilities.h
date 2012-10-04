/***************************************************************************
*
*  Utilities.h
*
*    Utilities for Debugging.
*
*    Copyright (c) 2004 XiMeta, Inc. All rights reserved.
*
**************************************************************************/

#ifndef _UTILITIES_H_
#define _UTILITIES_H_

#include <IOKit/IOLib.h>
#include <stdarg.h>

extern uint32_t	debugLevel;

#define DEBUG_MASK_RELEASE		0x00000000

#define DEBUG_MASK_PNP_ERROR	0x00000001
#define DEBUG_MASK_PNP_WARNING	0x00000002
#define DEBUG_MASK_PNP_TRACE	0x00000004
#define DEBUG_MASK_PNP_INFO		0x00000008

#define DEBUG_MASK_NDAS_ERROR	0x00000010
#define DEBUG_MASK_NDAS_WARNING	0x00000020
#define DEBUG_MASK_NDAS_TRACE	0x00000040
#define DEBUG_MASK_NDAS_INFO	0x00000080

#define DEBUG_MASK_USER_ERROR	0x00000100
#define DEBUG_MASK_USER_WARNING	0x00000200
#define DEBUG_MASK_USER_TRACE	0x00000400
#define DEBUG_MASK_USER_INFO	0x00000800

#define DEBUG_MASK_DISK_ERROR	0x00001000
#define DEBUG_MASK_DISK_WARNING	0x00002000
#define DEBUG_MASK_DISK_TRACE	0x00004000
#define DEBUG_MASK_DISK_INFO	0x00008000

#define DEBUG_MASK_LS_ERROR		0x00100000
#define DEBUG_MASK_LS_WARNING	0x00200000
#define DEBUG_MASK_LS_TRACE		0x00400000
#define DEBUG_MASK_LS_INFO		0x00800000

#define DEBUG_MASK_POWER_ERROR	0x01000000
#define DEBUG_MASK_POWER_WARNING	0x02000000
#define DEBUG_MASK_POWER_TRACE	0x04000000
#define DEBUG_MASK_POWER_INFO	0x08000000

#define DEBUG_MASK_RAID_ERROR	0x10000000
#define DEBUG_MASK_RAID_WARNING	0x20000000
#define DEBUG_MASK_RAID_TRACE	0x40000000
#define DEBUG_MASK_RAID_INFO	0x80000000

#define DEBUG_MASK_ALL			0xffffffff

#define DbgIOLog(debug, x)	if(debugLevel & debug) { IOLog("[%s:%d][%s] ", getName(), __LINE__, __FUNCTION__); IOLog x; }
#define DbgIOLogNC(debug, x)	if(debugLevel & debug) { IOLog x; }	// No Class

//#define DbgIOASSERT(a) { if (!(a)) { panic("File %s, line %d: assertion '%s' failed.\n", __FILE__, __LINE__, #a); } }
#define DbgIOASSERT(a) assert(a)

#define VENDOR_ID   "NDAS"
#define ND_PRODUCT_REVISION_LEVEL "0.0"

#define SECTOR_SIZE		512

#define MAX_BUFFER_SIZE (128 * SECTOR_SIZE)

// Since uuid functions not include in panther.
typedef	unsigned char	NDAS_uuid_t[16];

#if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= 1040
#define THREAD_T		thread_t
#define THREAD_FUNCTION	thread_continue_t
#else
#define THREAD_T		IOThread
#define THREAD_FUNCTION	IOThreadFunc
#endif

#ifdef __cplusplus
extern "C"
{
#endif
	
	void NDAS_clock_get_uptime(uint64_t *time);
	
	void NDAS_clock_get_system_value(uint32_t *time);
	
	void NDAS_uuid_generate(NDAS_uuid_t out);
	
	int mySnprintf(
				   char *buf,
				   size_t	size,
				   const char *format,
				   ...
				   );
	
	kern_return_t	myCreateThread(
								   THREAD_FUNCTION	continuation,
								   void				*parameter,
								   THREAD_T*new_thread
								);
	
	
	void myExitThread(THREAD_T *thread);
	
#ifdef __cplusplus
}
#endif

#endif // _UTILITIES_H_