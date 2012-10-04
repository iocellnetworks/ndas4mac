#include <CoreServices/CoreServices.h>
#include <syslog.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <paths.h>
#include <sysexits.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/network/IOEthernetInterface.h>
#include <IOKit/network/IOEthernetController.h>

#include "GetEthernetAddrSample.h"

#include "lpx.h"
#include "NDASNotification.h"
#include "NDASGetInformation.h"
#include "ndasHixUtil.h"


#ifndef RTL_NUMBER_OF
#define RTL_NUMBER_OF(A) (sizeof(A)/sizeof((A)[0]))
#endif

#define DBGPRT_ERR(...)	

extern EnetData	ethernetData[];
extern UInt32	numberOfNics;

typedef UInt32 (*GET_HOST_INFO_PROC)(UInt32 cbBuffer, UInt8 *lpBuffer);

UInt32 pGetOSFamily(UInt32 cbBuffer, UInt8 *lpbBuffer)
{
	const UInt32 cbRequired = sizeof(uint8_t);
	if (cbBuffer < cbRequired) return 0;
	
	UInt8* puc = (UInt8*) lpbBuffer;
	*puc = NHIX_HIC_OS_MAC;
	
	return cbRequired;
}

UInt32 pGetOSVerInfo(UInt32 cbBuffer, UInt8* lpbBuffer)
{
	const UInt32 cbRequired = sizeof(NDAS_HIX::HOST_INFO::VER_INFO);
	if (cbBuffer < cbRequired) return 0;
	
	SInt32					osVersion; 
	
	Gestalt(gestaltSystemVersion, &osVersion);
	
//	OSVERSIONINFOEX osvi = {0};
//	osvi.dwOSVersionInfoSize = sizeof(osvi);
//	BOOL fSuccess = ::GetVersionEx((LPOSVERSIONINFO)&osvi);
//	_ASSERTE(fSuccess);
	
	NDAS_HIX::HOST_INFO::PVER_INFO pVerInfo = 
		(NDAS_HIX::HOST_INFO::PVER_INFO) lpbBuffer;
	
	pVerInfo->VersionMajor = htons(((osVersion & 0x0000FF00) >> 8)); //htons(osvi.dwMajorVersion);
	pVerInfo->VersionMinor = htons((osVersion & 0x000000F0) >> 4);	//htons(osvi.dwMinorVersion);
	pVerInfo->VersionBuild = htons((osVersion & 0x0000000F));	//htons(osvi.dwBuildNumber);
	pVerInfo->VersionPrivate = htons(0); //htons(osvi.wServicePackMajor);
	
	return cbRequired;
}

UInt32 pGetNDASSWVerInfo(UInt32 cbBuffer, UInt8* lpbBuffer)
{
	const UInt32 cbRequired = sizeof(NDAS_HIX::HOST_INFO::VER_INFO);
	if (cbBuffer < cbRequired) return 0;
	
	NDAS_HIX::HOST_INFO::PVER_INFO pVerInfo = 
		(NDAS_HIX::HOST_INFO::PVER_INFO) lpbBuffer;
	
	// BUG BUG BUG
	pVerInfo->VersionMajor = htons(1);//htons(VER_FILEMAJORVERSION);
	pVerInfo->VersionMinor = htons(6);//htons(VER_FILEMINORVERSION);
	pVerInfo->VersionBuild = htons(0);//htons(VER_FILEBUILD);
	pVerInfo->VersionPrivate = htons(0);//htons(VER_FILEBUILD_QFE);
	
	return cbRequired;
}

UInt32 pGetUnicodeHostname(UInt32 cbBuffer, UInt8* lpBuffer)
{
	CFStringRef	cfsName;
	UInt32		cbRequired;
	SInt32		osVersion; 

	cfsName = CSCopyMachineName ();
	if (NULL == cfsName) {
		return 0;
	}
	
	cbRequired = (((CFStringGetLength(cfsName) + 1) * 2 > cbBuffer) ? cbBuffer : (CFStringGetLength(cfsName) + 1) * 2);
	
	memset(lpBuffer, 0, cbRequired);
	
	if (Gestalt(gestaltSystemVersion, &osVersion) == noErr) 
	{ 
		if ( osVersion >= 0x1040 ) 
		{
#if MAC_OS_X_VERSION_10_4 <= MAC_OS_X_VERSION_MAX_ALLOWED

			CFStringGetCString (
								cfsName,
								(char *)lpBuffer,
								cbBuffer,
								kCFStringEncodingUTF16BE
								);
#endif
			
		} else {

			CFStringGetCString (
								cfsName,
								(char *)lpBuffer,
								cbBuffer,
								kCFStringEncodingUnicode
								);				
		}
	} 
		
	CFRelease(cfsName);
	
	return cbRequired;
}

