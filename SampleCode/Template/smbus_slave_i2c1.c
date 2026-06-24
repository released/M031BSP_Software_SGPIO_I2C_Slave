/*_____ I N C L U D E S ____________________________________________________*/
#include <stdio.h>
#include "NuMicro.h"
#include "smbus_slave_core.h"

static SMBUS_SLAVE_CONTEXT_T g_smbus_i2c1_ctx;
static uint16_t g_smbus_i2c1_scl_low_ms;
static uint8_t g_smbus_i2c1_initialized;
static uint8_t g_smbus_i2c1_scl_low_state;

/*_____ F U N C T I O N S __________________________________________________*/

static uint32_t smbus_i2c1_next_control(void)
{
    uint32_t control;

    if (SMBusSlave_CoreGetAck(&g_smbus_i2c1_ctx) != 0U)
    {
        control = I2C_CTL_SI_AA;
    }
    else
    {
        control = I2C_CTL_SI;
    }

    return control;
}

static void smbus_i2c1_config_i2c_pins(void)
{
    SYS_UnlockReg();
    CLK_EnableModuleClock(I2C1_MODULE);
    SYS->GPA_MFPL = (SYS->GPA_MFPL & ~SMBUS_SLAVE_I2C1_MFPL_MASK) |
                    SMBUS_SLAVE_I2C1_MFPL_I2C;
    SYS_LockReg();

    GPIO_SetMode(SMBUS_SLAVE_I2C1_SDA_PORT,
                 SMBUS_SLAVE_I2C1_SDA_PIN_MASK,
                 GPIO_MODE_OPEN_DRAIN);
    GPIO_SetMode(SMBUS_SLAVE_I2C1_SCL_PORT,
                 SMBUS_SLAVE_I2C1_SCL_PIN_MASK,
                 GPIO_MODE_OPEN_DRAIN);
    SMBUS_SLAVE_I2C1_SDA = 1;
    SMBUS_SLAVE_I2C1_SCL = 1;
}

static void smbus_i2c1_config_gpio_bus_pins(void)
{
    SYS_UnlockReg();
    SYS->GPA_MFPL = (SYS->GPA_MFPL & ~SMBUS_SLAVE_I2C1_MFPL_MASK) |
                    SMBUS_SLAVE_I2C1_MFPL_GPIO;
    SYS_LockReg();

    GPIO_SetMode(SMBUS_SLAVE_I2C1_SDA_PORT,
                 SMBUS_SLAVE_I2C1_SDA_PIN_MASK,
                 GPIO_MODE_OPEN_DRAIN);
    GPIO_SetMode(SMBUS_SLAVE_I2C1_SCL_PORT,
                 SMBUS_SLAVE_I2C1_SCL_PIN_MASK,
                 GPIO_MODE_OPEN_DRAIN);
    SMBUS_SLAVE_I2C1_SDA = 1;
    SMBUS_SLAVE_I2C1_SCL = 1;
}

static uint8_t smbus_i2c1_bus_released(void)
{
    if ((SMBUS_SLAVE_I2C1_SDA != 0U) && (SMBUS_SLAVE_I2C1_SCL != 0U))
    {
        return 1U;
    }

    return 0U;
}

static void smbus_i2c1_bus_clear(void)
{
    uint8_t pulse_count;
    volatile uint8_t delay_count;

    smbus_i2c1_config_gpio_bus_pins();
    for (pulse_count = 0U; pulse_count < 9U; pulse_count++)
    {
        if (smbus_i2c1_bus_released() != 0U)
        {
            break;
        }

        SMBUS_SLAVE_I2C1_SCL = 1;
        for (delay_count = 0U; delay_count < 10U; delay_count++)
        {
        }
        SMBUS_SLAVE_I2C1_SCL = 0;
        for (delay_count = 0U; delay_count < 10U; delay_count++)
        {
        }
        SMBUS_SLAVE_I2C1_SCL = 1;
    }

    SMBUS_SLAVE_I2C1_SDA = 1;
    SMBUS_SLAVE_I2C1_SCL = 1;
}

static void smbus_i2c1_reset_clock_low_monitor(void)
{
    g_smbus_i2c1_scl_low_ms = 0U;
    g_smbus_i2c1_scl_low_state = 0U;
}

static void smbus_i2c1_check_clock_low_timeout_1ms(void)
{
    if (SMBUS_SLAVE_I2C1_SCL != 0U)
    {
        smbus_i2c1_reset_clock_low_monitor();
        return;
    }

    if (g_smbus_i2c1_scl_low_state == 0U)
    {
        g_smbus_i2c1_scl_low_ms = 1U;
        g_smbus_i2c1_scl_low_state = 1U;
    }
    else if (g_smbus_i2c1_scl_low_state == 1U)
    {
        if (g_smbus_i2c1_scl_low_ms < 0xFFFFU)
        {
            g_smbus_i2c1_scl_low_ms++;
        }

        if (g_smbus_i2c1_scl_low_ms >= SMBUS_SLAVE_CLOCK_LOW_TIMEOUT_MS)
        {
            g_smbus_i2c1_scl_low_state = 2U;
            SMBusSlave_CoreOnTimeout(&g_smbus_i2c1_ctx, SMBUS_STATUS_TIMEOUT);
        }
    }
}

