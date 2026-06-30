#ifndef __BOOT_MATRIX_H__
#define __BOOT_MATRIX_H__

#include <stdint.h>

#define BOOT_ADC_LEVEL_LOW      (0U)
#define BOOT_ADC_LEVEL_HIGH     (1U)
#define BOOT_ADC_LEVEL_MID      (2U)

#define BOOT_SMBUS_ROLE_VPP     (0U)
#define BOOT_SMBUS_ROLE_UBM     (1U)

#define BOOT_SGPIO_GROUP_OD0_3  (0U)
#define BOOT_SGPIO_GROUP_OD4_7  (1U)

typedef struct
{
    uint16_t raw;
    uint16_t mv;
    uint8_t level;
} BOOT_ADC_SAMPLE_T;

typedef struct
{
    BOOT_ADC_SAMPLE_T addr_sel;
    BOOT_ADC_SAMPLE_T smbus_role;
    BOOT_ADC_SAMPLE_T user_ch13;
    BOOT_ADC_SAMPLE_T sgpio_group;
    BOOT_ADC_SAMPLE_T user_ch15;
    uint16_t adc_vdd_mv;
    uint16_t adc_bandgap_raw;
    uint16_t adc_bandgap_trim;
    uint8_t i2c1_addr7;
    uint8_t usci0_addr7;
    uint8_t smbus_role_id;
    uint8_t user_ch13_on;
    uint8_t sgpio_group_id;
    uint8_t user_ch15_on;
} BOOT_MATRIX_T;

extern BOOT_MATRIX_T g_boot_matrix;

void BootMatrix_Init(void);
void BootMatrix_Print(void);
void BootMatrix_ApplySmbusConfig(void);

#endif
