#ifndef __SMBUS_TRANSACTION_H__
#define __SMBUS_TRANSACTION_H__

#include <stdint.h>
#include "smbus_slave.h"
#include "smbus_ubm_controller.h"

typedef enum
{
    SMBUS_TRANSACTION_PROTOCOL_UNKNOWN = 0,
    SMBUS_TRANSACTION_PROTOCOL_SEND_BYTE,
    SMBUS_TRANSACTION_PROTOCOL_RECEIVE_BYTE,
    SMBUS_TRANSACTION_PROTOCOL_WRITE_BYTE,
    SMBUS_TRANSACTION_PROTOCOL_WRITE_WORD,
    SMBUS_TRANSACTION_PROTOCOL_READ_BYTE,
    SMBUS_TRANSACTION_PROTOCOL_READ_WORD,
    SMBUS_TRANSACTION_PROTOCOL_BLOCK_WRITE,
    SMBUS_TRANSACTION_PROTOCOL_BLOCK_READ,
    SMBUS_TRANSACTION_PROTOCOL_PROCESS_CALL,
    SMBUS_TRANSACTION_PROTOCOL_BLOCK_WRITE_READ_PROCESS_CALL,
    SMBUS_TRANSACTION_PROTOCOL_UBM_CONTROLLER_READ,
    SMBUS_TRANSACTION_PROTOCOL_UBM_CONTROLLER_WRITE
} SMBUS_TRANSACTION_PROTOCOL_T;

typedef struct smbus_transaction_s
{
    uint8_t address_7bit;
    uint8_t command;
    uint8_t payload[SMBUS_SLAVE_MAX_BLOCK_SIZE + 1U];
    uint8_t data_len;
    uint8_t repeated_start;
    uint8_t pec_present;
    uint8_t pec_valid;
    SMBUS_TRANSACTION_PROTOCOL_T protocol;
} SMBUS_TRANSACTION_T;

typedef struct
{
    uint8_t command_profile;
    uint8_t last_send_byte;
    uint8_t receive_selector;
    uint8_t write_byte_shadow[2];
    uint16_t write_word_shadow[2];
    uint8_t block_shadow_length[2];
    uint8_t block_shadow[2][SMBUS_SLAVE_MAX_BLOCK_SIZE];
    uint8_t receive_byte_counter[2];
    uint8_t read_byte_counter[2];
    uint8_t read_word_counter[2];
    uint8_t block_read_counter[2];
    uint8_t process_counter[2];
    uint8_t block_process_counter[2];
    SMBUS_UBM_CONTROLLER_CONTEXT_T ubm;
} SMBUS_TRANSACTION_CONTEXT_T;

void SMBusTransaction_Init(SMBUS_TRANSACTION_CONTEXT_T *ctx);
void SMBusTransaction_SetProfile(SMBUS_TRANSACTION_CONTEXT_T *ctx, uint8_t profile);
uint8_t SMBusTransaction_GetProfile(const SMBUS_TRANSACTION_CONTEXT_T *ctx);
const char *SMBusTransaction_GetProfileName(uint8_t profile);
uint8_t SMBusTransaction_UsesSmbusPec(const SMBUS_TRANSACTION_CONTEXT_T *ctx);
uint8_t SMBusTransaction_GetCommandFlags(uint8_t command);
uint8_t SMBusTransaction_IsProcessCallCommand(uint8_t command);
uint8_t SMBusTransaction_IsBlockWriteReadProcessCallCommand(uint8_t command);
SMBUS_TRANSACTION_PROTOCOL_T SMBusTransaction_DetectProfileProtocol(const SMBUS_TRANSACTION_CONTEXT_T *ctx,
                                                                    uint8_t command,
                                                                    uint8_t data_len,
                                                                    uint8_t repeated_start);
uint8_t SMBusTransaction_GetReceiveByteCommand(const SMBUS_TRANSACTION_CONTEXT_T *ctx);
const char *SMBusTransaction_GetCommandName(uint8_t command);
const char *SMBusTransaction_GetCommandNameByProfile(uint8_t profile, uint8_t command);
uint8_t SMBusTransaction_Execute(SMBUS_TRANSACTION_CONTEXT_T *ctx,
                                 const SMBUS_TRANSACTION_T *transaction,
                                 uint8_t *tx_buffer,
                                 uint8_t *tx_length);

#endif