UInt32 pGetNDASTransport(UInt32 cbBuffer, UInt8* lpbBuffer)
{
	const UInt32 FIELD_SIZE = sizeof(NHIX_HIC_TRANSPORT_TYPE);
	if (cbBuffer < FIELD_SIZE) {
		return 0;
	}
	
	*lpbBuffer = NHIX_TF_LPX;
	// *lpbBuffer = NHIX_TF_IP;
	return FIELD_SIZE;
}

UInt32 pGetNDASHostFeature(UInt32 cbBuffer, UInt8* lpbBuffer)
{
	const UInt32 FIELD_SIZE = sizeof(NHIX_HIC_HOST_FEATURE_TYPE);
	if (cbBuffer < FIELD_SIZE) {
		return 0;
	}
	
	*((uint32_t*)lpbBuffer) = htonl(NHIX_HFF_DEFAULT);
	return FIELD_SIZE;
}
/*
DWORD pGetLpxAddr(DWORD cbBuffer, LPBYTE lpbBuffer)
{
	static const LPX_ADDRESS_LEN = 6 * sizeof(uint8_t);
	static const MAX_ADDRESS_COUNT = 4; // maximum 4 addresses are reported
	
	AutoSocket s = ::WSASocket(AF_LPX, SOCK_DGRAM, LPXPROTO_DGRAM, NULL, 0, 0);
	if (INVALID_SOCKET == s) {
		return 0;
	}
	
	LPSOCKET_ADDRESS_LIST pAddrList = pCreateSocketAddressList(s);
	if (NULL == pAddrList) {
		return 0;
	}
	AutoHLocal hLocal = (HLOCAL) pAddrList;
	
	DWORD cbUsed = 2;
	if (cbBuffer < cbUsed) {
		return 0;
	}
	
	PUCHAR pucCount = lpbBuffer;
	++lpbBuffer;
	PUCHAR pucAddressLen = lpbBuffer;
	++lpbBuffer;
	
	*pucAddressLen = LPX_ADDRESS_LEN;
	*pucCount = 0;
	
	for (DWORD i = 0; 
		 i < pAddrList->iAddressCount && 
		 cbUsed + LPX_ADDRESS_LEN <= cbBuffer &&
		 i < MAX_ADDRESS_COUNT; 
		 ++i) 
	{
		PSOCKADDR_LPX pSockAddrLpx = (PSOCKADDR_LPX)
		pAddrList->Address[i].lpSockaddr;
		::CopyMemory(
					 lpbBuffer, 
					 &pSockAddrLpx->LpxAddress.Node[0], 
					 LPX_ADDRESS_LEN);
		++(*pucCount);
		cbUsed += LPX_ADDRESS_LEN;
		lpbBuffer += LPX_ADDRESS_LEN;
	}
	
	return cbUsed;
}*/
/*
DWORD pGetIPv4Addr(DWORD cbBuffer, LPBYTE lpbBuffer)
{
	static const INET_ADDRESS_LEN = 4 * sizeof(uint8_t);
	static const MAX_ADDRESS_COUNT = 4; // maximum 4 ip addresses are reported
	
	AutoSocket s = ::WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, 0);
	if (INVALID_SOCKET == s) {
		return 0;
	}
	
	LPSOCKET_ADDRESS_LIST pAddrList = pCreateSocketAddressList(s);
	if (NULL == pAddrList) {
		return 0;
	}
	AutoHLocal hLocal = (HLOCAL) pAddrList;
	
	DWORD cbUsed = 2;
	if (cbBuffer < cbUsed) {
		return 0;
	}
	
	PUCHAR pucCount = lpbBuffer;
	++lpbBuffer;
	PUCHAR pucAddressLen = lpbBuffer;
	++lpbBuffer;
	*pucAddressLen = INET_ADDRESS_LEN;
	*pucCount = 0;
	
	for (DWORD i = 0; 
		 i < pAddrList->iAddressCount && 
		 cbUsed + INET_ADDRESS_LEN <= cbBuffer &&
		 i < MAX_ADDRESS_COUNT; 
		 ++i) 
	{
		PSOCKADDR_IN pSockAddr = (PSOCKADDR_IN)
		pAddrList->Address[i].lpSockaddr;
		::CopyMemory(
					 lpbBuffer, 
					 &pSockAddr->sin_addr,
					 INET_ADDRESS_LEN);
		++(*pucCount);
		cbUsed += INET_ADDRESS_LEN;
		lpbBuffer += INET_ADDRESS_LEN;
	}
	
	return cbUsed;
}
*/
UInt32 pGetHostInfo(UInt32 cbBuffer, PNDAS_HIX_HOST_INFO_DATA pHostInfoData)
{
	const UInt32 CB_DATA_HEADER = 
	sizeof(NDAS_HIX::HOST_INFO::DATA) -
	sizeof(NDAS_HIX::HOST_INFO::DATA::ENTRY);
	
	const UInt32 CB_ENTRY_HEADER = 
		sizeof(NDAS_HIX::HOST_INFO::DATA::ENTRY) -
		sizeof(uint8_t);
	
	static struct _PROC_TABLE {
		uint32_t Class;
		GET_HOST_INFO_PROC Proc;
	} procTable[] = {
	{NHIX_HIC_OS_FAMILY, pGetOSFamily},
	{NHIX_HIC_OS_VER_INFO, pGetOSVerInfo},
		//		{NHIX_HIC_NDFS_VER_INFO, pGetNDFSVerInfo},
	{NHIX_HIC_NDAS_SW_VER_INFO, pGetNDASSWVerInfo},
	{NHIX_HIC_UNICODE_HOSTNAME, pGetUnicodeHostname},
		//		{NHIX_HIC_UNICODE_FQDN,		pGetUnicodeFQDN},
		//		{NHIX_HIC_UNICODE_NETBIOSNAME, pGetUnicodeNetBIOSName},
		//		{NHIX_HIC_HOSTNAME,	pGetAnsiHostname},
		//		{NHIX_HIC_FQDN, pGetAnsiFQDN},
		//		{NHIX_HIC_NETBIOSNAME, pGetAnsiNetBIOSName},
	{NHIX_HIC_TRANSPORT, pGetNDASTransport},
	{NHIX_HIC_HOST_FEATURE, pGetNDASHostFeature},
	//{NHIX_HIC_ADDR_LPX, pGetLpxAddr},
	//{NHIX_HIC_ADDR_IPV4, pGetIPv4Addr}
		//		{NHIX_HIC_ADDR_IPV6, pGetIPv6Addr},
	};
	
	UInt8 *lpb = (UInt8 *) pHostInfoData;
	UInt32 cbBufferUsed = 0;
	UInt32 cbBufferRemaining = cbBuffer;
	
	PNDAS_HIX_HOST_INFO_DATA pData = pHostInfoData;
	
//	_ASSERTE(cbBufferRemaining >= CB_DATA_HEADER);
	
	pData->Count = 0;
	pData->Contains = 0;
	
	cbBufferUsed += CB_DATA_HEADER;
	cbBufferRemaining -= CB_DATA_HEADER;
	
	if (cbBufferRemaining == 0) {
		pData->Length = cbBufferUsed;
		return cbBufferUsed;
	}
	
	lpb += CB_DATA_HEADER;
	
	//
	// Get Host Information
	//
	for (UInt32 i = 0; i < RTL_NUMBER_OF(procTable); ++i) {
		
		if (cbBufferRemaining < (CB_ENTRY_HEADER + 1)) {
			// not enough buffer
			break;
		}
		
		NDAS_HIX::HOST_INFO::DATA::PENTRY pEntry = 
		reinterpret_cast<NDAS_HIX::HOST_INFO::DATA::PENTRY>(lpb);
		
		pEntry->Class = htonl(procTable[i].Class);
		pEntry->Length = (uint8_t) CB_ENTRY_HEADER;
		// pEntry->Data;
		
		cbBufferRemaining -= CB_ENTRY_HEADER;
		cbBufferUsed += CB_ENTRY_HEADER;
		
		UInt32 cbUsed = procTable[i].Proc(
										 cbBufferRemaining, 
										 pEntry->Data);
		
//		_ASSERTE(cbUsed <= cbBufferRemaining);
		
		if (cbUsed > 0) {
			
			pData->Contains |= pEntry->Class;
			pEntry->Length += (uint8_t)cbUsed;
			
			lpb += pEntry->Length;
			cbBufferUsed += cbUsed;
			cbBufferRemaining -= cbUsed;
			
			++(pData->Count);
			
		} else {
			
			// rollback
			cbBufferRemaining += CB_ENTRY_HEADER;
			cbBufferUsed -= CB_ENTRY_HEADER;
		}
		
		//_tprintf(_T("%d bytes total, %d bytes used, %d byte remaining."), 
		//	cbBuffer, 
		//	cbBufferUsed,
		//	cbBufferRemaining); 
		
		//		printBytes(cbBufferUsed, lpbBuffer);
	}
	
	//	printBytes(cbBufferUsed, lpbBuffer);
	//	_tprintf(_T("%d bytes total, %d byte used."), cbBuffer, cbBufferUsed); 
	
	//
	// Maximum length is 256 bytes
	pData->Length = (0xFF > cbBufferUsed) ? cbBufferUsed : 0xFF;
	return cbBufferUsed;
}

