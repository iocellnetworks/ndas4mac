/*
 * LanScsiDiskInformationBlock.h
 *
 * NDAS Disk Information Block
 *
 * Copyright (c) 2004 XiMeta, Inc. All rights reserved.
 *
 */

#ifndef _LanScsiDiskInformationBlock_H_
#define _LanScsiDiskInformationBlock_H_

#include <sys/types.h>

#pragma once

/*
 * Disk Information Block
 *
 * Rewrited from Windows platform
 */


//
//  disk information format
//
//


//
// cslee:
//
// Disk Information Block should be aligned to 512-byte (1 sector size)
//
// Checksum has not been used yet, so we can safely fix the size of
// the structure at this time without increasing the version of the block structure
//
// Note when you change members of the structure, you should increase the
// version of the structure and you SHOULD provide the migration
// path from the previous version.
//

//
// cslee:
//
// Disk Information Block should be aligned to 512-byte (1 sector size)
//
// Checksum has not been used yet, so we can safely fix the size of
// the structure at this time without increasing the version of the block structure
//
// Note when you change members of the structure, you should increase the
// version of the structure and you SHOULD provide the migration
// path from the previous version.
// 

typedef int64_t NDAS_BLOCK_LOCATION;
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_DIB_V1		= -1;		// Disk Information Block V1
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_DIB_V2		= -2;		// Disk Information Block V2
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_ENCRYPT	= -3;		// Content encryption information
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_RMD		= -4;		// RAID Meta data
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_RMD_T		= -5;		// RAID Meta data(for transaction)
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_OEM		= -0x0100;	// OEM Reserved
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_ADD_BIND	= -0x0200;	// Additional bind informations
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_BACL		= -0x0204;	// Block Access Control List
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_BITMAP		= -0x0ff0;	// Corruption Bitmap(Optional)
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_LWR_REV1	= -0x1000;	// Last written region(Optional). Used only by 3.10 RAID1 rev.1.

static const NDAS_BLOCK_LOCATION NDAS_BLOCK_SIZE_XAREA			= 0x1000;	// Total X Area Size
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_SIZE_BITMAP_REV1	= 0x0800;	// Corruption Bitmap(Optional)		
#define NDAS_BLOCK_SIZE_BITMAP	0x0010							// Corruption Bitmap(Optional) As of 3.20, Bitmap size is max 8k(16 sector)
//static const NDAS_BLOCK_LOCATION NDAS_BLOCK_SIZE_BITMAP			= 0x0010;	// Corruption Bitmap(Optional)

static const NDAS_BLOCK_LOCATION NDAS_BLOCK_SIZE_LWR_REV1		= 0x0001;	// Last written region. Used only by 3.10 RAID1 rev.1

static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_MBR		= 0;		// Master Boot Record
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_USER		= 0x80;		// Partion 1 starts here
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_LDM		= 0;		// depends on sizeUserSpace, last 1MB of user space

typedef uint8_t NDAS_DIB_DISK_TYPE;

#define NDAS_DIB_DISK_TYPE_SINGLE				0
#define NDAS_DIB_DISK_TYPE_MIRROR_MASTER		1
#define NDAS_DIB_DISK_TYPE_AGGREGATION_FIRST	2
#define NDAS_DIB_DISK_TYPE_MIRROR_SLAVE			11
#define NDAS_DIB_DISK_TYPE_AGGREGATION_SECOND	21
#define NDAS_DIB_DISK_TYPE_AGGREGATION_THIRD	22
#define NDAS_DIB_DISK_TYPE_AGGREGATION_FOURTH	23
//#define NDAS_DIB_DISK_TYPE_DVD		31
#define NDAS_DIB_DISK_TYPE_VDVD			32
//#define NDAS_DIB_DISK_TYPE_MO			33
//#define NDAS_DIB_DISK_TYPE_FLASHCATD	34
#define NDAS_DIB_DISK_TYPE_INVALID	0x80
#define NDAS_DIB_DISK_TYPE_BIND		0xC0

