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

#include <mach/mach_types.h>

extern uint32_t	debugLevel;

#define DEBUG_MASK_RELEASE		0x00000000

#define DEBUG_MASK_NKE_ERROR	0x00000001
#define DEBUG_MASK_NKE_WARING	0x00000002
#define DEBUG_MASK_NKE_TRACE	0x00000004
#define DEBUG_MASK_NKE_INFO		0x00000008

#define DEBUG_MASK_IF_ERROR		0x00000010
#define DEBUG_MASK_IF_WARING	0x00000020
#define DEBUG_MASK_IF_TRACE		0x00000040
#define DEBUG_MASK_IF_INFO		0x00000080

#define DEBUG_MASK_LPX_ERROR	0x00000100
#define DEBUG_MASK_LPX_WARING	0x00000200
#define DEBUG_MASK_LPX_TRACE	0x00000400
#define DEBUG_MASK_LPX_INFO		0x00000800

#define DEBUG_MASK_DGRAM_ERROR	0x00001000
#define DEBUG_MASK_DGRAM_WARING	0x00002000
#define DEBUG_MASK_DGRAM_TRACE	0x00004000
#define DEBUG_MASK_DGRAM_INFO	0x00008000

#define DEBUG_MASK_STREAM_ERROR		0x00100000
#define DEBUG_MASK_STREAM_WARING	0x00200000
#define DEBUG_MASK_STREAM_TRACE		0x00400000
#define DEBUG_MASK_STREAM_INFO		0x00800000

#define DEBUG_MASK_PCB_ERROR	0x01000000
#define DEBUG_MASK_PCB_WARING	0x02000000
#define DEBUG_MASK_PCB_TRACE	0x04000000
#define DEBUG_MASK_PCB_INFO		0x08000000

#define DEBUG_MASK_TIMER_ERROR	0x10000000
#define DEBUG_MASK_TIMER_WARING	0x20000000
#define DEBUG_MASK_TIMER_TRACE	0x40000000
#define DEBUG_MASK_TIMER_INFO	0x80000000

#define DEBUG_MASK_ALL			0xffffffff

#define DEBUG_PRINT(l, x)	if (debugLevel & l) { printf x; }

#define LOG_TRACE() printf("%s:%d\n", __FUNCTION__, __LINE__)

#endif // _UTILITIES_H_