BOOL
pIsValidNHIXHeader(UInt32 cbData, const NDAS_HIX::HEADER* pHeader)
{
	if (cbData < sizeof(NDAS_HIX::HEADER)) {
		DBGPRT_ERR(_FT("NHIX Data Buffer too small: ")
				   _T("Buffer %d bytes, Required at least %d bytes."), 
				   cbData, sizeof(NDAS_HIX::HEADER));
		return FALSE;
	}
	
	if (cbData < ntohs(pHeader->Length)) {
		DBGPRT_ERR(_FT("NHIX Data buffer too small: ")
				   _T("Buffer %d bytes, Len in header %d bytes."),
				   cbData, pHeader->Length);
		return FALSE;
	}
	
	if (NDAS_HIX_SIGNATURE != ntohl(pHeader->Signature)) {
		DBGPRT_ERR(_FT("NHIX Header Signature Mismatch [%c%c%c%c]"),
				   pHeader->SignatureChars[0],
				   pHeader->SignatureChars[1],
				   pHeader->SignatureChars[2],
				   pHeader->SignatureChars[3]);
		return FALSE;
	}
	
	if (NHIX_CURRENT_REVISION != pHeader->Revision) {
		DBGPRT_ERR(_FT("Unsupported NHIX Revision %d, Required Rev. %d"),
				   pHeader->Revision, NHIX_CURRENT_REVISION);
		return FALSE;
	}
	
	return TRUE;
}