// extended type information is stored in DIBEXT
#define NDAS_DIB_DISK_TYPE_EXTENDED				80

typedef uint8_t NDAS_DIBEXT_DISK_TYPE;

static const NDAS_DIBEXT_DISK_TYPE NDAS_DIBEXT_DISK_TYPE_SINGLE = 0;
static const NDAS_DIBEXT_DISK_TYPE NDAS_DIBEXT_DISK_TYPE_MIRROR_MASTER = 1;
static const NDAS_DIBEXT_DISK_TYPE NDAS_DIBEXT_DISK_TYPE_AGGREGATION_FIRST	= 2;
static const NDAS_DIBEXT_DISK_TYPE NDAS_DIBEXT_DISK_TYPE_MIRROR_SLAVE = 11;

typedef uint8_t NDAS_DIB_DISKTYPE;
typedef uint8_t NDAS_DIBEXT_DISKTYPE;

static const uint64_t NDAS_DIBEXT_SIGNATURE = 0x3F404A9FF4495F9FULL;

//
// obsolete types (NDAS_DIB_USAGE_TYPE)
//
#define NDAS_DIB_USAGE_TYPE_HOME    0x00
#define NDAS_DIB_USAGE_TYPE_OFFICE  0x10

#if BYTE_ORDER == BIG_ENDIAN
static const uint32_t NDAS_DIB_SIGNATURE_V1 = 0x4E7A03FE;
#else
static const uint32_t NDAS_DIB_SIGNATURE_V1 = 0xFE037A4E;
#endif

static const uint8_t NDAS_DIB_VERSION_MAJOR_V1 = 0;
static const uint8_t NDAS_DIB_VERSION_MINOR_V1 = 1;

/* Disk Information Block Version 1 */
typedef struct _NDAS_DIB_V1
{
    uint32_t Signature;         /* 4 (4) */

    uint8_t MajorVersion;       /* 1 (5) */
    uint8_t MinorVersion;       /* 1 (6) */
    uint8_t reserved1[2];       /* 1 * 2 (8) */

    uint32_t Sequence;          /* 4 (12) */

    uint8_t EtherAddress[6];    /* 1 * 6 (18) */
    uint8_t UnitNumber;         /* 1 (19) */
    uint8_t reserved2;          /* 1 (20) */

    uint8_t DiskType;           /* 1 (21) */
    uint8_t PeerAddress[6];     /* 1 * 6 (27) */
    uint8_t PeerUnitNumber;     /* 1 (28) */
    uint8_t reserved3;          /* 1 (29) */

    uint8_t UsageType;          /* 1 (30) */
    uint8_t reserved4[3];       /* 1 * 3 (33) */

    uint8_t reserved5[512 - 37];  /* should be 512 - ( 33 + 4 ) */

    uint32_t Checksum;          /* 4 (512) */

} __attribute__ ((packed)) NDAS_DIB_V1, * PNDAS_DIB_V1;

typedef struct _UNIT_DISK_INFO
{
	uint8_t HwVersion:4;	// Used to determine capability of offline RAID member.
	uint8_t Reserved1:4;
	uint8_t Reserved2;
} UNIT_DISK_INFO, *PUNIT_DISK_INFO;

typedef struct _UNIT_DISK_LOCATION
{
    uint8_t MACAddr[6];				/* 6 (6) */
    uint8_t UnitNumber;				/* 1 (7) */
	uint8_t	VID; // vendor ID		/* 1 (8) */
} __attribute__ ((packed)) UNIT_DISK_LOCATION, * PUNIT_DISK_LOCATION;

#define NDAS_MAX_UNITS_IN_BIND      16 // not system limit
#define NDAS_MAX_UNITS_IN_V2        32
#define NDAS_MAX_UNITS_IN_SECTOR    64

#if BYTE_ORDER == BIG_ENDIAN
static const uint64_t NDAS_DIB_SIGNATURE_V2 = 0x9F5F49F49F4A403FULL;
#else
static const uint64_t NDAS_DIB_SIGNATURE_V2 = 0x3F404A9FF4495F9FULL;
#endif

