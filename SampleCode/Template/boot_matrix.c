/*_____ I N C L U D E S ____________________________________________________*/
#include <stdio.h>
#include <string.h>
#include "NuMicro.h"

#include "boot_matrix.h"
#include "smbus_slave.h"

/*_____ D E F I N I T I O N S ______________________________________________*/
#define BOOT_ADC_MAX_CODE                       (4095UL)
#define BOOT_ADC_LOW_THRESHOLD_NUMERATOR       (3UL)
#define BOOT_ADC_HIGH_THRESHOLD_NUMERATOR      (7UL)
#define BOOT_ADC_THRESHOLD_DENOMINATOR         (10UL)
#define BOOT_ADC_CONVERSION_TIMEOUT            (100000UL)
#define BOOT_ADC_FMC_TIMEOUT                   (100000UL)
#define BOOT_ADC_SAMPLE_TIME                   (0x3FUL)
#define BOOT_ADC_BANDGAP_MV                    (1200UL)
#define BOOT_ADC_BANDGAP_CH                    (29U)
#define BOOT_ADC_BANDGAP_TRIM_ADDR             (0x0010UL)
#define BOOT_ADC_BANDGAP_SAMPLE_COUNT          (8U)
#define BOOT_ADC_ADDR_SEL_CH                   (11U)
#define BOOT_ADC_SMBUS_ROLE_CH                 (12U)
#define BOOT_ADC_USER_CH13                     (13U)
#define BOOT_ADC_SGPIO_GROUP_CH                (14U)
#define BOOT_ADC_USER_CH15                     (15U)

#define BOOT_ADC_STRAP_PIN_MASK                (BIT11 | BIT12 | BIT13 | BIT14 | BIT15)
#define BOOT_ADC_STRAP_MFPH_MASK               (SYS_GPB_MFPH_PB11MFP_Msk | \
                                                SYS_GPB_MFPH_PB12MFP_Msk | \
                                                SYS_GPB_MFPH_PB13MFP_Msk | \
                                                SYS_GPB_MFPH_PB14MFP_Msk | \
                                                SYS_GPB_MFPH_PB15MFP_Msk)
#define BOOT_ADC_STRAP_MFPH_ADC                (SYS_GPB_MFPH_PB11MFP_ADC0_CH11 | \
                                                SYS_GPB_MFPH_PB12MFP_ADC0_CH12 | \
                                                SYS_GPB_MFPH_PB13MFP_ADC0_CH13 | \
                                                SYS_GPB_MFPH_PB14MFP_ADC0_CH14 | \
                                                SYS_GPB_MFPH_PB15MFP_ADC0_CH15)
#define BOOT_ADC_GPIO_RESTORE_MFPH_MASK        BOOT_ADC_STRAP_MFPH_MASK
#define BOOT_ADC_GPIO_RESTORE_MFPH_GPIO        (SYS_GPB_MFPH_PB11MFP_GPIO | \
                                                SYS_GPB_MFPH_PB12MFP_GPIO | \
                                                SYS_GPB_MFPH_PB13MFP_GPIO | \
                                                SYS_GPB_MFPH_PB14MFP_GPIO | \
                                                SYS_GPB_MFPH_PB15MFP_GPIO)

/*_____ D E C L A R A T I O N S ____________________________________________*/
BOOT_MATRIX_T g_boot_matrix;

/*_____ F U N C T I O N S __________________________________________________*/
static uint8_t BootAdc_Classify(uint16_t raw)
{
    uint32_t scaled_raw;
    uint8_t level;

    scaled_raw = (uint32_t)raw * BOOT_ADC_THRESHOLD_DENOMINATOR;

    /*
     * Use MCU I/O DC characteristics for strap classification:
     * V_IL = 0 to 0.3 * VDD, V_IH = 0.7 * VDD to VDD.
     */
    if (scaled_raw <= (BOOT_ADC_MAX_CODE * BOOT_ADC_LOW_THRESHOLD_NUMERATOR))
    {
        level = BOOT_ADC_LEVEL_LOW;
    }
    else if (scaled_raw >= (BOOT_ADC_MAX_CODE * BOOT_ADC_HIGH_THRESHOLD_NUMERATOR))
    {
        level = BOOT_ADC_LEVEL_HIGH;
    }
    else
    {
        level = BOOT_ADC_LEVEL_MID;
    }

    return level;
}