BOOL
pIsValidNHIXRequestHeader(UInt32 cbData, const NDAS_HIX::HEADER* pHeader)
{
	if (!pIsValidNHIXHeader(cbData, pHeader)) {
		return FALSE;
	}
	
	if (pHeader->ReplyFlag) {
		DBGPRT_ERR(_FT("ReplyFlag is set to request header."));
		return FALSE;
	}
	
	return TRUE;
}

BOOL 
pIsValidNHIXReplyHeader(UInt32 cbData, const NDAS_HIX::HEADER* pHeader)
{
	if (!pIsValidNHIXHeader(cbData, pHeader)) {
		return FALSE;
	}
	
	if (!pHeader->ReplyFlag) {
		DBGPRT_ERR(_FT("ReplyFlag is not set to reply header."));
		return FALSE;
	}
	
	return TRUE;
}

void
pBuildNHIXHeader(
				 NDAS_HIX::PHEADER pHeader,
				 CFUUIDBytes pHostGuid,
				 BOOL ReplyFlag, 
				 UInt8 Type, 
				 UInt16 Length)
{
//	_ASSERTE(!IsBadWritePtr(pHeader, sizeof(NDAS_HIX::HEADER)));
	
	pHeader->Signature = htonl(NDAS_HIX_SIGNATURE);
	memcpy(&pHeader->HostId, &pHostGuid, sizeof(pHeader->HostId));
	pHeader->ReplyFlag = (ReplyFlag) ? 1 : 0;
	pHeader->Type = Type;
	pHeader->Revision = NHIX_CURRENT_REVISION;
	pHeader->Length = htons(Length);
}

