#ifndef NDUILIB_NETDISKID_H
#define NDUILIB_NETDISKID_H

#define NDAS_VID_NONE  0x00
#define NDAS_VID_DEFAULT 0x01	// Assume NDAS_VID_DEFAULT if NDAS_VID_NONE
#define NDAS_VID_SEAGATE 0x41

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

extern const NDASID_EXT_DATA NDAS_ID_EXTENSION_DEFAULT;
extern const NDASID_EXT_DATA NDAS_ID_EXTENSION_SEAGATE;

#endif