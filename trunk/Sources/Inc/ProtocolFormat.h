/***************************************************************************
 *
 *  ProtocolFormat.h
 *
 *    
 *
 *    Copyright (c) 2003 XiMeta, Inc. All rights reserved.
 *
 **************************************************************************/

#ifndef _PROTOCOL_FORMAT_H_
#define _PROTOCOL_FORMAT_H_

//
// Operation Codes.
//

// Host to Remote
#define NOP_H2R                     0x00
#define LOGIN_REQUEST               0x01
#define LOGOUT_REQUEST              0x02
#define TEXT_REQUEST                0x03
#define TASK_MANAGEMENT_REQUEST     0x04
#define SCSI_COMMAND                0x05
#define DATA_H2R                    0x06
#define IDE_COMMAND                 0x08
#define VENDER_SPECIFIC_COMMAND     0x0F    

// Remote to Host
#define NOP_R2H                     0x10
#define LOGIN_RESPONSE              0x11
#define LOGOUT_RESPONSE             0x12
#define TEXT_RESPONSE               0x13
#define TASK_MANAGEMENT_RESPONSE    0x14
#define SCSI_RESPONSE               0x15
#define DATA_R2H                    0x16
#define READY_TO_TRANSFER           0x17
#define IDE_RESPONSE                0x18
#define VENDER_SPECIFIC_RESPONSE    0x1F    

//
// Error code.
//
#define LANSCSI_RESPONSE_SUCCESS                0x00
#define LANSCSI_RESPONSE_RI_NOT_EXIST           0x10
#define LANSCSI_RESPONSE_RI_BAD_COMMAND         0x11
#define LANSCSI_RESPONSE_RI_COMMAND_FAILED      0x12
#define LANSCSI_RESPONSE_RI_VERSION_MISMATCH    0x13
#define LANSCSI_RESPONSE_RI_AUTH_FAILED         0x14
#define LANSCSI_RESPONSE_T_NOT_EXIST            0x20
#define LANSCSI_RESPONSE_T_BAD_COMMAND          0x21
#define LANSCSI_RESPONSE_T_COMMAND_FAILED       0x22
#define LANSCSI_RESPONSE_T_BROKEN_DATA          0x23

//
// Host To Remote PDU Format
//
typedef struct _LANSCSI_H2R_PDU_HEADER {
    uint8_t   Opocde;
    uint8_t   Flags;
    int16_t  Reserved1;
    
    uint32_t    HPID;
    int16_t  RPID;
    int16_t  CPSlot;
    uint32_t    DataSegLen;
    int16_t  AHSLen;
    int16_t  CSubPacketSeq;
    uint32_t    PathCommandTag;
    uint32_t    InitiatorTaskTag;
    uint32_t    DataTransferLength;
    uint32_t    TargetID;
    int64_t  Lun;
    
    uint32_t    Reserved2;
    uint32_t    Reserved3;
    uint32_t    Reserved4;
    uint32_t    Reserved5;
}__attribute__ ((packed)) LANSCSI_H2R_PDU_HEADER, *PLANSCSI_H2R_PDU_HEADER;

//
// Host To Remote PDU Format
//
typedef struct _LANSCSI_R2H_PDU_HEADER {
    uint8_t   Opocde;
    uint8_t   Flags;
    uint8_t   Response;
    
    uint8_t   Reserved1;
    
    uint32_t    HPID;
    int16_t  RPID;
    int16_t  CPSlot;
    uint32_t    DataSegLen;
    int16_t  AHSLen;
    int16_t  CSubPacketSeq;
    uint32_t    PathCommandTag;
    uint32_t    InitiatorTaskTag;
    uint32_t    DataTransferLength;
    uint32_t    TargetID;
    int64_t  Lun;
    
    uint32_t    Reserved2;
    uint32_t    Reserved3;
    uint32_t    Reserved4;
    uint32_t    Reserved5;
}__attribute__ ((packed)) LANSCSI_R2H_PDU_HEADER, *PLANSCSI_R2H_PDU_HEADER;


//
// Basic PDU.
//
typedef struct _LANSCSI_PDU {
    union {
        PLANSCSI_H2R_PDU_HEADER pH2RHeader;
        PLANSCSI_R2H_PDU_HEADER pR2HHeader;
    };
    char *                   pAHS;
    char *                   pHeaderDig;
    char *                   pDataSeg;
    char *                   pDataDig;
}__attribute__ ((packed)) LANSCSI_PDU, *PLANSCSI_PDU;