int 
pSendDiscover(
			  CFUUIDBytes pHostGuid,
			  PNDAS_HIX_UNITDEVICE_ENTRY_DATA	data,
			  struct sockaddr_lpx				*host
			  )
{
	NDAS_HIX_DISCOVER_REQUEST	request;
	CFDataRef					cfdRequest = NULL;
		
	// Build Request.
	pBuildNHIXHeader(
					 &request.Header,
					 pHostGuid,
					 0,
					 NHIX_TYPE_DISCOVER,
					 sizeof(NDAS_HIX_SURRENDER_ACCESS_REQUEST)
					 );
	
	request.Data.EntryCount = 1;
	
	memcpy(&request.Data.Entry[0], data, sizeof(NDAS_HIX_UNITDEVICE_ENTRY_DATA));

	cfdRequest = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, (UInt8 *) &request, sizeof(NDAS_HIX_SURRENDER_ACCESS_REQUEST), kCFAllocatorNull);

	// Send Request.
	// Per Interface.
	
	CFSocketRef				sockets[4];
	int						numInterfaces = 0;
	
	for(int i = 0; i < numberOfNics; i++) {
	
		CFDataRef	address = 0;
		struct sockaddr_lpx	addr = { 0 };
				
		// Build Socket.
		sockets[numInterfaces] = CFSocketCreate (
												 kCFAllocatorDefault,
												 PF_LPX,
												 SOCK_DGRAM,
												 0,
												 0,
												 0,
												 NULL
												 );
		if(sockets[numInterfaces] == NULL) {
			syslog(LOG_ERR, "pSendSurrenderAccess: Can't Create socket for surrender Access request.");
			
			return -1;
		}
		
		// Bind Interface Address.
		addr.slpx_family = AF_LPX;
		addr.slpx_len = sizeof(struct sockaddr_lpx);
		addr.slpx_port = 0;
		memcpy(addr.slpx_node, ethernetData[i].macAddress, 6);
		
		address = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, 
											  (UInt8 *) &addr, 
											  sizeof(struct sockaddr_lpx), 
											  kCFAllocatorNull);
		
		CFSocketError socketError = CFSocketSetAddress (
														sockets[numInterfaces],
														address
														);
		if(socketError != kCFSocketSuccess) {
			syslog(LOG_ERR, "pSendSurrenderAccess: Can't Bind.");
			continue;
		}
		
		if(address)		CFRelease(address);		address= NULL;
		
		// Create Broadcast address.
		addr.slpx_family = AF_LPX;
		addr.slpx_len = sizeof(struct sockaddr_lpx);
		memset(addr.slpx_node, 0xff, 6);
		addr.slpx_port = htons(NDAS_HIX_LISTEN_PORT);
		
		address = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, 
											  (UInt8 *) &addr, 
											  sizeof(struct sockaddr_lpx), 
											  kCFAllocatorNull);
		
		// Set Broadcast Flag.
		int on = 1;
		if(setsockopt(CFSocketGetNative(sockets[numInterfaces]), SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)) != 0) {
			syslog(LOG_ERR, "pSendSurrenderAccess: Error when set option.");
			return false;
		}
		
		// Send Request.
		socketError = CFSocketSendData (
										sockets[numInterfaces],
										address,
										cfdRequest,
										0
										);
		
		if(socketError != kCFSocketSuccess) {
			syslog(LOG_ERR, "pSendSurrenderAccess: Error when send reply.");
			return false;
		}
		
		numInterfaces++;
		
		if(address)		CFRelease(address);
		
	}
		
	if(cfdRequest)	CFRelease(cfdRequest);
	
	// Receive Reply.
	int			maxfdp1;
	socklen_t			fromlen;
	fd_set	rset;
	struct timeval timeout;
	int		bResult;
	struct sockaddr_lpx	addr = { 0 };
	int count;

	int numberOfHosts = 0;

	// HostID Dictionary.
	CFMutableDictionaryRef	HostIDs = CFDictionaryCreateMutable (
																 kCFAllocatorDefault,
																 0,
																 &kCFTypeDictionaryKeyCallBacks,
																 &kCFTypeDictionaryValueCallBacks
																 );
	if (NULL == HostIDs) {
		return -1;
	}
	
