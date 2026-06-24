#ifndef __SMBUS_SLAVE_H__
#define __SMBUS_SLAVE_H__

#include <stdint.h>
#include "NuMicro.h"

/*
 * SMBus slave buses:
 *   I2C1  -> PA2/I2C1_SDA, PA3/I2C1_SCL, addr 0x5A
 *   I2C0  -> PC0/I2C0_SDA, PC1/I2C0_SCL, addr 0x6A
 *   USCI0 -> PD0/USCI0_CLK, PD1/USCI0_DAT0, addr 0x4A
 */
#define SMBUS_SLAVE_I2C1_ADDRESS_7BIT        (0x5AU)
#define SMBUS_SLAVE_I2C0_ADDRESS_7BIT        (0x6AU)
/* Keep USCI0 out of the I2C reserved 0x78-0x7F / 10-bit prefix range. */
#define SMBUS_SLAVE_USCI0_ADDRESS_7BIT       (0x4AU)

#define SMBUS_SLAVE_BUS_CLOCK                (400000UL)
#define SMBUS_SLAVE_MAX_BLOCK_SIZE           (32U)
#define SMBUS_SLAVE_CLOCK_LOW_TIMEOUT_MS     (35U)

#ifndef PMBUS_DEBUG_ENABLE
#define PMBUS_DEBUG_ENABLE                   (1U)
#endif

#ifndef PMBUS_DEBUG_PRINT_RX_FRAME
#define PMBUS_DEBUG_PRINT_RX_FRAME           (1U)
#endif

#ifndef PMBUS_DEBUG_PRINT_TX_READY
#define PMBUS_DEBUG_PRINT_TX_READY           (1U)
#endif

#define SMBUS_SLAVE_I2C1_SDA_PORT            (PA)
#define SMBUS_SLAVE_I2C1_SDA_PIN_MASK        (BIT2)
#define SMBUS_SLAVE_I2C1_SDA                 (PA2)
#define SMBUS_SLAVE_I2C1_SDA_PIN_NAME        "PA2/I2C1_SDA"
#define SMBUS_SLAVE_I2C1_SCL_PORT            (PA)
#define SMBUS_SLAVE_I2C1_SCL_PIN_MASK        (BIT3)
#define SMBUS_SLAVE_I2C1_SCL                 (PA3)
#define SMBUS_SLAVE_I2C1_SCL_PIN_NAME        "PA3/I2C1_SCL"
#define SMBUS_SLAVE_I2C1_MFPL_MASK           (SYS_GPA_MFPL_PA2MFP_Msk | SYS_GPA_MFPL_PA3MFP_Msk)
#define SMBUS_SLAVE_I2C1_MFPL_I2C            (SYS_GPA_MFPL_PA2MFP_I2C1_SDA | SYS_GPA_MFPL_PA3MFP_I2C1_SCL)
#define SMBUS_SLAVE_I2C1_MFPL_GPIO           (SYS_GPA_MFPL_PA2MFP_GPIO | SYS_GPA_MFPL_PA3MFP_GPIO)

#define SMBUS_SLAVE_I2C0_SDA_PORT            (PC)
#define SMBUS_SLAVE_I2C0_SDA_PIN_MASK        (BIT0)
#define SMBUS_SLAVE_I2C0_SDA                 (PC0)
#define SMBUS_SLAVE_I2C0_SDA_PIN_NAME        "PC0/I2C0_SDA"
#define SMBUS_SLAVE_I2C0_SCL_PORT            (PC)
#define SMBUS_SLAVE_I2C0_SCL_PIN_MASK        (BIT1)
#define SMBUS_SLAVE_I2C0_SCL                 (PC1)
#define SMBUS_SLAVE_I2C0_SCL_PIN_NAME        "PC1/I2C0_SCL"
#define SMBUS_SLAVE_I2C0_MFPL_MASK           (SYS_GPC_MFPL_PC0MFP_Msk | SYS_GPC_MFPL_PC1MFP_Msk)
#define SMBUS_SLAVE_I2C0_MFPL_I2C            (SYS_GPC_MFPL_PC0MFP_I2C0_SDA | SYS_GPC_MFPL_PC1MFP_I2C0_SCL)
#define SMBUS_SLAVE_I2C0_MFPL_GPIO           (SYS_GPC_MFPL_PC0MFP_GPIO | SYS_GPC_MFPL_PC1MFP_GPIO)

