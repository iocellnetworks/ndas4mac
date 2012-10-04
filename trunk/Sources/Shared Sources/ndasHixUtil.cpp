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

#include "lpx.h"
#include "NDASNotification.h"
#include "NDASGetInformation.h"
#include "ndasHixUtil.h"


#ifndef RTL_NUMBER_OF
#define RTL_NUMBER_OF(A) (sizeof(A)/sizeof((A)[0]))
#endif

#define DBGPRT_ERR(...)	

typedef UInt32 (*GET_HOST_INFO_PROC)(UInt32 cbBuffer, UInt8 *lpBuffer);

UInt32 pGetOSFamily(UInt32 cbBuffer, UInt8 *lpbBuffer)
{
	const UInt32 cbRequired = sizeof(UCHAR);
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
	
	*lpbBuffer = htonl(NHIX_HFF_DEFAULT);
	return FIELD_SIZE;
}
/*
DWORD pGetLpxAddr(DWORD cbBuffer, LPBYTE lpbBuffer)
{
	static const LPX_ADDRESS_LEN = 6 * sizeof(UCHAR);
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
	static const INET_ADDRESS_LEN = 4 * sizeof(UCHAR);
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
		sizeof(UCHAR);
	
	static struct _PROC_TABLE {
		ULONG Class;
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
		pEntry->Length = (UCHAR) CB_ENTRY_HEADER;
		// pEntry->Data;
		
		cbBufferRemaining -= CB_ENTRY_HEADER;
		cbBufferUsed += CB_ENTRY_HEADER;
		
		UInt32 cbUsed = procTable[i].Proc(
										 cbBufferRemaining, 
										 pEntry->Data);
		
//		_ASSERTE(cbUsed <= cbBufferRemaining);
		
		if (cbUsed > 0) {
			
			pData->Contains |= pEntry->Class;
			pEntry->Length += (UCHAR)cbUsed;
			
			lpb += pEntry->Length;
			cbBufferUsed += cbUsed;
			cbBufferRemaining -= cbUsed;
			
			++(pData->Count);
			
		} else {
			
			// rollback
			cbBufferRemaining += CB_ENTRY_HEADER;
			cbBufferUsed -= CB_ENTRY_HEADER;
		}
		
		//_tprintf(_T("%d bytes total, %d bytes used, %d byte remaining.\n"), 
		//	cbBuffer, 
		//	cbBufferUsed,
		//	cbBufferRemaining); 
		
		//		printBytes(cbBufferUsed, lpbBuffer);
	}
	
	//	printBytes(cbBufferUsed, lpbBuffer);
	//	_tprintf(_T("%d bytes total, %d byte used.\n"), cbBuffer, cbBufferUsed); 
	
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
				   _T("Buffer %d bytes, Required at least %d bytes.\n"), 
				   cbData, sizeof(NDAS_HIX::HEADER));
		return FALSE;
	}
	
	if (cbData < ntohs(pHeader->Length)) {
		DBGPRT_ERR(_FT("NHIX Data buffer too small: ")
				   _T("Buffer %d bytes, Len in header %d bytes.\n"),
				   cbData, pHeader->Length);
		return FALSE;
	}
	
	if (NDAS_HIX_SIGNATURE != ntohl(pHeader->Signature)) {
		DBGPRT_ERR(_FT("NHIX Header Signature Mismatch [%c%c%c%c]\n"),
				   pHeader->SignatureChars[0],
				   pHeader->SignatureChars[1],
				   pHeader->SignatureChars[2],
				   pHeader->SignatureChars[3]);
		return FALSE;
	}
	
	if (NHIX_CURRENT_REVISION != pHeader->Revision) {
		DBGPRT_ERR(_FT("Unsupported NHIX Revision %d, Required Rev. %d\n"),
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
		DBGPRT_ERR(_FT("ReplyFlag is set to request header.\n"));
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
		DBGPRT_ERR(_FT("ReplyFlag is not set to reply header.\n"));
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

// Returns an iterator across all known Ethernet interfaces. Caller is responsible for
// releasing the iterator when iteration is complete.
kern_return_t FindEthernetInterfaces(io_iterator_t *matchingServices)
{
    kern_return_t		kernResult; 
    mach_port_t			masterPort;
    CFMutableDictionaryRef	classesToMatch;
	
	/*! @function IOMasterPort
		@abstract Returns the mach port used to initiate communication with IOKit.
		@discussion Functions that don't specify an existing object require the IOKit master port to be passed. This function obtains that port.
		@param bootstrapPort Pass MACH_PORT_NULL for the default.
    @param masterPort The master port is returned.
		@result A kern_return_t error code. */
	
    kernResult = IOMasterPort(MACH_PORT_NULL, &masterPort);
    if (KERN_SUCCESS != kernResult)
        printf("IOMasterPort returned %d\n", kernResult);
	
	/*! @function IOServiceMatching
		@abstract Create a matching dictionary that specifies an IOService class match.
		@discussion A very common matching criteria for IOService is based on its class. IOServiceMatching will create a matching dictionary that specifies any IOService of a class, or its subclasses. The class is specified by C-string name.
		@param name The class name, as a const C-string. Class matching is successful on IOService's of this class or any subclass.
		@result The matching dictionary created, is returned on success, or zero on failure. The dictionary is commonly passed to IOServiceGetMatchingServices or IOServiceAddNotification which will consume a reference, otherwise it should be released with CFRelease by the caller. */
	
    // Ethernet interfaces are instances of class kIOEthernetInterfaceClass
    classesToMatch = IOServiceMatching(kIOEthernetInterfaceClass);
	
    // Note that another option here would be:
    // classesToMatch = IOBSDMatching("enX");
    // where X is a number from 0 to the number of Ethernet interfaces on the system - 1.
    
    if (classesToMatch == NULL)
        printf("IOServiceMatching returned a NULL dictionary.\n");
    
    /*! @function IOServiceGetMatchingServices
        @abstract Look up registered IOService objects that match a matching dictionary.
        @discussion This is the preferred method of finding IOService objects currently registered by IOKit. IOServiceAddNotification can also supply this information and install a notification of new IOServices. The matching information used in the matching dictionary may vary depending on the class of service being looked up.
        @param masterPort The master port obtained from IOMasterPort().
        @param matching A CF dictionary containing matching information, of which one reference is consumed by this function. IOKitLib can contruct matching dictionaries for common criteria with helper functions such as IOServiceMatching, IOOpenFirmwarePathMatching.
        @param existing An iterator handle is returned on success, and should be released by the caller when the iteration is finished.
        @result A kern_return_t error code. */
	
    kernResult = IOServiceGetMatchingServices(masterPort, classesToMatch, matchingServices);    
    if (KERN_SUCCESS != kernResult)
        printf("IOServiceGetMatchingServices returned %d\n", kernResult);
	
    return kernResult;
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
	io_object_t		intfService;
    io_object_t		controllerService;
    kern_return_t	kernResult = KERN_FAILURE;    
	io_iterator_t	intfIterator;

	CFSocketRef				sockets[4];
	int						numInterfaces = 0;
	
    kernResult = FindEthernetInterfaces(&intfIterator);
	if (KERN_SUCCESS != kernResult) {
        printf("FindEthernetInterfaces returned 0x%08x\n", kernResult);
		
		return -1;
	}
	
	/*! @function IOIteratorNext
		@abstract Returns the next object in an iteration.
		@discussion This function returns the next object in an iteration, or zero if no more remain or the iterator is invalid.
		@param iterator An IOKit iterator handle.
		@result If the iterator handle is valid, the next element in the iteration is returned, otherwise zero is returned. The element should be released by the caller when it is finished. */
	
