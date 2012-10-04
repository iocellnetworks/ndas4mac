#include <CoreServices/CoreServices.h>
#include <IOKit/IOKitLib.h>

#include "NDASPreferences.h"

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus
	
kern_return_t NDASIOConnectRegister(PNDASPreferencesParameter	parameter, bool *bDuplicated);
kern_return_t NDASIOConnectUnregister(UInt32 slotNumber);
kern_return_t NDASIOConnectEnable(UInt32 slotNumber);
kern_return_t NDASIOConnectDisable(UInt32 slotNumber);
kern_return_t NDASIOConnectMount(UInt32 index, bool writable);
kern_return_t NDASIOConnectUnmount(UInt32 index);
kern_return_t NDASIOConnectEdit(UInt32 editType, PNDASPreferencesParameter parameter);
kern_return_t NDASIOConnectRefresh(UInt32 slotNumber, UInt32 unitNumber);
kern_return_t NDASIOConnectBind(UInt32 type, UInt32 NumberOfUnitDevices, UInt32 LogicalDeviceIndex[], PGUID raidID, bool sync);
kern_return_t NDASIOConnectUnbind(UInt32 index);
kern_return_t NDASIOConnectRebind(UInt32 index);
kern_return_t NDASIOConnectBindEdit(
									UInt32						editType,
									UInt32						index,
									CFStringRef					cfsName,
									UInt32						raidType
									);
kern_return_t NDASIOConnectRegisterKey(NDASPreferencesParameter *parameter);

#ifdef __cplusplus
}
#endif // __cplusplus