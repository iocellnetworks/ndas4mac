//
// NDAS HIX (Host Information Exchange) Protocol Definition
//
// Copyright (C) 2003-2004 XIMETA, Inc.
// All rights reserved.
//
// Revision History:
//
// 2004-10-21: Chesong Lee <cslee@ximeta.com>
// - Initial Documentattion
//
// 2004-XX-XX: Chesong Lee <cslee@ximeta.com>
// - Initial Implementation
//
//
// Introduction:
//
// HIX is a peer-to-peer protocol for NDAS hosts to communicate
// with hosts using a LPX Datagram.
// Using HIX, you can,
//
// - Discover the NDAS hosts that is using a NDAS device in the network
// - Request the NDAS host to surrender a specific access to the NDAS device
// - Query the NDAS host information such as OS version, host name,
//   network address of the host, and NDFS compatible version.
// - Get a notification of the DIB change.
//
// Implementations of the server:
//
// HIX server should listen to LPX datagram port NDAS_HIX_LISTEN_PORT
// Maximum size of the packet is 512-bytes.
// Any invalid header or the data packet shall be discarded from the server.
//

#ifndef _NDAS_HIX_H_
#define _NDAS_HIX_H_

#if defined (__WIN32__)

#pragma once

#include <pshpack1.h>
#pragma warning(disable: 4200)

#else 

#include <CoreServices/CoreServices.h>
#include <stdint.h>
/*
#ifndef GUID
typedef struct _GUID {
	char guid[16];
} GUID, *PGUID;
#endif
*/
#ifndef BOOL
typedef bool BOOL;
#endif

#endif

//
// LPX Datagram
//
// NDAS_HIX_LISTEN_PORT 7918
//
#define NDAS_HIX_LISTEN_PORT	0x00EE

//
// The following SIGNATURE value is little endian.
//

#define NDAS_HIX_SIGNATURE 0x4E686958
#define NDAS_HIX_SIGNATURE_CHAR_ARRAY {'N','h','i','X'}

#define NHIX_CURRENT_REVISION 0x01

//
// Maximum message length of the HIX packet is 512-bytes.
// Server implementation may allocate a fixed 512 byte buffer
// to receive the data.
//
#define NHIX_MAX_MESSAGE_LEN	512

#define NHIX_TYPE_DISCOVER			0x01
#define NHIX_TYPE_QUERY_HOST_INFO	0x02
#define NHIX_TYPE_SURRENDER_ACCESS	0x03
#define NHIX_TYPE_DEVICE_CHANGE		0x04
#define NHIX_TYPE_UNITDEVICE_CHANGE	0x05

//
// 32 byte header for NHIX packets.
// HIX_HEADER is also declared as a field of each specialized packet types.
// So you don't have to send the header and the data twice.
// 
struct _NDAS_HIX_HEADER {

	union {
		uint8_t SignatureChars[4]; // NDAS_HIX_SIGNATURE_CHAR_ARRAY
		uint32_t Signature;
	};
	uint8_t Revision;				// NHIX_CURRENT_REVISION
#if defined(__BIG_ENDIAN__)
	uint8_t Type : 7;				// NHIX_TYPE_XXX
	uint8_t ReplyFlag : 1;		// 0 for request, 1 for reply
#else
	uint8_t ReplyFlag : 1;		// 0 for request, 1 for reply
	uint8_t Type : 7;				// NHIX_TYPE_XXX
#endif	
	uint16_t Length;				// Including header
	union {
		uint8_t HostId[16];		// Generally 16 byte GUID is used
		GUID HostGuid;
	};
	uint8_t Reserved[8];			// Should be zero's

} __attribute__ ((packed));

typedef struct _NDAS_HIX_HEADER NDAS_HIX_HEADER, *PNDAS_HIX_HEADER;



#define NHIX_UDA_BIT_READ              0x80
#define NHIX_UDA_BIT_WRITE             0x40
#define NHIX_UDA_BIT_SHARED_RW         0x20
#define NHIX_UDA_BIT_SHARED_RW_PRIMARY 0x10
	
#define NHIX_UDA_NO_ACCESS    0x00
#define NHIX_UDA_READ_ACCESS  NHIX_UDA_BIT_READ
#define NHIX_UDA_WRITE_ACCESS NHIX_UDA_BIT_WRITE
/* it was 0xD0 */
#define NHIX_UDA_READ_WRITE_ACCESS \
        (NHIX_UDA_BIT_READ | NHIX_UDA_BIT_WRITE)
