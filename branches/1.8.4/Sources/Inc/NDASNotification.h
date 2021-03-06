/*
 *  NDASNotification.h
 *  NDASService
 *
 *  Created by 정균 안 on 05. 02. 22.
 *  Copyright 2005 __MyCompanyName__. All rights reserved.
 *
 */
#include <CoreServices/CoreServices.h>

#define NDAS_UI_OBJECT_ID		"com_ximeta_NDAS_UI_object_ID"
#define NDAS_SERVICE_OBJECT_ID	"com_ximeta_NDAS_service_object_ID"
#define NDAS_NOTIFICATION		"com_ximeta_NDAS_notification"

// Request Keys.
#define NDAS_NOTIFICATION_SENDER_ID_KEY_STRING		"Sender ID Of This Notification"
#define	NDAS_NOTIFICATION_REQUEST_KEY_STRING		"Request Of This Notificatioin"
#define	NDAS_NOTIFICATION_SLOTNUMBER_KEY_STRING		"SlotNumber Of This Notificatioin"
#define NDAS_NOTIFICATION_UNITNUMBER_KEY_STRING		"UnitNumber Of This Notification"
#define NDAS_NOTIFICATION_INDEX_KEY_STRING			"Index Of This Notification"
#define NDAS_NOTIFICATION_NAME_STRING				"NDAS Name Of This Notification"
#define NDAS_NOTIFICATION_ID_1_KEY_STRING			"NDAS ID Segment 1 Of This Notification"
#define NDAS_NOTIFICATION_ID_2_KEY_STRING			"NDAS ID Segment 2 Of This Notification"
#define NDAS_NOTIFICATION_ID_3_KEY_STRING			"NDAS ID Segment 3 Of This Notification"
#define NDAS_NOTIFICATION_ID_4_KEY_STRING			"NDAS ID Segment 4 Of This Notification"
#define NDAS_NOTIFICATION_WRITEKEY_KEY_STRING		"NDAS Write Key Of This Notification"
#define NDAS_NOTIFICATION_CONFIGURATION_KEY_STRING	"Configuration Of This Notification"
#define NDAS_NOTIFICATION_OPTIONS_KEY_STRING		"Options of This Notification"

#define NDAS_NOTIFICATION_RAID_TYPE_KEY_STRING					"RAID Type Of This Noification"
#define NDAS_NOTIFICATION_NUMBER_OF_LOGICAL_DEVICES_KEY_STRING	"Number Of Logical Devices Of This Noification"
#define NDAS_NOTIFICATION_LOGICAL_DEVICE_INDEX_KEY_STRING		"Logical Device %d Index Of This Noification"
#define NDAS_NOTIFICATION_RAID_SYNC_FLAG_STRING					"Sync After Binding"

// Reply Keys.
#define NDAS_NOTIFICATION_REPLY_OPERATION_KEY_STING		"Operation code of This Reply"
#define NDAS_NOTIFICATION_REPLY_ERROR_CODE_KEY_STRING	"Error code of This Reply"
#define NDAS_NOTIFICATION_REPLY_SENDER_ID_KEY_STRING	"Sender ID of This Reply"

// Operation code.
#define NDAS_NOTIFICATION_CHANGED								0
#define NDAS_NOTIFICATION_REQUEST_REGISTER						1
#define NDAS_NOTIFICATION_REQUEST_UNREGISTER					2
#define NDAS_NOTIFICATION_REQUEST_CONFIGURATE_NDAS				3
#define NDAS_NOTIFICATION_REQUEST_CONFIGURATE_LOGICAL_DEVICE	4
#define NDAS_NOTIFICATION_REQUEST_EDIT_NDAS_NAME				5
#define NDAS_NOTIFICATION_REQUEST_EDIT_NDAS_WRITEKEY			6
#define NDAS_NOTIFICATION_REQUEST_SURRENDER_ACCESS				7
#define NDAS_NOTIFICATION_REQUEST_BIND							8
#define NDAS_NOTIFICATION_REQUEST_UNBIND						9
#define NDAS_NOTIFICATION_REQUEST_BIND_EDIT						10
#define NDAS_NOTIFICATION_REQUEST_REBIND						11
#define NDAS_NOTIFICATION_REQUEST_REFRESH						12
#define NDAS_NOTIFICATION_REQUEST_REGISTER_KEY					13

// Error code.
#define NDAS_NOTIFICATION_ERROR_SUCCESS				0
#define NDAS_NOTIFICATION_ERROR_FAIL				1
#define NDAS_NOTIFICATION_ERROR_INVALID_OPERATION	2
#define NDAS_NOTIFICATION_ERROR_INVALID_SLOT_NUMBER 3
#define NDAS_NOTIFICATION_ERROR_INVALID_UNIT_NUMBER 4
#define NDAS_NOTIFICATION_ERROR_INVALID_PARAMETER	5

#define NDAS_NOTIFICATION_ERROR_ALREADY_REGISTERED	10

#define NDAS_NOTIFICATION_ERROR_NO_ACCESS			70

// Options.
#define NDAS_NOTIFICATION_OPTION_FOR_PHYSICAL_UNIT	1
#define NDAS_NOTIFICATION_OPTION_FOR_RAID_UNIT		2