static const char *BootAdc_LevelName(uint8_t level)
{
    const char *name;

    if (level == BOOT_ADC_LEVEL_LOW)
    {
        name = "LOW";
    }
    else if (level == BOOT_ADC_LEVEL_HIGH)
    {
        name = "HIGH";
    }
    else
    {
        name = "MID";
    }

    return name;
}

static const char *BootMatrix_SmbusRoleName(uint8_t role)
{
    const char *name;

    if (role == BOOT_SMBUS_ROLE_UBM)
    {
        name = "UBM";
    }
    else
    {
        name = "VPP";
    }

    return name;
}

static const char *BootMatrix_SgpioGroupName(uint8_t group)
{
    const char *name;

    if (group == BOOT_SGPIO_GROUP_OD4_7)
    {
        name = "SGPIO OD4-OD7";
    }
    else
    {
        name = "SGPIO OD0-OD3";
    }

    return name;
}

static uint16_t BootAdc_RatioToMv(uint16_t vdd_mv, uint32_t numerator)
{
    uint32_t mv;

    if (vdd_mv == 0U)
    {
        return 0U;
    }

    /* Add half denominator before division to round the threshold ratio to the nearest millivolt. */
    mv = ((uint32_t)vdd_mv * numerator) + (BOOT_ADC_THRESHOLD_DENOMINATOR / 2UL);
    mv = mv / BOOT_ADC_THRESHOLD_DENOMINATOR;

    return (uint16_t)mv;
}

static uint16_t BootAdc_RawToMv(uint16_t raw, uint16_t vdd_mv)
{
    uint32_t mv;

    if (vdd_mv == 0U)
    {
        return 0U;
    }

    /* Add half-scale before division to round the ADC ratio to the nearest millivolt. */
    mv = ((uint32_t)raw * (uint32_t)vdd_mv) + (BOOT_ADC_MAX_CODE / 2UL);
    mv = mv / BOOT_ADC_MAX_CODE;

    return (uint16_t)mv;
}

static void BootAdc_ConfigPins(void)
{
    SYS_UnlockReg();
    SYS->GPB_MFPH = (SYS->GPB_MFPH & ~BOOT_ADC_STRAP_MFPH_MASK) |
                    BOOT_ADC_STRAP_MFPH_ADC;
    SYS_LockReg();

    GPIO_SetMode(PB, BOOT_ADC_STRAP_PIN_MASK, GPIO_MODE_INPUT);
    GPIO_DISABLE_DIGITAL_PATH(PB, BOOT_ADC_STRAP_PIN_MASK);
}

static void BootAdc_RestorePins(void)
{
    SYS_UnlockReg();
    SYS->GPB_MFPH = (SYS->GPB_MFPH & ~BOOT_ADC_GPIO_RESTORE_MFPH_MASK) |
                    BOOT_ADC_GPIO_RESTORE_MFPH_GPIO;
    SYS_LockReg();

    GPIO_ENABLE_DIGITAL_PATH(PB, BOOT_ADC_STRAP_PIN_MASK);
    GPIO_SetMode(PB, BOOT_ADC_STRAP_PIN_MASK, GPIO_MODE_INPUT);
}

