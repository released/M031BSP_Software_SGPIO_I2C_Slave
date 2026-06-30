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
static uint8_t g_smbus_usci0_address_7bit = SMBUS_SLAVE_USCI0_ADDRESS_7BIT;
static uint8_t g_smbus_usci0_command_profile = SMBUS_SLAVE_COMMAND_PROFILE_GENERIC;
static uint8_t g_smbus_usci0_read_preloaded;
static uint8_t g_smbus_usci0_preloaded_tx_byte;
static uint8_t g_smbus_usci0_read_addressed;
static uint8_t g_smbus_usci0_tx_clocked;
static volatile uint8_t g_smbus_usci0_resync_pending;

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

static void smbus_usci0_clear_tx_state(void)
{
    /*
     * USCI UI2C can keep the TX data register loaded when the master issues
     * an address-read phase but does not clock data bytes before the next
     * START.  Clear the common TX state; the hardware TX register is not
     * cleared to 0x00 because UI2C may use that preloaded byte as the first
     * data byte before firmware sees the following SLA+R ACK event.
     */
    SMBusSlave_CoreOnTransmitDone(&g_smbus_usci0_ctx);
    g_smbus_usci0_read_preloaded = 0U;
    g_smbus_usci0_preloaded_tx_byte = 0U;
    g_smbus_usci0_read_addressed = 0U;
    g_smbus_usci0_tx_clocked = 0U;
}

static void smbus_usci0_request_tx_resync(void)
{
    smbus_usci0_clear_tx_state();
    g_smbus_usci0_resync_pending = 1U;
}

static void smbus_usci0_prime_address_read(uint8_t status)
{
    uint8_t tx_byte;

    tx_byte = SMBusSlave_CoreOnAddressRead(&g_smbus_usci0_ctx, status);
    g_smbus_usci0_preloaded_tx_byte = tx_byte;
    UI2C_SET_DATA(UI2C0, tx_byte);
    g_smbus_usci0_read_preloaded = 1U;
}

static uint8_t smbus_usci0_encode_bus_error_status(uint32_t status)
{
    uint8_t encoded_status;

    encoded_status = 0U;
    if ((status & UI2C_PROTSTS_ERRIF_Msk) != 0UL)
    {
        encoded_status = (uint8_t)(encoded_status | 0x01U);
    }
    if ((status & UI2C_PROTSTS_ARBLOIF_Msk) != 0UL)
    {
        encoded_status = (uint8_t)(encoded_status | 0x02U);
    }
    if ((status & UI2C_PROTSTS_BUSHANG_Msk) != 0UL)
    {
        encoded_status = (uint8_t)(encoded_status | 0x04U);
    }
    if ((status & UI2C_PROTSTS_STARIF_Msk) != 0UL)
    {
        encoded_status = (uint8_t)(encoded_status | 0x10U);
    }
    if ((status & UI2C_PROTSTS_STORIF_Msk) != 0UL)
    {
        encoded_status = (uint8_t)(encoded_status | 0x20U);
    }
    if ((status & UI2C_PROTSTS_NACKIF_Msk) != 0UL)
    {
        encoded_status = (uint8_t)(encoded_status | 0x40U);
    }

    if (encoded_status == 0U)
    {
        encoded_status = 0xFFU;
    }

    return encoded_status;
}

static uint8_t smbus_usci0_is_receive_selector(uint8_t command)
{
    if ((command == SMBUS_SLAVE_GENERIC_RECEIVE_SELECT_A) ||
        (command == SMBUS_SLAVE_GENERIC_RECEIVE_SELECT_B))
    {
        return 1U;
    }

    return 0U;
}

static uint8_t smbus_usci0_is_quick_read_cleanup(void)
{
    if ((g_smbus_usci0_read_addressed != 0U) &&
        (g_smbus_usci0_tx_clocked == 0U))
    {
        return 1U;
    }

    return 0U;
}

