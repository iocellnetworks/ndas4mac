#include "NDASDIB.h"

#include "LanScsiDiskInformationBlock.h"
#include "hdreg.h"
#include "crc.h"
#include "hash.h"

#include "Utilities.h"

//
// DIB Utilities
//

PUNIT_DISK_LOCATION getDIBUnitDiskLocation(PNDAS_DIB_V2 dib2, UInt32 sequence)
{
	// Check Parameter.
	if (NULL == dib2
		|| sequence > NDAS_MAX_UNITS_IN_V2) {
		return NULL;
	}
	
	return &dib2->UnitDisks[sequence];
}

SInt32	getDIBSequence(PNDAS_DIB_V2 dib2)
{
	// Check Parameter.
	if (NULL == dib2) {
		return -1;
	}
	
	return NDASSwap32LittleToHost(dib2->iSequence);
}

UInt32	getDIBCountOfUnitDisks(PNDAS_DIB_V2 dib2)
{
	// Check Parameter.
	if (NULL == dib2) {
		return 0;
	}
	
	return NDASSwap32LittleToHost(dib2->nDiskCount);
}

UInt32	getDIBCountOfSpareDevices(PNDAS_DIB_V2 dib2)
{
	// Check Parameter.
	if (NULL == dib2) {
		return 0;
	}
	
	return NDASSwap32LittleToHost(dib2->nSpareCount);
}

UInt64	getDIBSizeOfUserSpace(PNDAS_DIB_V2 dib2)
{
	// Check Parameter.
	if (NULL == dib2) {
		return 0;
	}
	
	return NDASSwap64LittleToHost(dib2->sizeUserSpace);
}

UInt32	getDIBSectorsPerBit(PNDAS_DIB_V2 dib2)
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

UInt32	getRMDUSN(PNDAS_RAID_META_DATA rmd)
{
	// Check Parameter.
	if (NULL == rmd) {
		return 0;
	}
	
	return NDASSwap32LittleToHost(rmd->uiUSN);
}

PGUID	getRMDGUID(PNDAS_RAID_META_DATA rmd)
{
	// Check Parameter.
	if (NULL == rmd) {
		return 0;
	}
	
	return &rmd->RaidSetId;
}

PGUID	getRMDConfigSetID(PNDAS_RAID_META_DATA rmd)
{
	// Check Parameter.
	if (NULL == rmd) {
		return 0;
	}
	
	return &rmd->ConfigSetId;
}

UInt32	getRMDStatus(PNDAS_RAID_META_DATA rmd)
{
	// Check Parameter.
	if (NULL == rmd) {
		return 0;
	}
	
	return NDASSwap32LittleToHost(rmd->state);
}

UInt8	getRMDStatusOfUnit(PNDAS_RAID_META_DATA rmd, int index)
{	
	// Check Parameter.
	if (NULL == rmd || index >= NDAS_MAX_UNITS_IN_V2) {
		return 0;
	}
	
	return rmd->UnitMetaData[index].UnitDeviceStatus;
}

SInt32	getRMDIndexOfUnit(PNDAS_RAID_META_DATA rmd, UInt32 sequence)
{
	int count;
	
	for (count = 0; count < NDAS_MAX_UNITS_IN_V2; count++) {
		if (NDASSwap16LittleToHost(rmd->UnitMetaData[count].iUnitDeviceIdx) == sequence) {
			return count;
		}
	}
		
	return -1;
}