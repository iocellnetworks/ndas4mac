/***************************************************************************
 *
 *  crc.c
 *
 *    
 *
 *    Copyright (c) 2003 XiMeta, Inc. All rights reserved.
 *
 **************************************************************************/

#include <stdint.h>

#ifdef WIN32
#include	"crc.h"
#include	<stdio.h>
#else
#include 	"crc.h"
#endif

void CRC_init_table(uint32_t *table)
{
    uint32_t	ulPolynomial = 0x04C11DB7;
	int				i,j;

    for (i = 0; i <= 0xFF; i++)
    {
        table[i] = CRC_reflect(i, 8) << 24;
        for (j = 0; j < 8; j++)
            table[i] = (table[i] << 1) ^ (table[i] & (1 << 31) ? ulPolynomial : 0);
        table[i] = CRC_reflect(table[i], 32);
    }
}


uint32_t CRC_reflect(uint32_t ref, char ch)
{
    uint32_t	value = 0;
	int				i;

    // Swap bit 0 for bit 7
    // bit 1 for bit 6, etc.
    for(i = 1; i < (ch + 1); i++)
    {
        if (ref & 1)
            value |= 1 << (ch - i);
        ref >>= 1;
    }
    return value;
}

uint32_t CRC_calculate(uint8_t *buffer, uint32_t size)
{
	uint32_t	lookup_table[256];
	uint32_t	CRC = 0xffffffff;

	CRC_init_table(lookup_table);

    while (size--) {
//		printf("%10u %10lu : %10lu\n", *buffer, (CRC & 0xFF) ^ *buffer, CRC);
		CRC = (CRC >> 8) ^ lookup_table[(CRC & 0xFF) ^ *buffer++];
	}
//	printf("\n");
//	printf("%lu\n", CRC);
	return(CRC);
}