static uint16_t BootAdc_ReadChannelRaw(uint8_t channel)
{
    uint32_t adc_mode;
    uint32_t channel_mask;
    uint32_t timeout;
    uint16_t raw;

    if (channel == BOOT_ADC_BANDGAP_CH)
    {
        adc_mode = ADC_ADCR_ADMD_SINGLE;
    }
    else
    {
        adc_mode = ADC_ADCR_ADMD_SINGLE_CYCLE;
    }

    channel_mask = (1UL << channel);
    timeout = BOOT_ADC_CONVERSION_TIMEOUT;
    raw = 0U;

    ADC_Open(ADC, ADC_ADCR_DIFFEN_SINGLE_END, adc_mode, channel_mask);
    ADC_SetExtendSampleTime(ADC, 0U, BOOT_ADC_SAMPLE_TIME);
    ADC_CLR_INT_FLAG(ADC, ADC_ADF_INT);
    ADC_START_CONV(ADC);

    while ((ADC_GET_INT_FLAG(ADC, ADC_ADF_INT) == 0U) && (timeout > 0UL))
    {
        timeout--;
    }

    if (timeout > 0UL)
    {
        raw = (uint16_t)ADC_GET_CONVERSION_DATA(ADC, channel);
    }

    ADC_STOP_CONV(ADC);
    ADC_CLR_INT_FLAG(ADC, ADC_ADF_INT);

    return raw;
}

static uint16_t BootAdc_ReadBandGapTrim(void)
{
    uint32_t data;
    uint32_t isp_was_enabled;
    uint32_t timeout;

    data = 0UL;
    isp_was_enabled = 0UL;
    timeout = BOOT_ADC_FMC_TIMEOUT;

    SYS_UnlockReg();

    if ((FMC->ISPCTL & FMC_ISPCTL_ISPEN_Msk) != 0UL)
    {
        isp_was_enabled = 1UL;
    }

    FMC_ENABLE_ISP();
    FMC_CLR_FAIL_FLAG();

    FMC->ISPCMD = FMC_ISPCMD_READ_UID;
    FMC->ISPADDR = BOOT_ADC_BANDGAP_TRIM_ADDR;
    FMC->ISPDAT = 0UL;
    FMC->ISPTRG = FMC_ISPTRG_ISPGO_Msk;

#if ISBEN
    __ISB();
#endif

    while (((FMC->ISPTRG & FMC_ISPTRG_ISPGO_Msk) != 0UL) && (timeout > 0UL))
    {
        timeout--;
    }

    if ((timeout > 0UL) && ((FMC->ISPCTL & FMC_ISPCTL_ISPFF_Msk) == 0UL))
    {
        data = FMC->ISPDAT & BOOT_ADC_MAX_CODE;
    }

    if ((FMC->ISPCTL & FMC_ISPCTL_ISPFF_Msk) != 0UL)
    {
        FMC_CLR_FAIL_FLAG();
    }

    if (isp_was_enabled == 0UL)
    {
        FMC_DISABLE_ISP();
    }

    SYS_LockReg();

    return (uint16_t)data;
}

static uint16_t BootAdc_ReadBandGapRaw(void)
{
    uint32_t sum;
    uint8_t count;

    sum = 0UL;

    for (count = 0U; count < BOOT_ADC_BANDGAP_SAMPLE_COUNT; count++)
    {
        CLK_SysTickDelay(100);
        sum += BootAdc_ReadChannelRaw(BOOT_ADC_BANDGAP_CH);
    }

    sum = (sum + (BOOT_ADC_BANDGAP_SAMPLE_COUNT / 2U)) / BOOT_ADC_BANDGAP_SAMPLE_COUNT;

    return (uint16_t)sum;
}

static uint16_t BootAdc_ReadVddMv(uint16_t *bandgap_raw, uint16_t *bandgap_trim)
{
    uint32_t vdd_mv;

    *bandgap_trim = BootAdc_ReadBandGapTrim();
    *bandgap_raw = BootAdc_ReadBandGapRaw();

    if ((*bandgap_trim == 0U) || (*bandgap_raw == 0U))
    {
        return 0U;
    }

    /* Add half denominator before division to round VDD from bandgap trim/raw ADC code. */
    vdd_mv = (BOOT_ADC_BANDGAP_MV * (uint32_t)(*bandgap_trim)) +
             ((uint32_t)(*bandgap_raw) / 2UL);
    vdd_mv = vdd_mv / (uint32_t)(*bandgap_raw);

    if (vdd_mv > 0xFFFFUL)
    {
        vdd_mv = 0xFFFFUL;
    }

    return (uint16_t)vdd_mv;
}

