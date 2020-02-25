/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/***************************************************************
 File:	mhi_dut.h
 Module:	Dut
 Purpose: 	
 Description: 
***************************************************************/
#ifndef __MHI_DUT_INCLUDED_H
#define __MHI_DUT_INCLUDED_H
//---------------------------------------------------------------------------------
//						Includes									
//---------------------------------------------------------------------------------
#include "mhi_ieee_address.h"
#include "mhi_mib_id.h"

#define   MTLK_PACK_ON
#include "mtlkpack.h"

//---------------------------------------------------------------------------------
//						Defines									
//---------------------------------------------------------------------------------
#define TPC_ARRAY_SIZE					(3)
#define DUT_ANT_VALUES_ARRAY_SIZE		(3)

#define BIST_RESULTS_ARRAY_SIZE			(7) 

#define UMI_DEBUG_BLOCK_SIZE            (384)
#define UMI_C100_DATA_SIZE   (UMI_DEBUG_BLOCK_SIZE - (sizeof(uint16) + sizeof(uint16) + sizeof(C100_MSG_HEADER)))
#define UMI_DEBUG_DATA_SIZE  (UMI_DEBUG_BLOCK_SIZE - (sizeof(uint16) + sizeof(uint16)))

#define DUT_MSG_DATA_LENGTH				(376)	
#define MAX_DUT_EEPROM_ACCESS_SIZE (DUT_MSG_DATA_LENGTH-16)

typedef enum dutChipModule
{
	DUT_CHIP_MODULE_UMAC_MEM	= 1,
	DUT_CHIP_MODULE_LMAC_MEM,
	DUT_CHIP_MODULE_PHY,
	DUT_CHIP_MODULE_RF,
	DUT_CHIP_MODULE_AFE,
} dutChipModule_e;

typedef enum dutNvMemoryType
{
	DUT_NV_MEMORY_EEPROM		= 1,
	DUT_NV_MEMORY_FLASH			= 2,
	DUT_NV_MEMORY_EFUSE			= 3,
} dutNvMemoryType_e;

typedef enum dutChipVar
{
	DUT_CHIP_VAR_TPC_FREQ		= 31,

} dutChipVar_e;

typedef enum dutRiscMode
{
	DUT_RISC_STOP  = 0,
	DUT_RISC_START
} dutRiscMode_e;