#define NHIX_UDA_SHARED_READ_WRITE_SECONDARY_ACCESS     \
        (NHIX_UDA_BIT_READ | NHIX_UDA_BIT_WRITE | NHIX_UDA_BIT_SHARED_RW)
#define NHIX_UDA_SHARED_READ_WRITE_PRIMARY_ACCESS       \
        (NHIX_UDA_BIT_READ | NHIX_UDA_BIT_WRITE | NHIX_UDA_BIT_SHARED_RW | NHIX_UDA_BIT_SHARED_RW_PRIMARY)

#define NHIX_UDA_NONE           NHIX_UDA_NO_ACCESS
#define NHIX_UDA_RO             NHIX_UDA_READ_ACCESS
#define NHIX_UDA_WO             NHIX_UDA_WRITE_ACCESS
#define NHIX_UDA_RW             NHIX_UDA_READ_WRITE_ACCESS
#define NHIX_UDA_SHRW_PRIM      NHIX_UDA_SHARED_READ_WRITE_PRIMARY_ACCESS
#define NHIX_UDA_SHRW_SEC       NHIX_UDA_SHARED_READ_WRITE_SECONDARY_ACCESS

//
// UDA bit set is an bit set structure for representing UDA
// You may cast this to NHIX_UDA
//
struct _NHIX_UDA_BITSET {
#if defined(__BIG_ENDIAN__)
	uint8_t ReadAccess : 1;
	uint8_t WriteAccess : 1;
	uint8_t SharedRW : 1;
	uint8_t SharedRWPrimary : 1;
	uint8_t Reserved : 4;
#else
	uint8_t Reserved : 4;
	uint8_t SharedRWPrimary : 1;
	uint8_t SharedRW : 1;
	uint8_t WriteAccess : 1;
	uint8_t ReadAccess : 1;
#endif
} __attribute__ ((packed));

typedef struct _NHIX_UDA_BITSET NHIX_UDA_BITSET, *PNHIX_UDA_BITSET;

typedef uint8_t NHIX_UDA;

struct _NDAS_HIX_UNITDEVICE_ENTRY_DATA {
	uint8_t DeviceId[6];
	uint8_t UnitNo;
	NHIX_UDA AccessType;
}  __attribute__ ((packed));

typedef struct _NDAS_HIX_UNITDEVICE_ENTRY_DATA NDAS_HIX_UNITDEVICE_ENTRY_DATA, *PNDAS_HIX_UNITDEVICE_ENTRY_DATA;

struct _NDAS_HIX_HOST_INFO_VER_INFO {
	uint16_t VersionMajor;
	uint16_t VersionMinor;
	uint16_t VersionBuild;
	uint16_t VersionPrivate;
} __attribute__ ((packed));

typedef struct _NDAS_HIX_HOST_INFO_VER_INFO NDAS_HIX_HOST_INFO_VER_INFO, *PNDAS_HIX_HOST_INFO_VER_INFO;

// Ad Hoc Information
struct _NDAS_HIX_HOST_INFO_AD_HOC {
	BOOL Unicode;
	uint8_t FieldLength; // up to 255 chars
	uint8_t DataLength; // up to 255 chars
	// BYTE Field[];
	// BYTE Data[];
} __attribute__ ((packed));

typedef struct _NDAS_HIX_HOST_INFO_AD_HOC NDAS_HIX_HOST_INFO_AD_HOC, *PNDAS_HIX_HOST_INFO_AD_HOC;

//
// NHDP Host Info Class Type and Fields
//

#define NHIX_HIC_OS_FAMILY				0x00000001
// Required
// Field: uint8_t, one of NHIX_HIC_OS_FAMILY_xxxx

typedef uint8_t NHIX_HIC_OS_FAMILY_TYPE;

#define NHIX_HIC_OS_UNKNOWN		0x00
#define NHIX_HIC_OS_WIN9X		0x01
#define NHIX_HIC_OS_WINNT		0x02
#define NHIX_HIC_OS_LINUX		0x03
#define NHIX_HIC_OS_WINCE		0x04
#define NHIX_HIC_OS_PS2			0x05
#define NHIX_HIC_OS_MAC			0x06
#define NHIX_HIC_OS_EMBEDDED_OS	0x07
#define NHIX_HIC_OS_OTHER		0xFF

// Required
// Field: VER_INFO
#define NHIX_HIC_OS_VER_INFO			0x00000002