static void smbus_i2c1_open_slave(void)
{
    smbus_i2c1_config_i2c_pins();
    I2C_Open(I2C1, SMBUS_SLAVE_BUS_CLOCK);
    I2C_DisableTimeout(I2C1);
    I2C_ClearTimeoutFlag(I2C1);
    I2C_SetSlaveAddr(I2C1, 0U, SMBUS_SLAVE_I2C1_ADDRESS_7BIT, 0U);
    I2C_SetSlaveAddrMask(I2C1, 0U, 0U);
    I2C_SET_CONTROL_REG(I2C1, I2C_CTL_SI_AA);
    I2C_EnableInt(I2C1);
    NVIC_ClearPendingIRQ(I2C1_IRQn);
    NVIC_EnableIRQ(I2C1_IRQn);
}

static void smbus_i2c1_recover_bus(void)
{
    g_smbus_i2c1_initialized = 0U;
    NVIC_DisableIRQ(I2C1_IRQn);
    I2C_DisableInt(I2C1);
    I2C_DisableTimeout(I2C1);
    I2C_ClearTimeoutFlag(I2C1);
    I2C_Close(I2C1);
    smbus_i2c1_bus_clear();
    SMBusSlave_CoreResetBus(&g_smbus_i2c1_ctx);
    smbus_i2c1_reset_clock_low_monitor();
    smbus_i2c1_open_slave();
    g_smbus_i2c1_initialized = 1U;
    SMBusSlave_CoreSetRecovered(&g_smbus_i2c1_ctx);
}

void SMBusSlave_I2C1_Init(void)
{
    SMBusSlave_CoreInit(&g_smbus_i2c1_ctx,
                        SMBUS_SLAVE_PORT_I2C1,
                        SMBUS_SLAVE_I2C1_ADDRESS_7BIT,
                        "I2C1");
    g_smbus_i2c1_initialized = 0U;
    smbus_i2c1_reset_clock_low_monitor();
    smbus_i2c1_open_slave();
    g_smbus_i2c1_initialized = 1U;

    printf("%s SMBus slave open-drain\r\n", SMBUS_SLAVE_I2C1_SDA_PIN_NAME);
    printf("%s SMBus slave open-drain\r\n", SMBUS_SLAVE_I2C1_SCL_PIN_NAME);
    printf("I2C1 SMBus slave addr7=0x%02X, PEC enabled in software\r\n",
           (unsigned int)SMBUS_SLAVE_I2C1_ADDRESS_7BIT);
}

void SMBusSlave_I2C1_Process(void)
{
    static SMBUS_SLAVE_EVENT_SNAPSHOT_T snapshot;

    NVIC_DisableIRQ(TMR1_IRQn);
    NVIC_DisableIRQ(I2C1_IRQn);
    SMBusSlave_CoreTakeEvents(&g_smbus_i2c1_ctx, &snapshot);
    NVIC_EnableIRQ(I2C1_IRQn);
    NVIC_EnableIRQ(TMR1_IRQn);

    if (snapshot.recover_pending != 0U)
    {
        SMBusSlave_UserTimeoutError(SMBUS_SLAVE_PORT_I2C1);
        smbus_i2c1_recover_bus();
    }

    SMBusSlave_CorePrintEvents(SMBUS_SLAVE_PORT_I2C1, &snapshot);
}

void SMBusSlave_I2C1_Timer1ms(void)
{
    if (g_smbus_i2c1_initialized == 0U)
    {
        return;
    }

    NVIC_DisableIRQ(I2C1_IRQn);
    smbus_i2c1_check_clock_low_timeout_1ms();
    NVIC_EnableIRQ(I2C1_IRQn);
}

void I2C1_IRQHandler(void)
{
    uint8_t status;
    uint8_t tx_byte;

    status = (uint8_t)I2C_GET_STATUS(I2C1);

    if (I2C_GET_TIMEOUT_FLAG(I2C1) != 0U)
    {
        I2C_ClearTimeoutFlag(I2C1);
        SMBusSlave_CoreOnIgnoredTimeout(&g_smbus_i2c1_ctx);
    }
    else
    {
        switch (status)
        {
            case SMBUS_STATUS_SLA_W_ACK:
                SMBusSlave_CoreOnAddressWrite(&g_smbus_i2c1_ctx);
                break;

            case SMBUS_STATUS_DATA_RX_ACK:
                SMBusSlave_CoreOnReceiveByte(&g_smbus_i2c1_ctx, (uint8_t)I2C_GET_DATA(I2C1));
                break;

            case SMBUS_STATUS_DATA_RX_NACK:
                SMBusSlave_CoreOnReceiveNack(&g_smbus_i2c1_ctx);
                break;

            case SMBUS_STATUS_STOP_RESTART:
                SMBusSlave_CoreOnStopOrRestart(&g_smbus_i2c1_ctx, status);
                break;

            case SMBUS_STATUS_SLA_R_ACK:
                tx_byte = SMBusSlave_CoreOnAddressRead(&g_smbus_i2c1_ctx, status);
                I2C_SET_DATA(I2C1, tx_byte);
                break;

            case SMBUS_STATUS_DATA_TX_ACK:
                tx_byte = SMBusSlave_CoreOnTransmitAck(&g_smbus_i2c1_ctx);
                I2C_SET_DATA(I2C1, tx_byte);
                break;

            case SMBUS_STATUS_DATA_TX_NACK:
            case SMBUS_STATUS_LAST_TX_ACK:
                SMBusSlave_CoreOnTransmitDone(&g_smbus_i2c1_ctx);
                break;

            case SMBUS_STATUS_BUS_ERROR:
                SMBusSlave_CoreOnBusError(&g_smbus_i2c1_ctx, status);
                break;

            default:
                SMBusSlave_CoreResetBus(&g_smbus_i2c1_ctx);
                break;
        }
    }

    I2C_SET_CONTROL_REG(I2C1, smbus_i2c1_next_control());
}