//	CFSocketNativeHandle sock;
	
    while (intfService = IOIteratorNext(intfIterator))
    {
        CFTypeRef	MACAddressAsCFData;        
		
        // IONetworkControllers can't be found directly by the IOServiceGetMatchingServices call, 
        // matching mechanism. So we've found the IONetworkInterface and will get its parent controller
        // by asking for it specifically.
        
        kernResult = IORegistryEntryGetParentEntry( intfService,
                                                    kIOServicePlane,
                                                    &controllerService );
		
        if (KERN_SUCCESS != kernResult)
            printf("IORegistryEntryGetParentEntry returned 0x%08x\n", kernResult);
        else {
			/*! @function IORegistryEntryCreateCFProperty
			@abstract Create a CF representation of a registry entry's property.
			@discussion This function creates an instantaneous snapshot of a registry entry property, creating a CF container analogue in the caller's task. Not every object available in the kernel is represented as a CF container; currently OSDictionary, OSArray, OSSet, OSSymbol, OSString, OSData, OSNumber, OSBoolean are created as their CF counterparts. 
			@param entry The registry entry handle whose property to copy.
			@param key A CFString specifying the property name.
			@param allocator The CF allocator to use when creating the CF container.
			@param options No options are currently defined.
			@result A CF container is created and returned the caller on success. The caller should release with CFRelease. */
			
            MACAddressAsCFData = IORegistryEntryCreateCFProperty( controllerService,
                                                                  CFSTR(kIOMACAddress),
                                                                  kCFAllocatorDefault,
                                                                  0);
            if (MACAddressAsCFData)
            {
				CFDataRef	address = 0;
				struct sockaddr_lpx	addr = { 0 };
				
                //CFShow(MACAddressAsCFData);
                CFDataGetBytes((CFDataRef)MACAddressAsCFData, CFRangeMake(0, kIOEthernetAddressSize), addr.slpx_node);
                CFRelease(MACAddressAsCFData);
				
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
					syslog(LOG_ERR, "pSendSurrenderAccess: Can't Create socket for surrender Access request.\n");
					
					return -1;
				}
				
				// Bind Interface Address.
				addr.slpx_family = AF_LPX;
				addr.slpx_len = sizeof(struct sockaddr_lpx);
				addr.slpx_port = 0;
				
				address = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, 
													  (UInt8 *) &addr, 
													  sizeof(struct sockaddr_lpx), 
													  kCFAllocatorNull);
								
				CFSocketError socketError = CFSocketSetAddress (
																sockets[numInterfaces],
																address
																);
				if(socketError != kCFSocketSuccess) {
					syslog(LOG_ERR, "pSendSurrenderAccess: Can't Bind.\n");
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
					syslog(LOG_ERR, "pSendSurrenderAccess: Error when set option.\n");
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
					syslog(LOG_ERR, "pSendSurrenderAccess: Error when send reply.\n");
					return false;
				}
					
				numInterfaces++;
				
				if(address)		CFRelease(address);
            }
			/*! @function IOObjectRelease
				@abstract Releases an object handle previously returned by IOKitLib.
				@discussion All objects returned by IOKitLib should be released with this function when access to them is no longer needed. Using the object after it has been released may or may not return an error, depending on how many references the task has to the same object in the kernel.
				@param object The IOKit object to release.
				@result A kern_return_t error code. */
			
            (void) IOObjectRelease(controllerService);
        }
        
        // We have sucked this service dry of information so release it now.
        (void) IOObjectRelease(intfService);        
    }
	
	IOObjectRelease(intfIterator);
	
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
							syslog(LOG_INFO, "pSendDiscover: Duplicated replay.\n");
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
	io_object_t		intfService;
    io_object_t		controllerService;
    kern_return_t	kernResult = KERN_FAILURE;    
	io_iterator_t	intfIterator;

	CFSocketRef				sockets[4];
	int						numInterfaces = 0;
	
    kernResult = FindEthernetInterfaces(&intfIterator);
	if (KERN_SUCCESS != kernResult) {
        printf("FindEthernetInterfaces returned 0x%08x\n", kernResult);
		
		return false;
	}
	
	/*! @function IOIteratorNext
		@abstract Returns the next object in an iteration.
		@discussion This function returns the next object in an iteration, or zero if no more remain or the iterator is invalid.
		@param iterator An IOKit iterator handle.
		@result If the iterator handle is valid, the next element in the iteration is returned, otherwise zero is returned. The element should be released by the caller when it is finished. */
	