//---------------------------------------------
//		Dut Message Definition									
//---------------------------------------------
typedef enum{
	// Dut Mngmnt Module Messages
	DUT_HW_DEPENDENT_CONFIG_REQ = 2,
	DUT_HW_DEPENDENT_CONFIG_CFM,
	DUT_CONFIGURE_REQ,
	DUT_CONFIGURE_CFM,
	DUT_SET_CAHNNEL_REQ,
	DUT_SET_CAHNNEL_CFM,
	DUT_READ_EEPROM_REQ,
	DUT_READ_EEPROM_CFM,
	DUT_WRITE_EEPROM_REQ,
	DUT_WRITE_EEPROM_CFM,
	DUT_READ_BIST_RESULTS_REQ,
	DUT_READ_BIST_RESULTS_CFM,
	DUT_READ_MEMORY_REQ,
	DUT_READ_MEMORY_CFM,
	DUT_WRITE_MEMORY_REQ,
	DUT_WRITE_MEMORY_CFM,
	DUT_WRITE_SECURED_REGISTER_REQ,
	DUT_WRITE_SECURED_REGISTER_CFM,
	DUT_READ_FREQ_TPC_STRUCTS_REQ,
	DUT_READ_FREQ_TPC_STRUCTS_CFM,
	DUT_RUN_CALIBRATION_REQ,		   
	DUT_RUN_CALIBRATION_CFM,           
	DUT_STOP_FW_REQ,
	DUT_STOP_FW_CFM,
	DUT_READ_CALIBRATION_RESULTS_REQ, //New - Optional
	DUT_READ_CALIBRATION_RESULTS_CFM, //New - Optional
	
	// Dut Hw Acces Module Messages
	DUT_READ_CHIP_VERSION_REQ = 0x100,
	DUT_READ_CHIP_VERSION_CFM,
	DUT_READ_GENRISC_VERSION_REQ ,	  		
	DUT_READ_GENRISC_VERSION_CFM,
	DUT_ENABLE_TX_ANTENNA_REQ,		
	DUT_ENABLE_TX_ANTENNA_CFM,
	DUT_ENABLE_RX_ANTENNA_REQ,
	DUT_ENABLE_RX_ANTENNA_CFM,
	DUT_SET_DEFAULT_ANTENNA_SET_REQ,	
	DUT_SET_DEFAULT_ANTENNA_SET_CFM,
	DUT_TX_TONE_REQ,					
	DUT_TX_TONE_CFM,
	DUT_TX_SPACELESS_REQ,
	DUT_TX_SPACELESS_CFM,
	DUT_SET_SCRAMBLER_MODE_REQ,
	DUT_SET_SCRAMBLER_MODE_CFM,
	DUT_SET_IFS_REQ,
	DUT_SET_IFS_CFM,
	DUT_SET_TPC_REQ,
	DUT_SET_TPC_CFM,
	DUT_READ_PACKET_COUNTERS_REQ,
	DUT_READ_PACKET_COUNTERS_CFM,
    DUT_SET_TX_POWER_LIMIT_REQ,
    DUT_SET_TX_POWER_LIMIT_CFM,
	DUT_SET_RISC_MODE_REQ,
	DUT_SET_RISC_MODE_CFM,
	//RFIC
	DUT_GET_TX_POWER_FEEDBACK_REQ,
	DUT_GET_TX_POWER_FEEDBACK_CFM,
	DUT_SET_TX_GAINS_REQ,
	DUT_SET_TX_GAINS_CFM,
	DUT_GET_TX_GAINS_REQ,
	DUT_GET_TX_GAINS_CFM,
	DUT_GET_RX_GAINS_REQ, 
	DUT_GET_RX_GAINS_CFM,
	DUT_GET_XTAL_VALUE_REQ,
	DUT_GET_XTAL_VALUE_CFM,
	DUT_SET_XTAL_VALUE_REQ,
	DUT_SET_XTAL_VALUE_CFM,
	DUT_GET_RSSI_REQ,
	DUT_GET_RSSI_CFM,
	DUT_GET_TEMEPERATUE_SENSOR_REQ,//New
	DUT_GET_TEMEPERATUE_SENSOR_CFM,//New
	DUT_SET_RFM_MODE_REQ,//New
	DUT_SET_RFM_MODE_CFM,//New
	DUT_SET_RF_LOOPBACK_REQ,//New
	DUT_SET_RF_LOOPBACK_CFM,//New
	DUT_SET_GIPO_OUTPUT_LEVEL_REQ,//New
	DUT_SET_GIPO_OUTPUT_LEVEL_CFM,//New
	DUT_CANCEL_TX_PHASE_REQ,
	DUT_CANCEL_TX_PHASE_CFM,
	DUT_SET_CDD_SET_REQ,
	DUT_SET_CDD_SET_CFM,
	DUT_SET_QBF_REQ,
	DUT_SET_QBF_CFM,
	DUT_GET_LNA_PARAMS_REQ,
	DUT_GET_LNA_PARAMS_CFM,
	DUT_SET_LNA_PARAMS_REQ,
	DUT_SET_LNA_PARAMS_CFM,
	
	// Dut Tx Module Messages
	DUT_START_TX_REQ	= 0x200,
	DUT_START_TX_CFM,
	DUT_STOP_TX_REQ,
	DUT_STOP_TX_CFM,
	DUT_UNKNOWN_MSG_CFM = 0x300,
} dutMessagesId_e;



typedef enum dutDriverMessagesIdEnum_t
{
	DUT_SERVER_MSG_RESET_MAC			= 0x01,
	DUT_SERVER_MSG_UPLOAD_PROGMODEL		= 0x02,
	DUT_SERVER_MSG_MAC_C100				= 0x03,
	DUT_SERVER_MSG_RESEND_NOT_IN_USE	= 0x04,
	DUT_SERVER_MSG_DRIVER_GENERAL		= 0x05,
 	DUT_SERVER_MSG_CNT 					= 0x06
} dutDriverMessagesId_e;

typedef enum
{
	// DGM - driver general message
	DUT_DGM_READ_NV_MEMORY_REQ = 0x2,
	DUT_DGM_READ_NV_MEMORY_CFM,
	DUT_DGM_WRITE_NV_MEMORY_REQ,
	DUT_DGM_WRITE_NV_MEMORY_CFM,
	DUT_DGM_FLUSH_NV_MEMORY_REQ,
	DUT_DGM_FLUSH_NV_MEMORY_CFM,
	DUT_DGM_GET_NV_FLASH_NAME_REQ,
	DUT_DGM_GET_NV_FLASH_NAME_CFM
} dutDriverGeneralMsgId_e;


