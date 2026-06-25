/*_____ I N C L U D E S ____________________________________________________*/
#include <stdio.h>
#include "NuMicro.h"
#include "smbus_slave_core.h"

#define SMBUS_USCI0_EVENT_ADDRESS_ACK        (0U)
#define SMBUS_USCI0_EVENT_RX_DATA            (1U)
#define SMBUS_USCI0_EVENT_TX_DATA            (2U)

static SMBUS_SLAVE_CONTEXT_T g_smbus_usci0_ctx;
static uint16_t g_smbus_usci0_scl_low_ms;
static uint8_t g_smbus_usci0_initialized;
static uint8_t g_smbus_usci0_event;
static uint8_t g_smbus_usci0_scl_low_state;

/*_____ F U N C T I O N S __________________________________________________*/

static uint32_t smbus_usci0_next_control(void)
{
    uint32_t control;

    control = UI2C_CTL_PTRG;
    if (SMBusSlave_CoreGetAck(&g_smbus_usci0_ctx) != 0U)
    {
        control |= UI2C_CTL_AA;
    }

    return control;
}

static void smbus_usci0_config_ui2c_pins(void)
{
    SYS_UnlockReg();
    CLK_EnableModuleClock(USCI0_MODULE);
    SYS->GPD_MFPL = (SYS->GPD_MFPL & ~SMBUS_SLAVE_USCI0_MFPL_MASK) |
                    SMBUS_SLAVE_USCI0_MFPL_UI2C;
    SYS_LockReg();

    GPIO_SetMode(SMBUS_SLAVE_USCI0_CLK_PORT,
                 SMBUS_SLAVE_USCI0_CLK_PIN_MASK,
                 GPIO_MODE_OPEN_DRAIN);
    GPIO_SetMode(SMBUS_SLAVE_USCI0_DAT0_PORT,
                 SMBUS_SLAVE_USCI0_DAT0_PIN_MASK,
                 GPIO_MODE_OPEN_DRAIN);
    SMBUS_SLAVE_USCI0_CLK = 1;
    SMBUS_SLAVE_USCI0_DAT0 = 1;
}

static void smbus_usci0_config_gpio_bus_pins(void)
{
    SYS_UnlockReg();
    SYS->GPD_MFPL = (SYS->GPD_MFPL & ~SMBUS_SLAVE_USCI0_MFPL_MASK) |
                    SMBUS_SLAVE_USCI0_MFPL_GPIO;
    SYS_LockReg();

    GPIO_SetMode(SMBUS_SLAVE_USCI0_CLK_PORT,
                 SMBUS_SLAVE_USCI0_CLK_PIN_MASK,
                 GPIO_MODE_OPEN_DRAIN);
    GPIO_SetMode(SMBUS_SLAVE_USCI0_DAT0_PORT,
                 SMBUS_SLAVE_USCI0_DAT0_PIN_MASK,
                 GPIO_MODE_OPEN_DRAIN);
    SMBUS_SLAVE_USCI0_CLK = 1;
    SMBUS_SLAVE_USCI0_DAT0 = 1;
}

static uint8_t smbus_usci0_bus_released(void)
{
    if ((SMBUS_SLAVE_USCI0_CLK != 0U) && (SMBUS_SLAVE_USCI0_DAT0 != 0U))
    {
        return 1U;
    }

    return 0U;
}

static void smbus_usci0_bus_clear(void)
{
    uint8_t pulse_count;
    volatile uint8_t delay_count;

    smbus_usci0_config_gpio_bus_pins();
    for (pulse_count = 0U; pulse_count < 9U; pulse_count++)
    {
        if (smbus_usci0_bus_released() != 0U)
        {
            break;
        }

        SMBUS_SLAVE_USCI0_CLK = 1;
        for (delay_count = 0U; delay_count < 10U; delay_count++)
        {
        }
        SMBUS_SLAVE_USCI0_CLK = 0;
        for (delay_count = 0U; delay_count < 10U; delay_count++)
        {
        }
        SMBUS_SLAVE_USCI0_CLK = 1;
    }

    SMBUS_SLAVE_USCI0_CLK = 1;
    SMBUS_SLAVE_USCI0_DAT0 = 1;
}