static const uint32_t NDAS_DIB_VERSION_MAJOR_V2 = 1;
static const uint32_t NDAS_DIB_VERSION_MINOR_V2 = 1;

#define NDAS_BITMAP_SECTOR_PER_BIT_DEFAULT (1<<16)		// 32Mbytes. Changed to 32M since 3.20
#define NDAS_USER_SPACE_ALIGN   (128)                   // 3.11 version required 128 sector alignment of user addressable space.
                                                        // Do not change the value to keep compatiblity with NDAS devices 
                                                        // formatted under NDAS software version 3.11.
 
#define NMT_INVALID			0		// operation purpose only : must NOT written on disk
#define NMT_SINGLE			1		// unbound
#define NMT_MIRROR			2		// 2 disks without repair information. need to be converted to RAID1
#define NMT_AGGREGATE		3		// 2 or more
#define NMT_RAID1			4		// with repair
#define NMT_RAID4			5		// with repair. Never released.
#define NMT_RAID0			6		// with repair. RMD is not used. Since the block size of this raid set is 512 * n, Mac OS X don't support this type.
#define NMT_SAFE_RAID1		7		// operation purpose only(add mirror) : must NOT written on disk. used in bind operation only
#define NMT_AOD				8		// Append only disk
#define NMT_RAID0R2			9		// Block size is 512B not 512 * n.
#define NMT_RAID1R2			10		// with repair, from 3.11
#define NMT_RAID4R2			11		// with repair, from 3.11. Never released
#define NMT_RAID1R3			12		// with DRAID & persistent OOS bitmap. Added as of 3.20
#define NMT_RAID4R3			13		// with DRAID & persistent OOS bitmap. Added as of 3.20. Not implemented yet.
#define NMT_VDVD			100		// virtual DVD
#define NMT_CDROM			101		// packet device, CD / DVD
#define NMT_OPMEM			102		// packet device, Magnetic Optical
#define NMT_FLASH			103		// block device, flash card
#define NMT_CONFLICT		255		// DIB information is conflicting with actual NDAS information. Used for internal purpose. Must not be written to disk.
									// To do: reimplement DIB read functions without using this value..

/*
Because prior NDAS service overwrites NDAS_BLOCK_LOCATION_DIB_V1 if Signature, version does not match,
	NDAS_DIB_V2 locates at NDAS_BLOCK_LOCATION_DIB_V2(-2).
 */

// Disk Information Block V2
typedef struct _NDAS_DIB_V2 {
	union{
		struct {
			uint64_t	Signature;		// Byte 0
			uint32_t	MajorVersion;	// Byte 8
			uint32_t	MinorVersion;	// Byte 12
			
			// sizeXArea + sizeLogicalSpace <= sizeTotalDisk
			uint64_t	sizeXArea; // Byte 16. in sectors, always 2 * 1024 * 2 (2 MB)
			
			uint64_t	sizeUserSpace; // Byte 24. in sectors
			
			uint32_t	iSectorsPerBit; // Byte 32. dirty bitmap bit unit. default : 128(2^7). Passed to RAID system through service.
											// default 64 * 1024(2^16, 32Mbyte per bit) since 3.20
			uint32_t	iMediaType; //  Byte 36. NDAS Media Type. NMT_*
			uint32_t	nDiskCount; // Byte 40. 1 ~ . physical disk units
			uint32_t	iSequence; // Byte 44. 0 ~. Sequence number of this unit in UnitDisks list.
									   //			uint8_t	AutoRecover; // Recover RAID automatically. Obsoleted
			uint8_t		Reserved0[4]; // Byte 48.
			uint32_t	BACLSize;	// Byte 52.In byte. Do not access BACL if zero
			uint8_t		Reserved1[16]; // Byte 56
			uint32_t	nSpareCount; // Byte 72.  0 ~ . used for fault tolerant RAID
			uint8_t		Reserved2[52]; // Byte 76
			UNIT_DISK_INFO 	UnitDiskInfos[NDAS_MAX_UNITS_IN_V2]; // Byte 128. Length 64.
		};
		uint8_t bytes_248[248];
	}; // 248
	uint32_t crc32; // 252
	uint32_t crc32_unitdisks; // 256
	
	UNIT_DISK_LOCATION	UnitDisks[NDAS_MAX_UNITS_IN_V2]; // 256 bytes
} NDAS_DIB_V2, *PNDAS_DIB_V2;