typedef enum dutStatus
{
	DUT_STATUS_PASS		= 1,
	DUT_STATUS_FAIL     = 2,
} dutStatus ;

//---------------------------------------------------------------------------------
//						Data Type Definition									
//---------------------------------------------------------------------------------
typedef struct dutMessage
{
	uint16 msgLength;
	uint16 status;
	uint8 reserved[2];
	uint16 msgId;
	uint8 data[MTLK_PAD4(DUT_MSG_DATA_LENGTH)];
} __MTLK_PACKED dutMessage_t;

#define dutDriverMessageHeaderLength 8

typedef struct _C100_MSG_HEADER
{
	uint8   u8Task;
	uint8   u8Instance;
	uint16  u16MsgId;
} __MTLK_PACKED C100_MSG_HEADER;

typedef struct _UMI_C100
{
	uint16 u16Length;
	uint16 u16stream;
	C100_MSG_HEADER sC100hdr;
	uint8  au8Data[MTLK_PAD4(UMI_C100_DATA_SIZE)];
} __MTLK_PACKED UMI_C100;

// To Align with mhi_umi convention
typedef  dutMessage_t UMI_DUT;
//----- Hw Dependent Params----------//
typedef struct dutHwDependentConfParams
{
	uint32 xtalValue;
	int8   extLNAbypass;
	int8   extLNAhigh;
} dutHwDependentConfParams_t;

//----- FW Configure Params----------//
typedef struct dutConfigureParams
{
	EEPROM_VERSION_TYPE sEepromInfo;
	uint32		txLifeTime;
	uint32		tpcTimerValue;
	uint16		erpRates11g;
	uint16		regDomain;
	uint16		calibrationAlgoMask;
	IEEE_ADDR	sIEEE_ADDR;
	uint8		phyType;	
	uint8		numOfTxChains;
	uint8		numOfRxChains;
	uint8		spectrumMode; //20M, Half Band, FULL.
	uint8		secondaryChannel;
	uint8		band;
	uint8		rfOscilFreq;
	uint8		afeClock;
	uint8		txPower;
	int8		forceTpc[TPC_ARRAY_SIZE];
	uint8		tpcModeIsInClosedLoop; //  1 - Close loop\ 0 - Open Loop
	uint8       tpcDebugMode;
	uint8		tpcDebugTxPower;
	int8        tpcPowerBoost;
	
} dutConfigureParams_t;

//----- FW SetChannel Params----------//
typedef struct dutSetChannelParams
{
	IEEE_ADDR	sIEEE_ADDR;
	EEPROM_VERSION_TYPE sEepromInfo;
	TPC_FREQ	TPC_Freqs[TPC_ARRAY_SIZE][TPC_FREQ_STR_NUM]; // for each ant, 2 CB and 2 nCB structures
	uint16		channel;
	uint16		calibrationAlgoMask;
	uint8		tpcModeIsInClosedLoop; //  1 - Close loop\ 0 - Open Loop
	uint8		txPower;
	uint8       tpcDebugMode;
	uint8		secondaryChannel;
} dutSetChannelParams_t;

//----- Eeprom Params----------//


typedef struct dutEepromParams
{
	uint32 address;
	uint32 eepromSize;
	uint32 length;
	uint8  data[MTLK_PAD4(MAX_DUT_EEPROM_ACCESS_SIZE)];
} dutEepromParams_t;

//----- NV memory Acces Params (Dut Driver Structure)----------//
typedef struct dutNvMemoryAccessParams
{
	uint32 nvMemoryType;
	uint32 address;
	uint32 eepromSize;
	uint32 length;
	uint8  data[MTLK_PAD4(MAX_DUT_EEPROM_ACCESS_SIZE)];
} __MTLK_PACKED dutNvMemoryAccessParams_t;


typedef struct dutNvMemoryFlushParams
{
	uint32 nvMemoryType;
	uint8 verifyNvmData;
    int8 verifyResult;
} __MTLK_PACKED dutNvMemoryFlushParams_t;

