/***************************************************************************
 *
 *  LanScsiProtocol.c
 *
 *    
 *
 *    Copyright (c) 2003 XiMeta, Inc. All rights reserved.
 *
 **************************************************************************/

#ifdef KERNEL

#include <IOKit/IOLib.h>
#include <IOKit/scsi/SCSICommandOperationCodes.h>
#include <IOKit/scsi/SCSITask.h>

//  #define bool	int
//  #define true	1
//  #define false  	0

//  #define fprintf(...) 
//  #define DebugPrint(...)

//  #include <sys/param.h>
//  #include <sys/systm.h>
//  #include <sys/proc.h>
//  #include <sys/mount.h>
//  #include <sys/kernel.h>
//  #include <sys/mbuf.h>
//  #include <sys/malloc.h>
//  #include <sys/vnode.h>
//  #include <sys/domain.h>
//  #include <sys/protosw.h>
//  #include <sys/socket.h>
//  #include <sys/socketvar.h>
//  #include <sys/syslog.h>
 // #include <sys/tprintf.h>
 // #include <machine/spl.h>

  #include "LanScsiSocket.h"

	#include "Utilities.h"

 #include "DebugPrint.h"

#else	/* if KERNEL */

  #include <IOKit/IOKitLib.h>
  #include <IOKit/scsi/SCSICommandOperationCodes.h>
  #include <IOKit/scsi/SCSITask.h>

  #include <ApplicationServices/ApplicationServices.h>
  #include <unistd.h>

  #include "DebugPrint.h"

//#define IOLog(...)

#endif	/* if KERNEL else */

#include <sys/types.h>
#include <sys/socket.h>

#include "lpx.h"
#include "LanScsiProtocol.h"

#include "BinaryParameters.h"
#include "ProtocolFormat.h"

#include "hash.h"
#include "hdreg.h"
#include "cdb.h"

#ifndef PCHAR
#define PCHAR   char *
#endif

/***************************************************************************
 *
 *  Helper function prototypes
 *
 **************************************************************************/
#ifdef KERNEL
inline size_t RecvIt(xi_socket_t sock, char *buf, size_t size );
inline size_t SendIt(xi_socket_t sock, char *buf, size_t size );
#else 	/* if KERNEL */
inline int RecvIt(int sock, char *buf, int size );
inline int SendIt(int sock, char *buf, int size );
#endif 	/* if KERNEL else */
int ReadReply(struct LanScsiSession *Session, char *pBuffer, PLANSCSI_PDU pPdu);
int SendRequest(struct LanScsiSession *Session, PLANSCSI_PDU pPdu);
int Lba_capacity_is_ok(struct hd_driveid *id );
void ConvertString(char * result, char * source, int size );


/***************************************************************************
 *
 *  APIs
 *
 **************************************************************************/

int
LoginLanscsi_V1(
				struct LanScsiSession *Session, 
				uint8_t LoginType, 
				uint32_t UserID, 
				char *Key
				)
{
    char                               	pduBuffer[MAX_REQUEST_SIZE];
    PLANSCSI_LOGIN_REQUEST_PDU_HEADER	loginRequestPdu;
    PLANSCSI_LOGIN_REPLY_PDU_HEADER     loginReplyHeader;
    PBIN_PARAM_SECURITY                 paramSecu;
    PBIN_PARAM_NEGOTIATION              paramNego;
    PAUTH_PARAMETER_CHAP                paramChap;
    LANSCSI_PDU                         pdu;
    uint32_t                            subSequence;
    int32_t                             result;
    uint32_t                            CHAP_I;
    uint32_t							CHAP_C;
    
    Session->HPID = 0; //random();
    Session->SessionPhase = FLAG_SECURITY_PHASE;
    Session->Tag = 0;
    
    subSequence = 0;

    //
    // First Packet.
    //
    bzero(pduBuffer, MAX_REQUEST_SIZE);

    loginRequestPdu = (PLANSCSI_LOGIN_REQUEST_PDU_HEADER)pduBuffer;

    loginRequestPdu->Opocde = LOGIN_REQUEST;
    loginRequestPdu->HPID = htonl(Session->HPID);
    loginRequestPdu->DataSegLen = htonl(BIN_PARAM_SIZE_LOGIN_FIRST_REQUEST);
    loginRequestPdu->CSubPacketSeq = htons(subSequence);
    loginRequestPdu->PathCommandTag = htonl(Session->Tag);
    loginRequestPdu->ParameterType = PARAMETER_TYPE_BINARY;
    loginRequestPdu->ParameterVer = PARAMETER_CURRENT_VERSION;
    loginRequestPdu->VerMax = HW_VERSION_1_0;
    loginRequestPdu->VerMin = 0;
    
    DebugPrint(4, false, "LoginLanscsi_V1: loginRequestPdu->CSubPacketSeq = %d\n", loginRequestPdu->CSubPacketSeq);

    paramSecu = (PBIN_PARAM_SECURITY)&pduBuffer[sizeof(LANSCSI_LOGIN_REQUEST_PDU_HEADER)];

    paramSecu->ParamType = BIN_PARAM_TYPE_SECURITY;
    paramSecu->LoginType = LoginType;
    paramSecu->AuthMethod = htons(AUTH_METHOD_CHAP);

    // Send Request.
    pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)loginRequestPdu;
    pdu.pDataSeg = (char *)paramSecu;

    if(SendRequest(Session, &pdu) != 0) {
        DebugPrint(1, true, "LoginLanscsi_V1: Send First Request \n");
        return -3;
    }

    // Read Request.
    result = ReadReply(Session, (char *)pduBuffer, &pdu);
    if(result < 0) {
		DebugPrint(2, false, "LoginLanscsi_V1: First Can't Read Reply.\n");
        return -3;
    }

    // Check Request Header.
    loginReplyHeader = (PLANSCSI_LOGIN_REPLY_PDU_HEADER)pdu.pR2HHeader;

#if 0
    if(loginReplyHeader->Opocde != LOGIN_RESPONSE)
//        IOLog("loginReplyHeader->Opocde = %d LOGIN_RESPONSE = %d\n",  loginReplyHeader->Opocde, LOGIN_RESPONSE);
        
    if(loginReplyHeader->T != 0)
//        IOLog("2\n");

    if(loginReplyHeader->CSG != FLAG_SECURITY_PHASE)
//        IOLog("3\n");

    if(loginReplyHeader->NSG != FLAG_SECURITY_PHASE)
//        IOLog("4\n");

    if(loginReplyHeader->VerActive > HW_VERSION_1_0)
