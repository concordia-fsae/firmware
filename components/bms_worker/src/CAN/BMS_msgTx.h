/*
 * BMS_msgTX.h
 * contains all the manual written message
 * will eventually be replaced by auto-generated ones
 */

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

// pack_VEH_BMS_criticalData_10ms

static inline void set_BMSVoltageMin(CAN_data_T* message, uint16_t value)
{
    message->u64 |= ((uint64_t)((value) & 0x3FF)) << 53;  // 10 bits, 5mV precision, range [0-5115]mV
}

static inline void set_BMSVoltageMax(CAN_data_T* message, uint16_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)(value) & 0x3FF)) << 43;  // 10 bits, 5mV precision, range [0-5115]mV
}

static inline void set_ENVPackVoltage(CAN_data_T* message, uint8_t value)
{
    message->u64 |= ((uint64_t)((uint8_t)value & 0x7F)) << 36;  // 7 bits, 1 deg C precision
}

static inline void set_ENVTempMax(CAN_data_T* message, uint8_t value)
{
    message->u64 |= ((uint64_t)((uint8_t)value & 0x7F)) << 29;  // 7 bits, 1 deg C precision
}

static inline void set_BMSChargeLimit(CAN_data_T* message, uint8_t value)
{
    message->u64 |= ((uint64_t)((uint8_t)value & 0x1F)) << 24;  // 5 bits, 1A precision
}

static inline void set_BMSDischargeLimit(CAN_data_T* message, uint8_t value)
{
    message->u64 |= ((uint64_t)((uint8_t)value & 0xFF)) << 16;  // 8 bits, 1A precision
}

static inline void set_BMSErrorFlag(CAN_data_T* message, bool value)
{
    message->u64 |= ((uint64_t)((value) ? 0x01 : 0U)) << 11;    // 1 bit
}

static inline void set_BMSFaultFlag(CAN_data_T* message, bool value)
{
    message->u64 |= ((uint64_t)((value ? 0x01 : 0U))) << 10;    // 1 bit
}

static inline void set_ENVErrorFlag(CAN_data_T* message, bool value)
{
    message->u64 |= ((uint64_t)((value ? 0x01 : 0U))) << 9;    // 1 bit
}

static inline void set_ENVFaultFlag(CAN_data_T* message, bool value)
{
    message->u64 |= ((uint64_t)((value) ? 0x01 : 0U)) << 8;    // 1 bit
}

static inline void set_Counter(CAN_data_T* message, uint8_t value)
{
    message->u64 |= value;   // 8 bits
}

// pack_VEH_BMS_averagesSOCcellTemps_1s

static inline void set_BMSVoltageAvg(CAN_data_T* message, float32_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)(value * 200) & 0x3FF)) << 54;// 10 bits, 5mv precisiion
}

static inline void set_ENVAvgTemp(CAN_data_T* message, float32_t value)
{
    message->u64 |= ((uint64_t)((uint8_t)value & 0x7F)) << 47;          // 7 bits, 1 deg C precission
}

static inline void set_BMSSOCMin(CAN_data_T* message, float32_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)(value * 10) & 0x3FF)) << 37; // 10 bits, 0.1% precision, range [0-102.3]%
}

static inline void set_BMSSOCAAvg(CAN_data_T* message, float32_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)(value * 10) & 0x3FF)) << 27; // 10 bits, 0.1% precision, range [0-102.3]%
}

static inline void set_BMSSOCMax(CAN_data_T* message, float32_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)(value * 10) & 0x3FF)) << 17; // 10 bits, 0.1% precision, range [0-102.3]%
}

static inline void set_CellTemp0(CAN_data_T* message, int16_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)value & 0x7F)) << 10;         // 7 bits, 1 deg C precision, range [0-127]deg C
}

static inline void set_CellTemp1(CAN_data_T* message, int16_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)value & 0x7F)) << 3;          // 7 bits, 1 deg C precision, range [0-127]deg C
}

// pack_VEH_BMS_cellTemp2To10_1s

static inline void set_CellTemp2(CAN_data_T* message, int16_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)value & 0x7F)) << 57; // 7 bits, 1 deg C precision, range [0-127]deg C
}

static inline void set_CellTemp3(CAN_data_T* message, int16_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)value & 0x7F)) << 50; // 7 bits, 1 deg C precision, range [0-127]deg C
}

