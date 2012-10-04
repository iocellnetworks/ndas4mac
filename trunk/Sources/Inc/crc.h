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

//#include	"NDAdmin_Prefix.h"

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus
	
void			CRC_init_table(unsigned long *table);
unsigned long	CRC_reflect(unsigned long ref, char ch);
unsigned long	CRC_calculate(unsigned char *buffer, unsigned int size);

#ifdef __cplusplus
}
#endif // __cplusplus
	
#endif