// Optional
// Field: CHAR[], null terminated
#define NHIX_HIC_HOSTNAME				0x00000004

// Optional
// Field: CHAR[], null terminated
#define NHIX_HIC_FQDN					0x00000008

// Optional
// Field: CHAR[], null terminated
#define NHIX_HIC_NETBIOSNAME			0x00000010

// Optional
// Field: WCHAR[], null terminated (WCHAR NULL (0x00 0x00))
// UNICODE on network byte order 
#define NHIX_HIC_UNICODE_HOSTNAME		0x00000020

// Optional
// Field: WCHAR[], null terminated (WCHAR NULL (0x00 0x00))
// UNICODE on network byte order 
#define NHIX_HIC_UNICODE_FQDN			0x00000040

// Optional
// Field: WCHAR[], null terminated (WCHAR NULL (0x00 0x00))
// UNICODE on network byte order 
#define NHIX_HIC_UNICODE_NETBIOSNAME	0x00000080

// Optional
// Field: uint8_t Count, uint8_t AddressLen = 6, uint8_t[][6]
#define NHIX_HIC_ADDR_LPX				0x00000100

// Optional
// Field: uint8_t Count, uint8_t AddressLen = 4, uint8_t[][4]
#define NHIX_HIC_ADDR_IPV4				0x00000200

// Optional
// Field: uint8_t Count, uint8_t AddressLen = 16, uint8_t[][16]
#define NHIX_HIC_ADDR_IPV6				0x00000400

// Required
// Field: VER_INFO
#define NHIX_HIC_NDAS_SW_VER_INFO		0x00000800

// Required if using NHIX_UDA_SHRW_xxx
// Field: VER_INFO
#define NHIX_HIC_NDFS_VER_INFO			0x00001000

// Required
// Field: uint32_t, flags (network byte order)
#define NHIX_HIC_HOST_FEATURE			0x00002000

// #define NHIX_HFF_SHARED_WRITE_HOST		0x00000001
#define NHIX_HFF_DEFAULT				0x00000001 // should be always set
#define NHIX_HFF_SHARED_WRITE_CLIENT	0x00000002
#define NHIX_HFF_SHARED_WRITE_SERVER	0x00000004
#define NHIX_HFF_AUTO_REGISTER			0x00000100 // reserved
#define NHIX_HFF_UPNP_HOST				0x00000200 // reserved

typedef uint32_t NHIX_HIC_HOST_FEATURE_TYPE;

// Required
// Field: uint8_t, one or more of NHIX_HIC_TRANSPORT_{LPX,IP}
#define NHIX_HIC_TRANSPORT				0x00004000

#define NHIX_TF_LPX		0x01
#define NHIX_TF_IP		0x02

typedef uint8_t NHIX_HIC_TRANSPORT_TYPE;

// Optional
// Field: AD_HOC
#define NHIX_HIC_AD_HOC					0x80000000

struct _NDAS_HIX_HOST_INFO_ENTRY {
	uint8_t Length; // Entire length (e.g. 5 + sizeof(data))
	uint32_t Class;
	uint8_t Data[1]; // at least one byte data is required
	// Class Specific Data
} __attribute__ ((packed));

typedef struct _NDAS_HIX_HOST_INFO_ENTRY NDAS_HIX_HOST_INFO_ENTRY, *PNDAS_HIX_HOST_INFO_ENTRY;

//
// NDAS_HIXHOST_INFO_DATA Length is constrained by 256 bytes due to
// the limitation of the length (uint8_t).
//
struct _NDAS_HIXHOST_INFO_DATA {
	uint8_t Length;
	uint32_t Contains; // Class Contains Flags
	uint8_t Count;
	NDAS_HIX_HOST_INFO_ENTRY Entry[1];
} __attribute__ ((packed));

typedef struct _NDAS_HIXHOST_INFO_DATA NDAS_HIX_HOST_INFO_DATA, *PNDAS_HIX_HOST_INFO_DATA;

