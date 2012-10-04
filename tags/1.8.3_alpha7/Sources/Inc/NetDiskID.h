#ifndef NDUILIB_NETDISKID_H
#define NDUILIB_NETDISKID_H

#define NDAS_VID_NONE  0x00
#define NDAS_VID_DEFAULT 0x01	// Assume NDAS_VID_DEFAULT if NDAS_VID_NONE
#define NDAS_VID_SEAGATE 0x41

//
//	Keys to enc/decrypt NetDisk ID/Write Password Version 1 05/17/2003
//  made by hootch
//

extern u_char	NDIDV1Key1[8] ;
extern u_char	NDIDV1Key2[8] ;
//extern u_char	NDIDV1VID ;
//extern u_char	NDIDV1Rsv[2] ;
//extern u_char	NDIDV1Rnd ;
// NetDisk Login Password for XiMeta samples : 00:F0:0F:xx:xx:xx
//extern u_char	NDIDV1PWSample[8] ;
// NetDisk Login Password for XiMeta product
//extern u_char	NDIDV1PW[8] ;
//extern u_char	NDIDV1SPW[8] ;

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

extern const NDASID_EXT_DATA NDAS_ID_EXTENSION_DEFAULT;
extern const NDASID_EXT_DATA NDAS_ID_EXTENSION_SEAGATE;

#endif