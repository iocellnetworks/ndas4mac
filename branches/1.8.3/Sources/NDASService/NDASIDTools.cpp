#include <CoreServices/CoreServices.h>
#include <syslog.h>

#include "NDASIDTools.h"
#include "NDASID.h"
#include "serial.h"

bool
checkSNValid(char *strID1, char *strID2, char *strID3, char *strID4, char *wKey, char *tempMac, bool *bWriteKey, uint8_t *vid)
{
	SERIAL_INFO	info;
	bool		ret;
	
	memcpy(info.ucSN[0], strID1, 5);
	memcpy(info.ucSN[1], strID2, 5);
	memcpy(info.ucSN[2], strID3, 5);
	memcpy(info.ucSN[3], strID4, 5);
	memcpy(info.ucWKey, wKey, 5);
	
	memcpy(info.ucKey1, NDAS_ID_KEY_DEFAULT.Key1, 8);
	memcpy(info.ucKey2, NDAS_ID_KEY_DEFAULT.Key2, 8);
	
	if((ret = DecryptSerial(&info)) == TRUE &&
	   strlen(strID1) == 5 &&
	   strlen(strID2) == 5 &&
	   strlen(strID3) == 5 &&
	   strlen(strID4) == 5 &&
	   (strlen(wKey) == 0 || (strlen(wKey) == 5 && info.bIsReadWrite == TRUE))) {
	
		if (NDAS_ID_EXTENSION_GENERAL.VID == info.ucVid
			&& NDAS_ID_EXTENSION_GENERAL.Seed == info.ucRandom
			&& NDAS_ID_EXTENSION_GENERAL.Reserved[0] == info.reserved[0]
			&& NDAS_ID_EXTENSION_GENERAL.Reserved[1] == info.reserved[1]) {
			// Default ID.
		} else if (NDAS_ID_EXTENSION_SEAGATE.VID == info.ucVid
				   && NDAS_ID_EXTENSION_SEAGATE.Seed == info.ucRandom
				   && NDAS_ID_EXTENSION_SEAGATE.Reserved[0] == info.reserved[0]
				   && NDAS_ID_EXTENSION_SEAGATE.Reserved[1] == info.reserved[1]) {
			// Seagate ID.
		} else {
			return FALSE;
		}

		memcpy(tempMac, info.ucAddr, 6);
		*bWriteKey = info.bIsReadWrite;
		*vid = info.ucVid;
		
		return TRUE;
	}
			
	return FALSE;
}	
