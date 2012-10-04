/***************************************************************************
 *
 *  BinaryParameters.h
 *
 *    
 *
 *    Copyright (c) 2003 XiMeta, Inc. All rights reserved.
 *
 **************************************************************************/

#ifndef _BINARY_PARAMETERS_H_
#define _BINARY_PARAMETERS_H_

//
// Parameter Types.
//
#define PARAMETER_CURRENT_VERSION   0

#define BIN_PARAM_TYPE_SECURITY     0x1
#define BIN_PARAM_TYPE_NEGOTIATION  0x2
#define BIN_PARAM_TYPE_TARGET_LIST  0x3
#define BIN_PARAM_TYPE_TARGET_DATA  0x4

#define BIN_PARAM_SIZE_LOGIN_FIRST_REQUEST      4
#define BIN_PARAM_SIZE_LOGIN_SECOND_REQUEST     8
#define BIN_PARAM_SIZE_LOGIN_THIRD_REQUEST      32
#define BIN_PARAM_SIZE_LOGIN_FOURTH_REQUEST     20
#define BIN_PARAM_SIZE_TEXT_TARGET_LIST_REQUEST 4
#define BIN_PARAM_SIZE_TEXT_TARGET_DATA_REQUEST 16

#define BIN_PARAM_SIZE_REPLY                    36

typedef struct _BIN_PARAM {
    uint8_t   ParamType;
    uint8_t   Reserved1;
    int16_t  Reserved2;
}__attribute__ ((packed)) BIN_PARAM, *PBIN_PARAM;

//
// BIN_PARAM_TYPE_SECURITY.
//

#define LOGIN_TYPE_NORMAL       0x00
#define LOGIN_TYPE_DISCOVERY    0xFF

//
// CHAP Auth Paramter.
//
#define AUTH_METHOD_CHAP        0x0001

#define FIRST_TARGET_RO_USER    0x00000001
#define FIRST_TARGET_RW_USER    0x00010001
#define SECOND_TARGET_RO_USER   0x00000002
#define SECOND_TARGET_RW_USER   0x00020002
#define SUPERVISOR              0xFFFFFFFF

typedef struct _AUTH_PARAMETER_CHAP {
    unsigned    CHAP_A;
    unsigned    CHAP_I;
    unsigned    CHAP_N;
    unsigned    CHAP_R[4];
    unsigned    CHAP_C[1];
}__attribute__ ((packed)) AUTH_PARAMETER_CHAP, *PAUTH_PARAMETER_CHAP;

#define HASH_ALGORITHM_MD5          0x00000005
#define CHAP_MAX_CHALLENGE_LENGTH   4

typedef struct _BIN_PARAM_SECURITY {
    uint8_t   ParamType;
    uint8_t   LoginType;
    int16_t  AuthMethod;
    union {
        uint8_t   AuthParamter[1];    // Variable Length.
        AUTH_PARAMETER_CHAP ChapParam;
    };
}__attribute__ ((packed)) BIN_PARAM_SECURITY, *PBIN_PARAM_SECURITY;

//
// BIN_PARAM_TYPE_NEGOTIATION.  
//
typedef struct _BIN_PARAM_NEGOTIATION {
    uint8_t           ParamType;
    uint8_t           HWType;
    uint8_t           HWVersion;
    uint8_t           Reserved;
    unsigned        NRSlot;
    unsigned        MaxBlocks;
    unsigned        MaxTargetID;
    unsigned        MaxLUNID;
    uint16_t HeaderEncryptAlgo;
    uint16_t HeaderDigestAlgo;
    uint16_t DataEncryptAlgo;
    uint16_t DataDigestAlgo;
}__attribute__ ((packed)) BIN_PARAM_NEGOTIATION, *PBIN_PARAM_NEGOTIATION;

#define HW_TYPE_ASIC            0x00
#define HW_TYPE_UNSPECIFIED     0xFF

#define HW_VERSION_CURRENT      0x00
#define HW_VERSION_UNSPECIFIIED 0xFF

//
// BIN_PARAM_TYPE_TARGET_LIST.
//
typedef struct _BIN_PARAM_TARGET_LIST_ELEMENT {
    uint32_t TargetID;
    uint8_t           NRRWHost;
    uint8_t           NRROHost;
    int16_t          Reserved1;
    uint64_t TargetData;
}__attribute__ ((packed)) BIN_PARAM_TARGET_LIST_ELEMENT, *PBIN_PARAM_TARGET_LIST_ELEMENT;

typedef struct _BIN_PARAM_TARGET_LIST {
    uint8_t   ParamType;
    uint8_t   NRTarget;
    int16_t  Reserved1;
    BIN_PARAM_TARGET_LIST_ELEMENT   PerTarget[1];
}__attribute__ ((packed)) BIN_PARAM_TARGET_LIST, *PBIN_PARAM_TARGET_LIST;

//
// BIN_PARAM_TYPE_TARGET_DATA.
//

#define PARAMETER_OP_GET    0x00
#define PARAMETER_OP_SET    0xFF

typedef struct _BIN_PARAM_TARGET_DATA {
    uint8_t  ParamType;
    uint8_t  GetOrSet;
    uint16_t Reserved1;
    uint32_t TargetID;
    uint64_t TargetData;
}__attribute__ ((packed)) BIN_PARAM_TARGET_DATA, *PBIN_PARAM_TARGET_DATA;


#endif /* !_BINARY_PARAMETERS_H_ */