//C_ASSERT(512 == sizeof(NDAS_DIB_V2));

#ifdef __PASS_DIB_CRC32_CHK__
#define IS_DIB_DATA_CRC_VALID(CRC_FUNC, DIB) TRUE
#define IS_DIB_UNIT_CRC_VALID(CRC_FUNC, DIB) TRUE
#else
#define IS_DIB_DATA_CRC_VALID(CRC_FUNC, DIB) ((DIB).crc32 == NDASSwap32LittleToHost(CRC_FUNC((uint8_t *)&(DIB), sizeof((DIB).bytes_248))))
#define IS_DIB_UNIT_CRC_VALID(CRC_FUNC, DIB) ((DIB).crc32_unitdisks == NDASSwap32LittleToHost(CRC_FUNC((uint8_t *)&(DIB).UnitDisks, sizeof((DIB).UnitDisks))))
#endif

#define IS_DIB_CRC_VALID(CRC_FUNC, DIB) (IS_DIB_DATA_CRC_VALID(CRC_FUNC, DIB) && IS_DIB_UNIT_CRC_VALID(CRC_FUNC, DIB))

#define SET_DIB_DATA_CRC(CRC_FUNC, DIB) ((DIB).crc32 = NDASSwap32HostToLittle(CRC_FUNC((uint8_t *)&(DIB), sizeof((DIB).bytes_248))))
#define SET_DIB_UNIT_CRC(CRC_FUNC, DIB) ((DIB).crc32_unitdisks = NDASSwap32HostToLittle(CRC_FUNC((uint8_t *)&(DIB).UnitDisks, sizeof((DIB).UnitDisks))))
#define SET_DIB_CRC(CRC_FUNC, DIB) {SET_DIB_DATA_CRC(CRC_FUNC, DIB); SET_DIB_UNIT_CRC(CRC_FUNC, DIB);}
//
// Unless ContentEncryptCRC32 matches the CRC32,
// no ContentEncryptXXX fields are valid.
//
// CRC32 is Only for ContentEncrypt Fields (LITTLE ENDIAN)
// CRC32 is calculated based on the following criteria
//
// p = ContentEncryptMethod;
// crc32(p, sizeof(ContentEncryptMethod + KeyLength + Key)
//
//
// Key is encrypted with DES to the size of
// NDAS_CONTENT_ENCRYPT_MAX_KEY_LENGTH
// Unused portion of the ContentEncryptKey must be filled with 0
// before encryption
//
// UCHAR key_value[8] = {0x0B,0xBC,0xAB,0x45,0x44,0x01,0x65,0xF0};
//

#define	NDAS_CONTENT_ENCRYPT_MAX_KEY_LENGTH		64		// 64 bytes. 512bits.
#define	NDAS_CONTENT_ENCRYPT_METHOD_NONE	0
#define	NDAS_CONTENT_ENCRYPT_METHOD_SIMPLE	1
#define	NDAS_CONTENT_ENCRYPT_METHOD_AES		2

#define NDAS_CONTENT_ENCRYPT_REV_MAJOR	0x0010
#define NDAS_CONTENT_ENCRYPT_REV_MINOR	0x0010

#define NDAS_CONTENT_ENCRYPT_REVISION \
	((NDAS_CONTENT_ENCRYPT_REV_MAJOR << 16) + NDAS_CONTENT_ENCRYPT_REV_MINOR)

//static const uint64_t NDAS_DIB_SIGNATURE_V2 = 0x3F404A9FF4495F9F;
#if BYTE_ORDER == BIG_ENDIAN
static const uint64_t NDAS_CONTENT_ENCRYPT_BLOCK_SIGNATURE = 0x7D4DF32D4C1F3876ULL;
#else
static const uint64_t NDAS_CONTENT_ENCRYPT_BLOCK_SIGNATURE = 0x76381F4C2DF34D7DULL;
#endif

