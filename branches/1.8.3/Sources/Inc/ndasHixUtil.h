#include <CoreServices/CoreServices.h>

#include "ndashix.h"

UInt32 pGetHostInfo(UInt32 cbBuffer, PNDAS_HIX_HOST_INFO_DATA pHostInfoData);

BOOL
pIsValidNHIXRequestHeader(UInt32 cbData, const NDAS_HIX::HEADER* pHeader);

BOOL 
pIsValidNHIXReplyHeader(UInt32 cbData, const NDAS_HIX::HEADER* pHeader);

void
pBuildNHIXHeader(
				 NDAS_HIX::PHEADER pHeader,
				 CFUUIDBytes pHostGuid,
				 BOOL ReplyFlag, 
				 UInt8 Type, 
				 UInt16 Length);

int 
pSendDiscover(
			  CFUUIDBytes pHostGuid,
			  PNDAS_HIX_UNITDEVICE_ENTRY_DATA	data,
			  struct sockaddr_lpx				*host
			  );

bool 
pSendSurrenderAccess(
					 CFUUIDBytes pHostGuid,
					 PNDAS_HIX_UNITDEVICE_ENTRY_DATA	data,
					 struct sockaddr_lpx				*host
					 );

bool 
pSendUnitDeviceChangeNotification(
								  CFUUIDBytes pHostGuid,
								  PNDAS_HIX_UNITDEVICE_CHANGE_NOTIFY	data
								  );