static void BootAdc_ReadSample(uint8_t channel, BOOT_ADC_SAMPLE_T *sample)
{
    sample->raw = BootAdc_ReadChannelRaw(channel);
    sample->mv = BootAdc_RawToMv(sample->raw, g_boot_matrix.adc_vdd_mv);
    sample->level = BootAdc_Classify(sample->raw);
}

void BootMatrix_Init(void)
{
    memset(&g_boot_matrix, 0, sizeof(g_boot_matrix));

    ADC_POWER_ON(ADC);

    g_boot_matrix.adc_vdd_mv = BootAdc_ReadVddMv(&g_boot_matrix.adc_bandgap_raw,
                                                 &g_boot_matrix.adc_bandgap_trim);

    BootAdc_ConfigPins();

    BootAdc_ReadSample(BOOT_ADC_ADDR_SEL_CH, &g_boot_matrix.addr_sel);
    BootAdc_ReadSample(BOOT_ADC_SMBUS_ROLE_CH, &g_boot_matrix.smbus_role);
    BootAdc_ReadSample(BOOT_ADC_USER_CH13, &g_boot_matrix.user_ch13);
    BootAdc_ReadSample(BOOT_ADC_SGPIO_GROUP_CH, &g_boot_matrix.sgpio_group);
    BootAdc_ReadSample(BOOT_ADC_USER_CH15, &g_boot_matrix.user_ch15);

    ADC_Close(ADC);
    ADC_POWER_DOWN(ADC);
    BootAdc_RestorePins();

    if (g_boot_matrix.addr_sel.level == BOOT_ADC_LEVEL_HIGH)
    {
        g_boot_matrix.i2c1_addr7 = SMBUS_SLAVE_I2C1_ADDRESS_HIGH_7BIT;
        g_boot_matrix.usci0_addr7 = SMBUS_SLAVE_USCI0_ADDRESS_HIGH_7BIT;
    }
    else
    {
        g_boot_matrix.i2c1_addr7 = SMBUS_SLAVE_I2C1_ADDRESS_LOW_7BIT;
        g_boot_matrix.usci0_addr7 = SMBUS_SLAVE_USCI0_ADDRESS_LOW_7BIT;
    }

    if (g_boot_matrix.smbus_role.level == BOOT_ADC_LEVEL_LOW)
    {
        g_boot_matrix.smbus_role_id = BOOT_SMBUS_ROLE_UBM;
    }
    else
    {
        g_boot_matrix.smbus_role_id = BOOT_SMBUS_ROLE_VPP;
    }

    if (g_boot_matrix.user_ch13.level == BOOT_ADC_LEVEL_LOW)
    {
        g_boot_matrix.user_ch13_on = 1U;
        /* User hook: add ADC0_CH13 LOW behavior here. */
    }
    else
    {
        g_boot_matrix.user_ch13_on = 0U;
        /* User hook: add ADC0_CH13 HIGH behavior here. */
    }

    if (g_boot_matrix.sgpio_group.level == BOOT_ADC_LEVEL_LOW)
    {
        g_boot_matrix.sgpio_group_id = BOOT_SGPIO_GROUP_OD4_7;
    }
    else
    {
        g_boot_matrix.sgpio_group_id = BOOT_SGPIO_GROUP_OD0_3;
    }

    if (g_boot_matrix.user_ch15.level == BOOT_ADC_LEVEL_LOW)
    {
        g_boot_matrix.user_ch15_on = 0U;
        /* User hook: add ADC0_CH15 LOW behavior here. */
    }
    else
    {
        g_boot_matrix.user_ch15_on = 1U;
        /* User hook: add ADC0_CH15 HIGH behavior here. */
    }
}

