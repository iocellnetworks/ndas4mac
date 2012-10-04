/***************************************************************************
 *
 *  Hash.h
 *
 *    
 *
 *    Copyright (c) 2003 XiMeta, Inc. All rights reserved.
 *
 **************************************************************************/

#ifndef _HASH_H_
#define _HASH_H_

//#define HASH_KEY_USER           0x1f4a50731530eabbULL
//#define HASH_KEY_SUPER          0x3E2B321A4750131EULL

#define KEY_CON0                    0x268F2736
#define KEY_CON1                    0x813A76BC

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

// Byte Convert Macros.
#define ByteConvert16(Data)	\
	(uint16_t)((((Data)&0x00FF) << 8) | (((Data)&0xFF00) >> 8))
	
	
#define ByteConvert32(Data)	\
	(uint32_t) ( (((Data)&0x000000FF) << 24) | (((Data)&0x0000FF00) << 8) \
				 | (((Data)&0x00FF0000)  >> 8) | (((Data)&0xFF000000) >> 24))
	
#define ByteConvert64(Data)    \
	(uint64_t) ( (((Data)&0x00000000000000FFULL) << 56) | (((Data)&0x000000000000FF00ULL) << 40) \
				 | (((Data)&0x0000000000FF0000ULL) << 24) | (((Data)&0x00000000FF000000ULL) << 8)  \
				 | (((Data)&0x000000FF00000000ULL) >> 8)  | (((Data)&0x0000FF0000000000ULL) >> 24) \
				 | (((Data)&0x00FF000000000000ULL) >> 40) | (((Data)&0xFF00000000000000ULL) >> 56))

// Byte Swap Macros.
#if defined(__BIG_ENDIAN__)

#define NDASSwap16HostToBig(Data)		(uint16_t)((Data))
#define NDASSwap32HostToBig(Data)		(uint32_t)((Data))
#define NDASSwap64HostToBig(Data)		(uint64_t)((Data))

#define NDASSwap16BigToHost(Data)		(uint16_t)((Data))
#define NDASSwap32BigToHost(Data)		(uint32_t)((Data))
#define NDASSwap64BigToHost(Data)		(uint64_t)((Data))

#define NDASSwap16HostToLittle(Data)	ByteConvert16(Data)
#define NDASSwap32HostToLittle(Data)	ByteConvert32(Data)
#define NDASSwap64HostToLittle(Data)	ByteConvert64(Data)
	
#define NDASSwap16LittleToHost(Data)	ByteConvert16(Data)
#define NDASSwap32LittleToHost(Data)	ByteConvert32(Data)
#define NDASSwap64LittleToHost(Data)	ByteConvert64(Data)
	
#else

#define NDASSwap16HostToBig(Data)		ByteConvert16(Data)
#define NDASSwap32HostToBig(Data)		ByteConvert32(Data)
#define NDASSwap64HostToBig(Data)		ByteConvert64(Data)
	
#define NDASSwap16BigToHost(Data)		ByteConvert16(Data)
#define NDASSwap32BigToHost(Data)		ByteConvert32(Data)
#define NDASSwap64BigToHost(Data)		ByteConvert64(Data)
	
#define NDASSwap16HostToLittle(Data)	(uint16_t)((Data))
#define NDASSwap32HostToLittle(Data)	(uint32_t)((Data))
#define NDASSwap64HostToLittle(Data)	(uint64_t)((Data))
	
#define NDASSwap16LittleToHost(Data)	(uint16_t)((Data))
#define NDASSwap32LittleToHost(Data)	(uint32_t)((Data))
#define NDASSwap64LittleToHost(Data)	(uint64_t)((Data))
	
#endif

void
Hash32To128(
            uint8_t   *pSource,
            uint8_t   *pResult,
            uint8_t   *pKey
            );

void
Encrypt32(
          uint8_t     *pData,
          uint32_t  uiDataLength,
          uint8_t     *pKey,
          uint8_t     *pPassword
          );

void
Decrypt32(
          uint8_t     *pData,
          uint32_t  uiDataLength,
          uint8_t     *pKey,
          uint8_t     *pPassword
          );

#ifdef __cplusplus
}
#endif	/* __cplusplus */

#endif	/* !_HASH_H_ */
