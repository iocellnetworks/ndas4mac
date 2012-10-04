/*
 *  lsp_wrapper.c
 *  NDASFamily
 *
 *  Created by aingoppa on 5/11/07.
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 */

#include "lsp_wrapper.h"

#include "LanScsiProtocol.h"
#include "LanScsiSocket.h"
#include "Utilities.h"

#include "lspx/lsp.h"
#include "lspx/lsp_util.h"
#include <pshpack4.h>

#include <lsp_type.h>
#include "lsp_binparm.h"

//#ifdef DEBUG_MASK_LS_ERROR
#if 0

#undef DEBUG_MASK_LS_ERROR
#undef DEBUG_MASK_LS_WARNING
#undef DEBUG_MASK_LS_TRACE
#undef DEBUG_MASK_LS_INFO

#define DEBUG_MASK_LS_ERROR		DEBUG_MASK_ALL
#define DEBUG_MASK_LS_WARNING	DEBUG_MASK_ALL
#define DEBUG_MASK_LS_TRACE		DEBUG_MASK_ALL
#define DEBUG_MASK_LS_INFO		DEBUG_MASK_ALL

#endif

static
char * getName()
{
	return "lsp_wrapper";
}

typedef struct _lsp_wrapper_transfer_context_t {
	lsp_wrapper_context_ptr_t socket_context;
	char *buffer_send, *buffer_recv;
	int len_buffer;
} lsp_wrapper_transfer_context_t;

typedef struct _lsp_wrapper_context_t {
	xi_socket_t s;
	lsp_handle_t lsp_handle;
	lsp_status_t lsp_status;
	int transfer_contexts_queued;
	lsp_wrapper_transfer_context_t transfer_context[LSP_MAX_CONCURRENT_TRANSFER];
	int force_log_transfer_context;
} lsp_wrapper_context_t;

static int
complete_lsp_wrapper_transfer_context_t(lsp_wrapper_transfer_context_t *transfer_context)
{
	DbgIOLog(DEBUG_MASK_LS_TRACE, ("[%s] CRITICAL ERROR\n", __FUNCTION__));
	// should not reach here : synchronous socket
	return 0;
}

