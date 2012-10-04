/***************************************************************************
 *
 *  hash.c
 *
 *    
 *
 *    Copyright (c) 2003 XiMeta, Inc. All rights reserved.
 *
 **************************************************************************/

#ifdef KERNEL

#include <sys/systm.h>

#else

//#include "stdafx.h"
//#include <IOKit/IOKitLib.h>
#include <stdio.h>
//#include <winsock2.h>

#endif

#include <string.h>
#include "Hash.h"

//
// pResult is Big Endian...
// pSource and pKey are Little Endian.
//
void
Hash32To128(
			unsigned char	*pSource,
			unsigned char	*pResult,
			unsigned char	*pKey
			)
{
	unsigned		uiTemp;
	unsigned char	ucKey_con0[4];
	unsigned char	ucKey_con1[4];
#if DEBUG
        int i;
#endif /* if DEBUG */
        
	uiTemp = NDASSwap32LittleToHost(KEY_CON0);
	memcpy(ucKey_con0, &uiTemp, 4);

	uiTemp = NDASSwap32LittleToHost(KEY_CON1);
	memcpy(ucKey_con1, &uiTemp, 4);

	pResult[ 0] = pKey[2] ^ pSource[1];
	pResult[ 1] = ucKey_con0[3] | pSource[2];
	pResult[ 2] = ucKey_con0[2] & pSource[1];
	pResult[ 3] = ~(pKey[6] ^ pSource[2]);
	pResult[ 4] = ucKey_con1[2] ^ pSource[3];
	pResult[ 5] = pKey[0] & pSource[0];
	pResult[ 6] = ~(ucKey_con1[0] ^ pSource[1]);
	pResult[ 7] = pKey[5] | pSource[3];
	pResult[ 8] = pKey[7] & pSource[2];
	pResult[ 9] = ucKey_con0[0] ^ pSource[1];
	pResult[10] = pKey[4] ^ pSource[3];
	pResult[11] = ~(ucKey_con1[1] ^ pSource[0]);
	pResult[12] = pKey[3] ^ pSource[2];
	pResult[13] = ucKey_con0[1] & pSource[3];
	pResult[14] = pKey[1] ^ pSource[3];
	pResult[15] = ucKey_con1[3] | pSource[0];

#if DEBUG
	printf("Hash: Source ");
	for(i = 0; i < 4; i++) {
		printf("0x%x ", pSource[i]);
	}

	printf("\nHash: Key ");
	for(i = 0; i < 8; i++) {
		printf("0x%x ", pKey[i]);
	}

	printf("\nHash: Key_con0 ");
	for(i = 0; i < 4; i++) {
		printf("0x%x ", ucKey_con0[i]);
	}

	printf("\nHash: Key_con1 ");
	for(i = 0; i < 4; i++) {
		printf("0x%x ", ucKey_con1[i]);
	}
	
	printf("\nHash: Result ");
	for(i = 0; i < 16; i++) {
		printf("0x%x ", pResult[i]);
	}
#endif /* if DEBUG */	
        
	return;
}

void
Encrypt32(
		  unsigned char		*pData,
		  uint32_t	uiDataLength,
		  unsigned char		*pKey,
		  unsigned char		*pPassword
		  )
{
	unsigned int	i, j;
	char			temp[4];
	unsigned char	*pDataPtr;

//        return;
        
#if DEBUG
        printf("Hash: Source ");
        for(i = 0; i < 4; i++) {
            printf("0x%x ", pKey[i]);
        }

        printf("\nHash: Key ");
        for(i = 0; i < 8; i++) {
            printf("0x%x ", pPassword[i]);
        }
#endif /* if DEBUG */
        for(i = 0; i < uiDataLength / 4; i++) {
		pDataPtr = &pData[i * 4];

		temp[0] = pPassword[1] ^ pPassword[7] ^ pDataPtr[3] ^ pKey[3];
		temp[1] = ~(pPassword[0] ^ pPassword[3] ^ pKey[0] ^ pDataPtr[0]);
		temp[2] = pPassword[2] ^ pPassword[6] ^ pDataPtr[1] ^ pKey[2];
		temp[3] = ~(pPassword[4] ^ pPassword[5] ^ pDataPtr[2] ^ pKey[1]);

                for(j = 0; j < 4; j++) {
			pDataPtr[j] = temp[j];
//                    printf("%x ", pDataPtr[j]);
                }

        }
//        printf("\n");
/*
	unsigned int	i;
	register unsigned int	tempInt;
	unsigned char	*pDataPtr;

	for(i = 0; i < uiDataLength / 4; i++) {
		pDataPtr = &pData[i * 4];

		tempInt = ((~(pPassword[4] ^ pPassword[5] ^ pDataPtr[2] ^ pKey[1])) << 24)
			| ((pPassword[2] ^ pPassword[6] ^ pDataPtr[1] ^ pKey[2]) << 16)
			| ((~(pPassword[0] ^ pPassword[3] ^ pKey[0] ^ pDataPtr[0])) << 8)
			| (pPassword[1] ^ pPassword[7] ^ pDataPtr[3] ^ pKey[3]);


		//memcpy(pDataPtr, &tempInt, 4);
		//sRtlCopyMemory(pDataPtr, &tempInt, 4);
		pDataPtr[0] = ((PUCHAR)&tempInt)[0];
		pDataPtr[1] = ((PUCHAR)&tempInt)[1];
		pDataPtr[2] = ((PUCHAR)&tempInt)[2];
		pDataPtr[3] = ((PUCHAR)&tempInt)[3];
	}
*/
}

void
Decrypt32(
		  unsigned char		*pData,
		  uint32_t	uiDataLength,
		  unsigned char		*pKey,
		  unsigned char		*pPassword
		  )
{
	unsigned int	i, j;
	char			temp[4];
	unsigned char	*pDataPtr;

//        return;
        
        for(i = 0; i < uiDataLength / 4; i++) {
		pDataPtr = &pData[i * 4];

//            for(j = 0; j < 4; j++)
//                printf("%x ", pData[i*4 + j]);

//            printf("\n");
        
		temp[3] = pPassword[1] ^ pPassword[7] ^ pDataPtr[0] ^ pKey[3];
		temp[0] = pPassword[0] ^ pPassword[3] ^ pKey[0] ^ ~(pDataPtr[1]);
		temp[1] = pPassword[2] ^ pPassword[6] ^ pDataPtr[2] ^ pKey[2];
		temp[2] = pPassword[4] ^ pPassword[5] ^ pDataPtr[3] ^ ~(pKey[1]);

                for(j = 0; j < 4; j++) {
			pDataPtr[j] = temp[j];
//                      printf("%x ", pDataPtr[j]);
                }
	}

//        printf("\n");
}