#define SMBUS_SLAVE_USCI0_CLK_PORT           (PD)
#define SMBUS_SLAVE_USCI0_CLK_PIN_MASK       (BIT0)
#define SMBUS_SLAVE_USCI0_CLK                (PD0)
#define SMBUS_SLAVE_USCI0_CLK_PIN_NAME       "PD0/USCI0_CLK"
#define SMBUS_SLAVE_USCI0_DAT0_PORT          (PD)
#define SMBUS_SLAVE_USCI0_DAT0_PIN_MASK      (BIT1)
#define SMBUS_SLAVE_USCI0_DAT0               (PD1)
#define SMBUS_SLAVE_USCI0_DAT0_PIN_NAME      "PD1/USCI0_DAT0"
#define SMBUS_SLAVE_USCI0_MFPL_MASK          (SYS_GPD_MFPL_PD0MFP_Msk | SYS_GPD_MFPL_PD1MFP_Msk)
#define SMBUS_SLAVE_USCI0_MFPL_UI2C          (SYS_GPD_MFPL_PD0MFP_USCI0_CLK | SYS_GPD_MFPL_PD1MFP_USCI0_DAT0)
#define SMBUS_SLAVE_USCI0_MFPL_GPIO          (SYS_GPD_MFPL_PD0MFP_GPIO | SYS_GPD_MFPL_PD1MFP_GPIO)

/*
 * USCI0 TOCNT is an interrupt service timeout, not an SMBus clock-low timer.
 * Keep it disabled by default when multiple slave interfaces share one bus.
 */
#define SMBUS_SLAVE_USCI0_TIMEOUT_INTERRUPT_ENABLE (0U)
#define SMBUS_SLAVE_USCI0_TIMEOUT_COUNT            (0x3FFUL)

#define SMBUS_SLAVE_PORT_I2C1                (0U)
#define SMBUS_SLAVE_PORT_I2C0                (1U)
#define SMBUS_SLAVE_PORT_USCI0               (2U)

#define SMBUS_SLAVE_COMMAND_FLAG_READ_BYTE   (0x01U)
#define SMBUS_SLAVE_COMMAND_FLAG_READ_WORD   (0x02U)
#define SMBUS_SLAVE_COMMAND_FLAG_BLOCK_READ  (0x04U)
#define SMBUS_SLAVE_COMMAND_FLAG_BLOCK_WRITE (0x08U)
#define SMBUS_SLAVE_COMMAND_FLAG_WRITE_BYTE  (0x10U)
#define SMBUS_SLAVE_COMMAND_FLAG_SEND_BYTE   (0x20U)

#define SMBUS_SLAVE_PMBUS_CLEAR_FAULTS       (0x03U)
#define SMBUS_SLAVE_PMBUS_REVISION           (0x98U)
#define SMBUS_SLAVE_PMBUS_MFR_ID             (0x99U)
#define SMBUS_SLAVE_PMBUS_MFR_MODEL          (0x9AU)

void SMBusSlave_I2C0_Init(void);
void SMBusSlave_I2C0_Process(void);
void SMBusSlave_I2C0_Timer1ms(void);
void SMBusSlave_I2C1_Init(void);
void SMBusSlave_I2C1_Process(void);
void SMBusSlave_I2C1_Timer1ms(void);
void SMBusSlave_USCI0_Init(void);
void SMBusSlave_USCI0_Process(void);
void SMBusSlave_USCI0_Timer1ms(void);

/*
 * User extension API.
 * Return 1U when the application handles the command, otherwise return 0U.
 */
uint8_t SMBusSlave_UserGetCommandFlags(uint8_t port_id, uint8_t command, uint8_t *flags);
uint8_t SMBusSlave_UserReadByte(uint8_t port_id, uint8_t command, uint8_t *value);
uint8_t SMBusSlave_UserReadWord(uint8_t port_id, uint8_t command, uint16_t *value);
uint8_t SMBusSlave_UserBlockRead(uint8_t port_id, uint8_t command, uint8_t *data, uint8_t *length);
uint8_t SMBusSlave_UserSendByte(uint8_t port_id, uint8_t command, uint8_t pec_present, uint8_t pec_valid);
uint8_t SMBusSlave_UserWriteByte(uint8_t port_id, uint8_t command, uint8_t value, uint8_t pec_present, uint8_t pec_valid);
uint8_t SMBusSlave_UserBlockWrite(uint8_t port_id, uint8_t command, const uint8_t *data, uint8_t length, uint8_t pec_present, uint8_t pec_valid);
void SMBusSlave_UserTimeoutError(uint8_t port_id);

#endif