static uint8_t smbus_usci0_has_read_context(void)
{
    if ((g_smbus_usci0_ctx.pending_read != 0U) ||
        (g_smbus_usci0_ctx.rx_length > 0U))
    {
        return 1U;
    }

    if ((g_smbus_usci0_ctx.transaction.last_send_byte == SMBUS_SLAVE_GENERIC_RECEIVE_SELECT_A) ||
        (g_smbus_usci0_ctx.transaction.last_send_byte == SMBUS_SLAVE_GENERIC_RECEIVE_SELECT_B))
    {
        return 1U;
    }

    return 0U;
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
    UI2C_SetSlaveAddr(UI2C0, 0U, g_smbus_usci0_address_7bit, UI2C_GCMODE_DISABLE);
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
    g_smbus_usci0_read_preloaded = 0U;
    g_smbus_usci0_preloaded_tx_byte = 0U;
    g_smbus_usci0_read_addressed = 0U;
    g_smbus_usci0_tx_clocked = 0U;
    g_smbus_usci0_resync_pending = 0U;
    SMBusSlave_CoreResetBus(&g_smbus_usci0_ctx);
    smbus_usci0_reset_clock_low_monitor();
    smbus_usci0_open_slave();
    g_smbus_usci0_initialized = 1U;
    SMBusSlave_CoreSetRecovered(&g_smbus_usci0_ctx);
}

static void smbus_usci0_resync_adapter(void)
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
    g_smbus_usci0_event = SMBUS_USCI0_EVENT_ADDRESS_ACK;
    g_smbus_usci0_read_preloaded = 0U;
    g_smbus_usci0_preloaded_tx_byte = 0U;
    g_smbus_usci0_read_addressed = 0U;
    g_smbus_usci0_tx_clocked = 0U;
    g_smbus_usci0_resync_pending = 0U;
    SMBusSlave_CoreResetBus(&g_smbus_usci0_ctx);
    smbus_usci0_reset_clock_low_monitor();
    smbus_usci0_open_slave();
    g_smbus_usci0_initialized = 1U;
}

static void smbus_usci0_handle_address(uint32_t status)
{
    uint8_t tx_byte;

    /* Discard the address byte latched in RXDAT before receiving command/data. */
    (void)UI2C_GET_DATA(UI2C0);

    if ((status & UI2C_PROTSTS_SLAREAD_Msk) != 0UL)
    {
        if (g_smbus_usci0_read_preloaded == 0U)
        {
            if (smbus_usci0_has_read_context() != 0U)
            {
                SMBusSlave_CoreOnStopOrRestart(&g_smbus_usci0_ctx, SMBUS_STATUS_STOP_RESTART);
                tx_byte = SMBusSlave_CoreOnAddressRead(&g_smbus_usci0_ctx, SMBUS_STATUS_SLA_R_ACK);
                UI2C_SET_DATA(UI2C0, tx_byte);
            }
            else
            {
                SMBusSlave_CoreResetBus(&g_smbus_usci0_ctx);
                UI2C_SET_DATA(UI2C0, 0xFFU);
            }
        }
        else
        {
            /*
             * CoreOnAddressRead() already prepared the response and advanced
             * tx_index to the second byte. Re-write the same first byte here
             * because UI2C can otherwise transmit the stale TXDAT byte on the
             * first data clock after SLA+R.
             */
            UI2C_SET_DATA(UI2C0, g_smbus_usci0_preloaded_tx_byte);
            g_smbus_usci0_read_preloaded = 0U;
            g_smbus_usci0_preloaded_tx_byte = 0U;
        }
        g_smbus_usci0_event = SMBUS_USCI0_EVENT_TX_DATA;
        g_smbus_usci0_read_addressed = 1U;
        g_smbus_usci0_tx_clocked = 0U;
    }
    else
    {
        if (g_smbus_usci0_read_preloaded != 0U)
        {
            smbus_usci0_clear_tx_state();
        }
        SMBusSlave_CoreOnAddressWrite(&g_smbus_usci0_ctx);
        g_smbus_usci0_event = SMBUS_USCI0_EVENT_RX_DATA;
        g_smbus_usci0_read_addressed = 0U;
        g_smbus_usci0_tx_clocked = 0U;
    }
}

