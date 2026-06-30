/*_____ I N C L U D E S ____________________________________________________*/
#include "NuMicro.h"
#include "sgpio_led.h"

/*_____ D E C L A R A T I O N S ____________________________________________*/
extern uint32_t get_tick(void);

/*_____ D E F I N I T I O N S ______________________________________________*/
#define SGPIO_LED_OD_COUNT                (8U)
#define SGPIO_LED_GROUP_COUNT             (4U)
#define SGPIO_LED_MODE_OFF                (0U)
#define SGPIO_LED_MODE_ON                 (1U)
#define SGPIO_LED_MODE_BLINK_1HZ          (2U)
#define SGPIO_LED_MODE_BLINK_4HZ          (3U)
#define SGPIO_LED_ACTIVITY_OFF_LEVEL      (0U)
#define SGPIO_LED_ACTIVITY_ON_LEVEL       (1U)
#define SGPIO_LED_STATUS_OFF_LEVEL        (1U)
#define SGPIO_LED_STATUS_ON_LEVEL         (0U)
#define SGPIO_LED_1HZ_HALF_PERIOD_MS      (500UL)
#define SGPIO_LED_4HZ_HALF_PERIOD_MS      (125UL)

#define LED0_4_ACT_N                      (PF4)
#define LED1_5_ACT_N                      (PF3)
#define LED2_6_ACT_N                      (PF2)
#define LED3_7_ACT_N                      (PC7)
#define LED3_7_STA_N                      (PC2)
#define LED2_6_STA_N                      (PC3)
#define LED1_5_STA_N                      (PC4)
#define LED0_4_STA_N                      (PC5)
#define LED0_4_ACT_N_PORT                 (PF)
#define LED1_5_ACT_N_PORT                 (PF)
#define LED2_6_ACT_N_PORT                 (PF)
#define LED3_7_ACT_N_PORT                 (PC)
#define LED3_7_STA_N_PORT                 (PC)
#define LED2_6_STA_N_PORT                 (PC)
#define LED1_5_STA_N_PORT                 (PC)
#define LED0_4_STA_N_PORT                 (PC)
#define LED0_4_ACT_N_PIN_MASK             (BIT4)
#define LED1_5_ACT_N_PIN_MASK             (BIT3)
#define LED2_6_ACT_N_PIN_MASK             (BIT2)
#define LED3_7_ACT_N_PIN_MASK             (BIT7)
#define LED3_7_STA_N_PIN_MASK             (BIT2)
#define LED2_6_STA_N_PIN_MASK             (BIT3)
#define LED1_5_STA_N_PIN_MASK             (BIT4)
#define LED0_4_STA_N_PIN_MASK             (BIT5)

#ifndef SPLIT_ODn_GROUP_A_B
#define SPLIT_ODn_GROUP_A_B               (0U)
#endif
#define SPLIT_ODn_GROUP_PCB_A             (0U)
#define SPLIT_ODn_GROUP_PCB_B             (1U)
#ifndef SPLIT_ODn_GROUP_BOARD
#define SPLIT_ODn_GROUP_BOARD             (SPLIT_ODn_GROUP_PCB_A)
#endif
#ifndef SGPIO_RUNTIME_ODn_GROUP_SELECT
#define SGPIO_RUNTIME_ODn_GROUP_SELECT    (1U)
#endif

static uint8_t g_sgpio_activity_mode[SGPIO_LED_GROUP_COUNT];
static uint8_t g_sgpio_status_mode[SGPIO_LED_GROUP_COUNT];
static uint32_t g_sgpio_led_last_apply_tick = 0xFFFFFFFFUL;
static uint8_t g_sgpio_group_select = SGPIO_LED_GROUP_OD0_3;

/*_____ F U N C T I O N S __________________________________________________*/
void SGPIO_LED_SetGroupSelect(uint8_t group_select)
{
    if ((group_select == SGPIO_LED_GROUP_OD0_3) ||
        (group_select == SGPIO_LED_GROUP_OD4_7))
    {
        g_sgpio_group_select = group_select;
    }
}