retry:

	FD_ZERO(&rset);
	
	maxfdp1 = 0;
	for(count = 0; count < numInterfaces; count++) {
		FD_SET( CFSocketGetNative(sockets[count]), &rset);
		maxfdp1 = (maxfdp1 > CFSocketGetNative(sockets[count])) ? maxfdp1 : CFSocketGetNative(sockets[count]);	
	}
	maxfdp1++;
	
	timeout.tv_sec = 2;	// 2 sec.
	timeout.tv_usec = 0;
			
	int result = select(maxfdp1, &rset, NULL, NULL, &timeout);
	if(result > 0) {
		NDAS_HIX_DISCOVER_REPLY	reply;
		int len;
		
		fromlen = sizeof(struct sockaddr_lpx);
		
		// Find socket.
		for(count = 0; count < numInterfaces; count++) {
			if (FD_ISSET( CFSocketGetNative(sockets[count]), &rset)) {
				
				len = recvfrom(CFSocketGetNative(sockets[count]), &reply, sizeof(NDAS_HIX_DISCOVER_REPLY), 0, (sockaddr *)&addr, &fromlen);
				
				if ( len < sizeof(NDAS_HIX_DISCOVER_REPLY) ) {
					syslog(LOG_WARNING, "pSendDiscover: Too short HIX Discover reply len : %d\n", len);
				
					continue;
				}
					
//				syslog(LOG_ERR, "pSendDiscover: Host %2x:%2x:%2x:%2x:%2x:%2x %d\n",
//					   addr.slpx_node[0], addr.slpx_node[1], addr.slpx_node[2], 
//					   addr.slpx_node[3], addr.slpx_node[4], addr.slpx_node[5], fromlen);
				
				//				syslog(LOG_ERR, "pSendDiscover: Access : %x", reply.Data.Entry[0].AccessType);
				
				if ( len >= sizeof(NDAS_HIX_DISCOVER_REPLY) ) {
					
					if (reply.Data.EntryCount == 1
						&& memcmp(data->DeviceId, reply.Data.Entry[0].DeviceId, 6) == 0
						&& data->UnitNo == reply.Data.Entry[0].UnitNo) {
						
						// Check Duplecation.
						CFDataRef hostID = CFDataCreate(
														kCFAllocatorDefault,
														reply.Header.HostId,
														16
														);
						if (NULL == hostID) {
							goto Out;
						}
						
						if (0 == CFDictionaryGetCountOfKey(HostIDs, hostID)) {
							
							// New Host.
							CFDictionarySetValue(
												 HostIDs,
												 hostID,
												 hostID);
														
							
							syslog(LOG_DEBUG, "[%s] HIX Replay Type : %d\n", __FUNCTION__, reply.Data.Entry[0].AccessType);
							
							switch (reply.Data.Entry[0].AccessType) {
								case NHIX_UDA_WRITE_ACCESS:
								case NHIX_UDA_READ_WRITE_ACCESS:
								case NHIX_UDA_SHARED_READ_WRITE_SECONDARY_ACCESS:
								case NHIX_UDA_SHARED_READ_WRITE_PRIMARY_ACCESS:
								{
									numberOfHosts++;
									
									// Copy Address.
									if (host) {
										memcpy(host, &addr, sizeof(struct sockaddr_lpx));
									}
								}
									break;
								default:
									break;
							}	
						} else {
							syslog(LOG_INFO, "pSendDiscover: Duplicated replay.");
						}
						
						CFRelease(hostID);
					}	
				}
			}
		}
		
		goto  retry;

	} else {
		// Error or timeout.
//		syslog(LOG_ERR, "pSendDiscover: select error or timeout %d Hosts %d\n", result, numberOfHosts);
		
		bResult =  numberOfHosts;
	}
	
	//if(socketSurrender) CFRelease(socketSurrender);     

Out:
		
	if (HostIDs) {
		CFDictionaryRemoveAllValues(HostIDs);
		CFRelease(HostIDs);
	}
	
	for(count = 0; count < numInterfaces; count++) {
		if(sockets[count]) CFRelease(sockets[count]);	
	}
		
	return bResult;
}

