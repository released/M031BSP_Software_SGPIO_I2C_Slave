#ifndef __SMBUS_SLAVE_H__
#define __SMBUS_SLAVE_H__

#include <stdint.h>
#include "NuMicro.h"

/*
 * SMBus slave buses:
 *   I2C1  -> PA13/I2C1_SDA, PA12/I2C1_SCL, strap-selected addr
 *   I2C0  -> PC0/I2C0_SDA, PC1/I2C0_SCL, addr 0x6A
 *   USCI0 -> PD0/USCI0_CLK, PD1/USCI0_DAT0, strap-selected addr
 */
#define SMBUS_SLAVE_ADDR_8BIT_TO_7BIT(addr)  ((uint8_t)((addr) >> 1U))
#define SMBUS_SLAVE_ADDR_7BIT_TO_WRITE(addr) ((uint8_t)((addr) << 1U))
#define SMBUS_SLAVE_ADDR_7BIT_TO_READ(addr)  ((uint8_t)(((addr) << 1U) | 0x01U))

#define SMBUS_SLAVE_I2C1_ADDRESS_LOW_8BIT    (0xC0U)
#define SMBUS_SLAVE_USCI0_ADDRESS_LOW_8BIT   (0xC2U)
#define SMBUS_SLAVE_I2C1_ADDRESS_HIGH_8BIT   (0xC4U)
#define SMBUS_SLAVE_USCI0_ADDRESS_HIGH_8BIT  (0xC6U)

#define SMBUS_SLAVE_I2C1_ADDRESS_LOW_7BIT    SMBUS_SLAVE_ADDR_8BIT_TO_7BIT(SMBUS_SLAVE_I2C1_ADDRESS_LOW_8BIT)
#define SMBUS_SLAVE_USCI0_ADDRESS_LOW_7BIT   SMBUS_SLAVE_ADDR_8BIT_TO_7BIT(SMBUS_SLAVE_USCI0_ADDRESS_LOW_8BIT)
#define SMBUS_SLAVE_I2C1_ADDRESS_HIGH_7BIT   SMBUS_SLAVE_ADDR_8BIT_TO_7BIT(SMBUS_SLAVE_I2C1_ADDRESS_HIGH_8BIT)
#define SMBUS_SLAVE_USCI0_ADDRESS_HIGH_7BIT  SMBUS_SLAVE_ADDR_8BIT_TO_7BIT(SMBUS_SLAVE_USCI0_ADDRESS_HIGH_8BIT)

#define SMBUS_SLAVE_I2C1_ADDRESS_7BIT        SMBUS_SLAVE_I2C1_ADDRESS_LOW_7BIT
#define SMBUS_SLAVE_I2C0_ADDRESS_7BIT        (0x6AU)
#define SMBUS_SLAVE_USCI0_ADDRESS_7BIT       SMBUS_SLAVE_USCI0_ADDRESS_LOW_7BIT

#define SMBUS_SLAVE_BUS_CLOCK                (400000UL)
#define SMBUS_SLAVE_MAX_BLOCK_SIZE           (32U)
#define SMBUS_SLAVE_CLOCK_LOW_TIMEOUT_MS     (35U)

#define SMBUS_PEC_POLICY_DISABLED            (0U)
#define SMBUS_PEC_POLICY_OPTIONAL            (1U)
#define SMBUS_PEC_POLICY_REQUIRED            (2U)

/*
 * PEC policy selection:
 *   DISABLED: no PEC generation/validation.
 *   OPTIONAL: validate host PEC when present and append read PEC.
 *   REQUIRED: write-side transactions require valid host PEC.
 */
#ifndef SMBUS_PEC_POLICY
#define SMBUS_PEC_POLICY                     SMBUS_PEC_POLICY_OPTIONAL
#endif

#define SMBUS_ENABLE_PEC                     ((SMBUS_PEC_POLICY) != SMBUS_PEC_POLICY_DISABLED)

#if (SMBUS_PEC_POLICY == SMBUS_PEC_POLICY_DISABLED)
#define SMBUS_PEC_POLICY_TEXT                "PEC disabled"
#elif (SMBUS_PEC_POLICY == SMBUS_PEC_POLICY_REQUIRED)
#define SMBUS_PEC_POLICY_TEXT                "PEC required in software"
#else
#define SMBUS_PEC_POLICY_TEXT                "PEC optional in software"
#endif

#ifndef SMBUS_DEBUG_ENABLE
#define SMBUS_DEBUG_ENABLE                   (1U)
#endif

#ifndef SMBUS_DEBUG_PRINT_RX_FRAME
#define SMBUS_DEBUG_PRINT_RX_FRAME           (1U)
#endif

#ifndef SMBUS_DEBUG_PRINT_TX_READY
#define SMBUS_DEBUG_PRINT_TX_READY           (1U)
#endif

#ifndef SMBUS_DEBUG_FRAME_QUEUE_SIZE
#define SMBUS_DEBUG_FRAME_QUEUE_SIZE         (8U)
#endif

#ifndef SMBUS_DEBUG_TX_QUEUE_SIZE
#define SMBUS_DEBUG_TX_QUEUE_SIZE            (8U)
#endif

