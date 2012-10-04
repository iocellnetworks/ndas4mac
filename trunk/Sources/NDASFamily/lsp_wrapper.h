/*
 *  lsp_wrapper.h
 *  NDASFamily
 *
 *  Created by aingoppa on 5/11/07.
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 */
#ifndef	_LSP_WRAPPER_H_
#define _LSP_WRAPPER_H_

#include <stdint.h>

#include "LanScsiSocket.h"
#include "LanScsiProtocol.h"
#include "Utilities.h"

typedef struct _lsp_wrapper_context_t *lsp_wrapper_context_ptr_t;

#ifdef __cplusplus
extern "C" {
#endif

int lsp_wrapper_context_size();
int lsp_wrapper_connect(
						lsp_wrapper_context_ptr_t context,
						struct sockaddr_lpx daddr,
						struct sockaddr_lpx saddr,
						int timeoutsec);
int lsp_wrapper_disconnect(
						   lsp_wrapper_context_ptr_t context
						   );
int lsp_wrapper_isconnected(
							lsp_wrapper_context_ptr_t context
							);
int lsp_wrapper_login(
					  lsp_wrapper_context_ptr_t context,
					  char *userPassword,
					  UInt8 discover,
					  UInt8 unit_no,
					  UInt8 write_access					  
					  );
int lsp_wrapper_logout(
					   lsp_wrapper_context_ptr_t context
					   );
int lsp_wrapper_ide_identify(
							 lsp_wrapper_context_ptr_t context,
							 void *info);
int lsp_wrapper_fill_target_info(
								 lsp_wrapper_context_ptr_t context,
								 TARGET_DATA *target_info);
int lsp_wrapper_text_command(
							 lsp_wrapper_context_ptr_t context,
							 int *nr_target,
							 TARGET_DATA *target_info);								
int lsp_wrapper_ide_read(
	lsp_wrapper_context_ptr_t context,
	uint64_t LogicalBlockAddress,
	uint32_t NumberOfBlocks,
	char *DataBuffer,
	uint32_t DataBufferLength
);

int lsp_wrapper_ide_write(
	lsp_wrapper_context_ptr_t context,
	uint64_t LogicalBlockAddress,
	uint32_t NumberOfBlocks,
	char *DataBuffer,
	uint32_t DataBufferLength
);

int lsp_wrapper_ide_packet_cmd(
	lsp_wrapper_context_ptr_t context,
	CDBd *cdb,
	char *DataBuffer,
	uint32_t DataBufferLength,
	UInt8 DataTransferDirection
);

int lsp_wrapper_get_max_request_blocks(
	lsp_wrapper_context_ptr_t context
);

#ifdef __cplusplus
}
#endif

#endif // _LSP_WRAPPER_H_