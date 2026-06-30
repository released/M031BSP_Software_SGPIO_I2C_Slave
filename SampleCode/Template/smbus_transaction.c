/*_____ I N C L U D E S ____________________________________________________*/
#include <string.h>
#include "smbus_transaction.h"

/*_____ D E F I N I T I O N S ______________________________________________*/

typedef enum
{
    SMBUS_TRANSACTION_RESPONSE_NONE = 0,
    SMBUS_TRANSACTION_RESPONSE_BYTE,
    SMBUS_TRANSACTION_RESPONSE_WORD,
    SMBUS_TRANSACTION_RESPONSE_BLOCK
} SMBUS_TRANSACTION_RESPONSE_T;

typedef struct
{
    uint8_t command;
    uint8_t read_kind;
    uint8_t flags;
    uint8_t index;
} SMBUS_TRANSACTION_DESCRIPTOR_T;

static const SMBUS_TRANSACTION_DESCRIPTOR_T g_smbus_transaction_descriptors[] =
{
    { SMBUS_SLAVE_GENERIC_SEND_BYTE_A,      SMBUS_TRANSACTION_RESPONSE_NONE,  SMBUS_SLAVE_COMMAND_FLAG_SEND_BYTE,    0U },
    { SMBUS_SLAVE_GENERIC_SEND_BYTE_B,      SMBUS_TRANSACTION_RESPONSE_NONE,  SMBUS_SLAVE_COMMAND_FLAG_SEND_BYTE,    1U },
    { SMBUS_SLAVE_GENERIC_RECEIVE_SELECT_A, SMBUS_TRANSACTION_RESPONSE_NONE,  SMBUS_SLAVE_COMMAND_FLAG_SEND_BYTE,    2U },
    { SMBUS_SLAVE_GENERIC_RECEIVE_SELECT_B, SMBUS_TRANSACTION_RESPONSE_NONE,  SMBUS_SLAVE_COMMAND_FLAG_SEND_BYTE,    3U },

    { SMBUS_SLAVE_GENERIC_WRITE_BYTE_A,     SMBUS_TRANSACTION_RESPONSE_NONE,  SMBUS_SLAVE_COMMAND_FLAG_WRITE_BYTE,   0U },
    { SMBUS_SLAVE_GENERIC_WRITE_BYTE_B,     SMBUS_TRANSACTION_RESPONSE_NONE,  SMBUS_SLAVE_COMMAND_FLAG_WRITE_BYTE,   1U },
    { SMBUS_SLAVE_GENERIC_READ_BYTE_A,      SMBUS_TRANSACTION_RESPONSE_BYTE,  0U,                                  0U },
    { SMBUS_SLAVE_GENERIC_READ_BYTE_B,      SMBUS_TRANSACTION_RESPONSE_BYTE,  0U,                                  1U },

    { SMBUS_SLAVE_GENERIC_WRITE_WORD_A,     SMBUS_TRANSACTION_RESPONSE_NONE,  SMBUS_SLAVE_COMMAND_FLAG_WRITE_WORD,   0U },
    { SMBUS_SLAVE_GENERIC_WRITE_WORD_B,     SMBUS_TRANSACTION_RESPONSE_NONE,  SMBUS_SLAVE_COMMAND_FLAG_WRITE_WORD,   1U },
    { SMBUS_SLAVE_GENERIC_READ_WORD_A,      SMBUS_TRANSACTION_RESPONSE_WORD,  0U,                                  0U },
    { SMBUS_SLAVE_GENERIC_READ_WORD_B,      SMBUS_TRANSACTION_RESPONSE_WORD,  0U,                                  1U },

    { SMBUS_SLAVE_GENERIC_BLOCK_WRITE_A,    SMBUS_TRANSACTION_RESPONSE_NONE,  SMBUS_SLAVE_COMMAND_FLAG_BLOCK_WRITE,  0U },
    { SMBUS_SLAVE_GENERIC_BLOCK_WRITE_B,    SMBUS_TRANSACTION_RESPONSE_NONE,  SMBUS_SLAVE_COMMAND_FLAG_BLOCK_WRITE,  1U },
    { SMBUS_SLAVE_GENERIC_BLOCK_READ_A,     SMBUS_TRANSACTION_RESPONSE_BLOCK, 0U,                                  0U },
    { SMBUS_SLAVE_GENERIC_BLOCK_READ_B,     SMBUS_TRANSACTION_RESPONSE_BLOCK, 0U,                                  1U },

    { SMBUS_SLAVE_GENERIC_PROCESS_CALL_A,   SMBUS_TRANSACTION_RESPONSE_WORD,  SMBUS_SLAVE_COMMAND_FLAG_PROCESS_CALL, 0U },
    { SMBUS_SLAVE_GENERIC_PROCESS_CALL_B,   SMBUS_TRANSACTION_RESPONSE_WORD,  SMBUS_SLAVE_COMMAND_FLAG_PROCESS_CALL, 1U },
    { SMBUS_SLAVE_GENERIC_BLOCK_PROC_CALL_A, SMBUS_TRANSACTION_RESPONSE_BLOCK,
      (uint8_t)(SMBUS_SLAVE_COMMAND_FLAG_BLOCK_WRITE | SMBUS_SLAVE_COMMAND_FLAG_BLOCK_READ), 0U },
    { SMBUS_SLAVE_GENERIC_BLOCK_PROC_CALL_B, SMBUS_TRANSACTION_RESPONSE_BLOCK,
      (uint8_t)(SMBUS_SLAVE_COMMAND_FLAG_BLOCK_WRITE | SMBUS_SLAVE_COMMAND_FLAG_BLOCK_READ), 1U }
};