static void smbus_usci0_handle_stop(uint32_t status)
{
    uint32_t clear_mask;
    uint8_t command;
    uint8_t prime_receive;

    clear_mask = UI2C_PROTSTS_STORIF_Msk;
    if ((status & UI2C_PROTSTS_ERRIF_Msk) != 0UL)
    {
        clear_mask |= UI2C_PROTSTS_ERRIF_Msk;
    }

    if (g_smbus_usci0_event == SMBUS_USCI0_EVENT_RX_DATA)
    {
        command = 0U;
        prime_receive = 0U;
        if (g_smbus_usci0_ctx.rx_length > 0U)
        {
            command = g_smbus_usci0_ctx.rx_buffer[0];
            prime_receive = smbus_usci0_is_receive_selector(command);
        }
        SMBusSlave_CoreOnStopOrRestart(&g_smbus_usci0_ctx, SMBUS_STATUS_STOP_RESTART);
        if (prime_receive != 0U)
        {
            smbus_usci0_prime_address_read(SMBUS_STATUS_SLA_R_ACK);
        }
    }
    else if (g_smbus_usci0_event == SMBUS_USCI0_EVENT_TX_DATA)
    {
        if (smbus_usci0_is_quick_read_cleanup() != 0U)
        {
            smbus_usci0_request_tx_resync();
        }
        else
        {
            /*
             * Pure SLA+R followed by STOP has no data/NACK phase.  USCI UI2C may
             * keep the first TX byte latched internally; request a background
             * adapter resync so the next Receive Byte cannot start with stale data.
             */
            smbus_usci0_request_tx_resync();
        }
    }

    g_smbus_usci0_event = SMBUS_USCI0_EVENT_ADDRESS_ACK;
    UI2C_CLR_PROT_INT_FLAG(UI2C0, clear_mask);
}

static void smbus_usci0_handle_start(uint32_t status)
{
    uint32_t clear_mask;

    clear_mask = UI2C_PROTSTS_STARIF_Msk;
    if (g_smbus_usci0_read_preloaded != 0U)
    {
        /*
         * For Receive Byte, the command selector ends with STOP and the next
         * pure read starts from ADDRESS_ACK state. Refresh TXDAT at START so
         * UI2C does not reuse a byte left by the previous read transaction.
         */
        UI2C_SET_DATA(UI2C0, g_smbus_usci0_preloaded_tx_byte);
    }

    if (g_smbus_usci0_event == SMBUS_USCI0_EVENT_TX_DATA)
    {
        if (smbus_usci0_is_quick_read_cleanup() != 0U)
        {
            smbus_usci0_request_tx_resync();
        }
        else
        {
            /*
             * A new START while TX_DATA is active means the host abandoned a slave
             * read before the normal data NACK path.  Resetting only the software
             * core is not enough for UI2C; the hardware TX shifter can retain the
             * previous first byte.
             */
            smbus_usci0_request_tx_resync();
        }
        if ((status & UI2C_PROTSTS_ERRIF_Msk) != 0UL)
        {
            clear_mask |= UI2C_PROTSTS_ERRIF_Msk;
        }
    }
    else if (g_smbus_usci0_event == SMBUS_USCI0_EVENT_RX_DATA)
    {
        SMBusSlave_CoreOnStopOrRestart(&g_smbus_usci0_ctx, SMBUS_STATUS_STOP_RESTART);
        if (g_smbus_usci0_ctx.pending_read != 0U)
        {
            smbus_usci0_prime_address_read(SMBUS_STATUS_SLA_R_ACK);
        }
        if ((status & UI2C_PROTSTS_ERRIF_Msk) != 0UL)
        {
            clear_mask |= UI2C_PROTSTS_ERRIF_Msk;
        }
    }

    g_smbus_usci0_event = SMBUS_USCI0_EVENT_ADDRESS_ACK;
    UI2C_CLR_PROT_INT_FLAG(UI2C0, clear_mask);
}