//
// Discover
//
// Discover Request packet is composed of HEADER and REQUEST DATA
// REQUEST data contains the array of Unit Device Entry 
// with a specific access (UDA) bit
//
// UDA bits are:
//
// 1 1 1 1 0 0 0 0
// | | | |
// | | | Shared RW Primary
// | | Shared RW
// | Write
// Read
//
// 1. When the NDAS unit device is used (aka Mounted) at the host,
//    with the any of the access (UDA) specified, the host should
//    reply with the current access (UDA) in the reply packet.
//
//    For example, when host A is using Unit Device U1 as RW mode (Non-NDFS),
//    and the host B sends the request for U1 with NHIX_UDA_RO,
//    the host A should reply with U1 with NHIX_UDA_RW
//
//    Possible configurations for the host A:
//    RO                        NHIX_UDA_RO        (10000000)
//    RW (Non-NDFS)             NHIX_UDA_RW        (11000000)
//    RW (Shared RW Secondary)  NHIX_UDA_SHRW_SEC  (11100000)
//    RW (Shared RW Primary)    NHIX_UDA_SHRW_PRIM (11110000)
//    
// 2. When the EntryCount is 0 (zero), any NDAS hosts that receive 
//    this request should reply with the valid HEADER and the 
//    REPLY data with EntryCount set to 0
//    This special type of DISCOVER is used for discovering
//    NDAS hosts in the network
//
struct _NDAS_HIX_DISCOVER_REQUEST_DATA {
	uint8_t EntryCount;
	NDAS_HIX_UNITDEVICE_ENTRY_DATA Entry[1];
} __attribute__ ((packed));

typedef struct _NDAS_HIX_DISCOVER_REQUEST_DATA NDAS_HIX_DISCOVER_REQUEST_DATA, *PNDAS_HIX_DISCOVER_REQUEST_DATA;

struct _NDAS_HIX_DISCOVER_REPLY_DATA {
	uint8_t EntryCount;
	NDAS_HIX_UNITDEVICE_ENTRY_DATA Entry[1];
} __attribute__ ((packed));

typedef struct _NDAS_HIX_DISCOVER_REPLY_DATA NDAS_HIX_DISCOVER_REPLY_DATA, *PNDAS_HIX_DISCOVER_REPLY_DATA;

struct _NDAS_HIX_DISCOVER_REQUEST {
	NDAS_HIX_HEADER Header;
	NDAS_HIX_DISCOVER_REQUEST_DATA Data;
} __attribute__ ((packed));

typedef struct _NDAS_HIX_DISCOVER_REQUEST  NDAS_HIX_DISCOVER_REQUEST, *PNDAS_HIX_DISCOVER_REQUEST;

struct _NDAS_HIX_DISCOVER_REPLY {
	NDAS_HIX_HEADER Header;
	NDAS_HIX_DISCOVER_REPLY_DATA Data;
} __attribute__ ((packed));

typedef struct _NDAS_HIX_DISCOVER_REPLY  NDAS_HIX_DISCOVER_REPLY, *PNDAS_HIX_DISCOVER_REPLY;

//
// Query Host Info
//
// Query Host Information Request is composed of a HEADER and dummy DATA.
// For the given request packet for the host information,
// the HIX server should reply with the HEADER and the REPLY data.
// 
// REPLY data contains the NDAS_HIX_HOST_INFO_DATA, 
// which contains the array of NDAS_HIX_HOST_INFO_ENTRY.
// The order of HOST_INFO_ENTRY is not defined and it may be arbitrary.
// Any available information of the host shall be contained 
// in the entry.
// 
// The header and the reply data should not be more than 512 bytes
// and the server may not include the non-critical data from the 
// packet in case of overflow.
//
struct _NDAS_HIX_QUERY_HOST_INFO_REQUEST_DATA {
	uint8_t Reserved[32];
} __attribute__ ((packed));

typedef struct _NDAS_HIX_QUERY_HOST_INFO_REQUEST_DATA NDAS_HIX_QUERY_HOST_INFO_REQUEST_DATA, *PNDAS_HIX_QUERY_HOST_INFO_REQUEST_DATA;

struct _NDAS_HIX_QUERY_HOST_INFO_REPLY_DATA {
	uint8_t Reserved[32];
	NDAS_HIX_HOST_INFO_DATA HostInfoData;
} __attribute__ ((packed));

typedef struct _NDAS_HIX_QUERY_HOST_INFO_REPLY_DATA NDAS_HIX_QUERY_HOST_INFO_REPLY_DATA, *PNDAS_HIX_QUERY_HOST_INFO_REPLY_DATA ;

struct _NDAS_HIX_QUERY_HOST_INFO_REQUEST {
	NDAS_HIX_HEADER Header;
	NDAS_HIX_QUERY_HOST_INFO_REQUEST_DATA Data;
} __attribute__ ((packed));