/*_____ F U N C T I O N S __________________________________________________*/

static const SMBUS_TRANSACTION_DESCRIPTOR_T *smbus_transaction_find_descriptor(uint8_t command)
{
    uint8_t index;
    uint8_t count;

    count = (uint8_t)(sizeof(g_smbus_transaction_descriptors) / sizeof(g_smbus_transaction_descriptors[0]));
    for (index = 0U; index < count; index++)
    {
        if (g_smbus_transaction_descriptors[index].command == command)
        {
            return &g_smbus_transaction_descriptors[index];
        }
    }

    return (const SMBUS_TRANSACTION_DESCRIPTOR_T *)0;
}

static uint8_t smbus_transaction_example_index(uint8_t index)
{
    if (index > 1U)
    {
        return 0U;
    }

    return index;
}

static uint8_t smbus_transaction_example_length(uint8_t index)
{
    index = smbus_transaction_example_index(index);
    if (index == 0U)
    {
        return 8U;
    }

    return 16U;
}

static uint8_t smbus_transaction_counter(uint8_t index, uint8_t *counter_table)
{
    uint8_t value;
    uint8_t base;

    index = smbus_transaction_example_index(index);
    value = counter_table[index];
    counter_table[index] = (uint8_t)(value + 1U);
    base = (uint8_t)((index == 0U) ? 0x00U : 0x80U);

    return (uint8_t)(base + value);
}

static uint16_t smbus_transaction_load_word(const uint8_t *payload)
{
    return (uint16_t)(((uint16_t)payload[1] << 8) | payload[0]);
}

static uint8_t smbus_transaction_receive_index(const SMBUS_TRANSACTION_CONTEXT_T *ctx)
{
    if (ctx->receive_selector == SMBUS_SLAVE_GENERIC_RECEIVE_SELECT_B)
    {
        return 1U;
    }

    return 0U;
}

static uint8_t smbus_transaction_execute_receive_byte(SMBUS_TRANSACTION_CONTEXT_T *ctx,
                                                      uint8_t *tx_buffer,
                                                      uint8_t *tx_length)
{
    uint8_t index;

    index = smbus_transaction_receive_index(ctx);
    tx_buffer[0] = smbus_transaction_counter(index, ctx->receive_byte_counter);
    *tx_length = 1U;
    ctx->last_send_byte = 0U;
    return 1U;
}

static uint8_t smbus_transaction_execute_read_byte(SMBUS_TRANSACTION_CONTEXT_T *ctx,
                                                   uint8_t index,
                                                   uint8_t *tx_buffer,
                                                   uint8_t *tx_length)
{
    tx_buffer[0] = smbus_transaction_counter(index, ctx->read_byte_counter);
    *tx_length = 1U;
    return 1U;
}

static uint8_t smbus_transaction_execute_read_word(SMBUS_TRANSACTION_CONTEXT_T *ctx,
                                                   uint8_t index,
                                                   uint8_t *tx_buffer,
                                                   uint8_t *tx_length)
{
    uint8_t counter;

    counter = smbus_transaction_counter(index, ctx->read_word_counter);
    tx_buffer[0] = (uint8_t)((index == 0U) ? 0x32U : 0x33U);
    tx_buffer[1] = counter;
    *tx_length = 2U;
    return 1U;
}