bool 
pSendSurrenderAccess(
					 CFUUIDBytes pHostGuid,
					 PNDAS_HIX_UNITDEVICE_ENTRY_DATA	data,
					 struct  sockaddr_lpx				*hostAddr
					 )
{
	NDAS_HIX_SURRENDER_ACCESS_REQUEST	request;
	CFDataRef							cfdRequest = NULL;
		
	// Build Request.
	pBuildNHIXHeader(
					 &request.Header,
					 pHostGuid,
					 0,
					 NHIX_TYPE_SURRENDER_ACCESS,
					 sizeof(NDAS_HIX_SURRENDER_ACCESS_REQUEST)
					 );
	
	request.Data.EntryCount = 1;
	
	memcpy(&request.Data.Entry[0], data, sizeof(NDAS_HIX_UNITDEVICE_ENTRY_DATA));

	cfdRequest = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, (UInt8 *) &request, sizeof(NDAS_HIX_SURRENDER_ACCESS_REQUEST), kCFAllocatorNull);

	// Send Request.
	// Per Interface.

	CFSocketRef				sockets[4];
	int						numInterfaces = 0;

	for(int i = 0; i < numberOfNics; i++) {
		
		CFDataRef	address = 0;
		struct sockaddr_lpx	addr = { 0 };
				
		// Build Socket.
		sockets[numInterfaces] = CFSocketCreate (
												 kCFAllocatorDefault,
												 PF_LPX,
												 SOCK_DGRAM,
												 0,
												 0,
												 0,
												 NULL
												 );
		if(sockets[numInterfaces] == NULL) {
			syslog(LOG_ERR, "pSendSurrenderAccess: Can't Create socket for surrender Access request.");
			
			return false;
		}
		
		// Bind Interface Address.
		addr.slpx_family = AF_LPX;
		addr.slpx_len = sizeof(struct sockaddr_lpx);
		addr.slpx_port = 0;
		memcpy(addr.slpx_node, ethernetData[i].macAddress, 6);
		
		address = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, 
											  (UInt8 *) &addr, 
											  sizeof(struct sockaddr_lpx), 
											  kCFAllocatorNull);
		
		CFSocketError socketError = CFSocketSetAddress (
														sockets[numInterfaces],
														address
														);
		if(socketError != kCFSocketSuccess) {
			syslog(LOG_ERR, "pSendSurrenderAccess: Can't Bind.");
			continue;
		}
		
		if(address)		CFRelease(address);		address= NULL;
		
		// Bind Peer Host Address.				
		address = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, 
											  (UInt8 *) hostAddr, 
											  sizeof(struct sockaddr_lpx), 
											  kCFAllocatorNull);
		
		// Send Request.
		socketError = CFSocketSendData (
										sockets[numInterfaces],
										address,
										cfdRequest,
										0
										);
		
		if(socketError != kCFSocketSuccess) {
			syslog(LOG_ERR, "pSendSurrenderAccess: Error when send reply.");
			return false;
		}
		
		numInterfaces++;
		
		if(address)		CFRelease(address);
		
	}
		
	if(cfdRequest)	CFRelease(cfdRequest);
	
	// Receive Reply.
	int			maxfdp1;
	socklen_t			fromlen;
	fd_set	rset;
	struct timeval timeout;
	bool	bResult;
	struct sockaddr_lpx	addr;
	int count;
	
	FD_ZERO(&rset);
	
	maxfdp1 = 0;
	for(count = 0; count < numInterfaces; count++) {
		FD_SET( CFSocketGetNative(sockets[count]), &rset);
		maxfdp1 = (maxfdp1 > CFSocketGetNative(sockets[count])) ? maxfdp1 : CFSocketGetNative(sockets[count]);	
	}
	maxfdp1++;
	
	timeout.tv_sec = 20;	// 20 sec.
	timeout.tv_usec = 0;
	
	int result = select(maxfdp1, &rset, NULL, NULL, &timeout);
	if(result > 0) {
		NDAS_HIX_SURRENDER_ACCESS_REPLY	reply;
		
		for(count = 0; count < numInterfaces; count++) {
			if (FD_ISSET( CFSocketGetNative(sockets[count]), &rset)) {
				
				int len;
				
				len = recvfrom(CFSocketGetNative(sockets[count]), &reply, sizeof(NDAS_HIX_SURRENDER_ACCESS_REPLY), 0, (sockaddr *)&addr, &fromlen);

				if(reply.Data.Status == NHIX_SURRENDER_REPLY_STATUS_QUEUED) {
					bResult = true;
				} else {
					bResult = false;
				}
			}
		}
	} else {
		// Error or timeout.
		syslog(LOG_ERR, "pSendSurrenderAccess: select error or timeout %d\n", result);
		
		bResult =  false;
	}
	
	//if(socketSurrender) CFRelease(socketSurrender);     
	for(count = 0; count < numInterfaces; count++) {
		if(sockets[count]) CFRelease(sockets[count]);	
	}
	
	return bResult;
}