static void smbus_usci0_reset_clock_low_monitor(void)
{
    g_smbus_usci0_scl_low_ms = 0U;
    g_smbus_usci0_scl_low_state = 0U;
}

static void smbus_usci0_check_clock_low_timeout_1ms(void)
{
    if (SMBUS_SLAVE_USCI0_CLK != 0U)
    {
        smbus_usci0_reset_clock_low_monitor();
        return;
    }

    if (g_smbus_usci0_scl_low_state == 0U)
    {
        g_smbus_usci0_scl_low_ms = 1U;
        g_smbus_usci0_scl_low_state = 1U;
    }
    else if (g_smbus_usci0_scl_low_state == 1U)
    {
        if (g_smbus_usci0_scl_low_ms < 0xFFFFU)
        {
            g_smbus_usci0_scl_low_ms++;
        }

        if (g_smbus_usci0_scl_low_ms >= SMBUS_SLAVE_CLOCK_LOW_TIMEOUT_MS)
        {
            g_smbus_usci0_scl_low_state = 2U;
            SMBusSlave_CoreOnTimeout(&g_smbus_usci0_ctx, SMBUS_STATUS_TIMEOUT);
        }
    }
}

static void smbus_usci0_open_slave(void)
{
    smbus_usci0_config_ui2c_pins();
    UI2C_Open(UI2C0, SMBUS_SLAVE_BUS_CLOCK);
    UI2C_DISABLE_10BIT_ADDR_MODE(UI2C0);
    UI2C_SetSlaveAddr(UI2C0, 0U, SMBUS_SLAVE_USCI0_ADDRESS_7BIT, UI2C_GCMODE_DISABLE);
    UI2C_SetSlaveAddrMask(UI2C0, 0U, 0U);
    UI2C0->ADMAT = UI2C_ADMAT_ADMAT0_Msk;
    UI2C_SET_CONTROL_REG(UI2C0, UI2C_CTL_PTRG | UI2C_CTL_AA);
    UI2C_ClearTimeoutFlag(UI2C0);
#if (SMBUS_SLAVE_USCI0_TIMEOUT_INTERRUPT_ENABLE != 0U)
    UI2C_EnableTimeout(UI2C0, SMBUS_SLAVE_USCI0_TIMEOUT_COUNT);
#else
    UI2C_DisableTimeout(UI2C0);
#endif
    UI2C_EnableInt(UI2C0,
#if (SMBUS_SLAVE_USCI0_TIMEOUT_INTERRUPT_ENABLE != 0U)
                   UI2C_TO_INT_MASK |
#endif
                   UI2C_STAR_INT_MASK |
                   UI2C_STOR_INT_MASK |
                   UI2C_NACK_INT_MASK |
                   UI2C_ARBLO_INT_MASK |
                   UI2C_ERR_INT_MASK |
                   UI2C_ACK_INT_MASK);
    NVIC_ClearPendingIRQ(USCI01_IRQn);
    NVIC_EnableIRQ(USCI01_IRQn);
}

static void smbus_usci0_recover_bus(void)
{
    g_smbus_usci0_initialized = 0U;
    NVIC_DisableIRQ(USCI01_IRQn);
    UI2C_DisableInt(UI2C0,
#if (SMBUS_SLAVE_USCI0_TIMEOUT_INTERRUPT_ENABLE != 0U)
                    UI2C_TO_INT_MASK |
#endif
                    UI2C_STAR_INT_MASK |
                    UI2C_STOR_INT_MASK |
                    UI2C_NACK_INT_MASK |
                    UI2C_ARBLO_INT_MASK |
                    UI2C_ERR_INT_MASK |
                    UI2C_ACK_INT_MASK);
    UI2C_DisableTimeout(UI2C0);
    UI2C_ClearTimeoutFlag(UI2C0);
    UI2C_Close(UI2C0);
    smbus_usci0_bus_clear();
    g_smbus_usci0_event = SMBUS_USCI0_EVENT_ADDRESS_ACK;
    SMBusSlave_CoreResetBus(&g_smbus_usci0_ctx);
    smbus_usci0_reset_clock_low_monitor();
    smbus_usci0_open_slave();
    g_smbus_usci0_initialized = 1U;
    SMBusSlave_CoreSetRecovered(&g_smbus_usci0_ctx);
}