//
// Login Operation.
//

// Stages.
#define FLAG_SECURITY_PHASE         0
#define FLAG_LOGIN_OPERATION_PHASE  1
#define FLAG_FULL_FEATURE_PHASE     3

#define LOGOUT_PHASE                4

// Parameter Types.
#define PARAMETER_TYPE_TEXT			0
#define PARAMETER_TYPE_BINARY		1

#define LOGIN_FLAG_CSG_MASK     0x0C
#define LOGIN_FLAG_NSG_MASK     0x03


// Login Request.
typedef struct _LANSCSI_LOGIN_REQUEST_PDU_HEADER {
    uint8_t   Opocde;
    
    // Flags.
    union {
        struct {
#if defined(__LITTLE_ENDIAN__)
            uint8_t   NSG:2;
            uint8_t   CSG:2;
            uint8_t   FlagReserved:2;
            uint8_t   C:1;
            uint8_t   T:1;
#elif defined(__BIG_ENDIAN__)
            uint8_t   T:1;
            uint8_t   C:1;
            uint8_t   FlagReserved:2;
            uint8_t   CSG:2;
            uint8_t   NSG:2;
#else
            endian is not defined
#endif                
        };
        uint8_t   Flags;
    };
    
    int16_t  	Reserved1;
    uint32_t    HPID;
    int16_t  	RPID;
    int16_t  	CPSlot;
    uint32_t    DataSegLen;
    int16_t  	AHSLen;
    int16_t  	CSubPacketSeq;
    uint32_t    PathCommandTag;
    
    uint32_t    Reserved2;
    uint32_t    Reserved3;
    uint32_t    Reserved4;
    uint64_t 	Reserved5;

    uint8_t   	ParameterType;
    uint8_t   	ParameterVer;
    uint8_t   	VerMax;
    uint8_t   	VerMin;
    
    uint32_t    Reserved6;
    uint32_t    Reserved7;
    uint32_t    Reserved8;
}__attribute__ ((packed)) LANSCSI_LOGIN_REQUEST_PDU_HEADER, *PLANSCSI_LOGIN_REQUEST_PDU_HEADER;


// Login Reply.
typedef struct _LANSCSI_LOGIN_REPLY_PDU_HEADER {
    uint8_t   Opocde;
    
    // Flags.
    union {
        struct {
#if defined(__LITTLE_ENDIAN__)
            uint8_t   NSG:2;
            uint8_t   CSG:2;
            uint8_t   FlagReserved:2;
            uint8_t   C:1;
            uint8_t   T:1;
#elif defined(__BIG_ENDIAN__)
            uint8_t   T:1;
            uint8_t   C:1;
            uint8_t   FlagReserved:2;
            uint8_t   CSG:2;
            uint8_t   NSG:2;
#else
            endian is not defined
#endif
        };
        uint8_t   Flags;
    };

    uint8_t	Response;
    
    uint8_t   	Reserved1;
    
    uint32_t 	HPID;
    uint16_t  	RPID;
    uint16_t  	CPSlot;
    uint32_t    DataSegLen;
    uint16_t  	AHSLen;
    uint16_t  	CSubPacketSeq;
    uint32_t    PathCommandTag;

    uint32_t    Reserved2;
    uint32_t    Reserved3;
    uint32_t    Reserved4;
    uint64_t  	Reserved5;
    
    uint8_t   	ParameterType;
    uint8_t   	ParameterVer;
    uint8_t   	VerMax;
    uint8_t   	VerActive;
    
	uint16_t	Revision;
    uint16_t    Reserved6;
	
    uint32_t    Reserved7;
    uint32_t    Reserved8;
}__attribute__ ((packed)) LANSCSI_LOGIN_REPLY_PDU_HEADER, *PLANSCSI_LOGIN_REPLY_PDU_HEADER;


//
// Text Operation.
//