typedef struct _NDAS_CONTENT_ENCRYPT_BLOCK {
	union{
		struct {
			uint64_t	Signature;	// Little Endian
			// INVARIANT
			uint32_t Revision;   // Little Endian
			// VARIANT BY REVISION (MAJOR)
			uint16_t	Method;		// Little Endian
			uint16_t	Reserved_0;	// Little Endian
			uint16_t	KeyLength;	// Little Endian
			uint16_t	Reserved_1;	// Little Endian
			union{
				uint8_t	Key32[32 /8];
				uint8_t	Key128[128 /8];
				uint8_t	Key192[192 /8];
				uint8_t	Key256[256 /8];
				uint8_t	Key[NDAS_CONTENT_ENCRYPT_MAX_KEY_LENGTH];
			};
			uint8_t Fingerprint[16]; // 128 bit, KeyPairMD5 = md5(ks . kd)
		};
		uint8_t bytes_508[508];
	};
	uint32_t CRC32;		// Little Endian
} NDAS_CONTENT_ENCRYPT_BLOCK, *PNDAS_CONTENT_ENCRYPT_BLOCK;

//C_ASSERT(512 == sizeof(NDAS_CONTENT_ENCRYPT_BLOCK));

#define IS_NDAS_DIBV1_WRONG_VERSION(DIBV1) \
	((0 != (DIBV1).MajorVersion) || \
	(0 == (DIBV1).MajorVersion && 1 != (DIBV1).MinorVersion))

#define IS_HIGHER_VERSION_V2(DIBV2) \
	((NDAS_DIB_VERSION_MAJOR_V2 < (DIBV2).MajorVersion) || \
	(NDAS_DIB_VERSION_MAJOR_V2 == (DIBV2).MajorVersion && \
	NDAS_DIB_VERSION_MINOR_V2 < (DIBV2).MinorVersion))

#define GET_TRAIL_SECTOR_COUNT_V2(DISK_COUNT) \
	(	((DISK_COUNT) > NDAS_MAX_UNITS_IN_V2) ? \
		(((DISK_COUNT) - NDAS_MAX_UNITS_IN_V2) / NDAS_MAX_UNITS_IN_SECTOR) +1 : 0)

// make older version should not access disks with NDAS_DIB_V2
#define NDAS_DIB_V1_INVALIDATE(DIB_V1)									\
	{	(DIB_V1).Signature		= NDAS_DIB_SIGNATURE_V1;					\
		(DIB_V1).MajorVersion	= NDAS_DIB_VERSION_MAJOR_V1;				\
		(DIB_V1).MinorVersion	= NDAS_DIB_VERSION_MINOR_V1;				\
		(DIB_V1).DiskType		= NDAS_DIB_DISK_TYPE_INVALID;	}


#if 0 // LAST_WRITTEN_SECTOR, LAST_WRITTEN_SECTORS is not used anymore.
typedef struct _LAST_WRITTEN_SECTOR {
	UINT64	logicalBlockAddress;
	UINT32	transferBlocks;
	ULONG	timeStamp;
} LAST_WRITTEN_SECTOR, *PLAST_WRITTEN_SECTOR;

//C_ASSERT(16 == sizeof(LAST_WRITTEN_SECTOR));

typedef struct _LAST_WRITTEN_SECTORS {
	LAST_WRITTEN_SECTOR LWS[32];
} LAST_WRITTEN_SECTORS, *PLAST_WRITTEN_SECTORS;

//C_ASSERT(512 == sizeof(LAST_WRITTEN_SECTORS));
#endif