static void sgpio_led_clear_modes(void)
{
    uint8_t group;

    for (group = 0U; group < SGPIO_LED_GROUP_COUNT; group++)
    {
        g_sgpio_activity_mode[group] = SGPIO_LED_MODE_OFF;
        g_sgpio_status_mode[group] = SGPIO_LED_MODE_OFF;
    }
}

static void sgpio_led_set_activity(uint8_t group, uint8_t enabled)
{
    uint8_t level;

    level = (enabled != 0U) ? SGPIO_LED_ACTIVITY_ON_LEVEL : SGPIO_LED_ACTIVITY_OFF_LEVEL;

    switch (group)
    {
        case 0U:
            LED0_4_ACT_N = level;
            break;

        case 1U:
            LED1_5_ACT_N = level;
            break;

        case 2U:
            LED2_6_ACT_N = level;
            break;

        case 3U:
            LED3_7_ACT_N = level;
            break;

        default:
            break;
    }
}

static void sgpio_led_set_status(uint8_t group, uint8_t enabled)
{
    uint8_t level;

    level = (enabled != 0U) ? SGPIO_LED_STATUS_ON_LEVEL : SGPIO_LED_STATUS_OFF_LEVEL;

    switch (group)
    {
        case 0U:
            LED0_4_STA_N = level;
            break;

        case 1U:
            LED1_5_STA_N = level;
            break;

        case 2U:
            LED2_6_STA_N = level;
            break;

        case 3U:
            LED3_7_STA_N = level;
            break;

        default:
            break;
    }
}

static void sgpio_led_config_pins(void)
{
    SYS_UnlockReg();
    SYS->GPC_MFPL = (SYS->GPC_MFPL & ~(SYS_GPC_MFPL_PC2MFP_Msk |
                                       SYS_GPC_MFPL_PC3MFP_Msk |
                                       SYS_GPC_MFPL_PC4MFP_Msk |
                                       SYS_GPC_MFPL_PC5MFP_Msk |
                                       SYS_GPC_MFPL_PC7MFP_Msk)) |
                    (SYS_GPC_MFPL_PC2MFP_GPIO |
                     SYS_GPC_MFPL_PC3MFP_GPIO |
                     SYS_GPC_MFPL_PC4MFP_GPIO |
                     SYS_GPC_MFPL_PC5MFP_GPIO |
                     SYS_GPC_MFPL_PC7MFP_GPIO);
    SYS->GPF_MFPL = (SYS->GPF_MFPL & ~(SYS_GPF_MFPL_PF2MFP_Msk |
                                       SYS_GPF_MFPL_PF3MFP_Msk |
                                       SYS_GPF_MFPL_PF4MFP_Msk)) |
                    (SYS_GPF_MFPL_PF2MFP_GPIO |
                     SYS_GPF_MFPL_PF3MFP_GPIO |
                     SYS_GPF_MFPL_PF4MFP_GPIO);
    SYS_LockReg();
}

static void sgpio_led_preset_off_levels(void)
{
    LED0_4_ACT_N = SGPIO_LED_ACTIVITY_OFF_LEVEL;
    LED1_5_ACT_N = SGPIO_LED_ACTIVITY_OFF_LEVEL;
    LED2_6_ACT_N = SGPIO_LED_ACTIVITY_OFF_LEVEL;
    LED3_7_ACT_N = SGPIO_LED_ACTIVITY_OFF_LEVEL;

    LED0_4_STA_N = SGPIO_LED_STATUS_OFF_LEVEL;
    LED1_5_STA_N = SGPIO_LED_STATUS_OFF_LEVEL;
    LED2_6_STA_N = SGPIO_LED_STATUS_OFF_LEVEL;
    LED3_7_STA_N = SGPIO_LED_STATUS_OFF_LEVEL;
}