// Text Request.
typedef struct _LANSCSI_TEXT_REQUEST_PDU_HEADER {
    uint8_t   Opocde;
    
    // Flags.
    union {
        struct {
#if defined(__LITTLE_ENDIAN__)
            uint8_t   FlagReserved:7;
            uint8_t   F:1;
#elif defined(__BIG_ENDIAN__)
            uint8_t   F:1;
            uint8_t   FlagReserved:7;
#else
            endian is not defined
#endif
        };
        uint8_t   Flags;
    };
    
    int16_t  Reserved1;
    uint32_t    HPID;
    int16_t  RPID;
    int16_t  CPSlot;
    uint32_t    DataSegLen;
    int16_t  AHSLen;
    int16_t  CSubPacketSeq;
    uint32_t    PathCommandTag;

    uint32_t    Reserved2;
    uint32_t    Reserved3;
    uint32_t    Reserved4;
    int64_t  Reserved5;
    
    uint8_t   ParameterType;
    uint8_t   ParameterVer;
    int16_t  Reserved6;  
    
    uint32_t    Reserved7;
    uint32_t    Reserved8;
    uint32_t    Reserved9;
}__attribute__ ((packed)) LANSCSI_TEXT_REQUEST_PDU_HEADER, *PLANSCSI_TEXT_REQUEST_PDU_HEADER;

// Text Reply.
typedef struct _LANSCSI_TEXT_REPLY_PDU_HEADER {
    uint8_t   Opocde;
    
    // Flags.
    union {
        struct {
#if defined(__LITTLE_ENDIAN__)
            uint8_t   FlagReserved:7;
            uint8_t   F:1;
#elif defined(__BIG_ENDIAN__)
            uint8_t   F:1;
            uint8_t   FlagReserved:7;
#else
            endian is not defined
#endif
        };
        uint8_t   Flags;
    };

    uint8_t   Response;
    
    uint8_t   Reserved1;
    
    uint32_t    HPID;
    int16_t  RPID;
    int16_t  CPSlot;
    uint32_t    DataSegLen;
    int16_t  AHSLen;
    int16_t  CSubPacketSeq;
    uint32_t    PathCommandTag;
    
    uint32_t    Reserved2;
    uint32_t    Reserved3;
    uint32_t    Reserved4;
    int64_t  Reserved5;
    
    uint8_t   ParameterType;
    uint8_t   ParameterVer;
    int16_t  Reserved6;
    
    uint32_t    Reserved7;
    uint32_t    Reserved8;
    uint32_t    Reserved9;
}__attribute__ ((packed)) LANSCSI_TEXT_REPLY_PDU_HEADER, *PLANSCSI_TEXT_REPLY_PDU_HEADER;


//
// Logout Operation.
//

// Logout Request.
typedef struct _LANSCSI_LOGOUT_REQUEST_PDU_HEADER {
    uint8_t   Opocde;
    
    // Flags.
    union {
        struct {
#if defined(__LITTLE_ENDIAN__)
            uint8_t   FlagReserved:7;
            uint8_t   F:1;
#elif defined(__BIG_ENDIAN__)
            uint8_t   F:1;
            uint8_t   FlagReserved:7;
#else
            endian is not defined
#endif
        };
        uint8_t   Flags;
    };
    
    int16_t  Reserved1;
    unsigned    HPID;
    int16_t  RPID;
    int16_t  CPSlot;
    unsigned    DataSegLen;
    int16_t  AHSLen;
    int16_t  CSubPacketSeq;
    unsigned    PathCommandTag;

    unsigned    Reserved2;  
    unsigned    Reserved3;
    unsigned    Reserved4;
    int64_t  Reserved5;
    
    unsigned    Reserved6;
    unsigned    Reserved7;
    unsigned    Reserved8;
    unsigned    Reserved9;
}__attribute__ ((packed)) LANSCSI_LOGOUT_REQUEST_PDU_HEADER, *PLANSCSI_LOGOUT_REQUEST_PDU_HEADER;

// Logout Reply.
typedef struct _LANSCSI_LOGOUT_REPLY_PDU_HEADER {
    uint8_t   Opocde;
    
    // Flags.
    union {
        struct {
#if defined(__LITTLE_ENDIAN__)
            uint8_t   FlagReserved:7;
            uint8_t   F:1;
#elif defined(__BIG_ENDIAN__)
            uint8_t   F:1;
            uint8_t   FlagReserved:7;
#else
            endian is not defined
#endif
        };
        uint8_t   Flags;
    };

    uint8_t   Response;
    
    uint8_t   Reserved1;
    
    unsigned    HPID;
    int16_t  RPID;
    int16_t  CPSlot;
    unsigned    DataSegLen;
    int16_t  AHSLen;
    int16_t  CSubPacketSeq;
    unsigned    PathCommandTag;

    unsigned    Reserved2;  
    unsigned    Reserved3;
    unsigned    Reserved4;
    int64_t  Reserved5;

    unsigned    Reserved6;
    unsigned    Reserved7;
    unsigned    Reserved8;
    unsigned    Reserved9;
}__attribute__ ((packed)) LANSCSI_LOGOUT_REPLY_PDU_HEADER, *PLANSCSI_LOGOUT_REPLY_PDU_HEADER;