//        IOLog("5\n");

    if(loginReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
//        IOLog("6\n");

    if(loginReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)
//        IOLog("7\n");
#endif /* if 0 */

    if((loginReplyHeader->Opocde != LOGIN_RESPONSE)
       || (loginReplyHeader->T != 0)
       || (loginReplyHeader->CSG != FLAG_SECURITY_PHASE)
       || (loginReplyHeader->NSG != FLAG_SECURITY_PHASE)
       || (loginReplyHeader->VerActive > HW_VERSION_1_0)
       || (loginReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
       || (loginReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {

        DebugPrint(2, false, "LoginLanscsi_V1: BAD First Reply Header.\n");
        return -1;
    }

    if(loginReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		DebugPrint(2, false, "LoginLanscsi_V1: First Failed.\n");
        return -1;
    }

    // Store Data.
    Session->RPID = ntohs(loginReplyHeader->RPID);

    paramSecu = (PBIN_PARAM_SECURITY)pdu.pDataSeg;
    DebugPrint(4, false, "LoginLanscsi_V1: Version %d Auth %d\n",
           loginReplyHeader->VerActive,
           ntohs(paramSecu->AuthMethod)
           );

    //
    // Second Packet.
    //
    bzero(pduBuffer, MAX_REQUEST_SIZE);

    loginRequestPdu = (PLANSCSI_LOGIN_REQUEST_PDU_HEADER)pduBuffer;

    loginRequestPdu->Opocde = LOGIN_REQUEST;
    loginRequestPdu->HPID = htonl(Session->HPID);
    loginRequestPdu->RPID = htons(Session->RPID);
    loginRequestPdu->DataSegLen = htonl(BIN_PARAM_SIZE_LOGIN_SECOND_REQUEST);
    loginRequestPdu->CSubPacketSeq = htons(++subSequence);
    loginRequestPdu->PathCommandTag = htonl(Session->Tag);
    loginRequestPdu->ParameterType = PARAMETER_TYPE_BINARY;
    loginRequestPdu->ParameterVer = PARAMETER_CURRENT_VERSION;

    DebugPrint(4, false, "LoginLanscsi_V1: loginRequestPdu->CSubPacketSeq = %d iSubSequence = %d\n", loginRequestPdu->CSubPacketSeq, subSequence);


    paramSecu = (PBIN_PARAM_SECURITY)&pduBuffer[sizeof(LANSCSI_LOGIN_REQUEST_PDU_HEADER)];

    paramSecu->ParamType = BIN_PARAM_TYPE_SECURITY;
    paramSecu->LoginType = LoginType;
    paramSecu->AuthMethod = htons(AUTH_METHOD_CHAP);

    paramChap = (PAUTH_PARAMETER_CHAP)paramSecu->AuthParamter;
    paramChap->CHAP_A = ntohl(HASH_ALGORITHM_MD5);

    // Send Request.
    pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)loginRequestPdu;
    pdu.pDataSeg = (char *)paramSecu;

    if(SendRequest(Session, &pdu) != 0) {
        DebugPrint(1, true, "LoginLanscsi_V1: Send Second Request ");
        return -3;
    }

    // Read Request.
    result = ReadReply(Session, (char *)pduBuffer, &pdu);
    if(result < 0) {
        DebugPrint(2, false, "LoginLanscsi_V1: Second Can't Read Reply.\n");
        return -3;
    }

    // Check Request Header.
    loginReplyHeader = (PLANSCSI_LOGIN_REPLY_PDU_HEADER)pdu.pR2HHeader;
    if((loginReplyHeader->Opocde != LOGIN_RESPONSE)
       || (loginReplyHeader->T != 0)
       || (loginReplyHeader->CSG != FLAG_SECURITY_PHASE)
       || (loginReplyHeader->NSG != FLAG_SECURITY_PHASE)
       || (loginReplyHeader->VerActive > HW_VERSION_1_0)
       || (loginReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
       || (loginReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {

        DebugPrint(2, false, "LoginLanscsi_V1: BAD Second Reply Header.\n");
        return -1;
    }

    if(loginReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
        DebugPrint(2, false, "LoginLanscsi_V1: Second Failed.\n");
        return -1;
    }

    // Check Data segment.
    if((ntohl(loginReplyHeader->DataSegLen) < BIN_PARAM_SIZE_REPLY)    // Minus AuthParamter[1]
       || (pdu.pDataSeg == NULL)) {

        DebugPrint(2, false, "LoginLanscsi_V1: BAD Second Reply Data.\n");
        return -1;
    }
    paramSecu = (PBIN_PARAM_SECURITY)pdu.pDataSeg;
    if(paramSecu->ParamType != BIN_PARAM_TYPE_SECURITY
       || paramSecu->LoginType != LoginType
       || paramSecu->AuthMethod != htons(AUTH_METHOD_CHAP)) {

        DebugPrint(2, false, "LoginLanscsi_V1: BAD Second Reply Parameters.\n");
        return -1;
    }

    // Store Challenge.
    paramChap = &paramSecu->ChapParam;
    CHAP_I = ntohl(paramChap->CHAP_I);
    Session->CHAP_C = ntohl(paramChap->CHAP_C[0]);

    DebugPrint(2, false, "LoginLanscsi_V1: Hash %x, Challenge %x CHAP_I = %x\n",
           ntohl(paramChap->CHAP_A),
           Session->CHAP_C,
           CHAP_I
           );

    //
    // Third Packet.
    //
    bzero(pduBuffer, MAX_REQUEST_SIZE);

    loginRequestPdu = (PLANSCSI_LOGIN_REQUEST_PDU_HEADER)pduBuffer;
    loginRequestPdu->Opocde = LOGIN_REQUEST;
    loginRequestPdu->T = 1;
    loginRequestPdu->CSG = FLAG_SECURITY_PHASE;
    loginRequestPdu->NSG = FLAG_LOGIN_OPERATION_PHASE;
    loginRequestPdu->HPID = htonl(Session->HPID);
    loginRequestPdu->RPID = htons(Session->RPID);
    loginRequestPdu->DataSegLen = htonl(BIN_PARAM_SIZE_LOGIN_THIRD_REQUEST);
    loginRequestPdu->CSubPacketSeq = htons(++subSequence);
    loginRequestPdu->PathCommandTag = htonl(Session->Tag);
    loginRequestPdu->ParameterType = PARAMETER_TYPE_BINARY;
    loginRequestPdu->ParameterVer = PARAMETER_CURRENT_VERSION;

    paramSecu = (PBIN_PARAM_SECURITY)&pduBuffer[sizeof(LANSCSI_LOGIN_REQUEST_PDU_HEADER)];

    paramSecu->ParamType = BIN_PARAM_TYPE_SECURITY;
    paramSecu->LoginType = LoginType;
    paramSecu->AuthMethod = htons(AUTH_METHOD_CHAP);

    paramChap = (PAUTH_PARAMETER_CHAP)paramSecu->AuthParamter;
    paramChap->CHAP_A = htonl(HASH_ALGORITHM_MD5);
    paramChap->CHAP_I = htonl(CHAP_I);
    paramChap->CHAP_N = htonl(UserID);

	CHAP_C = NDASSwap32LittleToHost(Session->CHAP_C);
 
    Hash32To128((unsigned char*)&CHAP_C, (unsigned char*)paramChap->CHAP_R, (uint8_t *)Key);

    // Send Request.
    pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)loginRequestPdu;
    pdu.pDataSeg = (char *)paramSecu;

    if(SendRequest(Session, &pdu) != 0) {
        DebugPrint(1, true, "LoginLanscsi_V1: Send Third Request ");
        return -3;
    }

    // Read Request.
    result = ReadReply(Session, (char *)pduBuffer, &pdu);
    if(result < 0) {
        DebugPrint(1, false, "LoginLanscsi_V1: Second Can't Read Reply.\n");
        return -3;
    }

    // Check Request Header.
    loginReplyHeader = (PLANSCSI_LOGIN_REPLY_PDU_HEADER)pdu.pR2HHeader;
    if((loginReplyHeader->Opocde != LOGIN_RESPONSE)
       || (loginReplyHeader->T == 0)
       || (loginReplyHeader->CSG != FLAG_SECURITY_PHASE)
       || (loginReplyHeader->NSG != FLAG_LOGIN_OPERATION_PHASE)
       || (loginReplyHeader->VerActive > HW_VERSION_1_0)
       || (loginReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
       || (loginReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {

        DebugPrint(2, false, "LoginLanscsi_V1: BAD Third Reply Header.\n");
        return -1;
    }

    if(loginReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
        DebugPrint(2, false, "LoginLanscsi_V1: Third Failed.\n");
        return -1;
    }

    // Check Data segment.
    if((ntohl(loginReplyHeader->DataSegLen) < BIN_PARAM_SIZE_REPLY)    // Minus AuthParamter[1]
       || (pdu.pDataSeg == NULL)) {

        DebugPrint(2, false, "LoginLanscsi_V1: BAD Third Reply Data.\n");
        return -1;
    }
    paramSecu = (PBIN_PARAM_SECURITY)pdu.pDataSeg;
    if(paramSecu->ParamType != BIN_PARAM_TYPE_SECURITY
       || paramSecu->LoginType != LoginType
       || paramSecu->AuthMethod != htons(AUTH_METHOD_CHAP)) {

        DebugPrint(2, false, "LoginLanscsi_V1: BAD Third Reply Parameters.\n");
        return -1;
    }

    Session->SessionPhase = FLAG_LOGIN_OPERATION_PHASE;

    //
    // Fourth Packet.
    //
    bzero(pduBuffer, MAX_REQUEST_SIZE);

    loginRequestPdu = (PLANSCSI_LOGIN_REQUEST_PDU_HEADER)pduBuffer;
    loginRequestPdu->Opocde = LOGIN_REQUEST;
    loginRequestPdu->T = 1;
    loginRequestPdu->CSG = FLAG_LOGIN_OPERATION_PHASE;
    loginRequestPdu->NSG = FLAG_FULL_FEATURE_PHASE;
    loginRequestPdu->HPID = htonl(Session->HPID);
    loginRequestPdu->RPID = htons(Session->RPID);
    loginRequestPdu->DataSegLen = htonl(BIN_PARAM_SIZE_LOGIN_FOURTH_REQUEST);
    loginRequestPdu->CSubPacketSeq = htons(++subSequence);
    loginRequestPdu->PathCommandTag = htonl(Session->Tag);
    loginRequestPdu->ParameterType = PARAMETER_TYPE_BINARY;
    loginRequestPdu->ParameterVer = PARAMETER_CURRENT_VERSION;

    paramNego = (PBIN_PARAM_NEGOTIATION)&pduBuffer[sizeof(LANSCSI_LOGIN_REQUEST_PDU_HEADER)];

    paramNego->ParamType = BIN_PARAM_TYPE_NEGOTIATION;

    // Send Request.
    pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)loginRequestPdu;
    pdu.pDataSeg = (char *)paramNego;

    if(SendRequest(Session, &pdu) != 0) {
        DebugPrint(1, true, "Login: Send Fourth Request ");
        return -3;
    }

    // Read Request.
    result = ReadReply(Session, (char *)pduBuffer, &pdu);
    if(result < 0) {
        DebugPrint(2, false, "LoginLanscsi_V1: Fourth Can't Read Reply.\n");
        return -3;
    }

    // Check Request Header.
    loginReplyHeader = (PLANSCSI_LOGIN_REPLY_PDU_HEADER)pdu.pR2HHeader;
    if((loginReplyHeader->Opocde != LOGIN_RESPONSE)
       || (loginReplyHeader->T == 0)
       || ((loginReplyHeader->Flags & LOGIN_FLAG_CSG_MASK) != (FLAG_LOGIN_OPERATION_PHASE << 2))
       || ((loginReplyHeader->Flags & LOGIN_FLAG_NSG_MASK) != FLAG_FULL_FEATURE_PHASE)
       || (loginReplyHeader->VerActive > HW_VERSION_1_0)
       || (loginReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
       || (loginReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {

        DebugPrint(2, false, "LoginLanscsi_V1: BAD Fourth Reply Header.\n");
        return -1;
    }

    if(loginReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
        DebugPrint(2, false, "LoginLanscsi_V1: Fourth Failed.\n");
        return -1;
    }

    // Check Data segment.
    if((ntohl(loginReplyHeader->DataSegLen) < BIN_PARAM_SIZE_REPLY)    // Minus AuthParamter[1]
       || (pdu.pDataSeg == NULL)) {

        DebugPrint(2, false, "LoginLanscsi_V1: BAD Fourth Reply Data.\n");
        return -1;
    }
    paramNego = (PBIN_PARAM_NEGOTIATION)pdu.pDataSeg;
    if(paramNego->ParamType != BIN_PARAM_TYPE_NEGOTIATION) {
        DebugPrint(2, false, "LoginLanscsi_V1: BAD Fourth Reply Parameters.\n");
        return -1;
    }

    DebugPrint(4, false, "LoginLanscsi_V1: Hw Type %d, Hw Version %d, NRSlots %d, W %d, MT %d ML %d\n",
           paramNego->HWType, paramNego->HWVersion,
           ntohl(paramNego->NRSlot), ntohl(paramNego->MaxBlocks),
           ntohl(paramNego->MaxTargetID), ntohl(paramNego->MaxLUNID)
           );
    DebugPrint(4, false, "LoginLanscsi_V1: Head Encrypt Algo %d, Head Digest Algo %d, Data Encrypt Algo %d, Data Digest Algo %d\n",
           ntohs(paramNego->HeaderEncryptAlgo),
           ntohs(paramNego->HeaderDigestAlgo),
           ntohs(paramNego->DataEncryptAlgo),
           ntohs(paramNego->DataDigestAlgo)
           );

    Session->MaxRequestBlocks = ntohl(paramNego->MaxBlocks);
    Session->HeaderEncryptAlgo = ntohs(paramNego->HeaderEncryptAlgo);
    Session->DataEncryptAlgo = ntohs(paramNego->DataEncryptAlgo);

    Session->SessionPhase = FLAG_FULL_FEATURE_PHASE;

    return 0;
}

int
LoginLanscsi_V2(
				struct LanScsiSession *Session, 
				uint8_t LoginType, 
				uint32_t UserID, 
				char *Key
				)
{
    char                               	pduBuffer[MAX_REQUEST_SIZE];
    PLANSCSI_LOGIN_REQUEST_PDU_HEADER	loginRequestPdu;
    PLANSCSI_LOGIN_REPLY_PDU_HEADER     loginReplyHeader;
    PBIN_PARAM_SECURITY                 paramSecu;
    PBIN_PARAM_NEGOTIATION              paramNego;
    PAUTH_PARAMETER_CHAP                paramChap;
    LANSCSI_PDU                         pdu;
    uint32_t                            subSequence;
    int32_t                             result;
    uint32_t                            CHAP_I;
    uint32_t							CHAP_C;
    
    DebugPrint(4, false, "LoginLanscsi_V2: for so@%lx, login type = %d Version %d\n", Session->DevSo, LoginType, Session->ui8HWVersion);
    
    Session->HPID = 0; //random();
    Session->SessionPhase = FLAG_SECURITY_PHASE;
    Session->Tag = 0;                                             
    
    subSequence = 0;

    //
    // First Packet.
    //
    bzero(pduBuffer, MAX_REQUEST_SIZE);

    loginRequestPdu = (PLANSCSI_LOGIN_REQUEST_PDU_HEADER)pduBuffer;

    loginRequestPdu->Opocde = LOGIN_REQUEST;
    loginRequestPdu->HPID = htonl(Session->HPID);
    loginRequestPdu->AHSLen= htons(BIN_PARAM_SIZE_LOGIN_FIRST_REQUEST);
    loginRequestPdu->CSubPacketSeq = htons(subSequence);
    loginRequestPdu->PathCommandTag = htonl(Session->Tag);
    loginRequestPdu->ParameterType = PARAMETER_TYPE_BINARY;
    loginRequestPdu->ParameterVer = PARAMETER_CURRENT_VERSION;
    loginRequestPdu->VerMax = Session->ui8HWVersion;
    loginRequestPdu->VerMin = 0;
    
    DebugPrint(4, false, "LoginLanscsi_V2: loginRequestPdu->CSubPacketSeq = %d\n", loginRequestPdu->CSubPacketSeq);

    paramSecu = (PBIN_PARAM_SECURITY)&pduBuffer[sizeof(LANSCSI_LOGIN_REQUEST_PDU_HEADER)];

    paramSecu->ParamType = BIN_PARAM_TYPE_SECURITY;
    paramSecu->LoginType = LoginType;
    paramSecu->AuthMethod = htons(AUTH_METHOD_CHAP);

    // Send Request.
    pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)loginRequestPdu;
    pdu.pAHS = (char *)paramSecu;

    if(SendRequest(Session, &pdu) != 0) {
        DebugPrint(1, false, "LoginLanscsi_V2: Send First Request ");
        return -3;
    }

    // Read Request.
    result = ReadReply(Session, (char *)pduBuffer, &pdu);
    if(result < 0) {
        DebugPrint(1, false, "LoginLanscsi_V2: First Can't Read Reply.\n");
        return -3;
    }

    // Check Request Header.
    loginReplyHeader = (PLANSCSI_LOGIN_REPLY_PDU_HEADER)pdu.pR2HHeader;

#if 0
    if(loginReplyHeader->Opocde != LOGIN_RESPONSE)
		DebugPrint(2, false, "LoginLanscsi_V2: loginReplyHeader->Opocde = %d LOGIN_RESPONSE = %d\n",  loginReplyHeader->Opocde, LOGIN_RESPONSE);
        
    if(loginReplyHeader->T != 0)
        DebugPrint(2, false, "2\n");

    if(loginReplyHeader->CSG != FLAG_SECURITY_PHASE)
        DebugPrint(2, false, "3\n");

    if(loginReplyHeader->NSG != FLAG_SECURITY_PHASE)
        DebugPrint(2, false, "4\n");

    if(loginReplyHeader->VerActive > Session->ui8HWVersion)
        DebugPrint(2, false, "5\n");

    if(loginReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
        DebugPrint(2, false, "6\n");

    if(loginReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)
        DebugPrint(2, false, "7 %d\n", loginReplyHeader->ParameterVer);
#endif /* if 0 */

    if((loginReplyHeader->Opocde != LOGIN_RESPONSE)
       || (loginReplyHeader->T != 0)
       || (loginReplyHeader->CSG != FLAG_SECURITY_PHASE)
       || (loginReplyHeader->NSG != FLAG_SECURITY_PHASE)
       || (loginReplyHeader->VerActive > Session->ui8HWVersion)
       || (loginReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
       || (loginReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {

		DebugPrint(2, false, "LoginLanscsi_V2: BAD First Reply Header.\n");
        return -1;
    }

    if(loginReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {

		DebugPrint(2, false, "LoginLanscsi_V2: First Failed.\n");
		
		return -1;
    }

    // Store Data.
    Session->RPID = ntohs(loginReplyHeader->RPID);

//    paramSecu = (PBIN_PARAM_SECURITY)pdu.pAHS;
//    IOLog("[LanScsiCli]login: Version %d Auth %d\n",
//           loginReplyHeader->VerActive,
//           ntohs(paramSecu->AuthMethod)
//           );

    //
    // Second Packet.
    //
    bzero(pduBuffer, MAX_REQUEST_SIZE);

    loginRequestPdu = (PLANSCSI_LOGIN_REQUEST_PDU_HEADER)pduBuffer;

    loginRequestPdu->Opocde = LOGIN_REQUEST;
    loginRequestPdu->HPID = htonl(Session->HPID);
    loginRequestPdu->RPID = htons(Session->RPID);
    loginRequestPdu->AHSLen= htons(BIN_PARAM_SIZE_LOGIN_SECOND_REQUEST);	//<<<<<<<<<<<<<<<
    loginRequestPdu->CSubPacketSeq = htons(++subSequence);
    loginRequestPdu->PathCommandTag = htonl(Session->Tag);
    loginRequestPdu->ParameterType = PARAMETER_TYPE_BINARY;
    loginRequestPdu->ParameterVer = PARAMETER_CURRENT_VERSION;
    loginRequestPdu->VerMax = Session->ui8HWVersion;
    loginRequestPdu->VerMin = 0;

//    IOLog("loginRequestPdu->CSubPacketSeq = %d iSubSequence = %d\n", loginRequestPdu->CSubPacketSeq, subSequence);


    paramSecu = (PBIN_PARAM_SECURITY)&pduBuffer[sizeof(LANSCSI_LOGIN_REQUEST_PDU_HEADER)];

    paramSecu->ParamType = BIN_PARAM_TYPE_SECURITY;
    paramSecu->LoginType = LoginType;
    paramSecu->AuthMethod = htons(AUTH_METHOD_CHAP);

    paramChap = (PAUTH_PARAMETER_CHAP)paramSecu->AuthParamter;
    paramChap->CHAP_A = ntohl(HASH_ALGORITHM_MD5);

    // Send Request.
    pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)loginRequestPdu;
    pdu.pAHS = (char *)paramSecu;

    if(SendRequest(Session, &pdu) != 0) {
        DebugPrint(1, true, "LoginLanscsi_V2: Send Second Request ");
        return -3;
    }
		
    // Read Request.
    result = ReadReply(Session, (char *)pduBuffer, &pdu);
    if(result < 0) {
        DebugPrint(1, true, "LoginLanscsi_V2: Second Can't Read Reply.\n");
        return -3;
    }

	// Check Reply Header.
	loginReplyHeader = (PLANSCSI_LOGIN_REPLY_PDU_HEADER)pdu.pR2HHeader;
    if((loginReplyHeader->Opocde != LOGIN_RESPONSE)
       || (loginReplyHeader->T != 0)
       || (loginReplyHeader->CSG != FLAG_SECURITY_PHASE)
       || (loginReplyHeader->NSG != FLAG_SECURITY_PHASE)
       || (loginReplyHeader->VerActive > Session->ui8HWVersion)
       || (loginReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
       || (loginReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {
        DebugPrint(1, false, "LoginLanscsi_V2: BAD Second Reply Header.\n");
        return -1;
    }

    if(loginReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
        DebugPrint(1, false, "LoginLanscsi_V2: Second Failed.\n");
        return -1;
    }

    // Check Data segment.
    if((ntohl(loginReplyHeader->AHSLen) < BIN_PARAM_SIZE_REPLY)    // Minus AuthParamter[1]
       || (pdu.pAHS == NULL)) {

		DebugPrint(1, false, "LoginLanscsi_V2: BAD Second Reply Data.\n");
        return -1;
    }
    paramSecu = (PBIN_PARAM_SECURITY)pdu.pAHS;
    if(paramSecu->ParamType != BIN_PARAM_TYPE_SECURITY
       || paramSecu->LoginType != LoginType
       || paramSecu->AuthMethod != htons(AUTH_METHOD_CHAP)) {

        DebugPrint(1, false, "LoginLanscsi_V2: BAD Second Reply Parameters.\n");
        return -1;
    }

    // Store Challenge.
    paramChap = &paramSecu->ChapParam;
    CHAP_I = ntohl(paramChap->CHAP_I);
    Session->CHAP_C = ntohl(paramChap->CHAP_C[0]);

//    IOLog("[LanScsiCli]login: Hash %x, Challenge %x CHAP_I = %x\n",
//           ntohl(paramChap->CHAP_A),
//           Session->CHAP_C,
//           CHAP_I
//           );

    //
    // Third Packet.
    //
    bzero(pduBuffer, MAX_REQUEST_SIZE);

    loginRequestPdu = (PLANSCSI_LOGIN_REQUEST_PDU_HEADER)pduBuffer;
    loginRequestPdu->Opocde = LOGIN_REQUEST;
    loginRequestPdu->T = 1;
    loginRequestPdu->CSG = FLAG_SECURITY_PHASE;
    loginRequestPdu->NSG = FLAG_LOGIN_OPERATION_PHASE;
    loginRequestPdu->HPID = htonl(Session->HPID);
    loginRequestPdu->RPID = htons(Session->RPID);
    loginRequestPdu->AHSLen= htons(BIN_PARAM_SIZE_LOGIN_THIRD_REQUEST);
    loginRequestPdu->CSubPacketSeq = htons(++subSequence);
    loginRequestPdu->PathCommandTag = htonl(Session->Tag);
    loginRequestPdu->ParameterType = PARAMETER_TYPE_BINARY;
    loginRequestPdu->ParameterVer = PARAMETER_CURRENT_VERSION;
    loginRequestPdu->VerMax = Session->ui8HWVersion;
    loginRequestPdu->VerMin = 0;

    paramSecu = (PBIN_PARAM_SECURITY)&pduBuffer[sizeof(LANSCSI_LOGIN_REQUEST_PDU_HEADER)];

    paramSecu->ParamType = BIN_PARAM_TYPE_SECURITY;
    paramSecu->LoginType = LoginType;
    paramSecu->AuthMethod = htons(AUTH_METHOD_CHAP);

    paramChap = (PAUTH_PARAMETER_CHAP)paramSecu->AuthParamter;
    paramChap->CHAP_A = htonl(HASH_ALGORITHM_MD5);
    paramChap->CHAP_I = htonl(CHAP_I);
    paramChap->CHAP_N = htonl(UserID);

    CHAP_C = NDASSwap32LittleToHost(Session->CHAP_C);

    Hash32To128((unsigned char*)&CHAP_C, (unsigned char*)paramChap->CHAP_R, (uint8_t *)Key);

    // Send Request.
    pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)loginRequestPdu;
    pdu.pAHS = (char *)paramSecu;

    if(SendRequest(Session, &pdu) != 0) {
        DebugPrint(1, true, "LoginLanscsi_V2: Send Third Request ");
        return -3;
    }

    // Read Request.
    result = ReadReply(Session, (char *)pduBuffer, &pdu);
    if(result < 0) {
        DebugPrint(1, true, "LoginLanscsi_V2: Third Can't Read Reply.\n");
        return -3;
    }

    // Check Request Header.
    loginReplyHeader = (PLANSCSI_LOGIN_REPLY_PDU_HEADER)pdu.pR2HHeader;
    if((loginReplyHeader->Opocde != LOGIN_RESPONSE)
       || (loginReplyHeader->T == 0)
       || (loginReplyHeader->CSG != FLAG_SECURITY_PHASE)
       || (loginReplyHeader->NSG != FLAG_LOGIN_OPERATION_PHASE)
       || (loginReplyHeader->VerActive > Session->ui8HWVersion)
       || (loginReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
       || (loginReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {

        DebugPrint(1, true, "LoginLanscsi_V2: BAD Third Reply Header.\n");
        return -1;
    }

    if(loginReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
        DebugPrint(1, true, "LoginLanscsi_V2: Third Failed.\n");
        return -1;
    }

    // Check Data segment.
    if((ntohl(loginReplyHeader->AHSLen) < BIN_PARAM_SIZE_REPLY)    // Minus AuthParamter[1]
       || (pdu.pAHS == NULL)) {

        DebugPrint(1, true, "LoginLanscsi_V2: BAD Third Reply Data.\n");
        return -1;
    }
    paramSecu = (PBIN_PARAM_SECURITY)pdu.pAHS;
    if(paramSecu->ParamType != BIN_PARAM_TYPE_SECURITY
       || paramSecu->LoginType != LoginType
       || paramSecu->AuthMethod != htons(AUTH_METHOD_CHAP)) {

        DebugPrint(1, true, "LoginLanscsi_V2: BAD Third Reply Parameters.\n");
        return -1;
    }

    Session->SessionPhase = FLAG_LOGIN_OPERATION_PHASE;

    //
    // Fourth Packet.
    //
    bzero(pduBuffer, MAX_REQUEST_SIZE);

    loginRequestPdu = (PLANSCSI_LOGIN_REQUEST_PDU_HEADER)pduBuffer;
    loginRequestPdu->Opocde = LOGIN_REQUEST;
    loginRequestPdu->T = 1;
    loginRequestPdu->CSG = FLAG_LOGIN_OPERATION_PHASE;
    loginRequestPdu->NSG = FLAG_FULL_FEATURE_PHASE;
    loginRequestPdu->HPID = htonl(Session->HPID);
    loginRequestPdu->RPID = htons(Session->RPID);
    loginRequestPdu->AHSLen= htons(BIN_PARAM_SIZE_LOGIN_FOURTH_REQUEST);
    loginRequestPdu->CSubPacketSeq = htons(++subSequence);
    loginRequestPdu->PathCommandTag = htonl(Session->Tag);
    loginRequestPdu->ParameterType = PARAMETER_TYPE_BINARY;
    loginRequestPdu->ParameterVer = PARAMETER_CURRENT_VERSION;
    loginRequestPdu->VerMax = Session->ui8HWVersion;
    loginRequestPdu->VerMin = 0;

    paramNego = (PBIN_PARAM_NEGOTIATION)&pduBuffer[sizeof(LANSCSI_LOGIN_REQUEST_PDU_HEADER)];

    paramNego->ParamType = BIN_PARAM_TYPE_NEGOTIATION;

    // Send Request.
    pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)loginRequestPdu;
    pdu.pAHS = (char *)paramNego;

    if(SendRequest(Session, &pdu) != 0) {
        DebugPrint(1, true, "LoginLanscsi_V2: Send Fourth Request ");
        return -3;
    }

    // Read Request.
    result = ReadReply(Session, (char *)pduBuffer, &pdu);
    if(result < 0) {
		DebugPrint(1, true, "LoginLanscsi_V2: Fourth Can't Read Reply.\n");
        return -3;
    }

    // Check Request Header.
    loginReplyHeader = (PLANSCSI_LOGIN_REPLY_PDU_HEADER)pdu.pR2HHeader;
    if((loginReplyHeader->Opocde != LOGIN_RESPONSE)
       || (loginReplyHeader->T == 0)
       || ((loginReplyHeader->Flags & LOGIN_FLAG_CSG_MASK) != (FLAG_LOGIN_OPERATION_PHASE << 2))
       || ((loginReplyHeader->Flags & LOGIN_FLAG_NSG_MASK) != FLAG_FULL_FEATURE_PHASE)
       || (loginReplyHeader->VerActive > Session->ui8HWVersion)
       || (loginReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
       || (loginReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {

        DebugPrint(1, true, "LoginLanscsi_V2: BAD Fourth Reply Header.\n");
        return -1;
    }

    if(loginReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
        DebugPrint(1, true, "LoginLanscsi_V2: Fourth Failed.\n");
        return -1;
    }

    // Check Data segment.
    if((ntohl(loginReplyHeader->AHSLen) < BIN_PARAM_SIZE_REPLY)    // Minus AuthParamter[1]
       || (pdu.pAHS == NULL)) {

        DebugPrint(1, true, "LoginLanscsi_V2: BAD Fourth Reply Data.\n");
        return -1;
    }
    paramNego = (PBIN_PARAM_NEGOTIATION)pdu.pAHS;
    if(paramNego->ParamType != BIN_PARAM_TYPE_NEGOTIATION) {
        DebugPrint(1, true, "LoginLanscsi_V2: BAD Fourth Reply Parameters.\n");
        return -1;
    }

    DebugPrint(4, false, "LoginLanscsi_V2: Hw Type %d, Hw Version %d, NRSlots %d, W %d, MT %d ML %d\n",
           paramNego->HWType, paramNego->HWVersion,
           ntohl(paramNego->NRSlot), ntohl(paramNego->MaxBlocks),
           ntohl(paramNego->MaxTargetID), ntohl(paramNego->MaxLUNID)
           );
    DebugPrint(4, false, "LoginLanscsi_V2: Head Encrypt Algo %d, Head Digest Algo %d, Data Encrypt Algo %d, Data Digest Algo %d\n",
           ntohs(paramNego->HeaderEncryptAlgo),
           ntohs(paramNego->HeaderDigestAlgo),
           ntohs(paramNego->DataEncryptAlgo),
           ntohs(paramNego->DataDigestAlgo)
           );

	// Revision.
	if(paramNego->HWVersion >= 2) {
		Session->ui16Revision = ntohs(loginReplyHeader->Revision);
	}
	
	Session->MaxRequestBlocks = ntohl(paramNego->MaxBlocks);
    Session->HeaderEncryptAlgo = ntohs(paramNego->HeaderEncryptAlgo);
    Session->DataEncryptAlgo = ntohs(paramNego->DataEncryptAlgo);

    Session->SessionPhase = FLAG_FULL_FEATURE_PHASE;

    return 0;
}

int
Login(
	  struct LanScsiSession *Session, 
	  uint8_t LoginType, 
	  uint32_t UserID, 
	  char *Key
	  )
{
	
	DebugPrint(4, false, "Login: HW Version %d\n", Session->ui8HWVersion);
	
	switch(Session->ui8HWVersion) {
		case HW_VERSION_1_0:
			
			return LoginLanscsi_V1(Session, LoginType, UserID, Key);
			
		case HW_VERSION_1_1:
		case HW_VERSION_2_0:
			
			return LoginLanscsi_V2(Session, LoginType, UserID, Key);
			
		default:
			DebugPrint(1, false, "Login: Undefined Version %d\n", Session->ui8HWVersion);

			return -1;
	}
	
}

int Logout(struct LanScsiSession *Session)
{
    int8_t                               PduBuffer[MAX_REQUEST_SIZE];
    PLANSCSI_LOGOUT_REQUEST_PDU_HEADER  pRequestHeader;
    PLANSCSI_LOGOUT_REPLY_PDU_HEADER    pReplyHeader;
    LANSCSI_PDU                         pdu;
    int                                 iResult;

    bzero(PduBuffer, MAX_REQUEST_SIZE);

    pRequestHeader = (PLANSCSI_LOGOUT_REQUEST_PDU_HEADER)PduBuffer;
    pRequestHeader->Opocde = LOGOUT_REQUEST;
    pRequestHeader->F = 1;
    pRequestHeader->HPID = htonl(Session->HPID);
    pRequestHeader->RPID = htons(Session->RPID);
    pRequestHeader->CPSlot = 0;
    pRequestHeader->DataSegLen = 0;
    pRequestHeader->AHSLen = 0;
    pRequestHeader->CSubPacketSeq = 0;
    pRequestHeader->PathCommandTag = htonl(++Session->Tag);

    // Send Request.
    pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;

    if(SendRequest(Session, &pdu) != 0) {

        DebugPrint(4, true, "[LanScsiProtocol]Logout: Send Request ERROR!\n");
        return -1;
    }

    // Read Request.
    iResult = ReadReply(Session, (char *)PduBuffer, &pdu);
    if(iResult < 0) {
//        IOLog( "[LanScsiCli]Logout: Can't Read Reply.\n");
        return -1;
    }

    // Check Request Header.
    pReplyHeader = (PLANSCSI_LOGOUT_REPLY_PDU_HEADER)pdu.pR2HHeader;

    if((pReplyHeader->Opocde != LOGOUT_RESPONSE)
       || (pReplyHeader->F == 0)) {

//        IOLog( "[LanScsiCli]Logout: BAD Reply Header.\n");
    }

    if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
//        IOLog( "[LanScsiCli]Logout: Failed.\n");
        return -1;
    }

    Session->SessionPhase = FLAG_SECURITY_PHASE;

    return 0;
}


int TextTargetList(struct LanScsiSession *Session, int *NRTarget, TARGET_DATA *PerTarget)
{
    int8_t                               PduBuffer[MAX_REQUEST_SIZE];
    PLANSCSI_TEXT_REQUEST_PDU_HEADER    pRequestHeader;
    PLANSCSI_TEXT_REPLY_PDU_HEADER      pReplyHeader;
    PBIN_PARAM_TARGET_LIST              pParam;
    LANSCSI_PDU                         pdu;
    int                                 iResult;
    int									i;
	int									iNumberOfTargets;

	DebugPrint(4, false, "TextTargetList: Entered. HW Version %u\n", Session->ui8HWVersion);

    bzero(PduBuffer, MAX_REQUEST_SIZE);

    pRequestHeader = (PLANSCSI_TEXT_REQUEST_PDU_HEADER)PduBuffer;
    pRequestHeader->Opocde = TEXT_REQUEST;
    pRequestHeader->F = 1;
    pRequestHeader->HPID = htonl(Session->HPID);
    pRequestHeader->RPID = htons(Session->RPID);
    pRequestHeader->CPSlot = 0;
	
    if(HW_VERSION_1_0 == Session->ui8HWVersion) {
      pRequestHeader->DataSegLen = htonl(BIN_PARAM_SIZE_TEXT_TARGET_LIST_REQUEST);
    } else {
      pRequestHeader->AHSLen = htons(BIN_PARAM_SIZE_TEXT_TARGET_LIST_REQUEST);
    }
	
    pRequestHeader->CSubPacketSeq = 0;
    pRequestHeader->PathCommandTag = htonl(++Session->Tag);
    pRequestHeader->ParameterType = PARAMETER_TYPE_BINARY;
    pRequestHeader->ParameterVer = PARAMETER_CURRENT_VERSION;

    // Make Parameter.
    pParam = (PBIN_PARAM_TARGET_LIST)&PduBuffer[sizeof(LANSCSI_H2R_PDU_HEADER)];
    pParam->ParamType = BIN_PARAM_TYPE_TARGET_LIST;

    // Send Request.
    pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;
	
    if(HW_VERSION_1_0 == Session->ui8HWVersion) {
      pdu.pDataSeg = (char *)pParam;
    } else {
      pdu.pAHS = (char *)pParam;
    }

    if(SendRequest(Session, &pdu) != 0) {
        DebugPrint(1, true, "TextTargetList: Send First Request ");
        return -1;
    }

    // Read Request.
    iResult = ReadReply(Session, (char *)PduBuffer, &pdu);
    if(iResult < 0) {
        DebugPrint(2, false, "TextTargetList: Can't Read Reply.\n");
        return -1;
    }

    pReplyHeader = (PLANSCSI_TEXT_REPLY_PDU_HEADER)pdu.pR2HHeader;

    // Check Request Header.
    if((pReplyHeader->Opocde != TEXT_RESPONSE)
       || (pReplyHeader->F == 0)
       || (pReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
       || (pReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {

//        IOLog( "[LanScsiCli]TextTargetList: BAD Reply Header.\n");
        return -1;
    }

    if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
//        IOLog( "[LanScsiCli]TextTargetList: Failed.\n");
        return -1;
    }

	if(HW_VERSION_1_0 == Session->ui8HWVersion) {
		if(ntohl(pReplyHeader->DataSegLen) < BIN_PARAM_SIZE_REPLY) {
			//        IOLog( "[LanScsiCli]TextTargetList: No Data Segment.\n");
			return -1;
		}
		pParam = (PBIN_PARAM_TARGET_LIST)pdu.pDataSeg;
	} else {
		if(ntohl(pReplyHeader->AHSLen) < BIN_PARAM_SIZE_REPLY) {
			//        IOLog( "[LanScsiCli]TextTargetList: No Data Segment.\n");
			return -1;
		}
		pParam = (PBIN_PARAM_TARGET_LIST)pdu.pAHS;
	}
	
    if(pParam->ParamType != BIN_PARAM_TYPE_TARGET_LIST) {
//        IOLog( "TEXT: Bad Parameter Type.\n");
        return -1;
    }
//    IOLog("[LanScsiCli]TextTargetList: NR Targets : %d\n", pParam->NRTarget);
    *NRTarget = pParam->NRTarget;

	iNumberOfTargets = 0;
	
	// Reset Infos.
	for (i = 0; i < MAX_NR_OF_TARGETS_PER_DEVICE; i++) {
		PerTarget[i].bPresent = FALSE;
	}
	
	// Update Infos.
    for(i = 0; i < pParam->NRTarget; i++) {
        PBIN_PARAM_TARGET_LIST_ELEMENT  pTarget;
        int                             iTargetId;

        pTarget = &pParam->PerTarget[i];
        iTargetId = ntohl(pTarget->TargetID);

//        IOLog("[LanScsiCli]TextTargetList: Target ID: %d, NR_RW: %d, NR_RO: %d, Data: %I64d \n",
//               ntohl(pTarget->TargetID),
//               pTarget->NRRWHost,
//               pTarget->NRROHost,
//               (int) pTarget->TargetData
//               );

        PerTarget[iTargetId].bPresent = TRUE;
        PerTarget[iTargetId].NRRWHost = pTarget->NRRWHost;
        PerTarget[iTargetId].NRROHost = pTarget->NRROHost;
        PerTarget[iTargetId].TargetData = pTarget->TargetData;
		
		iNumberOfTargets++;
    }
	
	Session->NumberOfTargets = iNumberOfTargets;
	
    return 0;
}

//
// Only Support MWORD DMA 2.
// Chip Rev 1.0, 1.1
//
int
SetDMA_V1(
		  struct LanScsiSession		*Session, 
		  uint32_t					TargetID, 
		  TARGET_DATA				*TargetData,
		  struct hd_driveid			*pInfo,
		  uint8_t					*pResult
		  )
{
	uint16_t	iFeature;
	char		cMode;
	int			iResult;
	
	//Bus_KdPrint_Def(BUS_DBG_IOS_ERROR, ("SetDMA_V1: dma_ultra 0x%x, dma_mword 0x%x PIO 0x%x\n",
	//									pInfo->dma_ultra, pInfo->dma_mword, pInfo->eide_pio_modes));
	
	//
	// DMA Mode.
	//		

	if(!(NDASSwap16LittleToHost(pInfo->dma_mword) & 0x0004)) {
		
		if(Session->ui8HWVersion <= HW_VERSION_1_1) {
//			Bus_KdPrint_Def(BUS_DBG_LPX_WARNING, ("SetDMA_V1: Set PIO Mode\n"));
			
			if(0 != (NDASSwap16LittleToHost(pInfo->eide_pio_modes) & 0x0003)) {
				// Find Fastest Mode.
				cMode = 0;
				
				if(NDASSwap16LittleToHost(pInfo->eide_pio_modes) & 0x0001)
					cMode = 3;
				if(NDASSwap16LittleToHost(pInfo->eide_pio_modes) & 0x0002)
					cMode = 4;
				
				iFeature = XFER_PIO_0 | cMode;
				
			} else {
				iFeature = XFER_PIO_2;
			}
			
			TargetData->SelectedTransferMode = iFeature;
			
			TargetData->bDMA = FALSE;
			
		} else {	
			//Bus_KdPrint_Def(BUS_DBG_LPX_ERROR, ("SetDMA_V1: Not Support DMA mode 2... 0x%x\n", pInfo->dma_mword));
			return -1;
		}
	} else {		
		iFeature = XFER_MW_DMA_2;
		TargetData->bDMA = TRUE;
		TargetData->SelectedTransferMode = XFER_MW_DMA_2;
	}
			
	if(!(NDASSwap16LittleToHost(pInfo->dma_mword) & 0x0400)) {
		
		// Set Transfer Mode.
		if((iResult = IdeCommand(Session, TargetID, TargetData, 0, WIN_SETFEATURES, 0, iFeature, SETFEATURES_XFER, NULL, 0, pResult, NULL, 0)) != 0) {
			DebugPrint(1, false, "SetDMA_V1: Set Feature Failed...\n");
            
			return iResult;
        }
	}
	
	return 0;
}

//
// Support Ultra DMA.
// Chip Rev 2.0 
//
int
SetDMA_V2(
		  struct LanScsiSession		*Session, 
		  uint32_t					TargetID,
		  TARGET_DATA				*TargetData,
		  struct hd_driveid			*pInfo,
		  uint8_t					*pResult
		)
{
	uint8_t		cMode = 0;
	uint8_t		uiSetFeature;
	int			iResult;
	bool		bUDMA;
	
	//	Bus_KdPrint_Def(BUS_DBG_IOS_ERROR, ("SetDMA_V2: dma_ultra 0x%x, dma_mword 0x%x\n",
	//										pInfo->dma_ultra, pInfo->dma_mword));
	
	uiSetFeature = 0;
	TargetData->bDMA = TRUE;
	
	if (Session->ui8HWVersion >= HW_VERSION_2_0 
		&& Session->ui16Revision == LANSCSIIDE_VER20_REV_1G) {
		bUDMA = true;
	} else {
		bUDMA = false;
	}
	
	//
	// Ultra DMA?
	//
	if(bUDMA 
	   && (0 != (NDASSwap16LittleToHost(pInfo->dma_ultra) & 0x00ff))) {		
		// Find Fastest Mode.
		if(NDASSwap16LittleToHost(pInfo->dma_ultra) & 0x0001)
			cMode = 0;
		if(NDASSwap16LittleToHost(pInfo->dma_ultra) & 0x0002)
			cMode = 1;
		if(NDASSwap16LittleToHost(pInfo->dma_ultra) & 0x0004)
			cMode = 2;
		
		if(TRUE == TargetData->bCable80) {	
			if(NDASSwap16LittleToHost(pInfo->dma_ultra) & 0x0008)
				cMode = 3;
			if(NDASSwap16LittleToHost(pInfo->dma_ultra) & 0x0010)
				cMode = 4;
			if(NDASSwap16LittleToHost(pInfo->dma_ultra) & 0x0020)
				cMode = 5;
			if(NDASSwap16LittleToHost(pInfo->dma_ultra) & 0x0040)
				cMode = 6;
			// No mode 7 in the Spec.
			//if(NDASSwap16LittleToHost(pInfo->dma_ultra) & 0x0080)
			//	cMode = 7;
		}
		
		uiSetFeature = cMode | 0x40;	// Ultra DMA mode.
		
		// Set Transfer Mode.
		iResult = IdeCommand(Session, TargetID, TargetData, 0, WIN_SETFEATURES, 0, uiSetFeature, SETFEATURES_XFER, NULL, 0, pResult, NULL, 0);
		if(iResult == 0) {
			
			TargetData->SelectedTransferMode = cMode | 0x40;
	        return 0;
        }
		
		//Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("SetDMA_V2: UDMA Set Feature Failed...\n"));
		
	}
	
	if(0 != (NDASSwap16LittleToHost(pInfo->dma_mword) & 0x0007)){
		// Find Fastest Mode.
		if(NDASSwap16LittleToHost(pInfo->dma_mword) & 0x0001)
			cMode = 0;
		if(NDASSwap16LittleToHost(pInfo->dma_mword) & 0x0002)
			cMode = 1;
		if(NDASSwap16LittleToHost(pInfo->dma_mword) & 0x0004)
			cMode = 2;
		
		uiSetFeature = cMode | 0x20;	// DMA mode.
		
		iResult = IdeCommand(Session, TargetID, TargetData, 0, WIN_SETFEATURES, 0, uiSetFeature, SETFEATURES_XFER, NULL, 0, pResult, NULL, 0);
		if(iResult == 0) {
			
			TargetData->SelectedTransferMode = cMode | 0x20;
			
			return 0;
        }

	//	Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("SetDMA_V2: DMA Set Feature Failed...\n"));
	}	
	
	// Set Default PIO Mode.
	
	DebugPrint(1, false, "Before PIO 0x%x\n", NDASSwap16LittleToHost(pInfo->eide_pio_modes));
	
	if(NDASSwap16LittleToHost(pInfo->eide_pio_modes) & 0x0003) {
		if(NDASSwap16LittleToHost(pInfo->eide_pio_modes) & 0x0002) {
			uiSetFeature = XFER_PIO_4;
		} if(NDASSwap16LittleToHost(pInfo->eide_pio_modes) & 0x0001) {
			uiSetFeature = XFER_PIO_3;
		}
	} else {
		uiSetFeature = XFER_PIO_SLOW;
	}
	
	TargetData->SelectedTransferMode = uiSetFeature;
	TargetData->bDMA = FALSE;
	
	if((iResult = IdeCommand(Session, TargetID, TargetData, 0, WIN_SETFEATURES, 0, uiSetFeature, SETFEATURES_XFER, NULL, 0, pResult, NULL, 0)) != 0) {
		return iResult;
	}

	DebugPrint(1, false, "Selected  0x%x\n", TargetData->SelectedTransferMode);

	return 0;
}

int
GetDiskInfo_V1(
			   struct LanScsiSession *Session, 
			   uint32_t TargetID, 
			   TARGET_DATA *TargetData
			   )
{
	struct hd_driveid	info;
	char				buffer[41];
	int					iResult;
	uint8_t				ui8Result;
		
	// identify.
	if((iResult = IdeCommand(Session, TargetID, TargetData, 0, WIN_IDENTIFY, 0, 0, 0, (char *)&info, sizeof(info), &ui8Result, NULL, 0)) != 0) {
		//        IOLog("[LanScsiCli]GetDiskInfo: Identify Failed...\n");
        return iResult;
    }	
	
	// Check and Set DMA
	iResult = SetDMA_V1(Session, TargetID, TargetData, &info, &ui8Result);
	if(0 != iResult) {
		DebugPrint(1, false, "GetDiskInfo_V1: Not Support DMA mode 2... 0x%x\n", info.dma_mword);
		return -1;
	}
	
	//Bus_KdPrint_Def(BUS_DBG_LPX_INFO, ("GetDiskInfo_V1: Target ID %d, Major 0x%x, Minor 0x%x, Capa 0x%x\n", 
	//								   TargetId, info.major_rev_num, info.minor_rev_num, info.capability)
	//				);
	
	//pPath->PerTarget[TargetId].bPacket = FALSE;
	
//	ConvertString((PCHAR)buffer, (PCHAR)info.serial_no, 20);
//	memcpy(pPath->PerTarget[TargetId].Serial, buffer, 20);
//	Bus_KdPrint_Def(BUS_DBG_LPX_INFO, ("GetDiskInfo_V1: Serial No: %s\n", buffer));
	
//	ConvertString((PCHAR)buffer, (PCHAR)info.fw_rev, 8);
//	memcpy(pPath->PerTarget[TargetId].FW_Rev, buffer, 8);
//	Bus_KdPrint_Def(BUS_DBG_LPX_INFO, ("GetDiskInfo_V1: Firmware rev: %s\n", buffer));
	
	bzero(buffer, 41);
    ConvertString((char *)buffer, (char *)info.model, 40);
	strncpy(TargetData->model, (char *)buffer, 40);
	
	bzero(buffer, 21);
	ConvertString((PCHAR)buffer, (PCHAR)info.serial_no, 20);
	strncpy(TargetData->serialNumber, (char *)buffer, 20);
	
	bzero(buffer, 9);
	ConvertString((PCHAR)buffer, (PCHAR)info.fw_rev, 8);
	strncpy(TargetData->firmware, (char *)buffer, 8);
	
	//
	// Support LBA?
	//
	if(!(info.capability & 0x02)) {
		TargetData->bLBA = FALSE;
	} else {
		TargetData->bLBA = TRUE;
	}
	
	//
	// Calc Capacity.
	// 
	if(NDASSwap16LittleToHost(info.command_set_2) & 0x0400 && NDASSwap16LittleToHost(info.cfs_enable_2) & 0x0400) {	// Support LBA48bit
		TargetData->bLBA48 = TRUE;
		TargetData->SectorCount = NDASSwap64LittleToHost(info.lba_capacity_2);
	} else {
		TargetData->bLBA48 = FALSE;
		
		if((info.capability & 0x02) && Lba_capacity_is_ok(&info)) {
			TargetData->SectorCount = NDASSwap32LittleToHost(info.lba_capacity);
		} else
			TargetData->SectorCount = info.cyls * info.heads * info.sectors;	
	}
	
	//Bus_KdPrint_Def(BUS_DBG_LPX_INFO, ("GetDiskInfo: LBA48 %d, Number of Sectors: %I64d\n", 
//	Bus_KdPrint_Def(BUS_DBG_LPX_INFO, ("GetDiskInfo_V1: LBA48 %d, Number of Sectors: %uld\n", 
//									   pPath->PerTarget[TargetId].bLBA48, 
//									   (ULONG)pPath->PerTarget[TargetId].SectorCount)
//					);
	
	return 0;
}

int
GetDiskInfo_V2(
			   struct LanScsiSession *Session, 
			   uint32_t TargetID, 
			   TARGET_DATA *TargetData
			   )
{
	struct hd_driveid	info;
	char				buffer[41];
	int					iResult;
	uint8_t				ui8Result;
	
	//
	// Changed by HONG, ILGU
	//
	
	// identify.
	iResult = IdeCommand(Session, TargetID, TargetData, 0, WIN_IDENTIFY, 0, 0, 0, (char *)&info, sizeof(info), &ui8Result, NULL, 0);
	if(0 != iResult || ui8Result != LANSCSI_RESPONSE_SUCCESS) {
		
		DebugPrint(4, false, ("GetDiskInfo_V2: Identify Failed...\n"));
		iResult = IdeCommand(Session, TargetID, TargetData, 0, WIN_PIDENTIFY, 0, 0, 0, (char *)&info, sizeof(info), &ui8Result, NULL, 0);
		if(0 != iResult || ui8Result != LANSCSI_RESPONSE_SUCCESS) {
			DebugPrint(1, false, ("GetDiskInfo_V2: First Identify Failed...\n"));
			return -1;
		}
		TargetData->bPacket = TRUE;
		 
	} else {
		TargetData->bPacket = FALSE;
	}
		
	//
	// Check Cable.
	// In Word 93, 13th bit (CBLID)
	//
	TargetData->bCable80 = ((0 == (NDASSwap16LittleToHost(info.hw_config) & 0x2000)) ? FALSE : TRUE);
	DebugPrint(4, false, "GetDiskInfo_V2: hw_config :0x%x 0x%x\n", info.hw_config, NDASSwap16LittleToHost(info.hw_config) & 0x2000);
	
	// Check and Set DMA
	switch(Session->ui8HWVersion) {
		case HW_VERSION_1_1:
			iResult = SetDMA_V1(Session, TargetID, TargetData, &info, &ui8Result);
			break;
		case HW_VERSION_2_0:
			iResult = SetDMA_V2(Session, TargetID, TargetData, &info, &ui8Result);	
			break;
		default:
			DebugPrint(1, false, "GetDiskInfo_V2: Unsupport Version... %d\n", Session->ui8HWVersion);
			return -1;
	}
	
	if(0 != iResult) {
		DebugPrint(1, false, ("GetDiskInfo_V2: SetDMA_V2 Failed.\n"));
		return -1;
	}
	
	//
	// Check selected transfer mode.
	//
	if(!TargetData->bPacket) {
		iResult = IdeCommand(Session, TargetID, TargetData, 0, WIN_IDENTIFY, 0, 0, 0, (char *)&info, sizeof(info), &ui8Result, NULL, 0);
	} else {
		iResult = IdeCommand(Session, TargetID, TargetData, 0, WIN_PIDENTIFY, 0, 0, 0, (char *)&info, sizeof(info), &ui8Result, NULL, 0);
	}
	
	if(0 != iResult || ui8Result != LANSCSI_RESPONSE_SUCCESS) {
		DebugPrint(1, false, ("GetDiskInfo_V2: Second Identify Failed...\n"));
		return -1;
	}

	if(NDASSwap16LittleToHost(info.field_valid) & 0x0004 &&
	   NDASSwap16LittleToHost(info.dma_ultra) & 0xff00) {
		if(NDASSwap16LittleToHost(info.dma_ultra) & 0x8000) {
			TargetData->SelectedTransferMode = 0x40 | 7;
		} else if(NDASSwap16LittleToHost(info.dma_ultra) & 0x4000) {
			TargetData->SelectedTransferMode = 0x40 | 6;
		} else if(NDASSwap16LittleToHost(info.dma_ultra) & 0x2000) {
			TargetData->SelectedTransferMode = 0x40 | 5;
		} else if(NDASSwap16LittleToHost(info.dma_ultra) & 0x1000) {
			TargetData->SelectedTransferMode = 0x40 | 4;
		} else if(NDASSwap16LittleToHost(info.dma_ultra) & 0x800) {
			TargetData->SelectedTransferMode = 0x40 | 3;
		} else if(NDASSwap16LittleToHost(info.dma_ultra) & 0x400) {
			TargetData->SelectedTransferMode = 0x40 | 2;
		} else if(NDASSwap16LittleToHost(info.dma_ultra) & 0x200) {
			TargetData->SelectedTransferMode = 0x40 | 1;
		} else if(NDASSwap16LittleToHost(info.dma_ultra) & 0x100) {
			TargetData->SelectedTransferMode = 0x40 | 0;
		}
	} else if(NDASSwap16LittleToHost(info.dma_mword) & 0xff00) {
		if(NDASSwap16LittleToHost(info.dma_mword) & 0x0400) {
			TargetData->SelectedTransferMode = 0x20 | 2;
		} else if(NDASSwap16LittleToHost(info.dma_mword) & 0x0200) {
			TargetData->SelectedTransferMode = 0x20 | 1;
		} else if(NDASSwap16LittleToHost(info.dma_mword) & 0x0100) {
			TargetData->SelectedTransferMode = 0x20 | 0;
		}	
	} else {
		// Use PIO Setting. 
	}
	
	
	DebugPrint(4, false, "GetDiskInfo_V2: Target ID %d, Major 0x%x, Minor 0x%x, Capa 0x%x\n", 
									   TargetID, info.major_rev_num, info.minor_rev_num, info.capability
					);
	
	bzero(buffer, 41);
    ConvertString((char *)buffer, (char *)info.model, 40);
	strncpy(TargetData->model, (char *)buffer, 40);
	
	bzero(buffer, 21);
	ConvertString((PCHAR)buffer, (PCHAR)info.serial_no, 20);
	strncpy(TargetData->serialNumber, (char *)buffer, 20);

	bzero(buffer, 9);
	ConvertString((PCHAR)buffer, (PCHAR)info.fw_rev, 8);
	strncpy(TargetData->firmware, (char *)buffer, 8);

	//
	// Support LBA?
	//
	if(!(info.capability & 0x02)) {
		TargetData->bLBA = FALSE;
	} else {
		TargetData->bLBA = TRUE;
	}
	
	//
	// Calc Capacity.
	// 
	if(NDASSwap16LittleToHost(info.command_set_2) & 0x0400 && NDASSwap16LittleToHost(info.cfs_enable_2) & 0x0400) {	// Support LBA48bit
		TargetData->bLBA48 = TRUE;
		TargetData->SectorCount = NDASSwap64LittleToHost(info.lba_capacity_2);
	} else {
		TargetData->bLBA48 = FALSE;
		
		if((info.capability & 0x02) && Lba_capacity_is_ok(&info)) {
			TargetData->SectorCount = NDASSwap32LittleToHost(info.lba_capacity);
		} else
			TargetData->SectorCount = info.cyls * info.heads * info.sectors;	
	}
	
	DebugPrint(4, false, "GetDiskInfo_V2: LBA48 %d, Number of Sectors: %lld\n", 
									   TargetData->bLBA48, 
									   TargetData->SectorCount
					);
	
	return 0;
}

int
GetDiskInfo(
			struct LanScsiSession *Session, 
			uint32_t TargetID, 
			TARGET_DATA *TargetData
			)
{
	switch(Session->ui8HWVersion) {
		case HW_VERSION_1_0:
			return GetDiskInfo_V1(Session, TargetID, TargetData);		
		case HW_VERSION_1_1:
		case HW_VERSION_2_0:
			return GetDiskInfo_V2(Session, TargetID, TargetData);
			
		default:
			return -1;
	}
}

int 
IdeCommand_V1(
			  struct LanScsiSession *Session,
              int32_t   TargetId,
              TARGET_DATA *TargetData,
              int64_t   LUN,
              uint8_t    Command,
              int64_t   Location,
              int16_t   SectorCount,
              int8_t    Feature,
              char *    pData,
			  uint8_t		*pResult
			  )
{
    int8_t								PduBuffer[MAX_REQUEST_SIZE];
    PLANSCSI_IDE_REQUEST_PDU_HEADER_V1  pRequestHeader;
    PLANSCSI_IDE_REPLY_PDU_HEADER_V1	pReplyHeader;
    LANSCSI_PDU							pdu;
    size_t									iResult;
    uint8_t								iCommandReg;
	
    //
    // Make Request.
    //
    bzero(PduBuffer, MAX_REQUEST_SIZE);

    pRequestHeader = (PLANSCSI_IDE_REQUEST_PDU_HEADER_V1)PduBuffer;
    pRequestHeader->Opocde = IDE_COMMAND;
    pRequestHeader->F = 1;
    pRequestHeader->HPID = htonl(Session->HPID);
    pRequestHeader->RPID = htons(Session->RPID);
    pRequestHeader->CPSlot = 0;
    pRequestHeader->DataSegLen = 0;
    pRequestHeader->AHSLen = 0;
    pRequestHeader->CSubPacketSeq = 0;
    pRequestHeader->PathCommandTag = htonl(++Session->Tag);
    pRequestHeader->TargetID = htonl(TargetId);
    pRequestHeader->LUN = 0;

    // Using Target ID. LUN is always 0.
    pRequestHeader->DEV = TargetId;
    pRequestHeader->Feature = 0;

    switch(Command) {
        case WIN_READ:
        {
            pRequestHeader->R = 1;
            pRequestHeader->W = 0;

            if(TargetData->bLBA48 == TRUE) {
                pRequestHeader->Command = WIN_READDMA_EXT;
            } else {
                pRequestHeader->Command = WIN_READDMA;
            }
        }
            break;
        case WIN_WRITE:
        {
            pRequestHeader->R = 0;
            pRequestHeader->W = 1;

            if(TargetData->bLBA48 == TRUE) {
                pRequestHeader->Command = WIN_WRITEDMA_EXT;
            } else {
                pRequestHeader->Command = WIN_WRITEDMA;
            }
        }
            break;
        case WIN_VERIFY:
        {
            pRequestHeader->R = 0;
            pRequestHeader->W = 0;

            if(TargetData->bLBA48 == TRUE) {
                pRequestHeader->Command = WIN_VERIFY_EXT;
            } else {
                pRequestHeader->Command = WIN_VERIFY;
            }
        }
            break;
        case WIN_IDENTIFY:
        {
            pRequestHeader->R = 1;
            pRequestHeader->W = 0;

            pRequestHeader->Command = WIN_IDENTIFY;
        }
            break;
        case WIN_SETFEATURES:
        {
            pRequestHeader->R = 0;
            pRequestHeader->W = 0;

            pRequestHeader->Feature = Feature;
            pRequestHeader->SectorCount_Curr = (uint8_t)SectorCount;
            pRequestHeader->Command = WIN_SETFEATURES;

//            IOLog("[LanScsiCli]IDECommand: SET Features Sector Count 0x%x\n", pRequestHeader->SectorCount_Curr);
        }
            break;
        default:
//            IOLog("[LanScsiCli]IDECommand: Not Supported IDE Command.\n");
            return -1;
    }

    if((Command == WIN_READ)
       || (Command == WIN_WRITE)
       || (Command == WIN_VERIFY)){

        if(TargetData->bLBA == FALSE) {
//            IOLog("[LanScsiCli]IDECommand: CHS not supported...\n");
            return -1;
        }

        pRequestHeader->LBA = 1;

        if(TargetData->bLBA48 == TRUE) {

            pRequestHeader->LBALow_Curr = (int8_t)(Location);
            pRequestHeader->LBAMid_Curr = (int8_t)(Location >> 8);
            pRequestHeader->LBAHigh_Curr = (int8_t)(Location >> 16);
            pRequestHeader->LBALow_Prev = (int8_t)(Location >> 24);
            pRequestHeader->LBAMid_Prev = (int8_t)(Location >> 32);
            pRequestHeader->LBAHigh_Prev = (int8_t)(Location >> 40);

            pRequestHeader->SectorCount_Curr = (int8_t)SectorCount;
            pRequestHeader->SectorCount_Prev = (int8_t)(SectorCount >> 8);

        } else {

            pRequestHeader->LBALow_Curr = (int8_t)(Location);
            pRequestHeader->LBAMid_Curr = (int8_t)(Location >> 8);
            pRequestHeader->LBAHigh_Curr = (int8_t)(Location >> 16);
            pRequestHeader->LBAHeadNR = (int8_t)(Location >> 24);

            pRequestHeader->SectorCount_Curr = (int8_t)SectorCount;
        }
    }

    // Backup Command.
    iCommandReg = pRequestHeader->Command;

    // Send Request.
    pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;

    if(SendRequest(Session, &pdu) != 0) {
        DebugPrint(1, true, "IdeCommand_V1: Send Request ");
        return -3;
    }

    // If Write, Send Data.
    if(Command == WIN_WRITE) {
        //
        // Encrypt Data.
        //
        if(Session->DataEncryptAlgo != 0) {

            uint32_t	CHAP_C;
            
            CHAP_C = NDASSwap32LittleToHost(Session->CHAP_C);

            Encrypt32(
                      (unsigned char*)pData,
                      SectorCount * 512,
                      (unsigned char*)&CHAP_C,
                      (unsigned char*)Session->Password
                      );
        }

        iResult = SendIt(
                         Session->DevSo,
                         pData,
                         SectorCount * 512
                         );
        if(iResult != SectorCount * 512) {
            DebugPrint(1, true, "IdeCommand_V1: Send data for WRITE ");
            return -3;
        }
    }

    // If Read, Identify Op... Read Data.
    switch(Command) {
        case WIN_READ:
        {
            iResult = RecvIt(
                             Session->DevSo,
                             pData,
                             SectorCount * 512
                             );
            if(iResult != SectorCount * 512) {
                DebugPrint(1, true, "IdeCommand_V1: Receive Data for READ \n");
                return -3;
            }

            //
            // Decrypt Data.
            //
            if(Session->DataEncryptAlgo != 0) {

                uint32_t	CHAP_C;

                CHAP_C = NDASSwap32LittleToHost(Session->CHAP_C);
                Decrypt32(
                          (unsigned char*)pData,
                          SectorCount * 512,
                          (unsigned char*)&CHAP_C,
                          (unsigned char*)Session->Password
                          );
            }
        }
            break;
        case WIN_IDENTIFY:
        {
//            int i;


            iResult = RecvIt(
                             Session->DevSo,
                             pData,
                             512
                             );
            if(iResult != 512) {
                DebugPrint(1, true, "IdeCommand_V1: Receive Data for IDENTIFY\n ");
                return -3;
            }

#if 0
            for (i=0; i<512; i++) {
                if(i%30 == 0)
                    IOLog("\n");
                IOLog("%2x", pData[i]);
            }
            IOLog("\n");
#endif
            //
            // Decrypt Data.
            //
            if(Session->DataEncryptAlgo != 0) {

                uint32_t	CHAP_C;

                CHAP_C = NDASSwap32LittleToHost(Session->CHAP_C);
                Decrypt32(
                          (unsigned char*)pData,
                          512,
                          (unsigned char*)&CHAP_C,
                          (unsigned char*)Session->Password
                          );
            }
        }
            break;
        default:
            break;
    }

    // Read Reply.
    iResult = ReadReply(Session, (char *)PduBuffer, &pdu);
    if(iResult <= 0) {
//        IOLog("[LanScsiCli]IDECommand: Can't Read Reply.\n");
        return -3;
    }

    // Check Request Header.
    pReplyHeader = (PLANSCSI_IDE_REPLY_PDU_HEADER_V1)pdu.pR2HHeader;

    if((pReplyHeader->Opocde != IDE_RESPONSE)
       || (pReplyHeader->F == 0)
       || (pReplyHeader->Command != iCommandReg)) {

//        IOLog("[LanScsiCli]IDECommand: BAD Reply Header. Flag: 0x%x, Req. Command: 0x%x Rep. Command: 0x%x\n",
//                pReplyHeader->Flags, iCommandReg, pReplyHeader->Command);
        return -3;
    }

    if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
//        IOLog("[LanScsiCli]IDECommand: Failed. Response 0x%x %d %d Req. Command: 0x%x Rep. Command: 0x%x\n",
//                pReplyHeader->Response, ntohl(pReplyHeader->DataTransferLength), ntohl(pReplyHeader->DataSegLen),
//                iCommandReg, pReplyHeader->Command
//                );
        return -2;
    }

	*pResult = pReplyHeader->Response;
	
    return 0;
}

int 
IdeCommand_V2(
			  struct LanScsiSession		*Session,
              int32_t					TargetId,
              TARGET_DATA				*TargetData,
              int64_t					LUN,
              uint8_t					Command,
              int64_t					Location,
              int16_t					SectorCount,
              int8_t					Feature,
              char *					pData,
			  uint32_t					BufferLength,
			  uint8_t					*pResult,
			  PCDBd						pCdb,
			  uint8_t					DataTransferDirection
			  )
{
    int8_t								PduBuffer[MAX_REQUEST_SIZE];
    PLANSCSI_IDE_REQUEST_PDU_HEADER_V2  pRequestHeader;
    PLANSCSI_IDE_REPLY_PDU_HEADER_V2	pReplyHeader;
    LANSCSI_PDU							pdu;
    size_t								iResult;
    uint8_t								iCommandReg;
	bool								bRead, bWrite;
	uint32_t							iBufferLen;
	uint32_t							itempLen;

	// 
	// Allign 4bytes data.
	//
	iBufferLen = BufferLength + 3;
	iBufferLen = (int)(iBufferLen >> 2) << 2;

	DebugPrint(4, false, "IdeCommand_V2: Location %lld, SectorCount %d\n", Location, SectorCount);
	DebugPrint(4, false, "IdeCommand_V2: BufferLength %d, iBufferLen %d\n", BufferLength, iBufferLen);
	DebugPrint(4, false, "IdeCommand_V2: Data Transfer Direction %d\n", DataTransferDirection);
	DebugPrint(4, false, "IdeCommand_V2: TargetID %d, LUN %d\n", TargetId, LUN);
	
    //
    // Make Request.
    //
    bzero(PduBuffer, MAX_REQUEST_SIZE);

    pRequestHeader = (PLANSCSI_IDE_REQUEST_PDU_HEADER_V2)PduBuffer;
    pRequestHeader->Opocde = IDE_COMMAND;
    pRequestHeader->F = 1;
    pRequestHeader->HPID = htonl(Session->HPID);
    pRequestHeader->RPID = htons(Session->RPID);
    pRequestHeader->CPSlot = 0;
    pRequestHeader->DataSegLen = 0;
    pRequestHeader->AHSLen = 0;
    pRequestHeader->CSubPacketSeq = 0;
    pRequestHeader->PathCommandTag = htonl(++Session->Tag);
    pRequestHeader->TargetID = htonl(TargetId);
    pRequestHeader->LUN = 0;

    // Using Target ID. LUN is always 0.
    pRequestHeader->DEV = TargetId;
    pRequestHeader->Feature_Curr = 0;
	pRequestHeader->Feature_Prev = 0;
    
	bRead = bWrite = FALSE;
	
	switch(Command) {
		case WIN_READ:
		{			
			bRead = TRUE;
			
			if(TRUE == TargetData->bDMA) {
				pRequestHeader->COM_TYPE_D_P = 1;
				pRequestHeader->Feature_Curr = 0x01;
				
				if(TargetData->bLBA48 == TRUE) {
					pRequestHeader->Command = WIN_READDMA_EXT;
					
					pRequestHeader->COM_TYPE_E = 1;
				} else {
					pRequestHeader->Command = WIN_READDMA;				
				}
				
			} else {
				pRequestHeader->COM_TYPE_D_P = 0;
				
				if(TargetData->bLBA48 == TRUE) {
					pRequestHeader->Command = WIN_READ_EXT;
					
					pRequestHeader->COM_TYPE_E = 1;
				} else {
					pRequestHeader->Command = WIN_READ;				
				}
			}
						
			itempLen = SectorCount * DISK_BLOCK_SIZE;
			
#if defined(__LITTLE_ENDIAN__)
           pRequestHeader->COM_LENG = (HTONL(itempLen) >> 8);
#elif defined(__BIG_ENDIAN__)
            pRequestHeader->COM_LENG = HTONL(itempLen);
#else
            endian is not defined
#endif
			//pRequestHeader->COM_LENG = HTONL(SectorCount * DISK_BLOCK_SIZE / 256);
			//pRequestHeader->COM_LENG >>= 8;
		}
			break;
		case WIN_WRITE:
		{
			bWrite = TRUE;
			
			if(TRUE == TargetData->bDMA) {
				pRequestHeader->COM_TYPE_D_P = 1;
				pRequestHeader->Feature_Curr = 0x01;
				
				if(TargetData->bLBA48 == TRUE) {				
					pRequestHeader->Command = WIN_WRITEDMA_EXT;
					pRequestHeader->COM_TYPE_E = 1;
				} else {
					pRequestHeader->Command = WIN_WRITEDMA;
				}
			} else {
				pRequestHeader->COM_TYPE_D_P = 0;
				
				if(TargetData->bLBA48 == TRUE) {				
					pRequestHeader->Command = WIN_WRITE_EXT;
					pRequestHeader->COM_TYPE_E = 1;
				} else {
					pRequestHeader->Command = WIN_WRITE;
				}
			}
			
			itempLen = SectorCount * DISK_BLOCK_SIZE;
			
#if defined(__LITTLE_ENDIAN__)
            pRequestHeader->COM_LENG = (HTONL(itempLen) >> 8);
#elif defined(__BIG_ENDIAN__)
            pRequestHeader->COM_LENG = HTONL(itempLen);
#else
            endian is not defined
#endif
				//pRequestHeader->COM_LENG = HTONL(SectorCount * DISK_BLOCK_SIZE / 256);
			//pRequestHeader->COM_LENG >>= 8;
		}
			break;
		case WIN_VERIFY:
		{			
			if(TargetData->bLBA48 == TRUE) {
				pRequestHeader->Command = WIN_VERIFY_EXT;
				pRequestHeader->COM_TYPE_E = 1;
			} else {
				pRequestHeader->Command = WIN_VERIFY;
			}
		}
			break;
		case WIN_IDENTIFY:
		case WIN_PIDENTIFY:
		{
			bRead = TRUE;
			
			pRequestHeader->Command = Command;
			
#if defined(__LITTLE_ENDIAN__)
            pRequestHeader->COM_LENG = (htonl(DISK_BLOCK_SIZE) >> 8);
#elif defined(__BIG_ENDIAN__)
            pRequestHeader->COM_LENG = htonl(DISK_BLOCK_SIZE);
#else
            endian is not defined
#endif
				//pRequestHeader->COM_LENG = HTONL(1 * DISK_BLOCK_SIZE / 256);
			//pRequestHeader->COM_LENG >>= 8;
		}
			break;
		case WIN_SETFEATURES:
		{
			pRequestHeader->Feature_Prev = 0;
			pRequestHeader->Feature_Curr = Feature;
			pRequestHeader->SectorCount_Curr = (uint8_t)SectorCount;
			pRequestHeader->Command = WIN_SETFEATURES;
		}
			break;
		case WIN_SETMULT:
		{	
			pRequestHeader->Feature_Prev = 0;
			pRequestHeader->Feature_Curr = 0;
			pRequestHeader->SectorCount_Curr = (uint8_t)SectorCount;
			pRequestHeader->Command = WIN_SETMULT;
		}
			break;
		case WIN_CHECKPOWERMODE1:
		{
			pRequestHeader->Feature_Prev = 0;
			pRequestHeader->Feature_Curr = 0;
			pRequestHeader->SectorCount_Curr = 0;
			pRequestHeader->Command = WIN_CHECKPOWERMODE1;
		}
			break;
		case WIN_STANDBY:
		{
			pRequestHeader->Feature_Prev = 0;
			pRequestHeader->Feature_Curr = 0;
			pRequestHeader->SectorCount_Curr = 0;
			pRequestHeader->Command = WIN_STANDBY;
		}
			break;	
		case WIN_PACKETCMD:
		{
			// Common.
			pRequestHeader->AHSLen = htons(PACKET_COMMAND_SIZE);
			pdu.pAHS = (PCHAR)((PCHAR)pRequestHeader + sizeof(LANSCSI_H2R_PDU_HEADER));
			
			// IDE Registers.
			pRequestHeader->SectorCount_Curr = 0;
			pRequestHeader->LBALow_Curr = 0;
			pRequestHeader->LBAMid_Curr = (UInt8)(BufferLength & 0xFF);
			pRequestHeader->LBAHigh_Curr = (UInt8)(BufferLength >> 8);
			pRequestHeader->Command = WIN_PACKETCMD;
			
			// Copy Cdb.
			memcpy(pdu.pAHS, pCdb, PACKET_COMMAND_SIZE);
			
//			DebugPrint(1, false, "IDECommand_V2: PacketCMD 0x%x.\n", pdu.pAHS[0]);
			
			pRequestHeader->COM_TYPE_P = 1;
						
			pRequestHeader->Feature_Curr = 0;
			pRequestHeader->COM_TYPE_D_P = 0;	// PIO;
			
			switch(DataTransferDirection) {
				case kSCSIDataTransfer_FromInitiatorToTarget:
				{
					bWrite = TRUE;
					
#if defined(__LITTLE_ENDIAN__)
					pRequestHeader->COM_LENG = (HTONL(BufferLength) >> 8);
#elif defined(__BIG_ENDIAN__)
					pRequestHeader->COM_LENG = htonl(BufferLength);
#else
					endian is not defined
#endif
				}
					break;
										
				case kSCSIDataTransfer_FromTargetToInitiator:
				{
					bRead = TRUE;
					
#if defined(__LITTLE_ENDIAN__)
					pRequestHeader->COM_LENG = (HTONL(BufferLength) >> 8);
#elif defined(__BIG_ENDIAN__)
					pRequestHeader->COM_LENG = htonl(BufferLength);
#else
					endian is not defined
#endif
				}
					break;
				case kSCSIDataTransfer_NoDataTransfer:
				default:
					break;
			}
			
			switch((unsigned char)pdu.pAHS[0]) {
				
				// Read Data.
				case kSCSICmd_READ_10:
				case kSCSICmd_READ_12:
				case kSCSICmd_READ_CD:
				{
					if(TRUE == TargetData->bDMA) {
						// Using DMA.
						pRequestHeader->Feature_Curr = 0x01;	// Set DMA bit.
						pRequestHeader->COM_TYPE_D_P = 1;
					}
				}
					break;
					
					// Write Data.
				case kSCSICmd_WRITE_10:
				case kSCSICmd_WRITE_12:
				case kSCSICmd_WRITE_AND_VERIFY_10:
				case kSCSICmd_MODE_SELECT_6:
				case kSCSICmd_MODE_SELECT_10:
				{
					if(TRUE == TargetData->bDMA) {
						// Using DMA.
						pRequestHeader->Feature_Curr = 0x01;	// Set DMA bit.
						pRequestHeader->COM_TYPE_D_P = 1;
					}
				}
					break;
					
					// DVD Key
				case kSCSICmd_SEND_KEY:
				{
					pRequestHeader->COM_TYPE_K = 1;

				}
					break;

				default:
					
					break;
			}
/*			
			Bus_KdPrint_Def(BUS_DBG_SS_INFO, ("IDECommand_V2: %x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x\n",
											  pdu.pAHS[0], pdu.pAHS[1], pdu.pAHS[2], pdu.pAHS[3],
											  pdu.pAHS[4], pdu.pAHS[5], pdu.pAHS[6], pdu.pAHS[7],
											  pdu.pAHS[8], pdu.pAHS[9], pdu.pAHS[10], pdu.pAHS[11]
											  ));
*/			
		}
			break;			
        default:
			//            IOLog("[LanScsiCli]IDECommand: Not Supported IDE Command.\n");
            return -1;
    }
	
	if(TRUE == bRead) {
		pRequestHeader->R = 1;
		pRequestHeader->COM_TYPE_R = 1;
	}
	
	if(TRUE == bWrite) {
		pRequestHeader->W = 1;
		pRequestHeader->COM_TYPE_W = 1;
	}
	
    if((Command == WIN_READ)
       || (Command == WIN_WRITE)
       || (Command == WIN_VERIFY)){

        if(TargetData->bLBA == FALSE) {
//            IOLog("[LanScsiCli]IDECommand: CHS not supported...\n");
            return -1;
        }

        pRequestHeader->LBA = 1;

        if(TargetData->bLBA48 == TRUE) {

            pRequestHeader->LBALow_Curr = (int8_t)(Location);
            pRequestHeader->LBAMid_Curr = (int8_t)(Location >> 8);
            pRequestHeader->LBAHigh_Curr = (int8_t)(Location >> 16);
            pRequestHeader->LBALow_Prev = (int8_t)(Location >> 24);
            pRequestHeader->LBAMid_Prev = (int8_t)(Location >> 32);
            pRequestHeader->LBAHigh_Prev = (int8_t)(Location >> 40);

            pRequestHeader->SectorCount_Curr = (int8_t)SectorCount;
            pRequestHeader->SectorCount_Prev = (int8_t)(SectorCount >> 8);

        } else {

            pRequestHeader->LBALow_Curr = (int8_t)(Location);
            pRequestHeader->LBAMid_Curr = (int8_t)(Location >> 8);
            pRequestHeader->LBAHigh_Curr = (int8_t)(Location >> 16);
            pRequestHeader->LBAHeadNR = (int8_t)(Location >> 24);

            pRequestHeader->SectorCount_Curr = (int8_t)SectorCount;
        }
    }

    // Backup Command.
    iCommandReg = pRequestHeader->Command;

    // Send Request.
    pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;

    if(SendRequest(Session, &pdu) != 0) {
        DebugPrint(1, true, "IDECommand_V2: Send Request ");
        return -3;
    }

    // If Write, Send Data.
	if(true == bWrite && iBufferLen > 0) {		
        //
        // Encrypt Data.
        //
        if(Session->DataEncryptAlgo != 0) {

            uint32_t	CHAP_C;
            
            CHAP_C = NDASSwap32LittleToHost(Session->CHAP_C);
            Encrypt32(
                      (unsigned char*)pData,
                      iBufferLen,
                      (unsigned char*)&CHAP_C,
                      (unsigned char*)Session->Password
                      );
        }

        iResult = SendIt(
                         Session->DevSo,
                         pData,
                         iBufferLen
                         );
        if(iResult != iBufferLen) {
            DebugPrint(1, true, "IDECommand_V2: Send data for WRITE ");
            return -3;
        } else {
            DebugPrint(4, false, "IDECommand_V2: Send data for WRITE %d\n ", iResult);			
		}
    }

    // If Read, Identify Op... Read Data.
	if(true == bRead && iBufferLen > 0) {

		iResult = RecvIt(
						 Session->DevSo,
						 pData,
						 iBufferLen
						 );
		if(iResult != iBufferLen) {
			DebugPrint(1, true, "IDECommand_V2: Receive Data for READ\n ");
			return -3;
		} else {
			DebugPrint(4, true, "IDECommand_V2: Receive Data for READ %d\n", iResult);
		}
		
		//
		// Decrypt Data.
		//
		if(Session->DataEncryptAlgo != 0) {
			
			uint32_t	CHAP_C;
			
			CHAP_C = NDASSwap32LittleToHost(Session->CHAP_C);
			Decrypt32(
					  (unsigned char*)pData,
					  iBufferLen,
					  (unsigned char*)&CHAP_C,
					  (unsigned char*)&Session->Password
					  );
		}
	}

    // Read Reply.
    iResult = ReadReply(Session, (char *)PduBuffer, &pdu);
    if(iResult <= 0) {
        DebugPrint(1, false, "IDECommand_V2: Can't Read Reply.\n");
        return -3;
    }

    // Check Reply Header.
    pReplyHeader = (PLANSCSI_IDE_REPLY_PDU_HEADER_V2)pdu.pR2HHeader;

    if((pReplyHeader->Opocde != IDE_RESPONSE)
       || (pReplyHeader->F == 0)) {

        DebugPrint(1, false, "IDECommand_V2: BAD Reply Header. Opcode: 0x%x, F 0x%x\n",
                pReplyHeader->Opocde, pReplyHeader->F);
        return -3;
    }

    if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
        DebugPrint(0, false, "IDECommand_V2: Failed. Response 0x%x %d %d Req. Command: 0x%x Rep. Returned Command Reg: 0x%x, Feature Reg: 0x%x\n",
               pReplyHeader->Response, ntohl(pReplyHeader->DataTransferLength), ntohl(pReplyHeader->DataSegLen),
                iCommandReg, pReplyHeader->Command, pReplyHeader->Feature_Curr
                );
        return -2;
    }

	*pResult = pReplyHeader->Response;
	
    return 0;
}

int 
IdeCommand(
		   struct LanScsiSession	*Session,
		   int32_t					TargetId,
		   TARGET_DATA				*TargetData,
		   int64_t					LUN,
		   uint8_t					Command,
		   int64_t					Location,
		   int16_t					SectorCount,
		   int8_t					Feature,
		   char *					pData,
		   uint32_t					BufferLengh,
		   uint8_t					*pResult,
		   PCDBd					pCdb,
		   uint8_t					DataTransferDirection
		   )
{
	switch(Session->ui8HWVersion) {
		case HW_VERSION_1_0:
			
			return IdeCommand_V1(Session, TargetId, TargetData, LUN, Command, Location, SectorCount, Feature, pData, pResult);
			
		case HW_VERSION_1_1:
		case HW_VERSION_2_0:
			
			return IdeCommand_V2(Session, TargetId, TargetData, LUN, Command, Location, SectorCount, Feature, pData, BufferLengh, pResult, pCdb, DataTransferDirection);
			
		default:
			return -1;
	}
}

/***************************************************************************
 *
 *  Internal helper functions
 *
 **************************************************************************/

int ReadReply(struct LanScsiSession *Session, char *pBuffer, PLANSCSI_PDU pPdu)
{
    size_t     iResult, iTotalRecved = 0;
    char *   pPtr = pBuffer;
    uint32_t	CHAP_C;

    // Read Header.
    iResult = RecvIt(
                     Session->DevSo,
                     pPtr,
                     sizeof(LANSCSI_H2R_PDU_HEADER)
                     );
    if(iResult != sizeof(LANSCSI_H2R_PDU_HEADER)) {
		DebugPrint(1, false, "ReadReply: Can't Recv Header...\n");

        return iResult;
    } else if(iResult == 0) {
		DebugPrint(1, false, "ReadReply: Disconnected...\n");

        return iResult;
    } else
        iTotalRecved += iResult;

    pPdu->pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pPtr;

    pPtr += sizeof(LANSCSI_H2R_PDU_HEADER);

    if(Session->SessionPhase == FLAG_FULL_FEATURE_PHASE
       && Session->HeaderEncryptAlgo != 0) {

        CHAP_C = NDASSwap32LittleToHost(Session->CHAP_C);
        Decrypt32(
                  (unsigned char*)pPdu->pH2RHeader,
                  sizeof(LANSCSI_H2R_PDU_HEADER),
                  (unsigned char *)&CHAP_C,
                  (unsigned char *)Session->Password
                  );
    }

	DebugPrint(4, false, "ReadReply: AHS %d, DataSeg %d\n", ntohs(pPdu->pH2RHeader->AHSLen), ntohs(pPdu->pH2RHeader->DataSegLen));
	
    // Read AHS.
    if(ntohs(pPdu->pH2RHeader->AHSLen) > 0) {
        iResult = RecvIt(
                         Session->DevSo,
                         pPtr,
                         ntohs(pPdu->pH2RHeader->AHSLen)
                         );
        if(iResult != ntohs(pPdu->pH2RHeader->AHSLen)) {
			DebugPrint(1, false, "ReadReply: Can't Recv AHS...\n");

            return iResult;
        } else if(iResult == 0) {
			DebugPrint(1, false, "ReadReply: Disconnected...\n");

            return iResult;
        } else
            iTotalRecved += iResult;

        pPdu->pAHS = pPtr;
		
        pPtr += ntohs(pPdu->pH2RHeader->AHSLen);
		
		if(Session->SessionPhase == FLAG_FULL_FEATURE_PHASE
		   && Session->HeaderEncryptAlgo != 0) {
			
			CHAP_C = NDASSwap32LittleToHost(Session->CHAP_C);
			Decrypt32(
					  (unsigned char*)pPdu->pAHS,
					  ntohs(pPdu->pH2RHeader->AHSLen),
					  (unsigned char *)&CHAP_C,
					  (unsigned char *)&Session->Password
					  );
		}
    }

    // Read Header Dig.
    pPdu->pHeaderDig = NULL;

    // Read Data segment.
    if(ntohl(pPdu->pH2RHeader->DataSegLen) > 0) {
        iResult = RecvIt(
                         Session->DevSo,
                         pPtr,
                         ntohl(pPdu->pH2RHeader->DataSegLen)
                         );
        if(iResult != ntohl(pPdu->pH2RHeader->DataSegLen)) {
			DebugPrint(1, false, "ReadReply: Can't Recv Data segment...\n");

            return iResult;
        } else if(iResult == 0) {
			DebugPrint(1, false, "ReadReply: Disconnected...\n");

            return iResult;
        } else
            iTotalRecved += iResult;

        pPdu->pDataSeg = pPtr;

//        pPtr += ntohl(pPdu->pH2RHeader->DataSegLen);


        if(Session->SessionPhase == FLAG_FULL_FEATURE_PHASE
           && Session->DataEncryptAlgo != 0) {

            CHAP_C = NDASSwap32LittleToHost(Session->CHAP_C);
            Decrypt32(
                      (unsigned char*)pPdu->pDataSeg,
                      ntohl(pPdu->pH2RHeader->DataSegLen),
                      (unsigned char *)&CHAP_C,
                      (unsigned char *)Session->Password
                      );
        }

    }

    // Read Data Dig.
    pPdu->pDataDig = NULL;

    return iTotalRecved;
}


int SendRequest(struct LanScsiSession *Session, PLANSCSI_PDU pPdu)
{
    PLANSCSI_H2R_PDU_HEADER pHeader;
    int                     iDataSegLen, iAHSLen, iResult;
    uint32_t		CHAP_C;
	
    pHeader = pPdu->pH2RHeader;
    iAHSLen = ntohs(pHeader->AHSLen);
    iDataSegLen = ntohl(pHeader->DataSegLen);
	
    //
    // Encrypt Header.
    //
    if(Session->SessionPhase == FLAG_FULL_FEATURE_PHASE
       && Session->HeaderEncryptAlgo != 0) {

        CHAP_C = NDASSwap32LittleToHost(Session->CHAP_C);
        Encrypt32(
                  (unsigned char*)pHeader,
                  sizeof(LANSCSI_H2R_PDU_HEADER),
                  (unsigned char *)&CHAP_C,
                  (unsigned char*)Session->Password
                  );

		// Encrypt AHS.	
		if(0 < iAHSLen) {
			CHAP_C = NDASSwap32LittleToHost(Session->CHAP_C);
			Encrypt32(
					  (unsigned char*)pPdu->pAHS,
					  iAHSLen,
					  (unsigned char *)&CHAP_C,
					  (unsigned char*)&Session->Password
					  );
		}
    }

    //
    // Encrypt Data.
    //
    if(Session->SessionPhase == FLAG_FULL_FEATURE_PHASE
       && Session->DataEncryptAlgo != 0
       && iDataSegLen > 0) {

        CHAP_C = NDASSwap32LittleToHost(Session->CHAP_C);
        Encrypt32(
                  (unsigned char*)pPdu->pDataSeg,
                  iDataSegLen,
                  (unsigned char *)&CHAP_C,
                  (unsigned char*)Session->Password
                  );
    }

    // Send Request.
    iResult = SendIt(
                     Session->DevSo,
                     (char *)pHeader,
                     sizeof(LANSCSI_H2R_PDU_HEADER) + iDataSegLen + iAHSLen
                     );
    if(iResult != (sizeof(LANSCSI_H2R_PDU_HEADER) + iDataSegLen + iAHSLen)) {
        DebugPrint(1, true, "SendRequest: Send Request Error result %d\n", iResult);
        return -1;
    }

    return 0;
}


int Lba_capacity_is_ok(struct hd_driveid *id )
{
    uint32_t lba_sects, chs_sects, head, tail;

    if((NDASSwap16LittleToHost(id->command_set_2) & 0x0400) && (NDASSwap16LittleToHost(id->cfs_enable_2) & 0x0400)) {
        // 48 Bit Drive.
        return 1;
    }

    /*
     The ATA spec tells large drivers to return
     C/H/S = 16383/16/63 independent of their size.
     Some drives can be jumpered to use 15 heads instead of 16.
     Some drives can be jumpered to use 4092 cyls instead of 16383
     */
    if((NDASSwap16LittleToHost(id->cyls) == 16383 || (NDASSwap16LittleToHost(id->cyls) == 4092 && NDASSwap16LittleToHost(id->cur_cyls) == 16383))
       && NDASSwap16LittleToHost(id->sectors) == 63
       && (NDASSwap16LittleToHost(id->heads) == 15 || NDASSwap16LittleToHost(id->heads) == 16)
       && NDASSwap32LittleToHost(id->lba_capacity) >= (unsigned)(16383 * 63 * NDASSwap16LittleToHost(id->heads)))
        return 1;

    lba_sects = NDASSwap32LittleToHost(id->lba_capacity);
    chs_sects = NDASSwap16LittleToHost(id->cyls) * NDASSwap16LittleToHost(id->heads) * NDASSwap16LittleToHost(id->sectors);

    /* Perform a rough sanity check on lba_sects: within 10% is OK */
    if((lba_sects - chs_sects) < chs_sects / 10) {
        return 1;
    }

    /* Some drives have the word order reversed */
    head = ((lba_sects >> 16) & 0xffff);
    tail = (lba_sects & 0xffff);
    lba_sects = (head | (tail << 16));
    if((lba_sects - chs_sects) < chs_sects / 10) {
        id->lba_capacity = NDASSwap32LittleToHost(lba_sects);
//        IOLog("!!!! Capacity reversed.... !!!!!!!!\n");
        return 1;
    }

    return 0;
}


#ifdef KERNEL
inline size_t RecvIt(xi_socket_t sock_ptr, char *buf, size_t size )
#else 	/* if KERNEL */
inline int RecvIt(int sock, char *buf, int size )
#endif 	/* if KERNEL else */
{
    int res;
    size_t len = size;
	
#if 0
    fprintf(stderr,"RecvIt\n");
#endif /* if 0 */
    while (len > 0) {
#ifdef KERNEL
        if ((res = xi_lpx_recv(sock_ptr, buf, len, 0)) <= 0) {
#else 	/* if KERNEL */
		if ((res = recv(sock, buf, len, 0)) <= 0) {
#endif 	/* if KERNEL else */
#if 0
			DebugPrint(1, true, "RecvIt: ");
#endif /* if 0 */
			return res;
		}
		len -= res;
		buf += res;
	}
		
	return size;
}
	
	
#ifdef KERNEL
inline size_t SendIt(xi_socket_t sock_ptr, char *buf, size_t size )
#else 	/* if KERNEL */
inline int SendIt(int sock, char *buf, int size )
#endif 	/* if KERNEL else */
{
	int res;
	size_t len = size;
			
#ifdef KERNEL
//    IOLog("SEND: Socket@%lx, buffer@%lx, len = %d\n", sock, buf, len);
#else /* if KERNEL */
//    fprintf(stderr, "SEND: Socket(%d), buffer@%lx, len = %d\n", sock, buf, len);
#endif /* if KERNEL else */
			
	while (len > 0) {
				
#ifdef KERNEL
		if ((res = xi_lpx_send(sock_ptr, buf, len, 0)) <= 0) {
#else 	/* if KERNEL */
		if ((res = send(sock, buf, len, 0)) <= 0) {
#endif 	/* if KERNEL else */
			DebugPrint(4, true, "SendIt: Send Error\n");
			return res;
		}
		len -= res;
		buf += res;
	}
			
	return size;
}
			
void ConvertString(char * result, char * source, int size )
{
    int i;

    for(i = 0; i < size / 2; i++) {
        result[i * 2] = source[i * 2 + 1];
        result[i * 2 + 1] = source[i * 2];
    }
/*
	for(i = 0; i < size / 2; i++) {
        result[i * 2] = source[i * 2];
        result[i * 2 + 1] = source[i * 2 + 1];
    }
*/
	result[size] = '\0';
}
