/***************************************************************************
 *
 *  LanScsiProtocol.h
 *
 *    LanScsiProtocol definitions
 *
 *    Copyright (c) 2003 XiMeta, Inc. All rights reserved.
 *
 **************************************************************************/

#ifndef _LANSCSIPROTOCOL_H_
#define _LANSCSIPROTOCOL_H_
	
#include "cdb.h"

#ifdef KERNEL
#include "LanScsiSocket.h"
#endif

#define MAX_NR_OF_TARGETS_PER_DEVICE	16
#define PACKET_COMMAND_SIZE				12

typedef struct _PNP_MESSAGE {
	unsigned char ucType;
	unsigned char ucVersion;
} PNP_MESSAGE, *PPNP_MESSAGE;

typedef struct _TARGET_DATA {
    bool		bPresent;
    uint8_t		NRRWHost;
    uint8_t		NRROHost;
    uint64_t	TargetData;

	uint32_t	DiskArrayType;
	
    // IDE Info.
    bool    bLBA;
    bool    bLBA48;
	
	bool	bPacket;
	
    bool	bDMA;
	uint8_t	SelectedTransferMode;
	
    bool    bCable80;
	
    uint64_t	SectorCount;
	uint32_t	SectorSize;
	
	uint32_t	configuration;
	uint32_t	status;
	
	char		model[42];
	char		firmware[10];
	char		serialNumber[22];
	
	uint32_t	MaxRequestBytes;
	
} TARGET_DATA, *PTARGET_DATA;


struct LanScsiSession {
#ifdef KERNEL
    xi_socket_t DevSo;
#else
    int		DevSo;
#endif

    char		Password[8];

    uint8_t     ui8HWType;
    uint8_t     ui8HWVersion;
	uint16_t	ui16Revision;
	
    int32_t     HPID;
    int16_t     RPID;
    uint32_t    Tag;
    uint32_t	SessionPhase;
   
    uint32_t    CHAP_C;
    
    uint32_t    MaxRequestBlocks;
    uint16_t	HeaderEncryptAlgo;
    uint16_t	DataEncryptAlgo;
	
	uint32_t	NumberOfTargets;
};

#define HW_VERSION_1_0				0
#define HW_VERSION_1_1				1
#define HW_VERSION_2_0				2

//
// NDAS hardware version 2.0 revisions
//
#define LANSCSIIDE_VER20_REV_1G_ORIGINAL	0x0000
#define LANSCSIIDE_VER20_REV_100M			0x0018
#define LANSCSIIDE_VER20_REV_1G				0x0010

#define LOGIN_TYPE_NORMAL       0x00
#define LOGIN_TYPE_DISCOVERY    0xFF

#define DISCOVERY_USER			0x00000000
#define FIRST_TARGET_RO_USER    0x00000001
#define FIRST_TARGET_RW_USER    0x00010001
#define	ALL_TARGET_RO_USER		0x00007fff
#define	ALL_TARGET_RW_USER		0x7fff7fff
#define SUPER_USER				0xffffffff



#define MAX_REQUEST_SIZE					1500

#define DISK_BLOCK_SIZE						512

#define DISK_CACHE_SIZE_BY_BLOCK_SIZE		(64 * 1024 * 1024 / DISK_BLOCK_SIZE)	// 64MB

#define MAX_REQUEST_BLOCKS_GIGA_CHIP_BUG	108	// 54KB

/***************************************************************************
 *
 *  Function prototypes
 *
 **************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus
	
extern int Login(struct LanScsiSession *Session, uint8_t LoginType, uint32_t UserID, char *Key);
extern int Logout(struct LanScsiSession *Session);
extern int TextTargetList(struct LanScsiSession *Session, int *NRTarget, TARGET_DATA *PerTarget);
extern int GetDiskInfo(struct LanScsiSession *Session, uint32_t TargetID, TARGET_DATA *TargetData);
extern int IdeCommand(struct LanScsiSession *Session,
                      int32_t   TargetId,
                      TARGET_DATA *TargetData,
                      int64_t   LUN,
                      uint8_t    Command,
                      int64_t   Location,
                      int16_t   SectorCount,
                      int8_t    Feature,
                      char *    pData,
					  uint32_t	BufferLength,
					  uint8_t   *pResult,
					  PCDBd		pCdb,
					  uint8_t	DataTransferDirection
					  );
	
#ifdef __cplusplus
}
#endif // __cplusplus 

#endif /* _LANSCSIPROTOCOL_H_ */