static void smbus_usci0_handle_address(uint32_t status)
{
    uint8_t tx_byte;

    /* Discard the address byte latched in RXDAT before receiving command/data. */
    (void)UI2C_GET_DATA(UI2C0);

    if ((status & UI2C_PROTSTS_SLAREAD_Msk) != 0UL)
    {
        SMBusSlave_CoreOnStopOrRestart(&g_smbus_usci0_ctx, SMBUS_STATUS_STOP_RESTART);
        tx_byte = SMBusSlave_CoreOnAddressRead(&g_smbus_usci0_ctx, SMBUS_STATUS_SLA_R_ACK);
        UI2C_SET_DATA(UI2C0, tx_byte);
        g_smbus_usci0_event = SMBUS_USCI0_EVENT_TX_DATA;
    }
    else
    {
        SMBusSlave_CoreOnAddressWrite(&g_smbus_usci0_ctx);
        g_smbus_usci0_event = SMBUS_USCI0_EVENT_RX_DATA;
    }
}

void SMBusSlave_USCI0_Init(void)
{
    SMBusSlave_CoreInit(&g_smbus_usci0_ctx,
                        SMBUS_SLAVE_PORT_USCI0,
                        SMBUS_SLAVE_USCI0_ADDRESS_7BIT,
                        "USCI0");
    g_smbus_usci0_initialized = 0U;
    smbus_usci0_reset_clock_low_monitor();
    g_smbus_usci0_event = SMBUS_USCI0_EVENT_ADDRESS_ACK;
    smbus_usci0_open_slave();
    g_smbus_usci0_initialized = 1U;

    printf("%s SMBus slave open-drain\r\n", SMBUS_SLAVE_USCI0_CLK_PIN_NAME);
    printf("%s SMBus slave open-drain\r\n", SMBUS_SLAVE_USCI0_DAT0_PIN_NAME);
    printf("USCI0 SMBus slave addr7=0x%02X, %s\r\n",
           (unsigned int)SMBUS_SLAVE_USCI0_ADDRESS_7BIT,
           PMBUS_PEC_POLICY_TEXT);
}

void SMBusSlave_USCI0_Process(void)
{
    static SMBUS_SLAVE_EVENT_SNAPSHOT_T snapshot;

    NVIC_DisableIRQ(TMR1_IRQn);
    NVIC_DisableIRQ(USCI01_IRQn);
    SMBusSlave_CoreTakeEvents(&g_smbus_usci0_ctx, &snapshot);
    NVIC_EnableIRQ(USCI01_IRQn);
    NVIC_EnableIRQ(TMR1_IRQn);

    if (snapshot.recover_pending != 0U)
    {
        SMBusSlave_UserTimeoutError(SMBUS_SLAVE_PORT_USCI0);
        smbus_usci0_recover_bus();
    }

    SMBusSlave_CorePrintEvents(SMBUS_SLAVE_PORT_USCI0, &snapshot);
}

void SMBusSlave_USCI0_Timer1ms(void)
{
    if (g_smbus_usci0_initialized == 0U)
    {
        return;
    }

    NVIC_DisableIRQ(USCI01_IRQn);
    smbus_usci0_check_clock_low_timeout_1ms();
    NVIC_EnableIRQ(USCI01_IRQn);
}