static int
transfer_context(lsp_wrapper_context_t *context)
{
	int ret;
	static int sc = 0;
	char *buffer;
	lsp_uint32_t len_buffer;
	int i;
	
	unsigned long dbgError = context->force_log_transfer_context ? DEBUG_MASK_ALL : DEBUG_MASK_LS_ERROR;
	unsigned long dbgWarning = context->force_log_transfer_context ? DEBUG_MASK_ALL : DEBUG_MASK_LS_WARNING;
	unsigned long dbgTrace = context->force_log_transfer_context ? DEBUG_MASK_ALL : DEBUG_MASK_LS_TRACE;
	unsigned long dbgInfo = context->force_log_transfer_context ? DEBUG_MASK_ALL : DEBUG_MASK_LS_INFO;
	
	while(0) {
		dbgError = dbgError;
		dbgWarning = dbgWarning;
		dbgTrace = dbgTrace;
		dbgInfo = dbgInfo;
		}
	
	DbgIOLog(dbgTrace, ("%p, socket(%p)\n", context, context->s));
	
	context->transfer_contexts_queued = 0;
	
	if(!context->lsp_handle) {
		DbgIOLog(dbgError, ("lsp_handle is NULL\n"));
		
		return -2;
	}
	
	for(;; context->lsp_status = lsp_process_next(context->lsp_handle))
	{
		if (LSP_REQUIRES_SEND == context->lsp_status)
		{
			buffer = (char *)lsp_get_buffer_to_send(context->lsp_handle, &len_buffer);
			
			DbgIOLog(dbgTrace, ("[%d] send(%p, %p, %d)\n", ++sc, context->s, buffer, len_buffer));
			if(debugLevel & dbgInfo)
			{
				for (i = 0; i < len_buffer; ++i)
				{
					DbgIOLogNC(dbgInfo, ("%02X ", (unsigned char)*(buffer + i)));
					if ((i+1)%16 == 0) DbgIOLogNC(dbgInfo, ("\n"));
					if(i >= 16 * 8)
					{
						DbgIOLogNC(dbgInfo, ("len_buffer = (%d) breaking log\n", len_buffer));
						break;
					}
				}
				DbgIOLogNC(dbgInfo, ("\n"));
			}
			{
				char *buf = buffer;
				int res;
				int len = len_buffer;
				
				while (len > 0) {
					
					if ((res = xi_lpx_send(context->s, buf, len, 0)) <= 0) {
						DbgIOLog(dbgError, ("Sent %d bytes failed with error %d.\n", len_buffer, res));
						return -3;
					}
					len -= res;
					buf += res;
				}
			}
		}
		else if (LSP_REQUIRES_RECEIVE == context->lsp_status)
		{
			buffer = (char *)lsp_get_buffer_to_receive(context->lsp_handle, &len_buffer);
			
			DbgIOLog(dbgTrace, ("[%d] recv(%p, %p, %d)\n", ++sc, context->s, buffer, len_buffer));
			{
				int res;
				char *buf = buffer;
				int len = len_buffer;
				
				while (len > 0) {
					res = xi_lpx_recv(context->s, buf, len, 0);
					if (res <= 0) {
						DbgIOLog(dbgError, ("Receive %d bytes failed with error %d.\n", len_buffer, res));
						return -3;
					}
					len -= res;
					buf += res;
				}
			}
			if(debugLevel & dbgInfo)
			{
				for (i = 0; i < len_buffer; ++i)
				{
					DbgIOLogNC(dbgInfo, ("%02X ", (unsigned char)*(buffer + i)));
					if ((i+1)%16 == 0) DbgIOLogNC(dbgInfo, ("\n"));
					if(i >= 16 * 8)
					{
						DbgIOLogNC(dbgInfo, ("len_buffer = (%d) breaking log\n", len_buffer));
						break;
					}
				}
				DbgIOLogNC(dbgInfo, ("\n"));
			}
		}
		else if (LSP_REQUIRES_SYNCHRONIZE == context->lsp_status)
		{
			for(i = 0; i < context->transfer_contexts_queued; i++)
			{
				ret = complete_lsp_wrapper_transfer_context_t(&context->transfer_context[i]);
				
				if(-1 == ret)
				{
					DbgIOLog(dbgError, ("complete_lsp_wrapper_transfer_context_t return -1\n"));

					return -3;
				}
			}
			
			context->transfer_contexts_queued = 0;
		}
		else
		{
			switch(context->lsp_status)
			{
				case LSP_STATUS_SUCCESS:
					return 0;
				case LSP_STATUS_RESPONSE_HEADER_INVALID:
					DbgIOLog(dbgError, ("LSP:0x%x\n", context->lsp_status));
					return -3;
				default:
					DbgIOLog(dbgError, ("LSP:0x%x\n", context->lsp_status));
					return -2;
			}
		}
	}
}

int lsp_wrapper_context_size()
{
	return sizeof(lsp_wrapper_context_t);
}

int lsp_wrapper_connect(
						lsp_wrapper_context_ptr_t context,
						struct sockaddr_lpx daddr,
						struct sockaddr_lpx saddr,
						int timeoutsec)
{
	int						ret = -1;
	int						error;
	if(!context)
		return -1;
	
	DbgIOLog(DEBUG_MASK_LS_TRACE, ("[%s] %p\n", __FUNCTION__, context));
	
	bzero(context, sizeof(lsp_wrapper_context_t));
	error = xi_lpx_connect(&context->s, daddr, saddr, timeoutsec);
	if(error)
	{
		DbgIOLog(DEBUG_MASK_LS_ERROR, ("xi_lpx_connect Failed. (%d)\n", error));
		ret = -3;
		goto Out;
	}
	DbgIOLog(DEBUG_MASK_LS_TRACE, ("xi_lpx_connect : socket(%p)\n", context->s));
	
	ret = 0;
Out:	
		return ret;
}

