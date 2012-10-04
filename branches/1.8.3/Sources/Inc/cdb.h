#include <IOKit/IOTypes.h>

#ifndef _CDB_H_
#define _CDB_H_

//
// Command Descriptor Block. Passed by SCSI controller chip over the SCSI bus
//

typedef union _CDBd {

    //
    // Generic 6-Byte CDB
    //

    struct _CDB6GENERIC {
       UInt8  OperationCode;
       UInt8  Immediate : 1;
       UInt8  CommandUniqueBits : 4;
       UInt8  LogicalUnitNumber : 3;
       UInt8  CommandUniqueBytes[3];
       UInt8  Link : 1;
       UInt8  Flag : 1;
       UInt8  Reserved : 4;
       UInt8  VendorUnique : 2;
    } CDB6GENERIC, *PCDB6GENERIC;

    //
    // Standard 6-byte CDB
    //

    struct _CDB6READWRITE {
        UInt8 OperationCode;
        UInt8 LogicalBlockMsb1 : 5;
        UInt8 LogicalUnitNumber : 3;
        UInt8 LogicalBlockMsb0;
        UInt8 LogicalBlockLsb;
        UInt8 TransferBlocks;
        UInt8 Control;
    } CDB6READWRITE, *PCDB6READWRITE;

    //
    // SCSI Inquiry CDB
    //

    struct _CDB6INQUIRY {
        UInt8 OperationCode;
        UInt8 Reserved1 : 5;
        UInt8 LogicalUnitNumber : 3;
        UInt8 PageCode;
        UInt8 IReserved;
        UInt8 AllocationLength;
        UInt8 Control;
    } CDB6INQUIRY, *PCDB6INQUIRY;

    struct _CDB6VERIFY {
        UInt8 OperationCode;
        UInt8 Fixed : 1;
        UInt8 ByteCompare : 1;
        UInt8 Immediate : 1;
        UInt8 Reserved : 2;
        UInt8 LogicalUnitNumber : 3;
        UInt8 VerificationLength[3];
        UInt8 Control;
    } CDB6VERIFY, *PCDB6VERIFY;

    //
    // SCSI Format CDB
    //

    struct _CDB6FORMAT {
        UInt8 OperationCode;
        UInt8 FormatControl : 5;
        UInt8 LogicalUnitNumber : 3;
        UInt8 FReserved1;
        UInt8 InterleaveMsb;
        UInt8 InterleaveLsb;
        UInt8 FReserved2;
    } CDB6FORMAT, *PCDB6FORMAT;

    //
    // Standard 10-byte CDB

    struct _CDB10 {
        UInt8 OperationCode;
        UInt8 RelativeAddress : 1;
        UInt8 Reserved1 : 2;
        UInt8 ForceUnitAccess : 1;
        UInt8 DisablePageOut : 1;
        UInt8 LogicalUnitNumber : 3;
        UInt8 LogicalBlockByte0;
        UInt8 LogicalBlockByte1;
        UInt8 LogicalBlockByte2;
        UInt8 LogicalBlockByte3;
        UInt8 Reserved2;
        UInt8 TransferBlocksMsb;
        UInt8 TransferBlocksLsb;
        UInt8 Control;
    } CDB10, *PCDB10;

    //
    // Standard 12-byte CDB
    //

    struct _CDB12 {
        UInt8 OperationCode;
        UInt8 RelativeAddress : 1;
        UInt8 Reserved1 : 2;
        UInt8 ForceUnitAccess : 1;
        UInt8 DisablePageOut : 1;
        UInt8 LogicalUnitNumber : 3;
        UInt8 LogicalBlock[4];      // [0]=MSB, [3]=LSB
        UInt8 TransferLength[4];    // [0]=MSB, [3]=LSB
        UInt8 Reserved2;
        UInt8 Control;
    } CDB12, *PCDB12;



    //
    // CD Rom Audio CDBs
    //

    struct _PAUSE_RESUME {
        UInt8 OperationCode;
        UInt8 Reserved1 : 5;
        UInt8 LogicalUnitNumber : 3;
        UInt8 Reserved2[6];
        UInt8 Action;
        UInt8 Control;
    } PAUSE_RESUME, *PPAUSE_RESUME;

    //
    // Read Table of Contents
    //

    struct _READ_TOC {
        UInt8 OperationCode;
        UInt8 Reserved0 : 1;
        UInt8 Msf : 1;
        UInt8 Reserved1 : 3;
        UInt8 LogicalUnitNumber : 3;
        UInt8 Format2 : 4;
        UInt8 Reserved2 : 4;
        UInt8 Reserved3[3];
        UInt8 StartingTrack;
        UInt8 AllocationLength[2];
        UInt8 Control : 6;
        UInt8 Format : 2;
    } READ_TOC, *PREAD_TOC;

    struct _READ_DISK_INFORMATION {
        UInt8 OperationCode;    // 0x51
        UInt8 Reserved1 : 5;
        UInt8 Lun : 3;
        UInt8 Reserved2[5];
        UInt8 AllocationLength[2];
        UInt8 Control;
    } READ_DISK_INFORMATION, *PREAD_DISK_INFORMATION;

    struct _READ_TRACK_INFORMATION {
        UInt8 OperationCode;    // 0x52
        UInt8 Track : 1;
        UInt8 Reserved1 : 3;
        UInt8 Reserved2 : 1;
        UInt8 Lun : 3;
        UInt8 BlockAddress[4];  // or Track Number
        UInt8 Reserved3;
        UInt8 AllocationLength[2];
        UInt8 Control;
    } READ_TRACK_INFORMATION, *PREAD_TRACK_INFORMATION;

    struct _READ_HEADER {
        UInt8 OperationCode;    // 0x44
        UInt8 Reserved1 : 1;
        UInt8 Msf : 1;
        UInt8 Reserved2 : 3;
        UInt8 Lun : 3;
        UInt8 LogicalBlockAddress[4];
        UInt8 Reserved3;
        UInt8 AllocationLength[2];
        UInt8 Control;
    } READ_HEADER, *PREAD_HEADER;

    struct _PLAY_AUDIO {
        UInt8 OperationCode;
        UInt8 Reserved1 : 5;
        UInt8 LogicalUnitNumber : 3;
        UInt8 StartingBlockAddress[4];
        UInt8 Reserved2;
        UInt8 PlayLength[2];
        UInt8 Control;
    } PLAY_AUDIO, *PPLAY_AUDIO;

    struct _PLAY_AUDIO_MSF {
        UInt8 OperationCode;
        UInt8 Reserved1 : 5;
        UInt8 LogicalUnitNumber : 3;
        UInt8 Reserved2;
        UInt8 StartingM;
        UInt8 StartingS;
        UInt8 StartingF;
        UInt8 EndingM;
        UInt8 EndingS;
        UInt8 EndingF;
        UInt8 Control;
    } PLAY_AUDIO_MSF, *PPLAY_AUDIO_MSF;
/*
    struct _PLAY_CD {
        UInt8 OperationCode;    // 0xBC
        UInt8 Reserved1 : 1;
        UInt8 CMSF : 1;         // LBA = 0, MSF = 1
        UInt8 ExpectedSectorType : 3;
        UInt8 Lun : 3;

        union {
            struct _LBA {
                UInt8 StartingBlockAddress[4];
                UInt8 PlayLength[4];
            } LBA;

            struct _MSF {
                UInt8 Reserved1;
                UInt8 StartingM;
                UInt8 StartingS;
                UInt8 StartingF;
                UInt8 EndingM;
                UInt8 EndingS;
                UInt8 EndingF;
                UInt8 Reserved2;
            } MSF;
        };

        UInt8 Audio : 1;
        UInt8 Composite : 1;
        UInt8 Port1 : 1;
        UInt8 Port2 : 1;
        UInt8 Reserved2 : 3;
        UInt8 Speed : 1;
        UInt8 Control;
    } PLAY_CD, *PPLAY_CD;
*/
    struct _SCAN_CD {
        UInt8 OperationCode;    // 0xBA
        UInt8 RelativeAddress : 1;
        UInt8 Reserved1 : 3;
        UInt8 Direct : 1;
        UInt8 Lun : 3;
        UInt8 StartingAddress[4];
        UInt8 Reserved2[3];
        UInt8 Reserved3 : 6;
        UInt8 Type : 2;
        UInt8 Reserved4;
        UInt8 Control;
    } SCAN_CD, *PSCAN_CD;

    struct _STOP_PLAY_SCAN {
        UInt8 OperationCode;    // 0x4E
        UInt8 Reserved1 : 5;
        UInt8 Lun : 3;
        UInt8 Reserved2[7];
        UInt8 Control;
    } STOP_PLAY_SCAN, *PSTOP_PLAY_SCAN;


    //
    // Read SubChannel Data
    //

    struct _SUBCHANNEL {
        UInt8 OperationCode;
        UInt8 Reserved0 : 1;
        UInt8 Msf : 1;
        UInt8 Reserved1 : 3;
        UInt8 LogicalUnitNumber : 3;
        UInt8 Reserved2 : 6;
        UInt8 SubQ : 1;
        UInt8 Reserved3 : 1;
        UInt8 Format;
        UInt8 Reserved4[2];
        UInt8 TrackNumber;
        UInt8 AllocationLength[2];
        UInt8 Control;
    } SUBCHANNEL, *PSUBCHANNEL;

    //
    // Read CD. Used by Atapi for raw sector reads.
    //

    struct _READ_CD {
        UInt8 OperationCode;
        UInt8 RelativeAddress : 1;
        UInt8 Reserved0 : 1;
        UInt8 ExpectedSectorType : 3;
        UInt8 Lun : 3;
        UInt8 StartingLBA[4];
        UInt8 TransferBlocks[3];
        UInt8 Reserved2 : 1;
        UInt8 ErrorFlags : 2;
        UInt8 IncludeEDC : 1;
        UInt8 IncludeUserData : 1;
        UInt8 HeaderCode : 2;
        UInt8 IncludeSyncData : 1;
        UInt8 SubChannelSelection : 3;
        UInt8 Reserved3 : 5;
        UInt8 Control;
    } READ_CD, *PREAD_CD;

    struct _READ_CD_MSF {
        UInt8 OperationCode;
        UInt8 RelativeAddress : 1;
        UInt8 Reserved1 : 1;
        UInt8 ExpectedSectorType : 3;
        UInt8 Lun : 3;
        UInt8 Reserved2;
        UInt8 StartingM;
        UInt8 StartingS;
        UInt8 StartingF;
        UInt8 EndingM;
        UInt8 EndingS;
        UInt8 EndingF;
        UInt8 Reserved3;
        UInt8 Reserved4 : 1;
        UInt8 ErrorFlags : 2;
        UInt8 IncludeEDC : 1;
        UInt8 IncludeUserData : 1;
        UInt8 HeaderCode : 2;
        UInt8 IncludeSyncData : 1;
        UInt8 SubChannelSelection : 3;
        UInt8 Reserved5 : 5;
        UInt8 Control;
    } READ_CD_MSF, *PREAD_CD_MSF;

    //
    // Plextor Read CD-DA
    //

    struct _PLXTR_READ_CDDA {
        UInt8 OperationCode;
        UInt8 Reserved0 : 5;
        UInt8 LogicalUnitNumber :3;
        UInt8 LogicalBlockByte0;
        UInt8 LogicalBlockByte1;
        UInt8 LogicalBlockByte2;
        UInt8 LogicalBlockByte3;
        UInt8 TransferBlockByte0;
        UInt8 TransferBlockByte1;
        UInt8 TransferBlockByte2;
        UInt8 TransferBlockByte3;
        UInt8 SubCode;
        UInt8 Control;
    } PLXTR_READ_CDDA, *PPLXTR_READ_CDDA;

    //
    // NEC Read CD-DA
    //

    struct _NEC_READ_CDDA {
        UInt8 OperationCode;
        UInt8 Reserved0;
        UInt8 LogicalBlockByte0;
        UInt8 LogicalBlockByte1;
        UInt8 LogicalBlockByte2;
        UInt8 LogicalBlockByte3;
        UInt8 Reserved1;
        UInt8 TransferBlockByte0;
        UInt8 TransferBlockByte1;
        UInt8 Control;
    } NEC_READ_CDDA, *PNEC_READ_CDDA;

    //
    // Mode sense
    //

    struct _MODE_SENSE {
        UInt8 OperationCode;
        UInt8 Reserved1 : 3;
        UInt8 Dbd : 1;
        UInt8 Reserved2 : 1;
        UInt8 LogicalUnitNumber : 3;
        UInt8 PageCode : 6;
        UInt8 Pc : 2;
        UInt8 Reserved3;
        UInt8 AllocationLength;
        UInt8 Control;
    } MODE_SENSE, *PMODE_SENSE;

    struct _MODE_SENSE10 {
        UInt8 OperationCode;
        UInt8 Reserved1 : 3;
        UInt8 Dbd : 1;
        UInt8 Reserved2 : 1;
        UInt8 LogicalUnitNumber : 3;
        UInt8 PageCode : 6;
        UInt8 Pc : 2;
        UInt8 Reserved3[4];
        UInt8 AllocationLength[2];
        UInt8 Control;
    } MODE_SENSE10, *PMODE_SENSE10;

    //
    // Mode select
    //

    struct _MODE_SELECT {
        UInt8 OperationCode;
        UInt8 SPBit : 1;
        UInt8 Reserved1 : 3;
        UInt8 PFBit : 1;
        UInt8 LogicalUnitNumber : 3;
        UInt8 Reserved2[2];
        UInt8 ParameterListLength;
        UInt8 Control;
    } MODE_SELECT, *PMODE_SELECT;

    struct _MODE_SELECT10 {
        UInt8 OperationCode;
        UInt8 SPBit : 1;
        UInt8 Reserved1 : 3;
        UInt8 PFBit : 1;
        UInt8 LogicalUnitNumber : 3;
        UInt8 Reserved2[5];
        UInt8 ParameterListLength[2];
        UInt8 Control;
    } MODE_SELECT10, *PMODE_SELECT10;

    struct _LOCATE {
        UInt8 OperationCode;
        UInt8 Immediate : 1;
        UInt8 CPBit : 1;
        UInt8 BTBit : 1;
        UInt8 Reserved1 : 2;
        UInt8 LogicalUnitNumber : 3;
        UInt8 Reserved3;
        UInt8 LogicalBlockAddress[4];
        UInt8 Reserved4;
        UInt8 Partition;
        UInt8 Control;
    } LOCATE, *PLOCATE;

    struct _LOGSENSE {
        UInt8 OperationCode;
        UInt8 SPBit : 1;
        UInt8 PPCBit : 1;
        UInt8 Reserved1 : 3;
        UInt8 LogicalUnitNumber : 3;
        UInt8 PageCode : 6;
        UInt8 PCBit : 2;
        UInt8 Reserved2;
        UInt8 Reserved3;
        UInt8 ParameterPointer[2];  // [0]=MSB, [1]=LSB
        UInt8 AllocationLength[2];  // [0]=MSB, [1]=LSB
        UInt8 Control;
    } LOGSENSE, *PLOGSENSE;

    struct _LOGSELECT {
        UInt8 OperationCode;
        UInt8 SPBit : 1;
        UInt8 PCRBit : 1;
        UInt8 Reserved1 : 3;
        UInt8 LogicalUnitNumber : 3;
        UInt8 Reserved : 6;
        UInt8 PCBit : 2;
        UInt8 Reserved2[4];
        UInt8 ParameterListLength[2];  // [0]=MSB, [1]=LSB
        UInt8 Control;
    } LOGSELECT, *PLOGSELECT;

    struct _PRINT {
        UInt8 OperationCode;
        UInt8 Reserved : 5;
        UInt8 LogicalUnitNumber : 3;
        UInt8 TransferLength[3];
        UInt8 Control;
    } PRINT, *PPRINT;

    struct _SEEK {
        UInt8 OperationCode;
        UInt8 Reserved1 : 5;
        UInt8 LogicalUnitNumber : 3;
        UInt8 LogicalBlockAddress[4];
        UInt8 Reserved2[3];
        UInt8 Control;
    } SEEK, *PSEEK;

    struct _ERASE {
        UInt8 OperationCode;
        UInt8 Long : 1;
        UInt8 Immediate : 1;
        UInt8 Reserved1 : 3;
        UInt8 LogicalUnitNumber : 3;
        UInt8 Reserved2[3];
        UInt8 Control;
    } ERASE, *PERASE;

    struct _START_STOP {
        UInt8 OperationCode;
        UInt8 Immediate: 1;
        UInt8 Reserved1 : 4;
        UInt8 LogicalUnitNumber : 3;
        UInt8 Reserved2[2];
        UInt8 Start : 1;
        UInt8 LoadEject : 1;
        UInt8 Reserved3 : 6;
        UInt8 Control;
    } START_STOP, *PSTART_STOP;

    struct _MEDIA_REMOVAL {
        UInt8 OperationCode;
        UInt8 Reserved1 : 5;
        UInt8 LogicalUnitNumber : 3;
        UInt8 Reserved2[2];

        UInt8 Prevent : 1;
        UInt8 Persistant : 1;
        UInt8 Reserved3 : 6;

        UInt8 Control;
    } MEDIA_REMOVAL, *PMEDIA_REMOVAL;

    //
    // Tape CDBs
    //

    struct _SEEK_BLOCK {
        UInt8 OperationCode;
        UInt8 Immediate : 1;
        UInt8 Reserved1 : 7;
        UInt8 BlockAddress[3];
        UInt8 Link : 1;
        UInt8 Flag : 1;
        UInt8 Reserved2 : 4;
        UInt8 VendorUnique : 2;
    } SEEK_BLOCK, *PSEEK_BLOCK;

    struct _REQUEST_BLOCK_ADDRESS {
        UInt8 OperationCode;
        UInt8 Reserved1[3];
        UInt8 AllocationLength;
        UInt8 Link : 1;
        UInt8 Flag : 1;
        UInt8 Reserved2 : 4;
        UInt8 VendorUnique : 2;
    } REQUEST_BLOCK_ADDRESS, *PREQUEST_BLOCK_ADDRESS;

    struct _PARTITION {
        UInt8 OperationCode;
        UInt8 Immediate : 1;
        UInt8 Sel: 1;
        UInt8 PartitionSelect : 6;
        UInt8 Reserved1[3];
        UInt8 Control;
    } PARTITION, *PPARTITION;

    struct _WRITE_TAPE_MARKS {
        UInt8 OperationCode;
        UInt8 Immediate : 1;
        UInt8 WriteSetMarks: 1;
        UInt8 Reserved : 3;
        UInt8 LogicalUnitNumber : 3;
        UInt8 TransferLength[3];
        UInt8 Control;
    } WRITE_TAPE_MARKS, *PWRITE_TAPE_MARKS;

    struct _SPACE_TAPE_MARKS {
        UInt8 OperationCode;
        UInt8 Code : 3;
        UInt8 Reserved : 2;
        UInt8 LogicalUnitNumber : 3;
        UInt8 NumMarksMSB ;
        UInt8 NumMarks;
        UInt8 NumMarksLSB;
        union {
            UInt8 value;
            struct {
                UInt8 Link : 1;
                UInt8 Flag : 1;
                UInt8 Reserved : 4;
                UInt8 VendorUnique : 2;
            } Fields;
        } Byte6;
    } SPACE_TAPE_MARKS, *PSPACE_TAPE_MARKS;

    //
    // Read tape position
    //

    struct _READ_POSITION {
        UInt8 Operation;
        UInt8 BlockType:1;
        UInt8 Reserved1:4;
        UInt8 Lun:3;
        UInt8 Reserved2[7];
        UInt8 Control;
    } READ_POSITION, *PREAD_POSITION;

    //
    // ReadWrite for Tape
    //

    struct _CDB6READWRITETAPE {
        UInt8 OperationCode;
        UInt8 VendorSpecific : 5;
        UInt8 Reserved : 3;
        UInt8 TransferLenMSB;
        UInt8 TransferLen;
        UInt8 TransferLenLSB;
        UInt8 Link : 1;
        UInt8 Flag : 1;
        UInt8 Reserved1 : 4;
        UInt8 VendorUnique : 2;
    } CDB6READWRITETAPE, *PCDB6READWRITETAPE;

    //
    // Medium changer CDB's
    //

    struct _INIT_ELEMENT_STATUS {
        UInt8 OperationCode;
        UInt8 Reserved1 : 5;
        UInt8 LogicalUnitNubmer : 3;
        UInt8 Reserved2[3];
        UInt8 Reserved3 : 7;
        UInt8 NoBarCode : 1;
    } INIT_ELEMENT_STATUS, *PINIT_ELEMENT_STATUS;

    struct _INITIALIZE_ELEMENT_RANGE {
        UInt8 OperationCode;
        UInt8 Range : 1;
        UInt8 Reserved1 : 4;
        UInt8 LogicalUnitNubmer : 3;
        UInt8 FirstElementAddress[2];
        UInt8 Reserved2[2];
        UInt8 NumberOfElements[2];
        UInt8 Reserved3;
        UInt8 Reserved4 : 7;
        UInt8 NoBarCode : 1;
    } INITIALIZE_ELEMENT_RANGE, *PINITIALIZE_ELEMENT_RANGE;

    struct _POSITION_TO_ELEMENT {
        UInt8 OperationCode;
        UInt8 Reserved1 : 5;
        UInt8 LogicalUnitNumber : 3;
        UInt8 TransportElementAddress[2];
        UInt8 DestinationElementAddress[2];
        UInt8 Reserved2[2];
        UInt8 Flip : 1;
        UInt8 Reserved3 : 7;
        UInt8 Control;
    } POSITION_TO_ELEMENT, *PPOSITION_TO_ELEMENT;

    struct _MOVE_MEDIUM {
        UInt8 OperationCode;
        UInt8 Reserved1 : 5;
        UInt8 LogicalUnitNumber : 3;
        UInt8 TransportElementAddress[2];
        UInt8 SourceElementAddress[2];
        UInt8 DestinationElementAddress[2];
        UInt8 Reserved2[2];
        UInt8 Flip : 1;
        UInt8 Reserved3 : 7;
        UInt8 Control;
    } MOVE_MEDIUM, *PMOVE_MEDIUM;

    struct _EXCHANGE_MEDIUM {
        UInt8 OperationCode;
        UInt8 Reserved1 : 5;
        UInt8 LogicalUnitNumber : 3;
        UInt8 TransportElementAddress[2];
        UInt8 SourceElementAddress[2];
        UInt8 Destination1ElementAddress[2];
        UInt8 Destination2ElementAddress[2];
        UInt8 Flip1 : 1;
        UInt8 Flip2 : 1;
        UInt8 Reserved3 : 6;
        UInt8 Control;
    } EXCHANGE_MEDIUM, *PEXCHANGE_MEDIUM;

    struct _READ_ELEMENT_STATUS {
        UInt8 OperationCode;
        UInt8 ElementType : 4;
        UInt8 VolTag : 1;
        UInt8 LogicalUnitNumber : 3;
        UInt8 StartingElementAddress[2];
        UInt8 NumberOfElements[2];
        UInt8 Reserved1;
        UInt8 AllocationLength[3];
        UInt8 Reserved2;
        UInt8 Control;
    } READ_ELEMENT_STATUS, *PREAD_ELEMENT_STATUS;

    struct _SEND_VOLUME_TAG {
        UInt8 OperationCode;
        UInt8 ElementType : 4;
        UInt8 Reserved1 : 1;
        UInt8 LogicalUnitNumber : 3;
        UInt8 StartingElementAddress[2];
        UInt8 Reserved2;
        UInt8 ActionCode : 5;
        UInt8 Reserved3 : 3;
        UInt8 Reserved4[2];
        UInt8 ParameterListLength[2];
        UInt8 Reserved5;
        UInt8 Control;
    } SEND_VOLUME_TAG, *PSEND_VOLUME_TAG;

    struct _REQUEST_VOLUME_ELEMENT_ADDRESS {
        UInt8 OperationCode;
        UInt8 ElementType : 4;
        UInt8 VolTag : 1;
        UInt8 LogicalUnitNumber : 3;
        UInt8 StartingElementAddress[2];
        UInt8 NumberElements[2];
        UInt8 Reserved1;
        UInt8 AllocationLength[3];
        UInt8 Reserved2;
        UInt8 Control;
    } REQUEST_VOLUME_ELEMENT_ADDRESS, *PREQUEST_VOLUME_ELEMENT_ADDRESS;

    //
    // Atapi 2.5 Changer 12-byte CDBs
    //

    struct _LOAD_UNLOAD {
        UInt8 OperationCode;
        UInt8 Immediate : 1;
        UInt8 Reserved1 : 4;
        UInt8 Lun : 3;
        UInt8 Reserved2[2];
        UInt8 Start : 1;
        UInt8 LoadEject : 1;
        UInt8 Reserved3: 6;
        UInt8 Reserved4[3];
        UInt8 Slot;
        UInt8 Reserved5[3];
    } LOAD_UNLOAD, *PLOAD_UNLOAD;

    struct _MECH_STATUS {
        UInt8 OperationCode;
        UInt8 Reserved : 5;
        UInt8 Lun : 3;
        UInt8 Reserved1[6];
        UInt8 AllocationLength[2];
        UInt8 Reserved2[1];
        UInt8 Control;
    } MECH_STATUS, *PMECH_STATUS;

    //
    // C/DVD 0.9 CDBs
    //

    struct _SYNCHRONIZE_CACHE10 {

        UInt8 OperationCode;    // 0x35

        UInt8 RelAddr : 1;
        UInt8 Immediate : 1;
        UInt8 Reserved : 3;
        UInt8 Lun : 3;

        UInt8 LogicalBlockAddress[4];   // Unused - set to zero
        UInt8 Reserved2;
        UInt8 BlockCount[2];            // Unused - set to zero
        UInt8 Control;
    } SYNCHRONIZE_CACHE10, *PSYNCHRONIZE_CACHE10;

    struct _GET_EVENT_STATUS_NOTIFICATION {
        UInt8 OperationCode;    // 0x4a

        UInt8 Immediate : 1;
        UInt8 Reserved : 4;
        UInt8 Lun : 3;

        UInt8 Reserved2[2];
        UInt8 NotificationClassRequest;
        UInt8 Reserved3[2];
        UInt8 EventListLength[2];  // [0]=MSB, [1]=LSB

        UInt8 Control;
    } GET_EVENT_STATUS_NOTIFICATION, *PGET_EVENT_STATUS_NOTIFICATION;

    struct _READ_DVD_STRUCTURE {
        UInt8 OperationCode;    // 0xAD
        UInt8 Reserved1 : 5;
        UInt8 Lun : 3;
        UInt8 RMDBlockNumber[4];
        UInt8 LayerNumber;
        UInt8 Format;
        UInt8 AllocationLength[2];  // [0]=MSB, [1]=LSB
        UInt8 Reserved3 : 6;
        UInt8 AGID : 2;
        UInt8 Control;
    } READ_DVD_STRUCTURE, *PREAD_DVD_STRUCTURE;

    struct _SEND_KEY {
        UInt8 OperationCode;    // 0xA3
        UInt8 Reserved1 : 5;
        UInt8 Lun : 3;
        UInt8 Reserved2[6];
        UInt8 ParameterListLength[2];
        UInt8 KeyFormat : 6;
        UInt8 AGID : 2;
        UInt8 Control;
    } SEND_KEY, *PSEND_KEY;

    struct _REPORT_KEY {
        UInt8 OperationCode;    // 0xA4
        UInt8 Reserved1 : 5;
        UInt8 Lun : 3;
        UInt8 LogicalBlockAddress[4];   // for title key
        UInt8 Reserved2[2];
        UInt8 AllocationLength[2];
        UInt8 KeyFormat : 6;
        UInt8 AGID : 2;
        UInt8 Control;
    } REPORT_KEY, *PREPORT_KEY;

    struct _SET_READ_AHEAD {
        UInt8 OperationCode;    // 0xA7
        UInt8 Reserved1 : 5;
        UInt8 Lun : 3;
        UInt8 TriggerLBA[4];
        UInt8 ReadAheadLBA[4];
        UInt8 Reserved2;
        UInt8 Control;
    } SET_READ_AHEAD, *PSET_READ_AHEAD;

    struct _READ_FORMATTED_CAPACITIES {
        UInt8 OperationCode;    // 0xA7
        UInt8 Reserved1 : 5;
        UInt8 Lun : 3;
        UInt8 Reserved2[5];
        UInt8 AllocationLength[2];
        UInt8 Control;
    } READ_FORMATTED_CAPACITIES, *PREAD_FORMATTED_CAPACITIES;

    //
    // SCSI-3
    //

    struct _REPORT_LUNS {
        UInt8 OperationCode;    // 0xA0
        UInt8 Reserved1[5];
        UInt8 AllocationLength[4];
        UInt8 Reserved2[1];
        UInt8 Control;
    } REPORT_LUNS, *PREPORT_LUNS;

    uint32_t AsUlong[4];
    UInt8 AsByte[16];

} CDBd, *PCDBd;

#endif