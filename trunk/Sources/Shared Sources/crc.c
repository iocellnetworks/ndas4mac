/***************************************************************************
 *
 *  crc.c
 *
 *    
 *
 *    Copyright (c) 2003 XiMeta, Inc. All rights reserved.
 *
 **************************************************************************/

#ifdef WIN32
#include	"crc.h"
#include	<stdio.h>
#else
#include 	"crc.h"
#endif

void CRC_init_table(unsigned long *table)
{
    unsigned long	ulPolynomial = 0x04C11DB7;
	int				i,j;

    for (i = 0; i <= 0xFF; i++)
    {
        table[i] = CRC_reflect(i, 8) << 24;
        for (j = 0; j < 8; j++)
            table[i] = (table[i] << 1) ^ (table[i] & (1 << 31) ? ulPolynomial : 0);
        table[i] = CRC_reflect(table[i], 32);
    }
}


unsigned long CRC_reflect(unsigned long ref, char ch)
{
    unsigned long	value = 0;
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

unsigned long CRC_calculate(unsigned char *buffer, unsigned int size)
{
	unsigned long	lookup_table[256];
	unsigned long	CRC = 0xffffffff;

	CRC_init_table(lookup_table);

    while (size--) {
//		printf("%10u %10lu : %10lu\n", *buffer, (CRC & 0xFF) ^ *buffer, CRC);
		CRC = (CRC >> 8) ^ lookup_table[(CRC & 0xFF) ^ *buffer++];
	}
//	printf("\n");
//	printf("%lu\n", CRC);
	return(CRC);
}