int lsp_wrapper_disconnect(
						   lsp_wrapper_context_ptr_t context
						   )
{
	int						ret = -1;
	
	DbgIOLog(DEBUG_MASK_LS_TRACE, ("[%s] %p\n", __FUNCTION__, context));
	
	if(!context || !context->s)
		return -1;
	
	xi_lpx_disconnect(context->s);
	context->s = NULL;
	
	ret = 0;
	
	return ret;
}

int lsp_wrapper_isconnected(
							lsp_wrapper_context_ptr_t context
							)
{
	
//	DbgIOLog(DEBUG_MASK_LS_ERROR, ("context = %p, context->s = %p\n", context, context->s));
	
	if (context && context->s) {
		return xi_lpx_isconnected(context->s);
	} else {
		return 0;
	}
}

int lsp_wrapper_login(
					  lsp_wrapper_context_ptr_t context,
					  char *userPassword,
					  UInt8 discover,
					  UInt8 unit_no,
					  UInt8 write_access					  
					  )
{
	int						ret = -1;
	int						error;
	
	lsp_handle_t			lsp_handle;
	lsp_login_info_t		lsp_login_info;
	
	void *					session_buffer = NULL;
	
	DbgIOLog(DEBUG_MASK_LS_TRACE, ("[%s] %p socket(%p)\n", __FUNCTION__, context, context->s));
	if(!context || !context->s || !userPassword)
		goto Out;
	
	session_buffer = IOMalloc(lsp_get_session_buffer_size());
	if(!session_buffer)
	{
		DbgIOLog(DEBUG_MASK_LS_ERROR, ("IOMalloc for session buffer Failed.\n"));
		goto Out;
	}
	DbgIOLog(DEBUG_MASK_LS_TRACE, ("%p = IOMalloc(%d) : session buffer.\n", session_buffer, lsp_get_session_buffer_size()));
	
	// Create session
	//	lsp_handle = lsp_create_session(session_buffer, (void*)context);
	lsp_handle = lsp_initialize_session(session_buffer, lsp_get_session_buffer_size());
	context->lsp_handle = lsp_handle;
	
	DbgIOLog(DEBUG_MASK_LS_TRACE, ("lsp_handle(%p).\n", context->lsp_handle));
	
	// Init lsp_login_info
	bzero(&lsp_login_info, sizeof(lsp_login_info_t));
	lsp_login_info.login_type = (discover) ? LSP_LOGIN_TYPE_DISCOVER : LSP_LOGIN_TYPE_NORMAL;
	memcpy(lsp_login_info.password, userPassword, 8);
	lsp_login_info.unit_no = unit_no;
	lsp_login_info.write_access = write_access;
	
	// login
	context->lsp_status = lsp_login(context->lsp_handle, &lsp_login_info);
	DbgIOLog(DEBUG_MASK_LS_TRACE, ("%d = lsp_login().\n", context->lsp_status));
	error = transfer_context(context);
	
	if(error || LSP_STATUS_SUCCESS != context->lsp_status)
	{
		DbgIOLog(DEBUG_MASK_LS_ERROR, ("lsp_login Failed(%d).\n", error));
		goto Out;
	}
	
	if(!discover)
	{
		context->lsp_status = lsp_ata_handshake(context->lsp_handle);
		DbgIOLog(DEBUG_MASK_LS_TRACE, ("%d = lsp_ata_handshake().\n", context->lsp_status));
		error = transfer_context(context);
		
		if(error || LSP_STATUS_SUCCESS != context->lsp_status)
		{
			DbgIOLog(DEBUG_MASK_LS_ERROR, ("lsp_ata_handshake Failed(%d).\n", error));
			goto Out;
		}
	}
	
	ret = 0;
Out:
		
		if(ret && session_buffer)
		{
			IOFree(session_buffer, lsp_get_session_buffer_size());
			session_buffer = NULL;
			context->lsp_handle = NULL;
		}
	return ret;
}