//	CFSocketNativeHandle sock;
	
    while (intfService = IOIteratorNext(intfIterator))
    {
        CFTypeRef	MACAddressAsCFData;        
		
        // IONetworkControllers can't be found directly by the IOServiceGetMatchingServices call, 
        // matching mechanism. So we've found the IONetworkInterface and will get its parent controller
        // by asking for it specifically.
        
        kernResult = IORegistryEntryGetParentEntry( intfService,
                                                    kIOServicePlane,
                                                    &controllerService );
		
        if (KERN_SUCCESS != kernResult)
            printf("IORegistryEntryGetParentEntry returned 0x%08x\n", kernResult);
        else {
			/*! @function IORegistryEntryCreateCFProperty
			@abstract Create a CF representation of a registry entry's property.
			@discussion This function creates an instantaneous snapshot of a registry entry property, creating a CF container analogue in the caller's task. Not every object available in the kernel is represented as a CF container; currently OSDictionary, OSArray, OSSet, OSSymbol, OSString, OSData, OSNumber, OSBoolean are created as their CF counterparts. 
			@param entry The registry entry handle whose property to copy.
			@param key A CFString specifying the property name.
			@param allocator The CF allocator to use when creating the CF container.
			@param options No options are currently defined.
			@result A CF container is created and returned the caller on success. The caller should release with CFRelease. */
			
            MACAddressAsCFData = IORegistryEntryCreateCFProperty( controllerService,
                                                                  CFSTR(kIOMACAddress),
                                                                  kCFAllocatorDefault,
                                                                  0);
            if (MACAddressAsCFData)
            {
				CFDataRef	address = 0;
				struct sockaddr_lpx	addr = { 0 };
				
                //CFShow(MACAddressAsCFData);
                CFDataGetBytes((CFDataRef)MACAddressAsCFData, CFRangeMake(0, kIOEthernetAddressSize), addr.slpx_node);
                CFRelease(MACAddressAsCFData);
				
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
					syslog(LOG_ERR, "pSendSurrenderAccess: Can't Create socket for surrender Access request.\n");
					
					return false;
				}
				
				// Bind Interface Address.
				addr.slpx_family = AF_LPX;
				addr.slpx_len = sizeof(struct sockaddr_lpx);
				addr.slpx_port = 0;
				
				address = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, 
													  (UInt8 *) &addr, 
													  sizeof(struct sockaddr_lpx), 
													  kCFAllocatorNull);
								
				CFSocketError socketError = CFSocketSetAddress (
																sockets[numInterfaces],
																address
																);
				if(socketError != kCFSocketSuccess) {
					syslog(LOG_ERR, "pSendSurrenderAccess: Can't Bind.\n");
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
					syslog(LOG_ERR, "pSendSurrenderAccess: Error when send reply.\n");
					return false;
				}
					
				numInterfaces++;
				
				if(address)		CFRelease(address);
            }
			/*! @function IOObjectRelease
				@abstract Releases an object handle previously returned by IOKitLib.
				@discussion All objects returned by IOKitLib should be released with this function when access to them is no longer needed. Using the object after it has been released may or may not return an error, depending on how many references the task has to the same object in the kernel.
				@param object The IOKit object to release.
				@result A kern_return_t error code. */
			
            (void) IOObjectRelease(controllerService);
        }
        
        // We have sucked this service dry of information so release it now.
        (void) IOObjectRelease(intfService);        
    }
	
	IOObjectRelease(intfIterator);
	
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
	io_object_t		intfService;
    io_object_t		controllerService;
    kern_return_t	kernResult = KERN_FAILURE;    
	io_iterator_t	intfIterator;
	
	CFSocketRef				sockets[4];
	int						numInterfaces = 0;
	
    kernResult = FindEthernetInterfaces(&intfIterator);
	if (KERN_SUCCESS != kernResult) {
        printf("FindEthernetInterfaces returned 0x%08x\n", kernResult);
		
		return false;
	}
	
	/*! @function IOIteratorNext
		@abstract Returns the next object in an iteration.
		@discussion This function returns the next object in an iteration, or zero if no more remain or the iterator is invalid.
		@param iterator An IOKit iterator handle.
		@result If the iterator handle is valid, the next element in the iteration is returned, otherwise zero is returned. The element should be released by the caller when it is finished. */
	
	//	CFSocketNativeHandle sock;
	
    while (intfService = IOIteratorNext(intfIterator))
    {
        CFTypeRef	MACAddressAsCFData;        
		
        // IONetworkControllers can't be found directly by the IOServiceGetMatchingServices call, 
        // matching mechanism. So we've found the IONetworkInterface and will get its parent controller
        // by asking for it specifically.
        
        kernResult = IORegistryEntryGetParentEntry( intfService,
                                                    kIOServicePlane,
                                                    &controllerService );
		
        if (KERN_SUCCESS != kernResult)
            printf("IORegistryEntryGetParentEntry returned 0x%08x\n", kernResult);
        else {
			/*! @function IORegistryEntryCreateCFProperty
			@abstract Create a CF representation of a registry entry's property.
			@discussion This function creates an instantaneous snapshot of a registry entry property, creating a CF container analogue in the caller's task. Not every object available in the kernel is represented as a CF container; currently OSDictionary, OSArray, OSSet, OSSymbol, OSString, OSData, OSNumber, OSBoolean are created as their CF counterparts. 
			@param entry The registry entry handle whose property to copy.
			@param key A CFString specifying the property name.
			@param allocator The CF allocator to use when creating the CF container.
			@param options No options are currently defined.
			@result A CF container is created and returned the caller on success. The caller should release with CFRelease. */
			
            MACAddressAsCFData = IORegistryEntryCreateCFProperty( controllerService,
                                                                  CFSTR(kIOMACAddress),
                                                                  kCFAllocatorDefault,
                                                                  0);
            if (MACAddressAsCFData)
            {
				CFDataRef	address = 0;
				struct sockaddr_lpx	addr = { 0 };
				
                //CFShow(MACAddressAsCFData);
                CFDataGetBytes((CFDataRef)MACAddressAsCFData, CFRangeMake(0, kIOEthernetAddressSize), addr.slpx_node);
                CFRelease(MACAddressAsCFData);
				
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
					syslog(LOG_ERR, "pSendSurrenderAccess: Can't Create socket for surrender Access request.\n");
					
					return false;
				}
				
				// Bind Interface Address.
				addr.slpx_family = AF_LPX;
				addr.slpx_len = sizeof(struct sockaddr_lpx);
				addr.slpx_port = 0;
				
				address = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, 
													  (UInt8 *) &addr, 
													  sizeof(struct sockaddr_lpx), 
													  kCFAllocatorNull);
				
				CFSocketError socketError = CFSocketSetAddress (
																sockets[numInterfaces],
																address
																);
				if(socketError != kCFSocketSuccess) {
					syslog(LOG_ERR, "pSendSurrenderAccess: Can't Bind.\n");
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
					syslog(LOG_ERR, "pSendSurrenderAccess: Error when set option.\n");
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
					syslog(LOG_ERR, "pSendSurrenderAccess: Error when send reply.\n");
					return false;
				}
				
				numInterfaces++;
				
				if(address)		CFRelease(address);
            }
			/*! @function IOObjectRelease
				@abstract Releases an object handle previously returned by IOKitLib.
				@discussion All objects returned by IOKitLib should be released with this function when access to them is no longer needed. Using the object after it has been released may or may not return an error, depending on how many references the task has to the same object in the kernel.
				@param object The IOKit object to release.
				@result A kern_return_t error code. */
			
            (void) IOObjectRelease(controllerService);
        }
        
        // We have sucked this service dry of information so release it now.
        (void) IOObjectRelease(intfService);        
    }
	
	IOObjectRelease(intfIterator);
	
	if(cfdRequest)	CFRelease(cfdRequest);
	
	return true;
}