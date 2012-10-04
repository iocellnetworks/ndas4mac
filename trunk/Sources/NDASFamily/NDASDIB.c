#include "NDASDIB.h"

#include "LanScsiDiskInformationBlock.h"
#include "hdreg.h"
#include "crc.h"
#include "hash.h"

#include "Utilities.h"

int scrutinize_x_area(
					  struct LanScsiSession *Session,
					  uint32_t				TargetID, 
					  TARGET_DATA			*TargetData
					  )
{
    int error;
    uint8_t ide_result;
    int64_t end;
    int64_t location;
    NDAS_DIB_V2 dib_v2;
	
	end = TargetData->SectorCount;
   
    /*** Scrutiny of x area of a newly attached disk */
	
    /**
		*  1. Read an NDAS_DIB_V2 structure from the NDAS Device at
     *     NDAS_BLOCK_LOCATION_DIB_V2.
     */
	
	//ND_LOG("1st..\n");
    location = end + NDAS_BLOCK_LOCATION_DIB_V2;
	
    if ((error = IdeCommand(Session, TargetID, TargetData, 0, WIN_READ,
							location, 1, 0, (char *) & dib_v2, sizeof(dib_v2), & ide_result, NULL, 0))
        != 0)
    {
        DbgIOLogNC(DEBUG_MASK_NDAS_ERROR, ("scrutinize_x_area: Reading DIB V2 of X area is failed.\n"));
		
        return -1;
    }
	
	//    print_dib_v2(& dib_v2);
	
    /**
		*  2. Check Signature, Version and CRC information in NDAS_DIB_V2 and
     *     accept if all the information is correct.
     */
		
	//ND_LOG("2nd..\n");
    if (NDAS_DIB_SIGNATURE_V2 == dib_v2.Signature) {
//        && dib_v2.crc32 ==
//		CRC_calculate((unsigned char *) & dib_v2, sizeof(dib_v2.bytes_248))
  //      && dib_v2.crc32_unitdisks ==
//		CRC_calculate((unsigned char *) & dib_v2.UnitDisks,
//					  sizeof(dib_v2.UnitDisks)))
//    {
        if (NDAS_DIB_VERSION_MAJOR_V2 < dib_v2.MajorVersion
            || (NDAS_DIB_VERSION_MAJOR_V2 == dib_v2.MajorVersion
                && NDAS_DIB_VERSION_MINOR_V2 <= dib_v2.MinorVersion))
        {
            /* UNKNOWN DIB VERSION */
            /* There is nothing to do yet. */
        }
		
		
        /**
		*  3. Read additional NDAS Device location information at
         *     NDAS_BLOCK_LOCATION_ADD_BIND in case of more than 32 NDAS Unit
         *     devices exist.
         */
		
		//ND_LOG("3rd..\n");
        if (dib_v2.nDiskCount > 1)
        {
            /* There is nothing to do yet. */
        }
		
        /* Copy DIB V2 to lanScsiAdapter */
//        memcpy((void *) & disk->dib_v2, (void *) & dib_v2, sizeof (dib_v2));
		
        /* Get a disk type */
        TargetData->DiskArrayType = ByteConvert4(dib_v2.iMediaType);
		DbgIOLogNC(DEBUG_MASK_NDAS_INFO, ("scrutinize_x_area: DIB2. 0x%x\n", ByteConvert4(dib_v2.iMediaType)));
		
        return 0;
    } else {
		DbgIOLogNC(DEBUG_MASK_NDAS_INFO, ("scrutinize_x_area: Bad DIB2. %llx\n", dib_v2.Signature));
		
		/* Get a disk type */
		TargetData->DiskArrayType = NMT_SINGLE;
		
		//    print_dib_v2(& dib_v2);
		
		return 0;
	}
		
#if 0	
    /**
		*  4. Read an NDAS_DIB_V1 information at NDAS_BLOCK_LOCATION_DIB_V1 if
     *     NDAS_DIB_V2 information is not acceptable.
     */
	
	//ND_LOG("4th..\n");
    location = end + NDAS_BLOCK_LOCATION_DIB_V1;
	
    if ((error = IdeCommand(Session, TargetID, TargetData, 0, WIN_READ,
							location, 1, 0, (char *) & dib_v1, sizeof(dib_v1), & ide_result, NULL))
        != 0)
    {
        DbgIOLog(DEBUG_MASK_NDAS_ERROR, ("[LanScsiCli]main: Reading DIB V1 of X area is failed.\n"));
		
        return -1;
    }
	
	//    print_dib_v1(& dib_v1);
	
	
    /** Create a new DIB ver. 2 */
	
    bzero((void *) & dib_v2, sizeof(NDAS_DIB_V2));
	
	//    print_dib_v2(& dib_v2);
	
	//    dib_v2.Signature = NDAS_DIB_SIGNATURE_V2;
    memcpy((void *) & dib_v2.Signature, (void *) & NDAS_DIB_SIGNATURE_V2,
		   sizeof (dib_v2.Signature));
    dib_v2.MajorVersion = NDAS_DIB_VERSION_MAJOR_V2;
    dib_v2.MinorVersion = NDAS_DIB_VERSION_MINOR_V2;
    dib_v2.sizeXArea = NDAS_SIZE_XAREA_SECTOR;
    dib_v2.iSectorsPerBit = 0;    /* No backup information */
    /* In case of mirror, use primary disk size */
    dib_v2.sizeUserSpace = TargetData->SectorCount - NDAS_SIZE_XAREA_SECTOR;
	
	
    /**
		*  5. Check Signature and Version information in NDAS_DIB_V1 and translate
     *     the NDAS_DIB_V1 to an NDAS_DIB_V2.
     */
	
	//ND_LOG("5th..\n");
    if (NDAS_DIB_SIGNATURE_V1 == dib_v1.Signature)
    {
		//IOLog("OK, THIS IS NDAS DIB V1..\n");
        if (NDAS_DIB_VERSION_MAJOR_V1 < dib_v1.MajorVersion
            || (NDAS_DIB_VERSION_MAJOR_V1 == dib_v1.MajorVersion
                && NDAS_DIB_VERSION_MINOR_V1 <= dib_v1.MinorVersion))
        {
            /* UNKNOWN DIB VERSION */
            /* There is nothing to do yet. */
        }
		
        if (dib_v1.DiskType == NDAS_DIB_DISK_TYPE_SINGLE)
        {
            dib_v2.iMediaType = NMT_SINGLE;
            dib_v2.iSequence = 0;
            dib_v2.nDiskCount = 1;
            bzero(& dib_v2.UnitDisks[0], sizeof(UNIT_DISK_LOCATION));
        }
        else    /* Not single type, version 1 has only pair disks (M&A). */
        {
            UNIT_DISK_LOCATION * pUnitDiskLocation0, * pUnitDiskLocation1;
			
            if (dib_v1.DiskType == NDAS_DIB_DISK_TYPE_MIRROR_MASTER ||
                dib_v1.DiskType == NDAS_DIB_DISK_TYPE_AGGREGATION_FIRST)
            {
                pUnitDiskLocation0 = & dib_v2.UnitDisks[0];
                pUnitDiskLocation1 = & dib_v2.UnitDisks[1];
            }
            else
            {
                pUnitDiskLocation0 = & dib_v2.UnitDisks[1];
                pUnitDiskLocation1 = & dib_v2.UnitDisks[0];
            }
			
			
            /** Store the MAC address into each UnitDisk */
			
            if (0x00 == dib_v1.EtherAddress[0] && 0x00 == dib_v1.EtherAddress[1] &&
                0x00 == dib_v1.EtherAddress[2] && 0x00 == dib_v1.EtherAddress[3] &&
                0x00 == dib_v1.EtherAddress[4] && 0x00 == dib_v1.EtherAddress[5])
            {
                /* Usually, there is no Ethernet address information */
//                memcpy(pUnitDiskLocation0->MACAddr, disk->DevAddr.slpx_node,
//					   sizeof(pUnitDiskLocation0->MACAddr));
                pUnitDiskLocation0->UnitNumber = 0;
            }
            else
            {
                memcpy(& pUnitDiskLocation0->MACAddr, dib_v1.EtherAddress,
					   sizeof(pUnitDiskLocation0->MACAddr));
                pUnitDiskLocation0->UnitNumber = dib_v1.UnitNumber;
            }
			
            memcpy(pUnitDiskLocation1->MACAddr, dib_v1.PeerAddress,
				   sizeof(pUnitDiskLocation1->MACAddr));
            pUnitDiskLocation1->UnitNumber = dib_v1.UnitNumber;
			
			
            dib_v2.nDiskCount = 2;
			
            switch (dib_v1.DiskType)
            {
                case NDAS_DIB_DISK_TYPE_MIRROR_MASTER:
                    dib_v2.iMediaType = NMT_MIRROR;
                    dib_v2.iSequence = 0;
                    break;
					
                case NDAS_DIB_DISK_TYPE_MIRROR_SLAVE:
                    dib_v2.iMediaType = NMT_MIRROR;
                    dib_v2.iSequence = 1;
                    break;
					
                case NDAS_DIB_DISK_TYPE_AGGREGATION_FIRST:
                    dib_v2.iMediaType = NMT_AGGREGATE;
                    dib_v2.iSequence = 0;
                    break;
					
                case NDAS_DIB_DISK_TYPE_AGGREGATION_SECOND:
                    dib_v2.iMediaType = NMT_AGGREGATE;
                    dib_v2.iSequence = 1;
                    break;
					
                default:
                    DbgIOLog(1, ("[LanScsiCli]main: Unknown disk type.\n"));
                    return -1;
            }
        }
		
        /* Make CRC codes */
        dib_v2.crc32 = CRC_calculate((unsigned char *) & dib_v2,
									 sizeof (dib_v2.bytes_248));
        dib_v2.crc32_unitdisks =
            CRC_calculate((unsigned char *) & dib_v2.UnitDisks,
						  sizeof (dib_v2.UnitDisks));
		
        /* Copy DIB V2 to lanScsiAdapter */
//        memcpy((void *) & disk->dib_v2, (void *) & dib_v2, sizeof (dib_v2));
		
        /* Get a disk type */
        TargetData->DiskArrayType = dib_v2.iMediaType;  /* NMT_SINGLE */
		
		/* KIHUN: DO NOT CLEAR DIB V1 FOR THE COMPATIBILITY */

        /* Clear DIB V1 */
        location = end + NDAS_BLOCK_LOCATION_DIB_V1;
        bzero((void *) & blank, sizeof (unsigned char) * 512);
		
        if ((error = IdeCommand(& disk->Session, 0, & disk->TargetData, 0,
								WIN_WRITE, location, 1, 0, (char *) blank, & ide_result))
            != 0)
        {
            DbgIOLog(1, ("[LanScsiCli]main: Clearing DIB V1 of X area is failed.\n"));
            return -1;
        }
		
        /* Write a new DIB V2 into the disk */
        location = end + NDAS_BLOCK_LOCATION_DIB_V2;
		
        bzero((void *) & blank, sizeof (unsigned char) * 512);
		
        if ((error = IdeCommand(Session, TargetID, TargetData, 0,
								WIN_WRITE, location, 1, 0, (char *) blank, sizeof(blank), & ide_result, NULL))
            != 0)
        {
            DbgIOLog(1, ("[LanScsiCli]main: Clearing DIB V2 of X area is failed.\n"));
            return -1;
        }
		
        if ((error = IdeCommand(Session, TargetID, TargetData, 0,
                                WIN_WRITE, location, 1, 0, (char *) & dib_v2, sizeof(dib_v2),
                                & ide_result, NULL))
            != 0)
        {
            DbgIOLog(1, ("[LanScsiCli]main: Updating DIB V2 of X area is failed.\n"));
            return -1;
        }
		
        return 0;
    }
	
	
    /**
		*  6. Create an NDAS_DIB_V2 as single NDAS Disk Device if the NDAS_DIB_V1
     *     is not acceptable either.
     */
	
	//ND_LOG("6th..\n");
    /* Finalize to create a DIB ver. 2 */
    dib_v2.iMediaType = NMT_SINGLE;
    dib_v2.iSequence = 0;
    dib_v2.nDiskCount = 1;
    bzero(& dib_v2.UnitDisks[0], sizeof(UNIT_DISK_LOCATION));
//    memcpy(dib_v2.UnitDisks[0].MACAddr, disk->DevAddr.slpx_node,
//		   sizeof(dib_v2.UnitDisks[0].MACAddr));
    dib_v2.UnitDisks[0].UnitNumber = 0;
	
	/* KIHUN: DO NOT CLEAR DIB V1 FOR THE COMPATIBILITY */

    /* Clear DIB V1 */
    location = end + NDAS_BLOCK_LOCATION_DIB_V1;
    bzero((void *) & blank, sizeof (unsigned char) * 512);
	
    if ((error = IdeCommand(& disk->Session, 0, & disk->TargetData, 0,
							WIN_WRITE, location, 1, 0, (char *) blank, & ide_result))
        != 0)
    {
        DbgIOLog(1, ("[LanScsiCli]main: Clearing DIB V1 of X area is failed.\n"));
        return -1;
    }
	
    /* Make CRC codes */
    dib_v2.crc32 = CRC_calculate((unsigned char *) & dib_v2,
								 sizeof (dib_v2.bytes_248));
    dib_v2.crc32_unitdisks = CRC_calculate((unsigned char *) & dib_v2.UnitDisks,
										   sizeof (dib_v2.UnitDisks));
	
    /* Copy DIB V2 to lanScsiAdapter */
//    memcpy((void *) & disk->dib_v2, (void *) & dib_v2, sizeof (dib_v2));
	
    /* Write a new DIB V2 into the disk */
    location = end + NDAS_BLOCK_LOCATION_DIB_V2;
	
	//    print_dib_v2(& dib_v2);
	
    bzero((void *) & blank, sizeof (unsigned char) * 512);
	
    if ((error = IdeCommand(Session, TargetID, TargetData, 0,
							WIN_WRITE, location, 1, 0, (char *) blank, sizeof(blank), & ide_result, NULL))
        != 0)
    {
        DbgIOLog(1, ("[LanScsiCli]main: Clearing DIB V2 of X area is failed.\n"));
        return -1;
    }
	
    if ((error = IdeCommand(Session, TargetID, TargetData, 0,
                            WIN_WRITE, location, 1, 0, (char *) & dib_v2, sizeof(dib_v2),
                            & ide_result, NULL))
        != 0)
    {
        DbgIOLog(1, ("[LanScsiCli]main: Creating DIB V2 of X area is failed.\n"));
        return -1;
    }
	
    /* Get a disk type */
    TargetData->DiskArrayType = NMT_SINGLE;
	
	//    print_dib_v2(& dib_v2);
	
    return 0;
#endif
	
} /* int scrutinize_x_area() */