int lsp_wrapper_logout(
					   lsp_wrapper_context_ptr_t context,
					   bool							connectionError
					   )
{
	int						ret = -1;
	int						error;
	
	DbgIOLog(DEBUG_MASK_LS_TRACE, ("[%s] %p\n", __FUNCTION__, context));
	if(!context || !context->s || !context->lsp_handle)
		goto Out;
	
	if (!connectionError && lsp_wrapper_isconnected(context)) {
		
		context->lsp_status = lsp_logout(context->lsp_handle);
		DbgIOLog(DEBUG_MASK_LS_TRACE, ("%d = lsp_logout().\n", context->lsp_status));
		error = transfer_context(context);
		if(error)
		{
			DbgIOLog(DEBUG_MASK_LS_ERROR, ("lsp_logout Failed(%d).\n", error));
			goto Out;
		}
		
		//	lsp_destroy_session(context->lsp_handle);
	}	
	
	ret = 0;
Out:
		if(context && context->lsp_handle)
		{
			DbgIOLog(DEBUG_MASK_LS_TRACE, ("lsp_handle(%p).\n", context->lsp_handle));
			IOFree(context->lsp_handle, lsp_get_session_buffer_size());
			context->lsp_handle = NULL;
		}
	return ret;
}

int lsp_wrapper_fill_target_info(
								 lsp_wrapper_context_ptr_t context,
								 TARGET_DATA *target_info)
{
	int						ret = -1;
	//	int						error;
	int						i;
	
	const lsp_ata_handshake_data_t *ata_handshake_data = NULL;
	const lsp_ide_identify_device_data_t *ide_identify_device_data = NULL;
	
	if(!context || !target_info)
		goto Out;
	
	ata_handshake_data = lsp_get_ata_handshake_data(context->lsp_handle);
	ide_identify_device_data = lsp_get_ide_identify_device_data(context->lsp_handle);
	
	memset(target_info, 0, sizeof(TARGET_DATA));
	target_info->bPresent = TRUE; // Should be TRUE
								  //    uint8_t		NRRWHost;
								  //  uint8_t		NRROHost;
								  //uint64_t	TargetData;
	
	//uint32_t	DiskArrayType;
	
	target_info->bLBA = (ata_handshake_data->lba == 1) ? TRUE : FALSE;
	target_info->bLBA48 = (ata_handshake_data->lba48 == 1) ? TRUE : FALSE;
	target_info->bPacket = (ata_handshake_data->device_type == 1) ? TRUE : FALSE;
	target_info->bDMA = (ata_handshake_data->dma_supported == 1) ? TRUE : FALSE;
	if (ata_handshake_data->dma_supported)
	{
		target_info->SelectedTransferMode = ata_handshake_data->active.dma_mode | ata_handshake_data->active.dma_level;
	}
	else
	{
		target_info->SelectedTransferMode = 0 | ata_handshake_data->support.pio_level;
	}
	target_info->bCable80 = (lsp_letohs(ide_identify_device_data->hardware_reset_result) & 0x2000) ? TRUE : FALSE;
	target_info->SectorCount = (uint64_t)ata_handshake_data->lba_capacity.quad; // *** check count
	target_info->SectorSize = ata_handshake_data->logical_block_size;
	
	//	uint32_t	configuration;
	//	uint32_t	status;
	
	memcpy(target_info->model, ide_identify_device_data->model_number, sizeof(ide_identify_device_data->model_number));
	for (i = 0; i < sizeof(target_info->model); i += 2)
	{
		unsigned short* s = (unsigned short*) &target_info->model[i];
		*s = lsp_byteswap_ushort(*s);
	}       
	memcpy(target_info->firmware, ide_identify_device_data->firmware_revision, sizeof(ide_identify_device_data->firmware_revision));
	for (i = 0; i < sizeof(target_info->firmware); i += 2)
	{
		unsigned short* s = (unsigned short*) &target_info->firmware[i];
		*s = lsp_byteswap_ushort(*s);
	}       
	memcpy(target_info->serialNumber, ide_identify_device_data->serial_number, sizeof(ide_identify_device_data->serial_number));
	for (i = 0; i < sizeof(target_info->serialNumber); i += 2)
	{
		unsigned short* s = (unsigned short*) &target_info->serialNumber[i];
		*s = lsp_byteswap_ushort(*s);
	}       
	
	ret = 0;
Out:
		return ret;
}