//
// IDE Operation.
//

// IDE Request.
typedef struct _LANSCSI_IDE_REQUEST_PDU_HEADER_V1 {
    uint8_t   Opocde;
    
    // Flags.
    union {
        struct {
#if defined(__LITTLE_ENDIAN__)
            uint8_t   FlagReserved:5;
            uint8_t   W:1;
            uint8_t   R:1;
            uint8_t   F:1;
#elif defined(__BIG_ENDIAN__)
            uint8_t   F:1;
            uint8_t   R:1;
            uint8_t   W:1;
            uint8_t   FlagReserved:5;
#else
            endian is not defined
#endif
        };
        uint8_t   Flags;
    };
    
    int16_t  Reserved1;
    unsigned    HPID;
    int16_t  RPID;
    int16_t  CPSlot;
    unsigned    DataSegLen;
    int16_t  AHSLen;
    int16_t  CSubPacketSeq;
    unsigned    PathCommandTag;
    unsigned    InitiatorTaskTag;
    unsigned    DataTransferLength; 
    unsigned    TargetID;
    int64_t  LUN;
    
    uint8_t   Reserved2;
    uint8_t   Feature;
    uint8_t   SectorCount_Prev;
    uint8_t   SectorCount_Curr;
    uint8_t   LBALow_Prev;
    uint8_t   LBALow_Curr;
    uint8_t   LBAMid_Prev;
    uint8_t   LBAMid_Curr;
    uint8_t   LBAHigh_Prev;
    uint8_t   LBAHigh_Curr;
    uint8_t   Reserved3;
    
    union {
        uint8_t   Device;
        struct {
#if defined(__LITTLE_ENDIAN__)
            uint8_t   LBAHeadNR:4;
            uint8_t   DEV:1;
            uint8_t   obs1:1;
            uint8_t   LBA:1;
            uint8_t   obs2:1;
#elif defined(__BIG_ENDIAN__)
            uint8_t   obs2:1;
            uint8_t   LBA:1;
            uint8_t   obs1:1;
            uint8_t   DEV:1;
            uint8_t   LBAHeadNR:4;
#else
            endian is not defined
#endif
        };
    };

    uint8_t   Reserved4;
    uint8_t   Command;
    int16_t  Reserved5;

}__attribute__ ((packed)) LANSCSI_IDE_REQUEST_PDU_HEADER_V1, *PLANSCSI_IDE_REQUEST_PDU_HEADER_V1;


// IDE Reply.
typedef struct _LANSCSI_IDE_REPLY_PDU_HEADER_V1 {
    uint8_t   Opocde;
    
    // Flags.
    union {
        struct {
#if defined(__LITTLE_ENDIAN__)
            uint8_t   FlagReserved:5;
            uint8_t   W:1;
            uint8_t   R:1;
            uint8_t   F:1;
#elif defined(__BIG_ENDIAN__)
            uint8_t   F:1;
            uint8_t   R:1;
            uint8_t   W:1;
            uint8_t   FlagReserved:5;
#else
            endian is not defined
#endif
        };
        uint8_t   Flags;
    };

    uint8_t   Response;
    
    uint8_t   Status;
    
    unsigned    HPID;
    int16_t  RPID;
    int16_t  CPSlot;
    unsigned    DataSegLen;
    int16_t  AHSLen;
    int16_t  CSubPacketSeq;
    unsigned    PathCommandTag;
    unsigned    InitiatorTaskTag;
    unsigned    DataTransferLength; 
    unsigned    TargetID;
    int64_t  LUN;
    
    uint8_t   Reserved2;
    uint8_t   Feature;
    uint8_t   SectorCount_Prev;
    uint8_t   SectorCount_Curr;
    uint8_t   LBALow_Prev;
    uint8_t   LBALow_Curr;
    uint8_t   LBAMid_Prev;
    uint8_t   LBAMid_Curr;
    uint8_t   LBAHigh_Prev;
    uint8_t   LBAHigh_Curr;
    uint8_t   Reserved3;

    union {
        uint8_t   Device;
        struct {
#if defined(__LITTLE_ENDIAN__)
            uint8_t   LBAHeadNR:4;
            uint8_t   DEV:1;
            uint8_t   obs1:1;
            uint8_t   LBA:1;
            uint8_t   obs2:1;
#elif defined(__BIG_ENDIAN__)
            uint8_t   obs2:1;
            uint8_t   LBA:1;
            uint8_t   obs1:1;
            uint8_t   DEV:1;
            uint8_t   LBAHeadNR:4;
#else
            endian is not defined
#endif
        };
    };

    uint8_t   Reserved4;
    uint8_t   Command;
    int16_t  Reserved5;
}__attribute__ ((packed)) LANSCSI_IDE_REPLY_PDU_HEADER_V1, *PLANSCSI_IDE_REPLY_PDU_HEADER_V1;

