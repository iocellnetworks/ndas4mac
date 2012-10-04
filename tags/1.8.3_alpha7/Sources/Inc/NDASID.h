#ifndef NDUILIB_NETDISKID_H
#define NDUILIB_NETDISKID_H

typedef enum _NDAS_VID {
	
	NDAS_VID_NONE = 0x00,
	NDAS_VID_DEFAULT = 0x01, 
	NDAS_VID_LINUX_ONLY = 0x10,
	NDAS_VID_WINDWOS_RO = 0x20,
	NDAS_VID_SEAGATE = 0x41,
	NDAS_VID_AUTO_REGISTRY = 0x60,
	NDAS_VID_SOFT_NETDISK = 0xA0,
	NDAS_VID_PNP = 0xFF,
	
} NDAS_VID;

typedef struct _NDASID_EXT_KEY {
	uint8_t Key1[8];
	uint8_t Key2[8];
} NDASID_EXT_KEY, *PNDASID_EXT_KEY;

typedef struct _NDASID_EXT_DATA {
	uint8_t Seed;
	uint8_t VID;
	uint8_t Reserved[2];
} NDASID_EXT_DATA, *PNDASID_EXT_DATA;

/* <TITLE NDAS_OEM_CODE>

NDAS OEM Code is a 8-byte array of bytes.
I64Value is provided as an union. 
It is recommended not to use I64Value but use Bytes fields.
Existing I64Value should be translated Bytes, 
considering endian of the original development target system.
*/

typedef union _NDAS_OEM_CODE
{
	uint8_t		Bytes[8];
	uint64_t	UI64Value;
//	ULARGE_INTEGER Value;
} NDAS_OEM_CODE, *PNDAS_OEM_CODE;

extern const NDASID_EXT_KEY	NDAS_ID_KEY_DEFAULT;

extern const NDAS_OEM_CODE NDAS_OEM_CODE_DEFAULT_SUPER;

extern const NDAS_OEM_CODE NDAS_OEM_CODE_SAMPLE;
extern const NDAS_OEM_CODE NDAS_OEM_CODE_DEFAULT;
extern const NDAS_OEM_CODE NDAS_OEM_CODE_RUTTER;
extern const NDAS_OEM_CODE NDAS_OEM_CODE_SEAGATE;
extern const NDAS_OEM_CODE NDAS_OEM_CODE_SOFT_NETDISK;

extern const NDASID_EXT_DATA NDAS_ID_EXTENSION_DEFAULT;
extern const NDASID_EXT_DATA NDAS_ID_EXTENSION_SEAGATE;
extern const NDASID_EXT_DATA NDAS_ID_EXTENSION_WINDOWS_RO;
extern const NDASID_EXT_DATA NDAS_ID_EXTENSION_AUTO_REGISTRY;
extern const NDASID_EXT_DATA NDAS_ID_EXTENSION_SOFT_NETDISK;

#endif