void USCI01_IRQHandler(void)
{
    uint32_t status;
    uint32_t current_status;
    uint32_t address_match;
    uint8_t tx_byte;

    status = UI2C_GET_PROT_STATUS(UI2C0);

    if ((status & UI2C_PROTSTS_TOIF_Msk) != 0UL)
    {
        UI2C_CLR_PROT_INT_FLAG(UI2C0, UI2C_PROTSTS_TOIF_Msk);
#if (SMBUS_SLAVE_USCI0_TIMEOUT_INTERRUPT_ENABLE != 0U)
        SMBusSlave_CoreOnTimeout(&g_smbus_usci0_ctx, SMBUS_STATUS_TIMEOUT);
#else
        SMBusSlave_CoreOnIgnoredTimeout(&g_smbus_usci0_ctx);
#endif
    }
    else
    {
        if ((status & (UI2C_PROTSTS_ERRIF_Msk | UI2C_PROTSTS_ARBLOIF_Msk | UI2C_PROTSTS_BUSHANG_Msk)) != 0UL)
        {
            UI2C_CLR_PROT_INT_FLAG(UI2C0, UI2C_PROTSTS_ERRIF_Msk | UI2C_PROTSTS_ARBLOIF_Msk);
            g_smbus_usci0_event = SMBUS_USCI0_EVENT_ADDRESS_ACK;
            SMBusSlave_CoreOnBusError(&g_smbus_usci0_ctx, SMBUS_STATUS_BUS_ERROR);
            UI2C_SET_CONTROL_REG(UI2C0, UI2C_CTL_PTRG | UI2C_CTL_STO | UI2C_CTL_AA);
            return;
        }

        if ((status & UI2C_PROTSTS_STARIF_Msk) != 0UL)
        {
            UI2C_CLR_PROT_INT_FLAG(UI2C0, UI2C_PROTSTS_STARIF_Msk);
            g_smbus_usci0_event = SMBUS_USCI0_EVENT_ADDRESS_ACK;
        }
        else if ((status & UI2C_PROTSTS_ACKIF_Msk) != 0UL)
        {
            UI2C_CLR_PROT_INT_FLAG(UI2C0, UI2C_PROTSTS_ACKIF_Msk);
            current_status = (UI2C_GET_PROT_STATUS(UI2C0) |
                              (status & (UI2C_PROTSTS_SLASEL_Msk |
                                         UI2C_PROTSTS_SLAREAD_Msk)));
            address_match = (UI2C0->ADMAT & UI2C_ADMAT_ADMAT0_Msk);
            if (address_match != 0UL)
            {
                UI2C0->ADMAT = UI2C_ADMAT_ADMAT0_Msk;
            }

            if (g_smbus_usci0_event == SMBUS_USCI0_EVENT_ADDRESS_ACK)
            {
                if ((address_match != 0UL) ||
                    ((current_status & UI2C_PROTSTS_SLASEL_Msk) != 0UL))
                {
                    smbus_usci0_handle_address(current_status);
                }
            }
            else if (g_smbus_usci0_event == SMBUS_USCI0_EVENT_TX_DATA)
            {
                tx_byte = SMBusSlave_CoreOnTransmitAck(&g_smbus_usci0_ctx);
                UI2C_SET_DATA(UI2C0, tx_byte);
            }
            else if (g_smbus_usci0_event == SMBUS_USCI0_EVENT_RX_DATA)
            {
                SMBusSlave_CoreOnReceiveByte(&g_smbus_usci0_ctx, (uint8_t)UI2C_GET_DATA(UI2C0));
            }
        }
        else if ((status & UI2C_PROTSTS_NACKIF_Msk) != 0UL)
        {
            UI2C_CLR_PROT_INT_FLAG(UI2C0, UI2C_PROTSTS_NACKIF_Msk);
            if (g_smbus_usci0_event == SMBUS_USCI0_EVENT_TX_DATA)
            {
                SMBusSlave_CoreOnTransmitDone(&g_smbus_usci0_ctx);
            }
            else if (g_smbus_usci0_event == SMBUS_USCI0_EVENT_RX_DATA)
            {
                SMBusSlave_CoreOnReceiveNack(&g_smbus_usci0_ctx);
            }
            g_smbus_usci0_event = SMBUS_USCI0_EVENT_ADDRESS_ACK;
        }
        else if ((status & UI2C_PROTSTS_STORIF_Msk) != 0UL)
        {
            UI2C_CLR_PROT_INT_FLAG(UI2C0, UI2C_PROTSTS_STORIF_Msk);
            if (g_smbus_usci0_event == SMBUS_USCI0_EVENT_RX_DATA)
            {
                SMBusSlave_CoreOnStopOrRestart(&g_smbus_usci0_ctx, SMBUS_STATUS_STOP_RESTART);
            }
            g_smbus_usci0_event = SMBUS_USCI0_EVENT_ADDRESS_ACK;
        }
    }

    UI2C_SET_CONTROL_REG(UI2C0, smbus_usci0_next_control());
}