#define SMBUS_SLAVE_I2C1_SDA_PORT            (PA)
#define SMBUS_SLAVE_I2C1_SDA_PIN_MASK        (BIT13)
#define SMBUS_SLAVE_I2C1_SDA                 (PA13)
#define SMBUS_SLAVE_I2C1_SDA_PIN_NAME        "PA13/I2C1_SDA"
#define SMBUS_SLAVE_I2C1_SCL_PORT            (PA)
#define SMBUS_SLAVE_I2C1_SCL_PIN_MASK        (BIT12)
#define SMBUS_SLAVE_I2C1_SCL                 (PA12)
#define SMBUS_SLAVE_I2C1_SCL_PIN_NAME        "PA12/I2C1_SCL"
#define SMBUS_SLAVE_I2C1_MFPH_MASK           (SYS_GPA_MFPH_PA12MFP_Msk | SYS_GPA_MFPH_PA13MFP_Msk)
#define SMBUS_SLAVE_I2C1_MFPH_I2C            (SYS_GPA_MFPH_PA12MFP_I2C1_SCL | SYS_GPA_MFPH_PA13MFP_I2C1_SDA)
#define SMBUS_SLAVE_I2C1_MFPH_GPIO           (SYS_GPA_MFPH_PA12MFP_GPIO | SYS_GPA_MFPH_PA13MFP_GPIO)

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

#define SMBUS_SLAVE_COMMAND_PROFILE_GENERIC        (0U)
#define SMBUS_SLAVE_COMMAND_PROFILE_UBM_CONTROLLER (1U)

#define SMBUS_SLAVE_COMMAND_FLAG_READ_BYTE   (0x01U)
#define SMBUS_SLAVE_COMMAND_FLAG_READ_WORD   (0x02U)
#define SMBUS_SLAVE_COMMAND_FLAG_BLOCK_READ  (0x04U)
#define SMBUS_SLAVE_COMMAND_FLAG_BLOCK_WRITE (0x08U)
#define SMBUS_SLAVE_COMMAND_FLAG_WRITE_BYTE  (0x10U)
#define SMBUS_SLAVE_COMMAND_FLAG_SEND_BYTE   (0x20U)
#define SMBUS_SLAVE_COMMAND_FLAG_WRITE_WORD  (0x40U)
#define SMBUS_SLAVE_COMMAND_FLAG_PROCESS_CALL (0x80U)

#define SMBUS_SLAVE_GENERIC_SEND_BYTE_A      (0x10U)
#define SMBUS_SLAVE_GENERIC_SEND_BYTE_B      (0x11U)
#define SMBUS_SLAVE_GENERIC_RECEIVE_SELECT_A (0x12U)
#define SMBUS_SLAVE_GENERIC_RECEIVE_SELECT_B (0x13U)
#define SMBUS_SLAVE_GENERIC_WRITE_BYTE_A     (0x20U)
#define SMBUS_SLAVE_GENERIC_WRITE_BYTE_B     (0x21U)
#define SMBUS_SLAVE_GENERIC_READ_BYTE_A      (0x22U)
#define SMBUS_SLAVE_GENERIC_READ_BYTE_B      (0x23U)
#define SMBUS_SLAVE_GENERIC_WRITE_WORD_A     (0x30U)
#define SMBUS_SLAVE_GENERIC_WRITE_WORD_B     (0x31U)
#define SMBUS_SLAVE_GENERIC_READ_WORD_A      (0x32U)
#define SMBUS_SLAVE_GENERIC_READ_WORD_B      (0x33U)
#define SMBUS_SLAVE_GENERIC_BLOCK_WRITE_A    (0x40U)
#define SMBUS_SLAVE_GENERIC_BLOCK_WRITE_B    (0x41U)
#define SMBUS_SLAVE_GENERIC_BLOCK_READ_A     (0x42U)
#define SMBUS_SLAVE_GENERIC_BLOCK_READ_B     (0x43U)
#define SMBUS_SLAVE_GENERIC_PROCESS_CALL_A   (0x50U)
#define SMBUS_SLAVE_GENERIC_PROCESS_CALL_B   (0x51U)
#define SMBUS_SLAVE_GENERIC_BLOCK_PROC_CALL_A (0x60U)
#define SMBUS_SLAVE_GENERIC_BLOCK_PROC_CALL_B (0x61U)

void SMBusSlave_I2C0_Init(void);
void SMBusSlave_I2C0_Process(void);
void SMBusSlave_I2C0_Timer1ms(void);
void SMBusSlave_I2C1_SetAddress(uint8_t address_7bit);
void SMBusSlave_I2C1_SetCommandProfile(uint8_t profile);
void SMBusSlave_I2C1_Init(void);
void SMBusSlave_I2C1_Process(void);
void SMBusSlave_I2C1_Timer1ms(void);
void SMBusSlave_USCI0_SetAddress(uint8_t address_7bit);
void SMBusSlave_USCI0_SetCommandProfile(uint8_t profile);
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
uint8_t SMBusSlave_UserWriteWord(uint8_t port_id, uint8_t command, uint16_t value, uint8_t pec_present, uint8_t pec_valid);
uint8_t SMBusSlave_UserBlockWrite(uint8_t port_id, uint8_t command, const uint8_t *data, uint8_t length, uint8_t pec_present, uint8_t pec_valid);
void SMBusSlave_UserTimeoutError(uint8_t port_id);

#endif
