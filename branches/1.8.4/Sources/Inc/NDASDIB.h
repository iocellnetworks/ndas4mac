#include "LanScsiProtocol.h"
#include "LanScsiDiskInformationBlock.h"

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

PUNIT_DISK_LOCATION getDIBUnitDiskLocation(PNDAS_DIB_V2 dib2, uint32_t index);
SInt32				getDIBSequence(PNDAS_DIB_V2 dib2);
uint32_t				getDIBCountOfUnitDisks(PNDAS_DIB_V2 dib2);
uint32_t				getDIBCountOfSpareDevices(PNDAS_DIB_V2 dib2);
uint64_t				getDIBSizeOfUserSpace(PNDAS_DIB_V2 dib2);
uint32_t				getDIBSectorsPerBit(PNDAS_DIB_V2 dib2);

uint32_t	getRMDUSN(PNDAS_RAID_META_DATA rmd);
PGUID	getRMDGUID(PNDAS_RAID_META_DATA rmd);
PGUID	getRMDConfigSetID(PNDAS_RAID_META_DATA rmd);
uint32_t	getRMDStatus(PNDAS_RAID_META_DATA rmd);
UInt8	getRMDStatusOfUnit(PNDAS_RAID_META_DATA rmd, int index);
SInt32	getRMDIndexOfUnit(PNDAS_RAID_META_DATA rmd, uint32_t sequence);

#ifdef __cplusplus
}
#endif // __cplusplus