static inline void set_CellTemp4(CAN_data_T* message, int16_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)value & 0x7F)) << 43;  // 7 bits, 1 deg C precision, range [0-127]deg C
}

static inline void set_CellTemp5(CAN_data_T* message, int16_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)value & 0x7F)) << 36; // 7 bits, 1 deg C precision, range [0-127]deg C
}

static inline void set_CellTemp6(CAN_data_T* message, int16_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)value & 0x7F)) << 29; // 7 bits, 1 deg C precision, range [0-127]deg C
}

static inline void set_CellTemp7(CAN_data_T* message, int16_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)value & 0x7F)) << 22; // 7 bits, 1 deg C precision, range [0-127]deg C
}

static inline void set_CellTemp8(CAN_data_T* message, int16_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)value & 0x7F)) << 15; // 7 bits, 1 deg C precision, range [0-127]deg C
}

static inline void set_CellTemp9(CAN_data_T* message, int16_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)value & 0x7F)) << 8;  // 7 bits, 1 deg C precision, range [0-127]deg C
}

static inline void set_CellTemp10(CAN_data_T* message, int16_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)value & 0x7F)) << 1;  // 7 bits, 1 deg C precision, range [0-127]deg C
}

// pack_VEH_BMS_cellTemp11To19_1s

static inline void set_CellTemp11(CAN_data_T* message, int16_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)value & 0x7F)) << 57; // 7 bits, 1 deg C precision, range [0-127]deg C
}

static inline void set_CellTemp12(CAN_data_T* message, int16_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)value & 0x7F)) << 50; // 7 bits, 1 deg C precision, range [0-127]deg C
}

static inline void set_CellTemp13(CAN_data_T* message, int16_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)value & 0x7F)) << 43; // 7 bits, 1 deg C precision, range [0-127]deg C
}

static inline void set_CellTemp14(CAN_data_T* message, int16_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)value & 0x7F)) << 36; // 7 bits, 1 deg C precision, range [0-127]deg C
}

static inline void set_CellTemp15(CAN_data_T* message, int16_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)value & 0x7F)) << 29; // 7 bits, 1 deg C precision, range [0-127]deg C
}

static inline void set_CellTemp16(CAN_data_T* message, int16_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)value & 0x7F)) << 22; // 7 bits, 1 deg C precision, range [0-127]deg C
}

static inline void set_CellTemp17(CAN_data_T* message, int16_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)value & 0x7F)) << 15; // 7 bits, 1 deg C precision, range [0-127]deg C
}

static inline void set_CellTemp18(CAN_data_T* message, int16_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)value & 0x7F)) << 8;  // 7 bits, 1 deg C precision, range [0-127]deg C
}

static inline void set_CellTemp19(CAN_data_T* message, int16_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)value & 0x7F)) << 1;  // 7 bits, 1 deg C precision, range [0-127]deg C
}

// pack_VEH_BMS_cellVoltage0To5_1s

static inline void set_CellVoltage0(CAN_data_T* message, float32_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)(value * 200) & 0x3FF)) << 54;   // 10 bits, 5mV precision, range [0-5120]mV
}

static inline void set_CellVoltage1(CAN_data_T* message, float32_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)(value * 200) & 0x3FF)) << 44;   // 10 bits, 5mV precision, range [0-5120]mV
}

static inline void set_CellVoltage2(CAN_data_T* message, float32_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)(value * 200) & 0x3FF)) << 34;   // 10 bits, 5mV precision, range [0-5120]mV
}

static inline void set_CellVoltage3(CAN_data_T* message, float32_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)(value * 200) & 0x3FF)) << 24;   // 10 bits, 5mV precision, range [0-5120]mV
}

static inline void set_CellVoltage4(CAN_data_T* message, float32_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)(value * 200) & 0x3FF)) << 14;   // 10 bits, 5mV precision, range [0-5120]mV
}

static inline void set_CellVoltage5(CAN_data_T* message, float32_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)(value * 200) & 0x3FF)) << 4;    // 10 bits, 5mV precision, range [0-5120]mV
}

// pack_VEH_BMS_cellVoltage6To11_1s

static inline void set_CellVoltage6(CAN_data_T* message, float32_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)(value * 200) & 0x3FF)) << 54;   // 10 bits, 5mV precision, range [0-5120]mV
}

static inline void set_CellVoltage7(CAN_data_T* message, float32_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)(value * 200) & 0x3FF)) << 44;   // 10 bits, 5mV precision, range [0-5120]mV
}