static uint8_t smbus_transaction_execute_block_read(SMBUS_TRANSACTION_CONTEXT_T *ctx,
                                                    uint8_t index,
                                                    uint8_t *tx_buffer,
                                                    uint8_t *tx_length)
{
    uint8_t length;
    uint8_t copy_index;
    uint8_t counter;
    uint8_t base;

    index = smbus_transaction_example_index(index);
    length = ctx->block_shadow_length[index];
    counter = smbus_transaction_counter(index, ctx->block_read_counter);

    if (length == 0U)
    {
        length = smbus_transaction_example_length(index);
        base = (uint8_t)((index == 0U) ? 0x10U : 0x20U);
        tx_buffer[0] = length;
        for (copy_index = 0U; copy_index < length; copy_index++)
        {
            tx_buffer[(uint8_t)(copy_index + 1U)] = (uint8_t)(base + copy_index);
        }
    }
    else
    {
        if (length > SMBUS_SLAVE_MAX_BLOCK_SIZE)
        {
            length = SMBUS_SLAVE_MAX_BLOCK_SIZE;
        }
        tx_buffer[0] = length;
        for (copy_index = 0U; copy_index < length; copy_index++)
        {
            tx_buffer[(uint8_t)(copy_index + 1U)] = ctx->block_shadow[index][copy_index];
        }
    }

    if (length > 0U)
    {
        tx_buffer[length] = counter;
    }
    *tx_length = (uint8_t)(length + 1U);
    return 1U;
}

static uint8_t smbus_transaction_execute_process_call(SMBUS_TRANSACTION_CONTEXT_T *ctx,
                                                      const SMBUS_TRANSACTION_T *transaction,
                                                      uint8_t index,
                                                      uint8_t *tx_buffer,
                                                      uint8_t *tx_length)
{
    uint8_t counter;
    uint16_t word_value;
    uint16_t response_word;

    if (transaction->data_len != 2U)
    {
        return 0U;
    }

    word_value = smbus_transaction_load_word(transaction->payload);
    counter = smbus_transaction_counter(index, ctx->process_counter);
    if (index == 0U)
    {
        response_word = (uint16_t)(word_value + 0x1111U);
    }
    else
    {
        response_word = (uint16_t)(word_value ^ 0xFFFFU);
    }

    tx_buffer[0] = (uint8_t)(response_word & 0x00FFU);
    tx_buffer[1] = counter;
    *tx_length = 2U;
    return 1U;
}

static uint8_t smbus_transaction_execute_block_process_call(SMBUS_TRANSACTION_CONTEXT_T *ctx,
                                                            const SMBUS_TRANSACTION_T *transaction,
                                                            uint8_t index,
                                                            uint8_t *tx_buffer,
                                                            uint8_t *tx_length)
{
    uint8_t block_len;
    uint8_t copy_index;
    uint8_t counter;

    if (transaction->data_len == 0U)
    {
        return 0U;
    }

    block_len = transaction->payload[0];
    if ((block_len > SMBUS_SLAVE_MAX_BLOCK_SIZE) ||
        (transaction->data_len != (uint8_t)(block_len + 1U)))
    {
        return 0U;
    }

    counter = smbus_transaction_counter(index, ctx->block_process_counter);
    tx_buffer[0] = block_len;
    for (copy_index = 0U; copy_index < block_len; copy_index++)
    {
        if (index == 0U)
        {
            tx_buffer[(uint8_t)(copy_index + 1U)] =
                (uint8_t)(transaction->payload[(uint8_t)(copy_index + 1U)] + 1U);
        }
        else
        {
            tx_buffer[(uint8_t)(copy_index + 1U)] =
                transaction->payload[(uint8_t)(block_len - copy_index)];
        }
    }

    if (block_len > 0U)
    {
        tx_buffer[block_len] = counter;
    }
    *tx_length = (uint8_t)(block_len + 1U);
    return 1U;
}

void SMBusTransaction_Init(SMBUS_TRANSACTION_CONTEXT_T *ctx)
{
    memset(ctx, 0, sizeof(SMBUS_TRANSACTION_CONTEXT_T));
    ctx->command_profile = SMBUS_SLAVE_COMMAND_PROFILE_GENERIC;
    SMBusUbmController_Init(&ctx->ubm);
}