typedef struct dutResetParams
{
  uint32 nvMemoryType;
  uint32 eepromSize;
  uint8  doReset; // 0 - Enable DUT mode on the fly, 1 - EnableDUT mode via acripts (using rmmode/insmode dut=1)
  uint8  reserved[3];
} __MTLK_PACKED dutResetParams_t;



//----- Memory access Params DUT_READ_MEMORY_REQ\CFM----------//
//----- Memory access Params DUT_WRITE_MEMORY_REQ\CFM----------//
//----- Memory access Params DUT_WRITE_SECURED_REGISTER_REQ\CFM----------//
#define MAX_DUT_MEMORY_ACCESS_SIZE (DUT_MSG_DATA_LENGTH-8)

typedef struct dutMemoryAccessParams
{
	uint16 module;
	uint16 length;
	uint32 address;
	uint8  data[MAX_DUT_MEMORY_ACCESS_SIZE];
} dutMemoryAccessParams_t;

typedef struct dutSecuredWriteParams
{
	/* modify secured register: */
	/* (*address) = (*address) & ~mask | data */
	uint32	address;
	uint32	mask;
	uint32	data;
} dutSecuredWriteParams_t;

//----- Frequency TPC Params DUT_READ_FREQ_TPC_STRUCTS_REQ\CFM----------//,
typedef struct dutFreqTpcParams
{
	TPC_FREQ tpcFreq[DUT_ANT_VALUES_ARRAY_SIZE*TPC_FREQ_STR_NUM];
} dutFreqTpcParams_t;

//----- Frequency TPC Params DUT_RUN_CALIBRATION_REQ\CFM----------//,
typedef struct dutCalibrationParams
{
	uint16 channel;
	uint16 calibrationMask;
	uint8		tpcModeIsInClosedLoop; //  1 - Close loop\ 0 - Open Loop
} dutCalibrationParams_t;

//----- Frequency TPC Params DUT_STOP_FW_REQ\CFM----------//,
typedef struct dutStopFwParams
{
	uint32 dummy;
} dutStopFwParams_t;


//----- Bist Results----------//
typedef struct dutBistResults
{
	int8 bistResults[BIST_RESULTS_ARRAY_SIZE];
} dutBistResults_t;

//----- Chip Version----------//
typedef struct dutChipVersion
{
	uint32 bbChip;
	uint32 rfChipVersion;
} dutChipVersion_t;

//--------------------------------------//
//	Tx Data Types						//
//--------------------------------------//
typedef struct dutTxParams
{
	IEEE_ADDR	addr1;
	IEEE_ADDR	addr2;
	IEEE_ADDR	addr3;
	IEEE_ADDR	addr4;
	uint16		repeats;
	uint16		packetLength;// 0xffff - Random size,other - data size.
	uint16		packetType;// 0xffff - Random Type,0x00XX - data fixed 0xXX.
	uint16		seqControl;
	uint16		qosCtrlField;
	uint16		frameCtrlField;
	uint16		txRate;
	uint8		packetSubType;
	uint8		priority;
	uint8		txExtendedRate;
	uint8		txPower;
	uint8		isTxEndless;
} dutTxParams_t;

//--------------------------------------//
//	Hw Configuration Data Types			//
//--------------------------------------//
//-----  Read Hw Params DUT_READ_HW_VERSION_REQ\CFM----------//
typedef struct dutHwParams
{
	uint32	revision;
	uint32	minor;
	uint8	branchId;
	uint8	released;
	uint8	modified;
	uint8	Tx3Enabled;
} dutHwParams_t;

//-----  Set Tpc Table Params (DUT_SET_TPC_REQ\CFM)----------//
typedef struct dutTpcParams
{
	uint8	powerTableIndex;							// The location in the TPC table
	uint8	powerGainIndex[DUT_ANT_VALUES_ARRAY_SIZE];  // requested TPC Index
} dutTpcParams_t;

//-----  Rfic Rssi Params DUT_GET_RSSI_REQ\CFM----------//
typedef struct dutRssiParams
{
	uint8 rssi[DUT_ANT_VALUES_ARRAY_SIZE];
	uint8 N;
} dutRssiParams_t;

