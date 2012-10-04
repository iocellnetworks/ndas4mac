/***************************************************************************
 *
 *  crc.h
 *
 *    
 *
 *    Copyright (c) 2003 XiMeta, Inc. All rights reserved.
 *
 **************************************************************************/

#ifndef		_CRC_
#define		_CRC_

#include	<stdint.h>
//#include	"NDAdmin_Prefix.h"

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus
	
void		CRC_init_table(uint32_t *table);
uint32_t	CRC_reflect(uint32_t ref, char ch);
uint32_t	CRC_calculate(uint8_t *buffer, uint32_t size);

#ifdef __cplusplus
}
#endif // __cplusplus
	
#endif