// All number is in little-endian
typedef struct _NDAS_OOS_BITMAP_BLOCK {
	uint64_t SequenceNumHead; // Increased when this block is updated.
	uint8_t Bits[512-16];
	uint64_t SequenceNumTail; // Same value of SequenceNumHead. If two value is not matched, this block is corrupted. 
} NDAS_OOS_BITMAP_BLOCK, *PNDAS_OOS_BITMAP_BLOCK;
//C_ASSERT( 512 == sizeof(NDAS_OOS_BITMAP_BLOCK));
#define NDAS_BYTE_PER_OOS_BITMAP_BLOCK  (512-16)
#define NDAS_BIT_PER_OOS_BITMAP_BLOCK  (NDAS_BYTE_PER_OOS_BITMAP_BLOCK * 8)

/*
member of the NDAS_RAID_META_DATA structure
8 bytes of size
*/
#define NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED	(1<<0) // Disk is not synced because of network down or new disk
															// NDAS_UNIT_META_BIND_STATUS_FAULT at RAID1R2
#define NDAS_UNIT_META_BIND_STATUS_SPARE		(1<<1)
#define NDAS_UNIT_META_BIND_STATUS_BAD_DISK	(1<<2)
#define NDAS_UNIT_META_BIND_STATUS_BAD_SECTOR	(1<<3)
#define NDAS_UNIT_META_BIND_STATUS_REPLACED_BY_SPARE (1<<4)
#define NDAS_UNIT_META_BIND_STATUS_DEFECTIVE	(NDAS_UNIT_META_BIND_STATUS_BAD_DISK |\
													NDAS_UNIT_META_BIND_STATUS_BAD_SECTOR|\
													NDAS_UNIT_META_BIND_STATUS_REPLACED_BY_SPARE)

typedef struct _NDAS_UNIT_META_DATA {
	uint16_t	iUnitDeviceIdx;	 	// Index in NDAS_DIB_V2.UnitDisks
	uint8_t		UnitDeviceStatus; 	// NDAS_UNIT_META_BIND_STATUS_*
	uint8_t		Reserved[5];
} NDAS_UNIT_META_DATA, *PNDAS_UNIT_META_DATA;

//C_ASSERT(8 == sizeof(NDAS_UNIT_META_DATA));

/*
NDAS_RAID_META_DATA structure contains status which changes by the change
of status of the RAID. For stable status, see NDAS_DIB_V2
All data is in littlen endian.

NDAS_RAID_META_DATA has 512 bytes of size

NDAS_RAID_META_DATA is for Fault tolerant bind only. ATM, RAID 1 and 4.
*/

#ifndef GUID
typedef struct _GUID {
	char guid[16];
} GUID, *PGUID;
#endif

#if BYTE_ORDER == BIG_ENDIAN
static const uint64_t NDAS_RAID_META_DATA_SIGNATURE = 0x01C3CD100C211BA0ULL;
#else
static const uint64_t NDAS_RAID_META_DATA_SIGNATURE = 0xA01B210C10CDC301ULL;
#endif

#define NDAS_RAID_META_DATA_STATE_MOUNTED 		(1<<0)
#define NDAS_RAID_META_DATA_STATE_UNMOUNTED 	(1<<1)
#define NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED (1<<2)	// This unit has ever been used in degraded mode.
															// Used to check independently updated RAID members
															// Not valid if disk is spare disk
#define NDAS_DRAID_ARBITER_TYPE_NONE		0	// no arbiter available
#define NDAS_DRAID_ARBITER_TYPE_LPX		1	

#define NDAS_DRAID_ARBITER_ADDR_COUNT	8

