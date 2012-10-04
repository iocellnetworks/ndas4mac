#include "LanScsiProtocol.h"
#include "LanScsiDiskInformationBlock.h"

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

PUNIT_DISK_LOCATION getDIBUnitDiskLocation(PNDAS_DIB_V2 dib2, UInt32 index);
SInt32				getDIBSequence(PNDAS_DIB_V2 dib2);
UInt32				getDIBCountOfUnitDisks(PNDAS_DIB_V2 dib2);
UInt32				getDIBCountOfSpareDevices(PNDAS_DIB_V2 dib2);
UInt64				getDIBSizeOfUserSpace(PNDAS_DIB_V2 dib2);
UInt32				getDIBSectorsPerBit(PNDAS_DIB_V2 dib2);

UInt32	getRMDUSN(PNDAS_RAID_META_DATA rmd);
PGUID	getRMDGUID(PNDAS_RAID_META_DATA rmd);
PGUID	getRMDConfigSetID(PNDAS_RAID_META_DATA rmd);
UInt32	getRMDStatus(PNDAS_RAID_META_DATA rmd);
UInt8	getRMDStatusOfUnit(PNDAS_RAID_META_DATA rmd, int index);
SInt32	getRMDIndexOfUnit(PNDAS_RAID_META_DATA rmd, UInt32 sequence);

#ifdef __cplusplus
}
#endif // __cplusplus