void SMBusTransaction_SetProfile(SMBUS_TRANSACTION_CONTEXT_T *ctx, uint8_t profile)
{
    if (ctx == (SMBUS_TRANSACTION_CONTEXT_T *)0)
    {
        return;
    }

    if (profile == SMBUS_SLAVE_COMMAND_PROFILE_UBM_CONTROLLER)
    {
        ctx->command_profile = SMBUS_SLAVE_COMMAND_PROFILE_UBM_CONTROLLER;
    }
    else
    {
        ctx->command_profile = SMBUS_SLAVE_COMMAND_PROFILE_GENERIC;
    }

    SMBusUbmController_Init(&ctx->ubm);
}

uint8_t SMBusTransaction_GetProfile(const SMBUS_TRANSACTION_CONTEXT_T *ctx)
{
    if (ctx == (const SMBUS_TRANSACTION_CONTEXT_T *)0)
    {
        return SMBUS_SLAVE_COMMAND_PROFILE_GENERIC;
    }

    return ctx->command_profile;
}

const char *SMBusTransaction_GetProfileName(uint8_t profile)
{
    if (profile == SMBUS_SLAVE_COMMAND_PROFILE_UBM_CONTROLLER)
    {
        return "UBM controller";
    }

    return "Generic SMBus";
}

uint8_t SMBusTransaction_UsesSmbusPec(const SMBUS_TRANSACTION_CONTEXT_T *ctx)
{
    if (SMBusTransaction_GetProfile(ctx) == SMBUS_SLAVE_COMMAND_PROFILE_UBM_CONTROLLER)
    {
        return 0U;
    }

    return 1U;
}

uint8_t SMBusTransaction_GetCommandFlags(uint8_t command)
{
    const SMBUS_TRANSACTION_DESCRIPTOR_T *descriptor;
    uint8_t flags;

    descriptor = smbus_transaction_find_descriptor(command);
    if (descriptor == (const SMBUS_TRANSACTION_DESCRIPTOR_T *)0)
    {
        return 0U;
    }

    flags = descriptor->flags;
    if (descriptor->read_kind == SMBUS_TRANSACTION_RESPONSE_BYTE)
    {
        flags = (uint8_t)(flags | SMBUS_SLAVE_COMMAND_FLAG_READ_BYTE);
    }
    else if (descriptor->read_kind == SMBUS_TRANSACTION_RESPONSE_WORD)
    {
        flags = (uint8_t)(flags | SMBUS_SLAVE_COMMAND_FLAG_READ_WORD);
    }
    else if (descriptor->read_kind == SMBUS_TRANSACTION_RESPONSE_BLOCK)
    {
        flags = (uint8_t)(flags | SMBUS_SLAVE_COMMAND_FLAG_BLOCK_READ);
    }
    else
    {
        /* no read-side flags */
    }

    return flags;
}

uint8_t SMBusTransaction_IsProcessCallCommand(uint8_t command)
{
    if ((command == SMBUS_SLAVE_GENERIC_PROCESS_CALL_A) ||
        (command == SMBUS_SLAVE_GENERIC_PROCESS_CALL_B))
    {
        return 1U;
    }

    return 0U;
}

uint8_t SMBusTransaction_IsBlockWriteReadProcessCallCommand(uint8_t command)
{
    if ((command == SMBUS_SLAVE_GENERIC_BLOCK_PROC_CALL_A) ||
        (command == SMBUS_SLAVE_GENERIC_BLOCK_PROC_CALL_B))
    {
        return 1U;
    }

    return 0U;
}

SMBUS_TRANSACTION_PROTOCOL_T SMBusTransaction_DetectProfileProtocol(const SMBUS_TRANSACTION_CONTEXT_T *ctx,
                                                                    uint8_t command,
                                                                    uint8_t data_len,
                                                                    uint8_t repeated_start)
{
    if (SMBusTransaction_GetProfile(ctx) != SMBUS_SLAVE_COMMAND_PROFILE_UBM_CONTROLLER)
    {
        return SMBUS_TRANSACTION_PROTOCOL_UNKNOWN;
    }

    if ((repeated_start != 0U) &&
        (SMBusUbmController_IsReadRequest(command, data_len) != 0U))
    {
        return SMBUS_TRANSACTION_PROTOCOL_UBM_CONTROLLER_READ;
    }

    if ((repeated_start == 0U) &&
        (SMBusUbmController_IsWriteRequest(command, data_len) != 0U))
    {
        return SMBUS_TRANSACTION_PROTOCOL_UBM_CONTROLLER_WRITE;
    }

    return SMBUS_TRANSACTION_PROTOCOL_UNKNOWN;
}