typedef struct _NDAS_RAID_META_DATA {
	union {
		struct {
			uint64_t	Signature;	// Little Endian
			GUID			RaidSetId;	// Prior RaidSetId
								// RaidSetId is created when RAID is created 
								//		and not changed until RAID configuration is changed by bindtool.
								// Both RaidSetId and ConfigSetId should be matched to work as RAID member
			uint32_t uiUSN; // Universal sequence number increases 1 each writes.
			uint32_t state; // Ored value of NDAS_RAID_META_DATA_STATE_*
			GUID			ConfigSetId;	// Added as of 3.20. For previous version this is 0.
					// Set when RAID is created and changed 
					// when RAID is reconfigured online with missing member.
					// Reconfiguration occurs when spare member is changed to active member to rule out missing member
					//	or on-line reconfiguration occurs.(Currently not implemented)
					// Meaningful only for RAID1 and RAID4/5 with 2+1 configuration with spare.

			//
			// DRAID Arbiter node information. Updated when state is changed to mounted.
			// Used only for redundent RAID.
			// Added as of 3.20
			//
			struct {
				uint8_t   Type; // Type of RAID master node. LPX/IP, running mode, etc.
								// Currently NDAS_DRAID_ARBITER_TYPE_LPX and NDAS_DRAID_ARBITER_TYPE_NONE only.
				uint8_t   Reserved[3];
				uint8_t	  Addr[8]; // MAC or IP. Need to be at least 16 byte to support IPv6?
			} ArbiterInfo[NDAS_DRAID_ARBITER_ADDR_COUNT];	// Assume max 8 network interface.
		};
		uint8_t bytes_248[248];
	}; // 248
	uint32_t crc32; // 252
	uint32_t crc32_unitdisks; // 256

	NDAS_UNIT_META_DATA UnitMetaData[NDAS_MAX_UNITS_IN_V2]; // 256 bytes
					// This array is always sorted by its member role in RAID.
} NDAS_RAID_META_DATA, *PNDAS_RAID_META_DATA;

//C_ASSERT(512 == sizeof(NDAS_RAID_META_DATA));

#ifdef __PASS_RMD_CRC32_CHK__
#define IS_RMD_DATA_CRC_VALID(CRC_FUNC, RMD) TRUE
#define IS_RMD_UNIT_CRC_VALID(CRC_FUNC, RMD) TRUE
#else
#define IS_RMD_DATA_CRC_VALID(CRC_FUNC, RMD) ((RMD).crc32 == NDASSwap32LittleToHost(CRC_FUNC((uint8_t *)&(RMD), sizeof((RMD).bytes_248))))
#define IS_RMD_UNIT_CRC_VALID(CRC_FUNC, RMD) ((RMD).crc32_unitdisks == NDASSwap32LittleToHost(CRC_FUNC((uint8_t *)&(RMD).UnitMetaData, sizeof((RMD).UnitMetaData))))
#endif

#define IS_RMD_CRC_VALID(CRC_FUNC, RMD) (IS_RMD_DATA_CRC_VALID(CRC_FUNC, RMD) && IS_RMD_UNIT_CRC_VALID(CRC_FUNC, RMD))

#define SET_RMD_DATA_CRC(CRC_FUNC, RMD) ((RMD).crc32 = NDASSwap32HostToLittle(CRC_FUNC((uint8_t *)&(RMD), sizeof((RMD).bytes_248))))
#define SET_RMD_UNIT_CRC(CRC_FUNC, RMD) ((RMD).crc32_unitdisks = NDASSwap32HostToLittle(CRC_FUNC((uint8_t *)&(RMD).UnitMetaData, sizeof((RMD).UnitMetaData))))
#define SET_RMD_CRC(CRC_FUNC, RMD) {SET_RMD_DATA_CRC(CRC_FUNC, RMD); SET_RMD_UNIT_CRC(CRC_FUNC, RMD);}

//#define RMD_IDX_RECOVER_INFO(DISK_COUNT, IDX_DEFECTED) (((DISK_COUNT) -1 == (IDX_DEFECTED)) ? 0 : (IDX_DEFECTED) +1)
//#define RMD_IDX_DEFEDTED(DISK_COUNT, IDX_RECOVER_INFO) ((0 == (IDX_RECOVER_INFO)) ? (DISK_COUNT) -1 : (IDX_RECOVER_INFO) -1)
#define RMD_IDX_RECOVER_INFO(DISK_COUNT, IDX_DEFECTED) (((IDX_DEFECTED) +1) % (DISK_COUNT))
#define RMD_IDX_DEFEDTED(DISK_COUNT, IDX_RECOVER_INFO) (((IDX_RECOVER_INFO) + (DISK_COUNT) -1) % (DISK_COUNT))

#endif /* _LanScsiDiskInformationBlock_H_ */