static inline void set_CellVoltage8(CAN_data_T* message, float32_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)(value * 200) & 0x3FF)) << 34;   // 10 bits, 5mV precision, range [0-5120]mV
}

static inline void set_CellVoltage9(CAN_data_T* message, float32_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)(value * 200) & 0x3FF)) << 24;   // 10 bits, 5mV precision, range [0-5120]mV
}

static inline void set_CellVoltage10(CAN_data_T* message, float32_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)(value * 200) & 0x3FF)) << 14;   // 10 bits, 5mV precision, range [0-5120]mV
}

static inline void set_CellVoltage11(CAN_data_T* message, float32_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)(value * 200) & 0x3FF)) << 4;    // 10 bits, 5mV precision, range [0-5120]mV
}

// pack_VEH_BMS_cellVoltage12To15_1s

static inline void set_CellVoltage12(CAN_data_T* message, float32_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)(value * 200) & 0x3FF)) << 54;    // 10 bits, 5mV precision, range [0-5120]mV
}

static inline void set_CellVoltage13(CAN_data_T* message, float32_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)(value * 200) & 0x3FF)) << 44;    // 10 bits, 5mV precision, range [0-5120]mV
}

static inline void set_CellVoltage14(CAN_data_T* message, float32_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)(value * 200) & 0x3FF)) << 34;    // 10 bits, 5mV precision, range [0-5120]mV
}

static inline void set_CellVoltage15(CAN_data_T* message, float32_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)(value * 200) & 0x3FF)) << 24;    // 10 bits, 5mV precision, range [0-5120]mV
}

static inline void set_PackVoltage(CAN_data_T* message, float32_t value)
{
    message->u64 |= ((uint64_t)((uint16_t)(value / 10) & 0x1FFF)) << 11;    // 13 bits, 10mV precision, range [0-81910]mV
}

// pack_VEH_BMS_tempHumidity_1s

static inline void set_BoardTemp0(CAN_data_T* message, float32_t value)
{
    message->u64 |= ((uint64_t)((uint8_t)(value) & 0x7F)) << 28;   // 7 bits, 1 deg C precision, range [0-128]deg C
}

static inline void set_BoardTemp1(CAN_data_T* message, float32_t value)
{
    message->u64 |= ((uint64_t)((uint8_t)(value) & 0x7F)) << 21;   // 7 bits, 1 deg C precision, range [0-128]deg C
}

static inline void set_MCUTemp(CAN_data_T* message, float32_t value)
{
    message->u64 |= ((uint64_t)((uint8_t)(value) & 0x7F)) << 14;   // 7 bits, 1 deg C precision, range [0-128]deg C
}

static inline void set_BoardAmbientTemp(CAN_data_T* message, float32_t value)
{
    message->u64 |= ((uint64_t)((uint8_t)(value) & 0x7F)) << 7;   // 7 bits, 1 deg C precision, range [0-128]deg C
}

static inline void set_BoardRelativeHumidity(CAN_data_T* message, float32_t value)
{
    message->u64 |= ((uint64_t)((uint8_t)(value) & 0x07F)); // 7 bits, 1% precision, range [0-128]%
}

// pack_VEH_BMS_fans_1s

static inline void set_CoolState0(CAN_data_T* message, bool value)
{
    message->u64 |= ((uint64_t)((value ? 0x01 : 0U))) << 63;
}

static inline void set_CoolPercentage0(CAN_data_T* message, uint8_t value)
{
    message->u64 |= ((uint64_t)(value & 0x7F)) << 56;   // 7 bits, 1% precision, range [0-128]%
}

static inline void set_CoolState1(CAN_data_T* message, bool value)
{
    message->u64 |= ((uint64_t)((value ? 0x01 : 0U))) << 55;
}

static inline void set_CoolPercentage1(CAN_data_T* message, uint8_t value)
{
    message->u64 |= ((uint64_t)(value & 0x7F)) << 48;   // 7 bits, 1% precision, range [0-128]%
}

static inline void set_FanRPM0(CAN_data_T* message, uint16_t value)
{
    message->u64 |= ((uint64_t)(value)) << 32;          // 7 bits, 1% precision, range [0-128]%
}

static inline void set_FanRPM1(CAN_data_T* message, uint16_t value)
{
    message->u64 |= ((uint64_t)(value)) << 16;          // 7 bits, 1% precision, range [0-128]%
}