static void BootMatrix_PrintSample(const char *name, const BOOT_ADC_SAMPLE_T *sample)
{
    printf("%s raw=%u mv=%u level=%s\r\n",
           name,
           (unsigned int)sample->raw,
           (unsigned int)sample->mv,
           BootAdc_LevelName(sample->level));
}

void BootMatrix_Print(void)
{
    printf("ADC VDD=%u mV bandgap_raw=%u bandgap_trim=%u low<=%u mV high>=%u mV\r\n",
           (unsigned int)g_boot_matrix.adc_vdd_mv,
           (unsigned int)g_boot_matrix.adc_bandgap_raw,
           (unsigned int)g_boot_matrix.adc_bandgap_trim,
           (unsigned int)BootAdc_RatioToMv(g_boot_matrix.adc_vdd_mv,
                                           BOOT_ADC_LOW_THRESHOLD_NUMERATOR),
           (unsigned int)BootAdc_RatioToMv(g_boot_matrix.adc_vdd_mv,
                                           BOOT_ADC_HIGH_THRESHOLD_NUMERATOR));

    BootMatrix_PrintSample("ADC0_CH11 addr select", &g_boot_matrix.addr_sel);
    printf("SMBus address matrix: I2C1 addr7=0x%02X write=0x%02X read=0x%02X, USCI0 addr7=0x%02X write=0x%02X read=0x%02X\r\n",
           (unsigned int)g_boot_matrix.i2c1_addr7,
           (unsigned int)SMBUS_SLAVE_ADDR_7BIT_TO_WRITE(g_boot_matrix.i2c1_addr7),
           (unsigned int)SMBUS_SLAVE_ADDR_7BIT_TO_READ(g_boot_matrix.i2c1_addr7),
           (unsigned int)g_boot_matrix.usci0_addr7,
           (unsigned int)SMBUS_SLAVE_ADDR_7BIT_TO_WRITE(g_boot_matrix.usci0_addr7),
           (unsigned int)SMBUS_SLAVE_ADDR_7BIT_TO_READ(g_boot_matrix.usci0_addr7));

    BootMatrix_PrintSample("ADC0_CH12 SMBus role", &g_boot_matrix.smbus_role);
    printf("SMBus role matrix: %s\r\n", BootMatrix_SmbusRoleName(g_boot_matrix.smbus_role_id));

    BootMatrix_PrintSample("ADC0_CH13 user option", &g_boot_matrix.user_ch13);
    printf("User option CH13: %s\r\n", (g_boot_matrix.user_ch13_on != 0U) ? "ON" : "OFF");

    BootMatrix_PrintSample("ADC0_CH14 SGPIO group", &g_boot_matrix.sgpio_group);
    printf("SGPIO group matrix: %s\r\n", BootMatrix_SgpioGroupName(g_boot_matrix.sgpio_group_id));

    BootMatrix_PrintSample("ADC0_CH15 user option", &g_boot_matrix.user_ch15);
    printf("User option CH15: %s\r\n", (g_boot_matrix.user_ch15_on != 0U) ? "ON" : "OFF");
}

void BootMatrix_ApplySmbusConfig(void)
{
    uint8_t command_profile;

    SMBusSlave_I2C1_SetAddress(g_boot_matrix.i2c1_addr7);
    SMBusSlave_USCI0_SetAddress(g_boot_matrix.usci0_addr7);

    if (g_boot_matrix.smbus_role_id == BOOT_SMBUS_ROLE_UBM)
    {
        command_profile = SMBUS_SLAVE_COMMAND_PROFILE_UBM_CONTROLLER;
    }
    else
    {
        command_profile = SMBUS_SLAVE_COMMAND_PROFILE_GENERIC;
    }

    SMBusSlave_I2C1_SetCommandProfile(command_profile);
    SMBusSlave_USCI0_SetCommandProfile(command_profile);
}