uint8_t SMBusTransaction_GetReceiveByteCommand(const SMBUS_TRANSACTION_CONTEXT_T *ctx)
{
    if (ctx == (const SMBUS_TRANSACTION_CONTEXT_T *)0)
    {
        return SMBUS_SLAVE_GENERIC_RECEIVE_SELECT_A;
    }

    if (smbus_transaction_receive_index(ctx) == 1U)
    {
        return SMBUS_SLAVE_GENERIC_RECEIVE_SELECT_B;
    }

    return SMBUS_SLAVE_GENERIC_RECEIVE_SELECT_A;
}

const char *SMBusTransaction_GetCommandName(uint8_t command)
{
    switch (command)
    {
        case SMBUS_SLAVE_GENERIC_SEND_BYTE_A:
            return "SMB_EXAMPLE_SEND_BYTE_A";

        case SMBUS_SLAVE_GENERIC_SEND_BYTE_B:
            return "SMB_EXAMPLE_SEND_BYTE_B";

        case SMBUS_SLAVE_GENERIC_RECEIVE_SELECT_A:
            return "SMB_EXAMPLE_RECEIVE_SELECT_A";

        case SMBUS_SLAVE_GENERIC_RECEIVE_SELECT_B:
            return "SMB_EXAMPLE_RECEIVE_SELECT_B";

        case SMBUS_SLAVE_GENERIC_WRITE_BYTE_A:
            return "SMB_EXAMPLE_WRITE_BYTE_A";

        case SMBUS_SLAVE_GENERIC_WRITE_BYTE_B:
            return "SMB_EXAMPLE_WRITE_BYTE_B";

        case SMBUS_SLAVE_GENERIC_READ_BYTE_A:
            return "SMB_EXAMPLE_READ_BYTE_A";

        case SMBUS_SLAVE_GENERIC_READ_BYTE_B:
            return "SMB_EXAMPLE_READ_BYTE_B";

        case SMBUS_SLAVE_GENERIC_WRITE_WORD_A:
            return "SMB_EXAMPLE_WRITE_WORD_A";

        case SMBUS_SLAVE_GENERIC_WRITE_WORD_B:
            return "SMB_EXAMPLE_WRITE_WORD_B";

        case SMBUS_SLAVE_GENERIC_READ_WORD_A:
            return "SMB_EXAMPLE_READ_WORD_A";

        case SMBUS_SLAVE_GENERIC_READ_WORD_B:
            return "SMB_EXAMPLE_READ_WORD_B";

        case SMBUS_SLAVE_GENERIC_BLOCK_WRITE_A:
            return "SMB_EXAMPLE_BLOCK_WRITE_A";

        case SMBUS_SLAVE_GENERIC_BLOCK_WRITE_B:
            return "SMB_EXAMPLE_BLOCK_WRITE_B";

        case SMBUS_SLAVE_GENERIC_BLOCK_READ_A:
            return "SMB_EXAMPLE_BLOCK_READ_A";

        case SMBUS_SLAVE_GENERIC_BLOCK_READ_B:
            return "SMB_EXAMPLE_BLOCK_READ_B";

        case SMBUS_SLAVE_GENERIC_PROCESS_CALL_A:
            return "SMB_EXAMPLE_PROCESS_CALL_A";

        case SMBUS_SLAVE_GENERIC_PROCESS_CALL_B:
            return "SMB_EXAMPLE_PROCESS_CALL_B";

        case SMBUS_SLAVE_GENERIC_BLOCK_PROC_CALL_A:
            return "SMB_EXAMPLE_BLOCK_PROC_CALL_A";

        case SMBUS_SLAVE_GENERIC_BLOCK_PROC_CALL_B:
            return "SMB_EXAMPLE_BLOCK_PROC_CALL_B";

        default:
            break;
    }

    return (const char *)0;
}

const char *SMBusTransaction_GetCommandNameByProfile(uint8_t profile, uint8_t command)
{
    if (profile == SMBUS_SLAVE_COMMAND_PROFILE_UBM_CONTROLLER)
    {
        return SMBusUbmController_GetCommandName(command);
    }

    return SMBusTransaction_GetCommandName(command);
}