static void smbus_usci0_handle_ack(uint32_t status)
{
    uint32_t current_status;
    uint32_t address_match;
    uint8_t tx_byte;

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
        g_smbus_usci0_tx_clocked = 1U;
        tx_byte = SMBusSlave_CoreOnTransmitAck(&g_smbus_usci0_ctx);
        UI2C_SET_DATA(UI2C0, tx_byte);
    }
    else if (g_smbus_usci0_event == SMBUS_USCI0_EVENT_RX_DATA)
    {
        SMBusSlave_CoreOnReceiveByte(&g_smbus_usci0_ctx, (uint8_t)UI2C_GET_DATA(UI2C0));
    }

    UI2C_CLR_PROT_INT_FLAG(UI2C0, UI2C_PROTSTS_ACKIF_Msk);
}

static void smbus_usci0_handle_nack(uint32_t status)
{
    uint32_t clear_mask;

    clear_mask = UI2C_PROTSTS_NACKIF_Msk;
    if (g_smbus_usci0_event == SMBUS_USCI0_EVENT_TX_DATA)
    {
        if (smbus_usci0_is_quick_read_cleanup() == 0U)
        {
            g_smbus_usci0_tx_clocked = 1U;
            smbus_usci0_clear_tx_state();
        }
        else
        {
            smbus_usci0_request_tx_resync();
        }
        if ((status & UI2C_PROTSTS_ERRIF_Msk) != 0UL)
        {
            clear_mask |= UI2C_PROTSTS_ERRIF_Msk;
        }
    }
    else if (g_smbus_usci0_event == SMBUS_USCI0_EVENT_RX_DATA)
    {
        SMBusSlave_CoreOnReceiveNack(&g_smbus_usci0_ctx);
    }

    g_smbus_usci0_event = SMBUS_USCI0_EVENT_ADDRESS_ACK;
    UI2C_CLR_PROT_INT_FLAG(UI2C0, clear_mask);
}

static void smbus_usci0_handle_bus_error(uint32_t status)
{
    uint8_t encoded_status;

    if (((status & (UI2C_PROTSTS_ARBLOIF_Msk | UI2C_PROTSTS_BUSHANG_Msk)) == 0UL) &&
        (smbus_usci0_is_quick_read_cleanup() != 0U))
    {
        UI2C_CLR_PROT_INT_FLAG(UI2C0, UI2C_PROTSTS_ERRIF_Msk);
        smbus_usci0_request_tx_resync();
        UI2C_SET_CONTROL_REG(UI2C0, UI2C_CTL_PTRG | UI2C_CTL_AA);
        return;
    }

    encoded_status = smbus_usci0_encode_bus_error_status(status);
    UI2C_CLR_PROT_INT_FLAG(UI2C0, UI2C_PROTSTS_ERRIF_Msk | UI2C_PROTSTS_ARBLOIF_Msk);
    g_smbus_usci0_event = SMBUS_USCI0_EVENT_ADDRESS_ACK;
    g_smbus_usci0_read_preloaded = 0U;
    g_smbus_usci0_preloaded_tx_byte = 0U;
    g_smbus_usci0_read_addressed = 0U;
    g_smbus_usci0_tx_clocked = 0U;
    g_smbus_usci0_resync_pending = 1U;
    SMBusSlave_CoreOnBusError(&g_smbus_usci0_ctx, encoded_status);
    UI2C_SET_CONTROL_REG(UI2C0, UI2C_CTL_PTRG | UI2C_CTL_STO | UI2C_CTL_AA);
}

void SMBusSlave_USCI0_SetAddress(uint8_t address_7bit)
{
    if ((address_7bit > 0U) && (address_7bit < 0x80U))
    {
        g_smbus_usci0_address_7bit = address_7bit;
    }
}

void SMBusSlave_USCI0_SetCommandProfile(uint8_t profile)
{
    if (profile == SMBUS_SLAVE_COMMAND_PROFILE_UBM_CONTROLLER)
    {
        g_smbus_usci0_command_profile = SMBUS_SLAVE_COMMAND_PROFILE_UBM_CONTROLLER;
    }
    else
    {
        g_smbus_usci0_command_profile = SMBUS_SLAVE_COMMAND_PROFILE_GENERIC;
    }
}

