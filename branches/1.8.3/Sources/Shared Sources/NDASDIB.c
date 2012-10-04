#include "NDASDIB.h"

#include "LanScsiDiskInformationBlock.h"
#include "hdreg.h"
#include "crc.h"
#include "hash.h"

#include "Utilities.h"

//
// DIB Utilities
//

inline PUNIT_DISK_LOCATION getDIBUnitDiskLocation(PNDAS_DIB_V2 dib2, uint32_t sequence)
{
	// Check Parameter.
	if (NULL == dib2
		|| sequence > NDAS_MAX_UNITS_IN_V2) {
		return NULL;
	}
	
	return &dib2->UnitDisks[sequence];
}

inline SInt32	getDIBSequence(PNDAS_DIB_V2 dib2)
{
	// Check Parameter.
	if (NULL == dib2) {
		return -1;
	}
	
	return NDASSwap32LittleToHost(dib2->iSequence);
}

inline uint32_t	getDIBCountOfUnitDisks(PNDAS_DIB_V2 dib2)
{
	// Check Parameter.
	if (NULL == dib2) {
		return 0;
	}
	
	return NDASSwap32LittleToHost(dib2->nDiskCount);
}

inline uint32_t	getDIBCountOfSpareDevices(PNDAS_DIB_V2 dib2)
{
	// Check Parameter.
	if (NULL == dib2) {
		return 0;
	}
	
	return NDASSwap32LittleToHost(dib2->nSpareCount);
}

inline uint64_t	getDIBSizeOfUserSpace(PNDAS_DIB_V2 dib2)
{
	// Check Parameter.
	if (NULL == dib2) {
		return 0;
	}
	
	return NDASSwap64LittleToHost(dib2->sizeUserSpace);
}

inline uint32_t	getDIBSectorsPerBit(PNDAS_DIB_V2 dib2)
{
	// Check Parameter.
	if (NULL == dib2) {
		return 128;	// Default
	}
	
	return NDASSwap32LittleToHost(dib2->iSectorsPerBit);
}

//
// RMD Utilities
//

inline uint32_t	getRMDUSN(PNDAS_RAID_META_DATA rmd)
{
	// Check Parameter.
	if (NULL == rmd) {
		return 0;
	}
	
	return NDASSwap32LittleToHost(rmd->uiUSN);
}

inline PGUID	getRMDGUID(PNDAS_RAID_META_DATA rmd)
{
	// Check Parameter.
	if (NULL == rmd) {
		return 0;
	}
	
	return &rmd->RaidSetId;
}

inline PGUID	getRMDConfigSetID(PNDAS_RAID_META_DATA rmd)
{
	// Check Parameter.
	if (NULL == rmd) {
		return 0;
	}
	
	return &rmd->ConfigSetId;
}

inline uint32_t	getRMDStatus(PNDAS_RAID_META_DATA rmd)
{
	// Check Parameter.
	if (NULL == rmd) {
		return 0;
	}
	
	return NDASSwap32LittleToHost(rmd->state);
}

inline UInt8	getRMDStatusOfUnit(PNDAS_RAID_META_DATA rmd, int index)
{	
	// Check Parameter.
	if (NULL == rmd || index >= NDAS_MAX_UNITS_IN_V2) {
		return 0;
	}
	
	return rmd->UnitMetaData[index].UnitDeviceStatus;
}

inline SInt32	getRMDIndexOfUnit(PNDAS_RAID_META_DATA rmd, uint32_t sequence)
{
	int count;
	
	for (count = 0; count < NDAS_MAX_UNITS_IN_V2; count++) {
		if (NDASSwap16LittleToHost(rmd->UnitMetaData[count].iUnitDeviceIdx) == sequence) {
			return count;
		}
	}
		
	return -1;
}