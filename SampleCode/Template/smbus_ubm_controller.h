#ifndef __SMBUS_UBM_CONTROLLER_H__
#define __SMBUS_UBM_CONTROLLER_H__

#include <stdint.h>
#include "smbus_slave.h"

#define SMBUS_UBM_CONTROLLER_DESCRIPTOR_COUNT     (2U)
#define SMBUS_UBM_CONTROLLER_DFC_DESCRIPTOR_SIZE  (8U)
#define SMBUS_UBM_CONTROLLER_CCC_DESCRIPTOR_SIZE  (35U)
#define SMBUS_UBM_CONTROLLER_FLEX_DESCRIPTOR_SIZE (5U)
#define SMBUS_UBM_CONTROLLER_POWER_EVENT_SIZE     (32U)

struct smbus_transaction_s;

typedef struct
{
    uint8_t operational_state;
    uint8_t last_command_status;
    uint8_t features[2];
    uint8_t change_count[2];
    uint8_t dfc_index;
    uint8_t ccc_control;
    uint8_t ccc_index;
    uint8_t flex_index;
    uint8_t dfc_descriptors[SMBUS_UBM_CONTROLLER_DESCRIPTOR_COUNT][SMBUS_UBM_CONTROLLER_DFC_DESCRIPTOR_SIZE];
    uint8_t ccc_descriptor[SMBUS_UBM_CONTROLLER_CCC_DESCRIPTOR_SIZE];
    uint8_t flex_descriptor[SMBUS_UBM_CONTROLLER_FLEX_DESCRIPTOR_SIZE];
    uint8_t power_event[SMBUS_UBM_CONTROLLER_POWER_EVENT_SIZE];
    uint8_t pmdt_shadow[SMBUS_SLAVE_MAX_BLOCK_SIZE];
    uint8_t pmdt_length;
} SMBUS_UBM_CONTROLLER_CONTEXT_T;

void SMBusUbmController_Init(SMBUS_UBM_CONTROLLER_CONTEXT_T *ctx);
uint8_t SMBusUbmController_IsKnownCommand(uint8_t command);
uint8_t SMBusUbmController_IsReadRequest(uint8_t command, uint8_t data_len);
uint8_t SMBusUbmController_IsWriteRequest(uint8_t command, uint8_t data_len);
const char *SMBusUbmController_GetCommandName(uint8_t command);
uint8_t SMBusUbmController_Execute(SMBUS_UBM_CONTROLLER_CONTEXT_T *ctx,
                                   uint8_t address_7bit,
                                   const struct smbus_transaction_s *transaction,
                                   uint8_t *tx_buffer,
                                   uint8_t *tx_length);

#endif