uint8_t SMBusTransaction_Execute(SMBUS_TRANSACTION_CONTEXT_T *ctx,
                                 const SMBUS_TRANSACTION_T *transaction,
                                 uint8_t *tx_buffer,
                                 uint8_t *tx_length)
{
    const SMBUS_TRANSACTION_DESCRIPTOR_T *descriptor;
    uint8_t index;
    uint8_t block_len;
    uint8_t copy_index;

    if ((ctx == (SMBUS_TRANSACTION_CONTEXT_T *)0) ||
        (transaction == (const SMBUS_TRANSACTION_T *)0) ||
        (tx_buffer == (uint8_t *)0) ||
        (tx_length == (uint8_t *)0))
    {
        return 0U;
    }

    *tx_length = 0U;
    if (ctx->command_profile == SMBUS_SLAVE_COMMAND_PROFILE_UBM_CONTROLLER)
    {
        return SMBusUbmController_Execute(&ctx->ubm,
                                          transaction->address_7bit,
                                          transaction,
                                          tx_buffer,
                                          tx_length);
    }

    if (transaction->protocol == SMBUS_TRANSACTION_PROTOCOL_RECEIVE_BYTE)
    {
        return smbus_transaction_execute_receive_byte(ctx, tx_buffer, tx_length);
    }

    descriptor = smbus_transaction_find_descriptor(transaction->command);
    if (descriptor == (const SMBUS_TRANSACTION_DESCRIPTOR_T *)0)
    {
        return 0U;
    }

    index = descriptor->index;
    switch (transaction->protocol)
    {
        case SMBUS_TRANSACTION_PROTOCOL_SEND_BYTE:
            ctx->last_send_byte = transaction->command;
            if ((transaction->command == SMBUS_SLAVE_GENERIC_RECEIVE_SELECT_A) ||
                (transaction->command == SMBUS_SLAVE_GENERIC_RECEIVE_SELECT_B))
            {
                ctx->receive_selector = transaction->command;
            }
            return 1U;

        case SMBUS_TRANSACTION_PROTOCOL_WRITE_BYTE:
            if ((index > 1U) || (transaction->data_len != 1U))
            {
                break;
            }
            ctx->write_byte_shadow[index] = transaction->payload[0];
            return 1U;

        case SMBUS_TRANSACTION_PROTOCOL_WRITE_WORD:
            if ((index > 1U) || (transaction->data_len != 2U))
            {
                break;
            }
            ctx->write_word_shadow[index] = smbus_transaction_load_word(transaction->payload);
            return 1U;

        case SMBUS_TRANSACTION_PROTOCOL_READ_BYTE:
            return smbus_transaction_execute_read_byte(ctx, index, tx_buffer, tx_length);

        case SMBUS_TRANSACTION_PROTOCOL_READ_WORD:
            return smbus_transaction_execute_read_word(ctx, index, tx_buffer, tx_length);

        case SMBUS_TRANSACTION_PROTOCOL_BLOCK_WRITE:
            if ((index > 1U) || (transaction->data_len == 0U))
            {
                break;
            }
            block_len = transaction->payload[0];
            if ((block_len > SMBUS_SLAVE_MAX_BLOCK_SIZE) ||
                (transaction->data_len != (uint8_t)(block_len + 1U)))
            {
                break;
            }
            ctx->block_shadow_length[index] = block_len;
            for (copy_index = 0U; copy_index < block_len; copy_index++)
            {
                ctx->block_shadow[index][copy_index] = transaction->payload[(uint8_t)(copy_index + 1U)];
            }
            return 1U;

        case SMBUS_TRANSACTION_PROTOCOL_BLOCK_READ:
            return smbus_transaction_execute_block_read(ctx, index, tx_buffer, tx_length);

        case SMBUS_TRANSACTION_PROTOCOL_PROCESS_CALL:
            return smbus_transaction_execute_process_call(ctx, transaction, index, tx_buffer, tx_length);

        case SMBUS_TRANSACTION_PROTOCOL_BLOCK_WRITE_READ_PROCESS_CALL:
            return smbus_transaction_execute_block_process_call(ctx, transaction, index, tx_buffer, tx_length);

        default:
            break;
    }

    return 0U;
}