int lsp_wrapper_text_command(
							 lsp_wrapper_context_ptr_t context,
							 int *nr_target,
							 TARGET_DATA *target_info)
{
	int						ret = -1;
	int						error;
	int						i;
	lsp_text_target_list_t	list;
	
	DbgIOLog(DEBUG_MASK_LS_TRACE, ("[%s] %p\n", __FUNCTION__, context));
	if(!context || !context->s || !context->lsp_handle)
		goto Out;
	
	list.type = LSP_TEXT_BINPARAM_TYPE_TARGET_LIST;
	
	context->lsp_status = lsp_text_command(
										   context->lsp_handle,
										   0x01, /* text_type_binary */
										   0x00, /* text_type_version */
										   &list,
										   LSP_BINPARM_SIZE_TEXT_TARGET_LIST_REQUEST,
										   &list,
										   LSP_BINPARM_SIZE_TEXT_TARGET_LIST_REPLY);
	
	error = transfer_context(context);
	if(error)
	{
		DbgIOLog(DEBUG_MASK_LS_ERROR, ("lsp_text_command Failed(%d).\n", error));
		goto Out;
	}
	
	*nr_target = list.number_of_elements;
	DbgIOLog(DEBUG_MASK_LS_TRACE, ("[%s] list.number_of_elements(%d)\n", __FUNCTION__, list.number_of_elements));
	
	// Reset Infos.
	for (i = 0; i < MAX_NR_OF_TARGETS_PER_DEVICE; i++) {
		target_info[i].bPresent = FALSE;
		target_info[i].MaxRequestBytes = lsp_wrapper_get_max_request_blocks(context) * DISK_BLOCK_SIZE;
	}
	
	// Update Infos.
    for(i = 0; i < *nr_target; i++) {
        target_info[i].bPresent = TRUE;
        target_info[i].NRRWHost = list.elements[i].rw_hosts;
        target_info[i].NRROHost = list.elements[i].ro_hosts;
        memcpy(&target_info[i].TargetData, list.elements[i].target_data, sizeof(list.elements[i].target_data));
		DbgIOLog(DEBUG_MASK_LS_INFO, ("target_info[%d](%d, %d, %d, %02X %02X %02X %02X %02X %02X %02X %02X)\n",
									  i,
									  target_info[i].bPresent,
									  target_info[i].NRRWHost,
									  target_info[i].NRROHost,
									  ((lsp_uint8_t *)&target_info[i].TargetData)[0],
									  ((lsp_uint8_t *)&target_info[i].TargetData)[1],
									  ((lsp_uint8_t *)&target_info[i].TargetData)[2],
									  ((lsp_uint8_t *)&target_info[i].TargetData)[3],
									  ((lsp_uint8_t *)&target_info[i].TargetData)[4],
									  ((lsp_uint8_t *)&target_info[i].TargetData)[5],
									  ((lsp_uint8_t *)&target_info[i].TargetData)[6],
									  ((lsp_uint8_t *)&target_info[i].TargetData)[7]));
    }
	
	ret = 0;
Out:
		return ret;
}