typedef struct _NDAS_HIX_QUERY_HOST_INFO_REQUEST NDAS_HIX_QUERY_HOST_INFO_REQUEST, *PNDAS_HIX_QUERY_HOST_INFO_REQUEST;

struct _NDAS_HIX_QUERY_HOST_INFO_REPLY {
	NDAS_HIX_HEADER Header;
	NDAS_HIX_QUERY_HOST_INFO_REPLY_DATA Data;
} __attribute__ ((packed));

typedef struct _NDAS_HIX_QUERY_HOST_INFO_REPLY NDAS_HIX_QUERY_HOST_INFO_REPLY, *PNDAS_HIX_QUERY_HOST_INFO_REPLY;

//
// Surrender Access
//
// Requesting surrender access to the unit device with UDA
// is interpreted with the following criteria.
// At this time, surrender access is vaild only for the single
// Unit Device Entry.
// For a given UDA of the unit device,
//
// - Only READ_ACCESS and WRITE_ACCESS bit can be set.
//
//   If other bits are set, the request may be invalidated
//   or just the entry can be ignored.
//
//   * Only WRITE_ACCESS and (READ_ACCESS | WRITE_ACCESS) 
//     requests are valid for Windows hosts.
//     Single invalid entry invalidates the entire request.
//
// - READ_ACCESS | WRITE_ACCESS
//
//   To request to surrender both READ and WRITE access, turn the both bit on.
//   The requestee (host) will respond with only the result of 
//   the validation of the packet
//
//   The request host should query the status of the surrender
//   by query NDAS Hosts count from the NDAS device or query DISCOVER
//   packet.
//
// - WRITE_ACCESS
//
//   To request to surrender WRITE access only.
//   the host can 
//   - change the access mode to READ-ONLY or
//   - disconnect the access. 
//   Final reply contains the changed access mode.
//
// - READ_ACCESS (DO NOT USE AT THIS TIME)
//
//   To request to surrender READ access, the requester can turn on
//   only READ_ACCESS bit. READ_ACCESS bit implies to surrender
//   the READ_ACCESS and the host may not surrender WRITE_ACCESS
//   when there are WRITE-ONLY-ACCESSIBLE NDAS devices.
//   This request may be treated as invalid request.
//

struct _NDAS_HIX_SURRENDER_ACCESS_REQUEST_DATA {
	struct {
		uint8_t EntryCount; // Should be always 1
		NDAS_HIX_UNITDEVICE_ENTRY_DATA Entry[1];
	};
} __attribute__ ((packed));

typedef struct _NDAS_HIX_SURRENDER_ACCESS_REQUEST_DATA NDAS_HIX_SURRENDER_ACCESS_REQUEST_DATA, *PNDAS_HIX_SURRENDER_ACCESS_REQUEST_DATA;

#define NHIX_SURRENDER_REPLY_STATUS_QUEUED 0x01
#define NHIX_SURRENDER_REPLY_STATUS_ERROR  0x02
#define NHIX_SURRENDER_REPLY_STATUS_NO_ACCESS  0x03
#define NHIX_SURRENDER_REPLY_STATUS_INVALID_REQUEST 0xFF

struct _NDAS_HIX_SURRENDER_ACCESS_REPLY_DATA {
	uint8_t Status;
} __attribute__ ((packed));

typedef struct _NDAS_HIX_SURRENDER_ACCESS_REPLY_DATA NDAS_HIX_SURRENDER_ACCESS_REPLY_DATA, *PNDAS_HIX_SURRENDER_ACCESS_REPLY_DATA;

struct _NDAS_HIX_SURRENDER_ACCESS_REQUEST {
	NDAS_HIX_HEADER Header;
	NDAS_HIX_SURRENDER_ACCESS_REQUEST_DATA Data;
} __attribute__ ((packed));

typedef struct _NDAS_HIX_SURRENDER_ACCESS_REQUEST NDAS_HIX_SURRENDER_ACCESS_REQUEST, *PNDAS_HIX_SURRENDER_ACCESS_REQUEST;

struct _NDAS_HIX_SURRENDER_ACCESS_REPLY {
	NDAS_HIX_HEADER Header;
	NDAS_HIX_SURRENDER_ACCESS_REPLY_DATA Data;
} __attribute__ ((packed)); 

typedef struct _NDAS_HIX_SURRENDER_ACCESS_REPLY NDAS_HIX_SURRENDER_ACCESS_REPLY, *PNDAS_HIX_SURRENDER_ACCESS_REPLY;