//-----  Tpc Feedback params Params(DUT_GET_TPC_FEEDBACK_REQ\CFM) ----------//
typedef struct dutTpcFeedbackparams
{
	uint32	numOfSamples;
	uint32	actualNumOfSamples;
	uint32	valueSum[DUT_ANT_VALUES_ARRAY_SIZE];
} dutTpcFeedbackparams_t;
//-----  Tx Tone Params (DUT_SET_TX_TONE_REQ\CFM) ----------//
typedef struct dutTxToneParams
{
	uint8 onOff;
	uint8 amplitude;
	int8 power;
	uint8 binNum;
} dutTxToneParams_t;

//-----  Tx Spaceless Params (DUT_SET_SPACELESS_TX_REQ\CFM) ----------//
typedef struct dutTxSpacelessParams
{
	uint8 dummy;
} dutTxSpacelessParams_t;

//-----  Scrambler Mode Params (DUT_TX_SPACELESS_REQ\CFM) ----------//
typedef struct dutScramblerParams
{
	uint8 mode;
} dutScramblerParams_t;

//-----  Set IFS Params (DUT_SET_IFS_REQ\CFM) ----------//
typedef struct dutIfsParams
{
	uint32 ifsValue;
} dutIfsParams_t;

//-----  Counters Params DUT_READ_PACKET_COUNTERS_REQ\CFM----------//
typedef struct packetCountersParams
{
	uint32 phyRxPacketCounter;
	uint32 phyRxCrcErrorCounter;
	uint32 fwPacketCounter;
	uint8  doReset;
} packetCountersParams_t;

//-----  Set Tx Power Limit  Params (DUT_SET_TX_POWER_LIMIT_REQ\CFM)----------//
typedef struct powerLimitParams
{
    uint8 powerLimit;
} powerLimitParams_t;

//-----  Counters Params DUT_SET_RISC_MODE_REQ\CFM----------//
typedef struct riscModeParams
{
	dutRiscMode_e riscMode; // 0- Off 1 - On
} riscModeParams_t;

//-----  Set Antenna Params Params DUT_ENABLE_TX_ANTENNA_REQ\CFM----------//
//-----  Set Antenna Params Params DUT_ENABLE_RX_ANTENNA_REQ\CFM----------//
typedef struct dutSetAntennaParams
{
	uint8 enabledAntMask; // bit 0-2: Ants 0-2 ON (3 - ants 0,1 ON, 7 - ants 0,1,2 ON, 0 - all off)
} dutSetAntennaParams_t;

//-----  Set Antenna Params Params (DUT_SET_DEFAULT_ANTENNA_SET_REQ\CFM) ----------//
typedef struct antSelectionParams
{
	uint8	antSet;
} antSelectionParams_t;
//-----  Rfic Gains Params DUT_SET_TX_GAINS_REQ\CFM ----------//
//-----  Rfic Gains Params DUT_GET_TX_GAINS_REQ\CFM ----------//
typedef struct dutAntTxGains
{
	uint8	bbGain;
	uint8	paDriverGain;		// 3bit 0-High 1- Medium 2-Low
} dutAntTxGains_t;

typedef struct dutRficTxGainsParams
{
	dutAntTxGains_t antTxGains[DUT_ANT_VALUES_ARRAY_SIZE]; 
} dutRficTxGainsParams_t;

//-----  Rfic Gains Params DUT_GET_RX_GAINS_REQ\CFM ----------//
typedef struct dutAntRxGains
{
	int16	bbGain;
	int8	lnaGain;		// 3bit 0-High 1- Medium 2-Low
	uint8	reserved;
} dutAntRxGains_t;

typedef struct dutRficRxGainsParams
{
	dutAntRxGains_t antRxGains[DUT_ANT_VALUES_ARRAY_SIZE]; 
} dutRficRxGainsParams_t;

//-----  Xtal Params DUT_GET_XTAL_VALUE_REQ\CFM----------//
typedef struct dutXtalParams
{
	uint32 xtalValue;
} dutXtalParams_t;

typedef struct dutLnaParams
{
	int8	extLNAbypass;
	int8	extLNAhigh;
} dutLnaParam_t;

typedef struct dutTxPhaseParams
{
	uint32 TxPhaseValue;
} dutTxPhaseParams_t;

typedef struct dutCddParams
{
	uint32 CDDvalue;
} dutCddParams_t;

typedef struct dutQbfParams
{
	uint32 QBFvalue;
} dutQbfParams_t;



#define MTLK_PACK_OFF
#include "mtlkpack.h"

#endif
