// IDE Request.
typedef struct _LANSCSI_IDE_REQUEST_PDU_HEADER_V2 {
	uint8_t	Opocde;
	
	// Flags.
	union {
		struct {
#if defined(__LITTLE_ENDIAN__)
            uint8_t   FlagReserved:5;
            uint8_t   W:1;
            uint8_t   R:1;
            uint8_t   F:1;
#elif defined(__BIG_ENDIAN__)
            uint8_t   F:1;
            uint8_t   R:1;
            uint8_t   W:1;
            uint8_t   FlagReserved:5;
#else
            endian is not defined
#endif
		};
		uint8_t	Flags;
	};
	
	uint16_t	Reserved1;
	uint32_t	HPID;
	uint16_t	RPID;
	uint16_t	CPSlot;
	uint32_t	DataSegLen;
	uint16_t	AHSLen;
	uint16_t	CSubPacketSeq;
	uint32_t	PathCommandTag;
	uint32_t	InitiatorTaskTag;
	uint32_t	DataTransferLength;	
	uint32_t	TargetID;
	uint64_t	LUN;
	
#if defined(__LITTLE_ENDIAN__)
	uint32_t COM_Reserved : 2;
	uint32_t COM_TYPE_E : 1;
	uint32_t COM_TYPE_R : 1;
	uint32_t COM_TYPE_W : 1;
	uint32_t COM_TYPE_D_P : 1;
	uint32_t COM_TYPE_K : 1;
	uint32_t COM_TYPE_P : 1;
	uint32_t COM_LENG : 24;	
#elif defined(__BIG_ENDIAN__)
	uint32_t COM_TYPE_P : 1;
	uint32_t COM_TYPE_K : 1;
	uint32_t COM_TYPE_D_P : 1;
	uint32_t COM_TYPE_W : 1;
	uint32_t COM_TYPE_R : 1;
	uint32_t COM_TYPE_E : 1;
	uint32_t COM_Reserved : 2;
	uint32_t COM_LENG : 24;
#else
	endian is not defined
#endif
		
	uint8_t	Feature_Prev;
	uint8_t	Feature_Curr;
	uint8_t	SectorCount_Prev;
	uint8_t	SectorCount_Curr;
	uint8_t	LBALow_Prev;
	uint8_t	LBALow_Curr;
	uint8_t	LBAMid_Prev;
	uint8_t	LBAMid_Curr;
	uint8_t	LBAHigh_Prev;
	uint8_t	LBAHigh_Curr;
	uint8_t	Command;
	
	union {
		uint8_t	Device;
		struct {
#if defined(__LITTLE_ENDIAN__)
            uint8_t   LBAHeadNR:4;
            uint8_t   DEV:1;
            uint8_t   obs1:1;
            uint8_t   LBA:1;
            uint8_t   obs2:1;
#elif defined(__BIG_ENDIAN__)
            uint8_t   obs2:1;
            uint8_t   LBA:1;
            uint8_t   obs1:1;
            uint8_t   DEV:1;
            uint8_t   LBAHeadNR:4;
#else
            endian is not defined
#endif
		};
	};
	
}__attribute__ ((packed)) LANSCSI_IDE_REQUEST_PDU_HEADER_V2, *PLANSCSI_IDE_REQUEST_PDU_HEADER_V2;