//
// Unit Device Change Notify
// 
// This is a notification packet for changes of DIB(Disk Information Block)
// to the NDAS hosts in the network. This notification is sent 
// through the broadcast.
//
// Receiving the packet of this type of the given unit device,
// the NDAS host should invalidate the status of DIB of that unit device,
// and should re-read the DIB information from the unit device.
//
// This notification is generally sent from the NDAS Bind application.
//
struct _NDAS_HIX_DEVICE_CHANGE_NOTIFY {
	NDAS_HIX_HEADER Header;
	uint8_t DeviceId[6];
} __attribute__ ((packed));

typedef struct _NDAS_HIX_DEVICE_CHANGE_NOTIFY NDAS_HIX_DEVICE_CHANGE_NOTIFY, *PNDAS_HIX_DEVICE_CHANGE_NOTIFY;

struct _NDAS_HIX_UNITDEVICE_CHANGE_NOTIFY {
	NDAS_HIX_HEADER Header;
	uint8_t DeviceId[6];
	uint8_t UnitNo;
} __attribute__ ((packed));

typedef struct _NDAS_HIX_UNITDEVICE_CHANGE_NOTIFY NDAS_HIX_UNITDEVICE_CHANGE_NOTIFY, *PNDAS_HIX_UNITDEVICE_CHANGE_NOTIFY;

#ifdef __cplusplus
//
// Nested Type Definitions for C++
//
struct NDAS_HIX {

	typedef NDAS_HIX_HEADER HEADER, *PHEADER;
	typedef NDAS_HIX_UNITDEVICE_ENTRY_DATA 
		UNITDEVICE_ENTRY_DATA, *PUNITDEVICE_ENTRY_DATA;

	struct HOST_INFO {
		typedef struct _DATA : NDAS_HIX_HOST_INFO_DATA {
			typedef NDAS_HIX_HOST_INFO_ENTRY ENTRY, *PENTRY;
		} DATA, *PDATA;
		typedef NDAS_HIX_HOST_INFO_VER_INFO VER_INFO, *PVER_INFO;
		typedef NDAS_HIX_HOST_INFO_AD_HOC AD_HOC, *PAD_HOC;
	};

	struct DISCOVER {
		typedef struct _REQUEST : NDAS_HIX_DISCOVER_REQUEST {
			typedef NDAS_HIX_DISCOVER_REQUEST_DATA DATA, *PDATA;
		} REQUEST, *PREQUEST;
		typedef struct _REPLY : NDAS_HIX_DISCOVER_REPLY {
			typedef NDAS_HIX_DISCOVER_REPLY_DATA DATA, *PDATA;
		} REPLY, *PREPLY;
	};

	struct QUERY_HOST_INFO {
		typedef struct _REQUEST : NDAS_HIX_QUERY_HOST_INFO_REQUEST {
			typedef NDAS_HIX_QUERY_HOST_INFO_REQUEST_DATA DATA, *PDATA;
		} REQUEST, *PREQUEST;
		typedef struct _REPLY : NDAS_HIX_QUERY_HOST_INFO_REPLY {
			typedef NDAS_HIX_QUERY_HOST_INFO_REPLY_DATA DATA, *PDATA;
		} REPLY, *PREPLY;
	};

	struct SURRENDER_ACCESS {
		typedef struct _REQUEST : NDAS_HIX_SURRENDER_ACCESS_REQUEST {
			typedef NDAS_HIX_SURRENDER_ACCESS_REQUEST_DATA DATA, *PDATA;
		} REQUEST, *PREQUEST;
		typedef struct _REPLY : NDAS_HIX_SURRENDER_ACCESS_REPLY {
			typedef NDAS_HIX_SURRENDER_ACCESS_REPLY_DATA DATA, *PDATA;
		} REPLY, *PREPLY;
	};

	struct DEVICE_CHANGE {
		typedef NDAS_HIX_DEVICE_CHANGE_NOTIFY NOTIFY, *PNOTIFY;
	};

	struct UNITDEVICE_CHANGE {
		typedef NDAS_HIX_UNITDEVICE_CHANGE_NOTIFY NOTIFY, *PNOTIFY;
	};

} __attribute__ ((packed));

#endif // __cplusplus

#if defined(__WIN32__)

#pragma warning(default: 4200)
#include <poppack.h>

#endif

#endif // _NDAS_HIX_H_

