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

extern UInt32	debugLevel;

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

#define DbgIOLog(debug, x)	if(debugLevel & debug) { IOLog("[%s:%d] ", getName(), __LINE__); IOLog x; }
#define DbgIOLogNC(debug, x)	if(debugLevel & debug) { IOLog x; }	// No Class

#define DbgIOASSERT(a) { if (!(a)) { panic("File "__FILE__", line %d: assertion '%s' failed.\n", __LINE__, #a); } }

#define VENDOR_ID   "NDAS"
#define ND_PRODUCT_REVISION_LEVEL "0.0"

#define SECTOR_SIZE		512

#define MAX_BUFFER_SIZE (128 * SECTOR_SIZE)

typedef struct _ReadCapacityData {
    UInt32  logicalBlockAddress;
    UInt32  bytesPerBlock;
} ReadCapacityData;

typedef struct _MODE_PARAMETER_HEADER_6 {
    UInt8 ModeDataLength;
    UInt8 MediumType;
    UInt8 DeviceSpecificParameter;
    UInt8 BlockDescriptorLength;
}MODE_PARAMETER_HEADER_6, *PMODE_PARAMETER_HEADER_6;

typedef struct _MODE_PARAMETER_HEADER_10 {
    UInt16 ModeDataLength; // big endian
    UInt8 MediumType;
    UInt8 DeviceSpecificParameter;
    UInt8 Reserved[2];
    UInt8 BlockDescriptorLength;
}MODE_PARAMETER_HEADER_10, *PMODE_PARAMETER_HEADER_10;

#define MODE_FD_SINGLE_SIDE     0x01
#define MODE_FD_DOUBLE_SIDE     0x02
#define MODE_FD_MAXIMUM_TYPE    0x1E
#define MODE_DSP_FUA_SUPPORTED  0x10
#define kWriteProtectMask		0x80

//
// Define the mode parameter block.
//

typedef struct _MODE_PARAMETER_BLOCK {
    UInt8 DensityCode;
    UInt8 NumberOfBlocks[3];
    UInt8 Reserved;
    UInt8 BlockLength[3];
}MODE_PARAMETER_BLOCK, *PMODE_PARAMETER_BLOCK;

// Since uuid functions not include in panther.
typedef	unsigned char	NDAS_uuid_t[16];

#ifdef __cplusplus
extern "C"
{
#endif

void NDAS_clock_get_uptime(UInt64 *time);
	
void NDAS_clock_get_system_value(UInt32 *time);

void NDAS_uuid_generate(NDAS_uuid_t out);

#ifdef __cplusplus
}
#endif

#endif // _UTILITIES_H_