// IDE Reply.
typedef struct _LANSCSI_IDE_REPLY_PDU_HEADER_V2 {
	uint8_t	Opocde;
	
	// Flags.
	union {
		struct {
#if defined(__LITTLE_ENDIAN__)
            uint8_t   FlagReserved:5;
            uint8_t   W:1;
            uint8_t   R:1;
            uint8_t   F:1;
#elif defined(__BIG_ENDIAN__)
            uint8_t   F:1;
            uint8_t   R:1;
            uint8_t   W:1;
            uint8_t   FlagReserved:5;
#else
            endian is not defined
#endif
		};
		uint8_t	Flags;
	};
	
	uint8_t	Response;
	
	uint8_t	Status;
	
	uint32_t	HPID;
	uint16_t	RPID;
	uint16_t	CPSlot;
	uint32_t	DataSegLen;
	uint16_t	AHSLen;
	uint16_t 	CSubPacketSeq;
	uint32_t	PathCommandTag;
	uint32_t	InitiatorTaskTag;
	uint32_t	DataTransferLength;	
	uint32_t	TargetID;
	uint64_t	LUN;
	
#if defined(__LITTLE_ENDIAN__)
	uint32_t COM_Reserved : 2;
	uint32_t COM_TYPE_E : 1;
	uint32_t COM_TYPE_R : 1;
	uint32_t COM_TYPE_W : 1;
	uint32_t COM_TYPE_D_P : 1;
	uint32_t COM_TYPE_K : 1;
	uint32_t COM_TYPE_P : 1;
	uint32_t COM_LENG : 24;	
#elif defined(__BIG_ENDIAN__)
	uint32_t COM_TYPE_P : 1;
	uint32_t COM_TYPE_K : 1;
	uint32_t COM_TYPE_D_P : 1;
	uint32_t COM_TYPE_W : 1;
	uint32_t COM_TYPE_R : 1;
	uint32_t COM_TYPE_E : 1;
	uint32_t COM_Reserved : 2;
	uint32_t COM_LENG : 24;
#else
	endian is not defined
#endif
		
	uint8_t	Feature_Prev;
	uint8_t	Feature_Curr;
	uint8_t	SectorCount_Prev;
	uint8_t	SectorCount_Curr;
	uint8_t	LBALow_Prev;
	uint8_t	LBALow_Curr;
	uint8_t	LBAMid_Prev;
	uint8_t	LBAMid_Curr;
	uint8_t	LBAHigh_Prev;
	uint8_t	LBAHigh_Curr;
	uint8_t	Command;
	
	union {
		uint8_t	Device;
		struct {
#if defined(__LITTLE_ENDIAN__)
            uint8_t   LBAHeadNR:4;
            uint8_t   DEV:1;
            uint8_t   obs1:1;
            uint8_t   LBA:1;
            uint8_t   obs2:1;
#elif defined(__BIG_ENDIAN__)
            uint8_t   obs2:1;
            uint8_t   LBA:1;
            uint8_t   obs1:1;
            uint8_t   DEV:1;
            uint8_t   LBAHeadNR:4;
#else
            endian is not defined
#endif
		};
	};
	
}__attribute__ ((packed)) LANSCSI_IDE_REPLY_PDU_HEADER_V2, *PLANSCSI_IDE_REPLY_PDU_HEADER_V2;

//
// Vender Specific Operation.
//

#define	NKC_VENDER_ID				0x0001
#define	VENDER_OP_CURRENT_VERSION	0x01

#define	VENDER_OP_SET_MAX_RET_TIME	0x01
#define VENDER_OP_SET_MAX_CONN_TIME	0x02
#define VENDER_OP_SET_SUPERVISOR_PW	0x11
#define VENDER_OP_SET_USER_PW		0x12
#define VENDER_OP_SET_ENC_OPT		0x13
#define VENDER_OP_SET_STANBY_TIMER  0x14
#define VENDER_OP_RESET				0xFF
#define VENDER_OP_RESET2			0xFE

#define	VENDER_OP_GET_MAX_RET_TIME	0x03
#define	VENDER_OP_GET_MAX_CONN_TIME	0x04

#define VENDER_OP_SET_SEMA			0x05
#define VENDER_OP_FREE_SEMA			0x06
#define VENDER_OP_GET_SEMA			0x07
#define VENDER_OP_OWNER_SEMA		0x08
#define VENDER_OP_SET_DYNAMIC_MAX_CONN_TIME			0x0a
#define VENDER_OP_SET_DELAY			0x16
#define VENDER_OP_GET_DELAY			0x17
#define VENDER_OP_SET_DYNAMIC_MAX_RET_TIME			0x18
#define VENDER_OP_GET_DYNAMIC_MAX_RET_TIME			0x19
#define VENDER_OP_SET_D_ENC_OPT						0x1A
#define VENDER_OP_GET_D_ENC_OPT						0x1B

#define VENDER_OP_SET_EEP			0x2A
#define VENDER_OP_GET_EEP			0x2B
#define VENDER_OP_GET_STANBY_TIMER	0x15

#endif /* _PROTOCOL_FORMAT_H_ */