static uint8_t sgpio_led_mode_is_on(uint8_t mode, uint32_t now)
{
    uint8_t enabled;

    enabled = 0U;

    switch (mode)
    {
        case SGPIO_LED_MODE_ON:
            enabled = 1U;
            break;

        case SGPIO_LED_MODE_BLINK_1HZ:
            enabled = (uint8_t)((((now / SGPIO_LED_1HZ_HALF_PERIOD_MS) & 1UL) == 0UL) ? 1U : 0U);
            break;

        case SGPIO_LED_MODE_BLINK_4HZ:
            enabled = (uint8_t)((((now / SGPIO_LED_4HZ_HALF_PERIOD_MS) & 1UL) == 0UL) ? 1U : 0U);
            break;

        default:
            break;
    }

    return enabled;
}

static void sgpio_led_apply_outputs(uint32_t now)
{
    uint8_t group;
    uint8_t activity_on;
    uint8_t status_on;

    for (group = 0U; group < SGPIO_LED_GROUP_COUNT; group++)
    {
        activity_on = sgpio_led_mode_is_on(g_sgpio_activity_mode[group], now);
        status_on = sgpio_led_mode_is_on(g_sgpio_status_mode[group], now);
        sgpio_led_set_activity(group, activity_on);
        sgpio_led_set_status(group, status_on);
    }
}

void SGPIO_LED_Process(void)
{
    uint32_t now;

    now = get_tick();
    if (g_sgpio_led_last_apply_tick == now)
    {
        return;
    }

    g_sgpio_led_last_apply_tick = now;
    sgpio_led_apply_outputs(now);
}

static void sgpio_led_update_group(uint8_t group, uint8_t activity_mode, uint8_t status_mode)
{
    if (group >= SGPIO_LED_GROUP_COUNT)
    {
        return;
    }

    if (activity_mode > g_sgpio_activity_mode[group])
    {
        g_sgpio_activity_mode[group] = activity_mode;
    }
    if (status_mode > g_sgpio_status_mode[group])
    {
        g_sgpio_status_mode[group] = status_mode;
    }
}

static uint8_t sgpio_led_slot_to_group(uint8_t slot, uint8_t *group)
{
    uint8_t accepted;

    accepted = 0U;

#if (SGPIO_RUNTIME_ODn_GROUP_SELECT != 0U)
    if (g_sgpio_group_select == SGPIO_LED_GROUP_OD0_3)
    {
        if (slot < SGPIO_LED_GROUP_COUNT)
        {
            *group = slot;
            accepted = 1U;
        }
    }
    else
    {
        if ((slot >= SGPIO_LED_GROUP_COUNT) && (slot < SGPIO_LED_OD_COUNT))
        {
            *group = (uint8_t)(slot - SGPIO_LED_GROUP_COUNT);
            accepted = 1U;
        }
    }
#elif (SPLIT_ODn_GROUP_A_B != 0U)
    #if (SPLIT_ODn_GROUP_BOARD == SPLIT_ODn_GROUP_PCB_A)
    if (slot < SGPIO_LED_GROUP_COUNT)
    {
        *group = slot;
        accepted = 1U;
    }
    #elif (SPLIT_ODn_GROUP_BOARD == SPLIT_ODn_GROUP_PCB_B)
    if ((slot >= SGPIO_LED_GROUP_COUNT) && (slot < SGPIO_LED_OD_COUNT))
    {
        *group = (uint8_t)(slot - SGPIO_LED_GROUP_COUNT);
        accepted = 1U;
    }
    #else
    #error "Invalid SPLIT_ODn_GROUP_BOARD setting"
    #endif
#else
    if (slot < SGPIO_LED_OD_COUNT)
    {
        *group = (uint8_t)(slot & 0x03U);
        accepted = 1U;
    }
#endif

    return accepted;
}