int lsp_wrapper_ide_identify(
							 lsp_wrapper_context_ptr_t context,
							 void *info)
{
	int						ret = -1;
	
	lsp_ide_identify_device_data_t	buf;
	
	DbgIOLog(DEBUG_MASK_LS_TRACE, ("[%s] %p\n", __FUNCTION__, context));
	if(!context || !context->s || !context->lsp_handle)
		goto Out;
	
	context->lsp_status = lsp_ide_identify(context->lsp_handle, &buf);
	ret = transfer_context(context);
	if(ret)
	{
		DbgIOLog(DEBUG_MASK_LS_ERROR, ("transfer_context Failed(%d).\n", ret));
		goto Out;
	}
	
	ret = 0;
Out:
		return ret;
}

int lsp_wrapper_ide_read(
						 lsp_wrapper_context_ptr_t context,
						 uint64_t LogicalBlockAddress,
						 uint32_t NumberOfBlocks,
						 char *DataBuffer,
						 uint32_t DataBufferLength
						 )
{
	int						ret = -1;
	
	if (context == NULL || context->lsp_handle == NULL) {
		DbgIOLog(DEBUG_MASK_LS_ERROR, ("Null context!!! context %p.\n", context));
		
		goto Out;
	}
	
	lsp_large_integer_t	LBA;
	LBA.quad = LogicalBlockAddress;
	
	context->lsp_status = lsp_ide_read(
									   context->lsp_handle,
									   &LBA, 
									   NumberOfBlocks, 
									   (void *)DataBuffer, 
									   DataBufferLength);
	ret = transfer_context(context);
	if(ret)
	{
		DbgIOLog(DEBUG_MASK_LS_ERROR, ("transfer_context Failed(%d).\n", ret));
		goto Out;
	}
	
	ret = 0;
Out:
		return ret;
}

int lsp_wrapper_ide_write(
						  lsp_wrapper_context_ptr_t context,
						  uint64_t LogicalBlockAddress,
						  uint32_t NumberOfBlocks,
						  char *DataBuffer,
						  uint32_t DataBufferLength
						  )
{
	int						ret = -1;
	
	if (context == NULL || context->lsp_handle == NULL) {
		DbgIOLog(DEBUG_MASK_LS_ERROR, ("Null context!!! context %p.\n", context));
		
		goto Out;
	}
	
	lsp_large_integer_t	LBA;
	LBA.quad = LogicalBlockAddress;
	
	context->lsp_status = lsp_ide_write(
										context->lsp_handle,
										&LBA, 
										NumberOfBlocks, 
										DataBuffer, 
										DataBufferLength);
	ret = transfer_context(context);
	if(ret)
	{
		DbgIOLog(DEBUG_MASK_LS_ERROR, ("transfer_context Failed(%d).\n", ret));
		goto Out;
	}
	
	ret = 0;
Out:
		return ret;
}

int lsp_wrapper_ide_packet_cmd(
							   lsp_wrapper_context_ptr_t context,
							   CDBd *cdb,
							   char *DataBuffer,
							   uint32_t DataBufferLength,
							   UInt8 DataTransferDirection
							   )
{
	int						ret = -1;
	
	context->lsp_status = lsp_ide_packet_cmd(
											 context->lsp_handle,
											 cdb,
											 DataBuffer, 
											 DataBufferLength,
											 DataTransferDirection);
	
	ret = transfer_context(context);

	if(ret)
	{
		DbgIOLog(DEBUG_MASK_LS_ERROR, ("transfer_context Failed(%d).\n", ret));
		goto Out;
	}
	
	ret = 0;
Out:
		return ret;
}

int lsp_wrapper_get_max_request_blocks(
									   lsp_wrapper_context_ptr_t context
									   )
{
	const lsp_hardware_data_t *hardware_data;
	if(!context)
	{
		return 128; // error condition
	}
	hardware_data = lsp_get_hardware_data(context->lsp_handle);
	
	return hardware_data->maximum_transfer_blocks;
}