bool 
pSendUnitDeviceChangeNotification(
								  CFUUIDBytes pHostGuid,
								  PNDAS_HIX_UNITDEVICE_CHANGE_NOTIFY	data
								  )
{
	CFDataRef							cfdRequest = NULL;
	
	// Build Request.
	pBuildNHIXHeader(
					 &data->Header,
					 pHostGuid,
					 0,
					 NHIX_TYPE_UNITDEVICE_CHANGE,
					 sizeof(NDAS_HIX_UNITDEVICE_CHANGE_NOTIFY)
					 );
	
	cfdRequest = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, (UInt8 *) data, sizeof(NDAS_HIX_UNITDEVICE_CHANGE_NOTIFY), kCFAllocatorNull);
	
	// Send Request.
	// Per Interface.
	
	CFSocketRef				sockets[16];
	int						numInterfaces = 0;

	for(int i = 0; i < numberOfNics; i++) {
	
		CFDataRef	address = 0;
		struct sockaddr_lpx	addr = { 0 };
		
		// Build Socket.
		sockets[numInterfaces] = CFSocketCreate (
												 kCFAllocatorDefault,
												 PF_LPX,
												 SOCK_DGRAM,
												 0,
												 0,
												 0,
												 NULL
												 );
		if(sockets[numInterfaces] == NULL) {
			syslog(LOG_ERR, "pSendSurrenderAccess: Can't Create socket for surrender Access request.");
			
			return false;
		}
		
		// Bind Interface Address.
		addr.slpx_family = AF_LPX;
		addr.slpx_len = sizeof(struct sockaddr_lpx);
		addr.slpx_port = 0;
		memcpy(addr.slpx_node, ethernetData[i].macAddress, 6);
		
		address = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, 
											  (UInt8 *) &addr, 
											  sizeof(struct sockaddr_lpx), 
											  kCFAllocatorNull);
		
		CFSocketError socketError = CFSocketSetAddress (
														sockets[numInterfaces],
														address
														);
		if(socketError != kCFSocketSuccess) {
			syslog(LOG_ERR, "pSendSurrenderAccess: Can't Bind.");
			continue;
		}
		
		if(address)		CFRelease(address);		address= NULL;
		
		// Create Broadcast address.
		addr.slpx_family = AF_LPX;
		addr.slpx_len = sizeof(struct sockaddr_lpx);
		memset(addr.slpx_node, 0xff, 6);
		addr.slpx_port = htons(NDAS_HIX_LISTEN_PORT);
		
		address = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, 
											  (UInt8 *) &addr, 
											  sizeof(struct sockaddr_lpx), 
											  kCFAllocatorNull);
		
		// Set Broadcast Flag.
		int on = 1;
		if(setsockopt(CFSocketGetNative(sockets[numInterfaces]), SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)) != 0) {
			syslog(LOG_ERR, "pSendSurrenderAccess: Error when set option.");
			return false;
		}
		
		// Send Request.
		socketError = CFSocketSendData (
										sockets[numInterfaces],
										address,
										cfdRequest,
										0
										);
		
		if(socketError != kCFSocketSuccess) {
			syslog(LOG_ERR, "pSendSurrenderAccess: Error when send reply.");
			return false;
		}
		
		numInterfaces++;
		
		if(address)		CFRelease(address);
		
	}
	
	if(cfdRequest)	CFRelease(cfdRequest);
	
	return true;
}