static void sgpio_led_decode_slot(uint8_t slot, uint8_t act, uint8_t locate, uint8_t fail)
{
    uint8_t group;
    uint8_t activity_mode;
    uint8_t status_mode;

    if (sgpio_led_slot_to_group(slot, &group) == 0U)
    {
        return;
    }

    activity_mode = SGPIO_LED_MODE_OFF;
    status_mode = SGPIO_LED_MODE_OFF;

    if ((locate != 0U) && (fail == 0U))
    {
        activity_mode = SGPIO_LED_MODE_BLINK_4HZ;
        status_mode = SGPIO_LED_MODE_BLINK_4HZ;
    }
    else if ((locate == 0U) && (fail != 0U))
    {
        status_mode = SGPIO_LED_MODE_ON;
    }
    else if ((locate != 0U) && (fail != 0U))
    {
        status_mode = SGPIO_LED_MODE_BLINK_1HZ;
    }
    else if (act != 0U)
    {
        activity_mode = SGPIO_LED_MODE_BLINK_4HZ;
    }
    else
    {
        activity_mode = SGPIO_LED_MODE_ON;
    }

    sgpio_led_update_group(group, activity_mode, status_mode);
}

void SGPIO_LED_OnFrameDecoded(const SGPIO_FRAME_T *frame)
{
    uint8_t slot;
    uint8_t act;
    uint8_t locate;
    uint8_t fail;
    uint16_t bit_index;
    uint16_t slot_mask;
    uint32_t led_tick;

    if ((frame == (const SGPIO_FRAME_T *)0) ||
        (frame->valid == 0U) ||
        (frame->overflow != 0U))
    {
        return;
    }

    sgpio_led_clear_modes();

    for (slot = 0U; slot < SGPIO_LED_OD_COUNT; slot++)
    {
        bit_index = (uint16_t)(slot * SGPIO_SLAVE_DATA_BITS_PER_SLOT);
        if ((uint16_t)(bit_index + 2U) >= frame->bit_count)
        {
            break;
        }

        slot_mask = (uint16_t)(1UL << slot);
        act = (uint8_t)(((frame->act_mask & slot_mask) != 0U) ? 1U : 0U);
        locate = (uint8_t)(((frame->locate_mask & slot_mask) != 0U) ? 1U : 0U);
        fail = (uint8_t)(((frame->fail_mask & slot_mask) != 0U) ? 1U : 0U);

        sgpio_led_decode_slot(slot, act, locate, fail);
    }

    led_tick = get_tick();
    g_sgpio_led_last_apply_tick = led_tick;
    sgpio_led_apply_outputs(led_tick);
}

void SGPIO_LED_Init(void)
{
    sgpio_led_config_pins();
    sgpio_led_preset_off_levels();

    GPIO_SetMode(LED0_4_ACT_N_PORT, LED0_4_ACT_N_PIN_MASK, GPIO_MODE_OUTPUT);
    GPIO_SetMode(LED1_5_ACT_N_PORT, LED1_5_ACT_N_PIN_MASK, GPIO_MODE_OUTPUT);
    GPIO_SetMode(LED2_6_ACT_N_PORT, LED2_6_ACT_N_PIN_MASK, GPIO_MODE_OUTPUT);
    GPIO_SetMode(LED3_7_ACT_N_PORT, LED3_7_ACT_N_PIN_MASK, GPIO_MODE_OUTPUT);
    GPIO_SetMode(LED0_4_STA_N_PORT, LED0_4_STA_N_PIN_MASK, GPIO_MODE_OUTPUT);
    GPIO_SetMode(LED1_5_STA_N_PORT, LED1_5_STA_N_PIN_MASK, GPIO_MODE_OUTPUT);
    GPIO_SetMode(LED2_6_STA_N_PORT, LED2_6_STA_N_PIN_MASK, GPIO_MODE_OUTPUT);
    GPIO_SetMode(LED3_7_STA_N_PORT, LED3_7_STA_N_PIN_MASK, GPIO_MODE_OUTPUT);

    sgpio_led_clear_modes();
    g_sgpio_led_last_apply_tick = 0xFFFFFFFFUL;
    SGPIO_LED_Process();
}