void SMBusSlave_USCI0_Init(void)
{
    SMBusSlave_CoreInit(&g_smbus_usci0_ctx,
                        SMBUS_SLAVE_PORT_USCI0,
                        g_smbus_usci0_address_7bit,
                        "USCI0");
    SMBusTransaction_SetProfile(&g_smbus_usci0_ctx.transaction, g_smbus_usci0_command_profile);
    g_smbus_usci0_initialized = 0U;
    smbus_usci0_reset_clock_low_monitor();
    g_smbus_usci0_event = SMBUS_USCI0_EVENT_ADDRESS_ACK;
    g_smbus_usci0_read_preloaded = 0U;
    g_smbus_usci0_preloaded_tx_byte = 0U;
    g_smbus_usci0_read_addressed = 0U;
    g_smbus_usci0_tx_clocked = 0U;
    g_smbus_usci0_resync_pending = 0U;
    smbus_usci0_open_slave();
    g_smbus_usci0_initialized = 1U;

    printf("%s SMBus slave open-drain\r\n", SMBUS_SLAVE_USCI0_CLK_PIN_NAME);
    printf("%s SMBus slave open-drain\r\n", SMBUS_SLAVE_USCI0_DAT0_PIN_NAME);
    printf("USCI0 SMBus slave addr7=0x%02X write=0x%02X read=0x%02X, profile=%s, %s\r\n",
           (unsigned int)g_smbus_usci0_address_7bit,
           (unsigned int)SMBUS_SLAVE_ADDR_7BIT_TO_WRITE(g_smbus_usci0_address_7bit),
           (unsigned int)SMBUS_SLAVE_ADDR_7BIT_TO_READ(g_smbus_usci0_address_7bit),
           SMBusTransaction_GetProfileName(g_smbus_usci0_command_profile),
           (g_smbus_usci0_command_profile == SMBUS_SLAVE_COMMAND_PROFILE_UBM_CONTROLLER) ?
           "UBM checksum seed 0xA5" :
           SMBUS_PEC_POLICY_TEXT);
}

void SMBusSlave_USCI0_Process(void)
{
    static SMBUS_SLAVE_EVENT_SNAPSHOT_T snapshot;
    uint8_t resync_pending;

    NVIC_DisableIRQ(TMR1_IRQn);
    NVIC_DisableIRQ(USCI01_IRQn);
    SMBusSlave_CoreTakeEvents(&g_smbus_usci0_ctx, &snapshot);
    resync_pending = g_smbus_usci0_resync_pending;
    g_smbus_usci0_resync_pending = 0U;
    NVIC_EnableIRQ(USCI01_IRQn);
    NVIC_EnableIRQ(TMR1_IRQn);

    if (snapshot.recover_pending != 0U)
    {
        SMBusSlave_UserTimeoutError(SMBUS_SLAVE_PORT_USCI0);
        smbus_usci0_recover_bus();
    }
    else if (resync_pending != 0U)
    {
        smbus_usci0_resync_adapter();
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
        if ((status & UI2C_PROTSTS_STARIF_Msk) != 0UL)
        {
            smbus_usci0_handle_start(status);
        }
        else if ((status & UI2C_PROTSTS_ACKIF_Msk) != 0UL)
        {
            smbus_usci0_handle_ack(status);
        }
        else if ((status & UI2C_PROTSTS_NACKIF_Msk) != 0UL)
        {
            smbus_usci0_handle_nack(status);
        }
        else if ((status & UI2C_PROTSTS_STORIF_Msk) != 0UL)
        {
            smbus_usci0_handle_stop(status);
        }
        else if ((status & (UI2C_PROTSTS_ERRIF_Msk | UI2C_PROTSTS_ARBLOIF_Msk | UI2C_PROTSTS_BUSHANG_Msk)) != 0UL)
        {
            smbus_usci0_handle_bus_error(status);
            return;
        }
    }

    UI2C_SET_CONTROL_REG(UI2C0, smbus_usci0_next_control());
}
