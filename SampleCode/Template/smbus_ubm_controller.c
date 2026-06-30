/*_____ I N C L U D E S ____________________________________________________*/
#include "smbus_transaction.h"

/*_____ D E F I N I T I O N S ______________________________________________*/

#define SMBUS_UBM_STATUS_FAILED               (0x00U)
#define SMBUS_UBM_STATUS_SUCCESS              (0x01U)
#define SMBUS_UBM_STATUS_INVALID_CHECKSUM     (0x02U)
#define SMBUS_UBM_STATUS_TOO_MANY_BYTES       (0x03U)
#define SMBUS_UBM_STATUS_NO_ACCESS            (0x04U)
#define SMBUS_UBM_STATUS_CHANGE_MISMATCH      (0x05U)
#define SMBUS_UBM_STATUS_NOT_IMPLEMENTED      (0x07U)
#define SMBUS_UBM_STATUS_INVALID_INDEX        (0x08U)

#define SMBUS_UBM_OPERATIONAL_READY           (0x03U)
#define SMBUS_UBM_OPERATIONAL_REDUCED         (0x04U)

#define SMBUS_UBM_CMD_OPERATIONAL_STATE       (0x00U)
#define SMBUS_UBM_CMD_LAST_COMMAND_STATUS     (0x01U)
#define SMBUS_UBM_CMD_SILICON_ID              (0x02U)
#define SMBUS_UBM_CMD_UPDATE_CAPS             (0x03U)
#define SMBUS_UBM_CMD_ENTER_UPDATE            (0x20U)
#define SMBUS_UBM_CMD_PMDT                    (0x21U)
#define SMBUS_UBM_CMD_EXIT_UPDATE             (0x22U)
#define SMBUS_UBM_CMD_HFC_INFO                (0x30U)
#define SMBUS_UBM_CMD_BACKPLANE_INFO          (0x31U)
#define SMBUS_UBM_CMD_STARTING_SLOT           (0x32U)
#define SMBUS_UBM_CMD_CAPABILITIES            (0x33U)
#define SMBUS_UBM_CMD_FEATURES                (0x34U)
#define SMBUS_UBM_CMD_CHANGE_COUNT            (0x35U)
#define SMBUS_UBM_CMD_DFC_INDEX               (0x36U)
#define SMBUS_UBM_CMD_CCC                     (0x37U)
#define SMBUS_UBM_CMD_CCC_RESULT_INDEX        (0x38U)
#define SMBUS_UBM_CMD_DFC_DESCRIPTOR          (0x40U)
#define SMBUS_UBM_CMD_CCC_DESCRIPTOR          (0x41U)
#define SMBUS_UBM_CMD_FLEX_INDEX              (0x50U)
#define SMBUS_UBM_CMD_FLEX_DESCRIPTOR         (0x51U)
#define SMBUS_UBM_CMD_POWER_EVENT_DATA        (0x60U)

#define SMBUS_UBM_UPDATE_ADDRESS_7BIT         (0x5CU)
#define SMBUS_UBM_CHECKSUM_SEED               (0xA5U)

typedef struct
{
    uint8_t command;
    uint8_t read_len;
    uint8_t write_len;
    const char *name;
} SMBUS_UBM_DESCRIPTOR_T;

static const SMBUS_UBM_DESCRIPTOR_T g_smbus_ubm_descriptors[] =
{
    { SMBUS_UBM_CMD_OPERATIONAL_STATE,   1U,  0U, "UBM_OPERATIONAL_STATE" },
    { SMBUS_UBM_CMD_LAST_COMMAND_STATUS, 1U,  0U, "UBM_LAST_COMMAND_STATUS" },
    { SMBUS_UBM_CMD_SILICON_ID,         14U,  0U, "UBM_SILICON_IDENTITY_VERSION" },
    { SMBUS_UBM_CMD_UPDATE_CAPS,         1U,  0U, "UBM_PROGRAMMING_UPDATE_MODE_CAPABILITIES" },
    { SMBUS_UBM_CMD_ENTER_UPDATE,        5U,  5U, "UBM_ENTER_PROGRAMMABLE_UPDATE_MODE" },
    { SMBUS_UBM_CMD_PMDT,                8U, 32U, "UBM_PROGRAMMABLE_MODE_DATA_TRANSFER" },
    { SMBUS_UBM_CMD_EXIT_UPDATE,         4U,  4U, "UBM_EXIT_PROGRAMMABLE_UPDATE_MODE" },
    { SMBUS_UBM_CMD_HFC_INFO,            1U,  0U, "UBM_HOST_FACING_CONNECTOR_INFO" },
    { SMBUS_UBM_CMD_BACKPLANE_INFO,      1U,  0U, "UBM_BACKPLANE_INFO" },
    { SMBUS_UBM_CMD_STARTING_SLOT,       1U,  0U, "UBM_STARTING_SLOT" },
    { SMBUS_UBM_CMD_CAPABILITIES,        2U,  0U, "UBM_CAPABILITIES" },
    { SMBUS_UBM_CMD_FEATURES,            2U,  2U, "UBM_FEATURES" },
    { SMBUS_UBM_CMD_CHANGE_COUNT,        2U,  2U, "UBM_CHANGE_COUNT" },
    { SMBUS_UBM_CMD_DFC_INDEX,           1U,  1U, "UBM_DFC_STATUS_CONTROL_DESCRIPTOR_INDEX" },
    { SMBUS_UBM_CMD_CCC,                 1U,  1U, "UBM_CABLE_CONTIGUOUS_CHECK" },
    { SMBUS_UBM_CMD_CCC_RESULT_INDEX,    1U,  1U, "UBM_CCC_RESULT_INDEX" },
    { SMBUS_UBM_CMD_DFC_DESCRIPTOR,      8U,  8U, "UBM_DFC_STATUS_CONTROL_DESCRIPTOR" },
    { SMBUS_UBM_CMD_CCC_DESCRIPTOR,     35U,  0U, "UBM_CCC_RESULT_DESCRIPTOR" },
    { SMBUS_UBM_CMD_FLEX_INDEX,          1U,  1U, "UBM_FLEX_IO_DESCRIPTOR_INDEX" },
    { SMBUS_UBM_CMD_FLEX_DESCRIPTOR,     5U,  5U, "UBM_FLEX_IO_DESCRIPTOR" },
    { SMBUS_UBM_CMD_POWER_EVENT_DATA,   32U,  0U, "UBM_POWER_EVENT_DATA" }
};

/*_____ F U N C T I O N S __________________________________________________*/

static const SMBUS_UBM_DESCRIPTOR_T *smbus_ubm_find_descriptor(uint8_t command)
{
    uint8_t index;
    uint8_t count;

    count = (uint8_t)(sizeof(g_smbus_ubm_descriptors) / sizeof(g_smbus_ubm_descriptors[0]));
    for (index = 0U; index < count; index++)
    {
        if (g_smbus_ubm_descriptors[index].command == command)
        {
            return &g_smbus_ubm_descriptors[index];
        }
    }

    return (const SMBUS_UBM_DESCRIPTOR_T *)0;
}

static uint8_t smbus_ubm_checksum(const uint8_t *bytes, uint8_t length)
{
    uint8_t index;
    uint8_t sum;

    sum = SMBUS_UBM_CHECKSUM_SEED;
    for (index = 0U; index < length; index++)
    {
        sum = (uint8_t)(sum + bytes[index]);
    }

    return (uint8_t)(0U - sum);
}

static uint8_t smbus_ubm_validate_command_checksum(uint8_t address_7bit, uint8_t command, uint8_t checksum)
{
    uint8_t frame[2];

    frame[0] = SMBUS_SLAVE_ADDR_7BIT_TO_WRITE(address_7bit);
    frame[1] = command;

    return (uint8_t)(smbus_ubm_checksum(frame, 2U) == checksum);
}

static uint8_t smbus_ubm_validate_write_checksum(uint8_t address_7bit,
                                                 uint8_t command,
                                                 const uint8_t *data,
                                                 uint8_t length,
                                                 uint8_t checksum)
{
    uint8_t frame[SMBUS_SLAVE_MAX_BLOCK_SIZE + 2U];
    uint8_t index;
    uint8_t frame_len;

    frame[0] = SMBUS_SLAVE_ADDR_7BIT_TO_WRITE(address_7bit);
    frame[1] = command;
    frame_len = 2U;
    for (index = 0U; index < length; index++)
    {
        frame[frame_len] = data[index];
        frame_len = (uint8_t)(frame_len + 1U);
    }

    return (uint8_t)(smbus_ubm_checksum(frame, frame_len) == checksum);
}

static void smbus_ubm_append_read_checksum(uint8_t *tx_buffer, uint8_t *tx_length)
{
    uint8_t checksum;

    checksum = smbus_ubm_checksum(tx_buffer, *tx_length);
    tx_buffer[*tx_length] = checksum;
    *tx_length = (uint8_t)(*tx_length + 1U);
}

void SMBusUbmController_Init(SMBUS_UBM_CONTROLLER_CONTEXT_T *ctx)
{
    uint8_t index;

    if (ctx == (SMBUS_UBM_CONTROLLER_CONTEXT_T *)0)
    {
        return;
    }

    ctx->operational_state = SMBUS_UBM_OPERATIONAL_READY;
    ctx->last_command_status = SMBUS_UBM_STATUS_SUCCESS;
    ctx->features[0] = 0x00U;
    ctx->features[1] = 0x00U;
    ctx->change_count[0] = 0x01U;
    ctx->change_count[1] = 0x00U;
    ctx->dfc_index = 0U;
    ctx->ccc_control = 0U;
    ctx->ccc_index = 0U;
    ctx->flex_index = 0U;
    ctx->pmdt_length = 0U;

    for (index = 0U; index < SMBUS_UBM_CONTROLLER_DFC_DESCRIPTOR_SIZE; index++)
    {
        ctx->dfc_descriptors[0][index] = 0U;
        ctx->dfc_descriptors[1][index] = 0U;
    }
    ctx->dfc_descriptors[0][0] = 0x01U;
    ctx->dfc_descriptors[0][1] = 0x10U;
    ctx->dfc_descriptors[0][5] = 0x01U;
    ctx->dfc_descriptors[1][0] = 0x01U;
    ctx->dfc_descriptors[1][1] = 0x11U;
    ctx->dfc_descriptors[1][5] = 0x01U;

    for (index = 0U; index < SMBUS_UBM_CONTROLLER_CCC_DESCRIPTOR_SIZE; index++)
    {
        ctx->ccc_descriptor[index] = (uint8_t)(0xA0U + index);
    }
    for (index = 0U; index < SMBUS_UBM_CONTROLLER_FLEX_DESCRIPTOR_SIZE; index++)
    {
        ctx->flex_descriptor[index] = (uint8_t)(0x10U + index);
    }
    for (index = 0U; index < SMBUS_UBM_CONTROLLER_POWER_EVENT_SIZE; index++)
    {
        ctx->power_event[index] = (uint8_t)(0x60U + index);
        ctx->pmdt_shadow[index] = 0U;
    }
}

uint8_t SMBusUbmController_IsKnownCommand(uint8_t command)
{
    return (uint8_t)(smbus_ubm_find_descriptor(command) != (const SMBUS_UBM_DESCRIPTOR_T *)0);
}

uint8_t SMBusUbmController_IsReadRequest(uint8_t command, uint8_t data_len)
{
    const SMBUS_UBM_DESCRIPTOR_T *descriptor;

    descriptor = smbus_ubm_find_descriptor(command);
    if (descriptor == (const SMBUS_UBM_DESCRIPTOR_T *)0)
    {
        return 0U;
    }

    if ((data_len == 1U) && (descriptor->read_len > 0U))
    {
        return 1U;
    }

    return 0U;
}

uint8_t SMBusUbmController_IsWriteRequest(uint8_t command, uint8_t data_len)
{
    const SMBUS_UBM_DESCRIPTOR_T *descriptor;

    descriptor = smbus_ubm_find_descriptor(command);
    if (descriptor == (const SMBUS_UBM_DESCRIPTOR_T *)0)
    {
        return 0U;
    }

    if ((data_len > 0U) && (descriptor->write_len > 0U))
    {
        return 1U;
    }

    return 0U;
}

const char *SMBusUbmController_GetCommandName(uint8_t command)
{
    const SMBUS_UBM_DESCRIPTOR_T *descriptor;

    descriptor = smbus_ubm_find_descriptor(command);
    if (descriptor != (const SMBUS_UBM_DESCRIPTOR_T *)0)
    {
        return descriptor->name;
    }

    return "UBM_RESERVED_OR_VENDOR";
}

static uint8_t smbus_ubm_prepare_read(SMBUS_UBM_CONTROLLER_CONTEXT_T *ctx,
                                      uint8_t address_7bit,
                                      const struct smbus_transaction_s *transaction,
                                      uint8_t *tx_buffer,
                                      uint8_t *tx_length)
{
    const SMBUS_UBM_DESCRIPTOR_T *descriptor;
    uint8_t index;

    descriptor = smbus_ubm_find_descriptor(transaction->command);
    if (descriptor == (const SMBUS_UBM_DESCRIPTOR_T *)0)
    {
        return 0U;
    }

    if (smbus_ubm_validate_command_checksum(address_7bit,
                                            transaction->command,
                                            transaction->payload[0]) == 0U)
    {
        ctx->last_command_status = SMBUS_UBM_STATUS_INVALID_CHECKSUM;
        return 0U;
    }

    *tx_length = 0U;
    switch (transaction->command)
    {
        case SMBUS_UBM_CMD_OPERATIONAL_STATE:
            tx_buffer[0] = ctx->operational_state;
            *tx_length = 1U;
            break;

        case SMBUS_UBM_CMD_LAST_COMMAND_STATUS:
            tx_buffer[0] = ctx->last_command_status;
            *tx_length = 1U;
            break;

        case SMBUS_UBM_CMD_SILICON_ID:
            tx_buffer[0] = 0x15U;
            tx_buffer[1] = 0xABU;
            tx_buffer[2] = 0x10U;
            tx_buffer[3] = 0x00U;
            tx_buffer[4] = 0x32U;
            tx_buffer[5] = 0x00U;
            tx_buffer[6] = 0x00U;
            tx_buffer[7] = 0x00U;
            tx_buffer[8] = 0x00U;
            tx_buffer[9] = 0x00U;
            tx_buffer[10] = 0x00U;
            tx_buffer[11] = 0x01U;
            tx_buffer[12] = 0x55U;
            tx_buffer[13] = 0x42U;
            *tx_length = 14U;
            break;

        case SMBUS_UBM_CMD_UPDATE_CAPS:
            tx_buffer[0] = 0x01U;
            *tx_length = 1U;
            break;

        case SMBUS_UBM_CMD_ENTER_UPDATE:
            tx_buffer[0] = SMBUS_SLAVE_ADDR_7BIT_TO_WRITE(SMBUS_UBM_UPDATE_ADDRESS_7BIT);
            tx_buffer[1] = 0x55U;
            tx_buffer[2] = 0x42U;
            tx_buffer[3] = 0x4DU;
            tx_buffer[4] = 0x00U;
            *tx_length = 5U;
            break;

        case SMBUS_UBM_CMD_PMDT:
            tx_buffer[0] = 0x00U;
            tx_buffer[1] = ctx->pmdt_length;
            for (index = 0U; index < 6U; index++)
            {
                tx_buffer[(uint8_t)(index + 2U)] = ctx->pmdt_shadow[index];
            }
            *tx_length = 8U;
            break;

        case SMBUS_UBM_CMD_EXIT_UPDATE:
            tx_buffer[0] = 0x55U;
            tx_buffer[1] = 0x42U;
            tx_buffer[2] = 0x4DU;
            tx_buffer[3] = 0x00U;
            *tx_length = 4U;
            break;

        case SMBUS_UBM_CMD_HFC_INFO:
            tx_buffer[0] = 0x01U;
            *tx_length = 1U;
            break;

        case SMBUS_UBM_CMD_BACKPLANE_INFO:
            tx_buffer[0] = 0x01U;
            *tx_length = 1U;
            break;

        case SMBUS_UBM_CMD_STARTING_SLOT:
            tx_buffer[0] = 0x00U;
            *tx_length = 1U;
            break;

        case SMBUS_UBM_CMD_CAPABILITIES:
            tx_buffer[0] = 0x80U;
            tx_buffer[1] = 0x60U;
            *tx_length = 2U;
            break;

        case SMBUS_UBM_CMD_FEATURES:
            tx_buffer[0] = ctx->features[0];
            tx_buffer[1] = ctx->features[1];
            *tx_length = 2U;
            break;

        case SMBUS_UBM_CMD_CHANGE_COUNT:
            tx_buffer[0] = ctx->change_count[0];
            tx_buffer[1] = ctx->change_count[1];
            *tx_length = 2U;
            break;

        case SMBUS_UBM_CMD_DFC_INDEX:
            tx_buffer[0] = ctx->dfc_index;
            *tx_length = 1U;
            break;

        case SMBUS_UBM_CMD_CCC:
            tx_buffer[0] = ctx->ccc_control;
            *tx_length = 1U;
            break;

        case SMBUS_UBM_CMD_CCC_RESULT_INDEX:
            tx_buffer[0] = ctx->ccc_index;
            *tx_length = 1U;
            break;

        case SMBUS_UBM_CMD_DFC_DESCRIPTOR:
            if (ctx->dfc_index >= SMBUS_UBM_CONTROLLER_DESCRIPTOR_COUNT)
            {
                ctx->last_command_status = SMBUS_UBM_STATUS_INVALID_INDEX;
                return 0U;
            }
            for (index = 0U; index < SMBUS_UBM_CONTROLLER_DFC_DESCRIPTOR_SIZE; index++)
            {
                tx_buffer[index] = ctx->dfc_descriptors[ctx->dfc_index][index];
            }
            *tx_length = SMBUS_UBM_CONTROLLER_DFC_DESCRIPTOR_SIZE;
            break;

        case SMBUS_UBM_CMD_CCC_DESCRIPTOR:
            for (index = 0U; index < SMBUS_UBM_CONTROLLER_CCC_DESCRIPTOR_SIZE; index++)
            {
                tx_buffer[index] = ctx->ccc_descriptor[index];
            }
            *tx_length = SMBUS_UBM_CONTROLLER_CCC_DESCRIPTOR_SIZE;
            break;

        case SMBUS_UBM_CMD_FLEX_INDEX:
            tx_buffer[0] = ctx->flex_index;
            *tx_length = 1U;
            break;

        case SMBUS_UBM_CMD_FLEX_DESCRIPTOR:
            for (index = 0U; index < SMBUS_UBM_CONTROLLER_FLEX_DESCRIPTOR_SIZE; index++)
            {
                tx_buffer[index] = ctx->flex_descriptor[index];
            }
            *tx_length = SMBUS_UBM_CONTROLLER_FLEX_DESCRIPTOR_SIZE;
            break;

        case SMBUS_UBM_CMD_POWER_EVENT_DATA:
            for (index = 0U; index < SMBUS_UBM_CONTROLLER_POWER_EVENT_SIZE; index++)
            {
                tx_buffer[index] = ctx->power_event[index];
            }
            *tx_length = SMBUS_UBM_CONTROLLER_POWER_EVENT_SIZE;
            break;

        default:
            return 0U;
    }

    if (*tx_length != descriptor->read_len)
    {
        return 0U;
    }

    smbus_ubm_append_read_checksum(tx_buffer, tx_length);
    return 1U;
}

static uint8_t smbus_ubm_execute_write(SMBUS_UBM_CONTROLLER_CONTEXT_T *ctx,
                                       uint8_t address_7bit,
                                       const struct smbus_transaction_s *transaction)
{
    const SMBUS_UBM_DESCRIPTOR_T *descriptor;
    uint8_t data_len;
    uint8_t checksum;
    uint8_t index;

    descriptor = smbus_ubm_find_descriptor(transaction->command);
    if (descriptor == (const SMBUS_UBM_DESCRIPTOR_T *)0)
    {
        ctx->last_command_status = SMBUS_UBM_STATUS_NOT_IMPLEMENTED;
        return 1U;
    }
    if (descriptor->write_len == 0U)
    {
        ctx->last_command_status = SMBUS_UBM_STATUS_NO_ACCESS;
        return 1U;
    }
    if (transaction->data_len == 0U)
    {
        ctx->last_command_status = SMBUS_UBM_STATUS_FAILED;
        return 1U;
    }

    data_len = (uint8_t)(transaction->data_len - 1U);
    checksum = transaction->payload[data_len];
    if ((transaction->command == SMBUS_UBM_CMD_PMDT) && (data_len > descriptor->write_len))
    {
        ctx->last_command_status = SMBUS_UBM_STATUS_TOO_MANY_BYTES;
        return 1U;
    }
    if ((transaction->command != SMBUS_UBM_CMD_PMDT) && (data_len != descriptor->write_len))
    {
        if (data_len > descriptor->write_len)
        {
            ctx->last_command_status = SMBUS_UBM_STATUS_TOO_MANY_BYTES;
        }
        else
        {
            ctx->last_command_status = SMBUS_UBM_STATUS_FAILED;
        }
        return 1U;
    }
    if (smbus_ubm_validate_write_checksum(address_7bit,
                                          transaction->command,
                                          transaction->payload,
                                          data_len,
                                          checksum) == 0U)
    {
        ctx->last_command_status = SMBUS_UBM_STATUS_INVALID_CHECKSUM;
        return 1U;
    }

    switch (transaction->command)
    {
        case SMBUS_UBM_CMD_ENTER_UPDATE:
            if ((transaction->payload[1] == 0x55U) &&
                (transaction->payload[2] == 0x42U) &&
                (transaction->payload[3] == 0x4DU) &&
                ((transaction->payload[4] & 0x01U) != 0U))
            {
                ctx->operational_state = SMBUS_UBM_OPERATIONAL_REDUCED;
            }
            ctx->last_command_status = SMBUS_UBM_STATUS_SUCCESS;
            break;

        case SMBUS_UBM_CMD_PMDT:
            ctx->pmdt_length = data_len;
            for (index = 0U; index < data_len; index++)
            {
                ctx->pmdt_shadow[index] = transaction->payload[index];
            }
            ctx->last_command_status = SMBUS_UBM_STATUS_SUCCESS;
            break;

        case SMBUS_UBM_CMD_EXIT_UPDATE:
            if ((transaction->payload[0] == 0x55U) &&
                (transaction->payload[1] == 0x42U) &&
                (transaction->payload[2] == 0x4DU) &&
                ((transaction->payload[3] & 0x01U) != 0U))
            {
                ctx->operational_state = SMBUS_UBM_OPERATIONAL_READY;
            }
            ctx->last_command_status = SMBUS_UBM_STATUS_SUCCESS;
            break;

        case SMBUS_UBM_CMD_FEATURES:
            ctx->features[0] = transaction->payload[0];
            ctx->features[1] = transaction->payload[1];
            ctx->last_command_status = SMBUS_UBM_STATUS_SUCCESS;
            break;

        case SMBUS_UBM_CMD_CHANGE_COUNT:
            if ((transaction->payload[0] == ctx->change_count[0]) &&
                (transaction->payload[1] == ctx->change_count[1]))
            {
                ctx->last_command_status = SMBUS_UBM_STATUS_SUCCESS;
            }
            else
            {
                ctx->last_command_status = SMBUS_UBM_STATUS_CHANGE_MISMATCH;
            }
            break;

        case SMBUS_UBM_CMD_DFC_INDEX:
            if (transaction->payload[0] >= SMBUS_UBM_CONTROLLER_DESCRIPTOR_COUNT)
            {
                ctx->last_command_status = SMBUS_UBM_STATUS_INVALID_INDEX;
            }
            else
            {
                ctx->dfc_index = transaction->payload[0];
                ctx->last_command_status = SMBUS_UBM_STATUS_SUCCESS;
            }
            break;

        case SMBUS_UBM_CMD_CCC:
            ctx->ccc_control = transaction->payload[0];
            ctx->last_command_status = SMBUS_UBM_STATUS_SUCCESS;
            break;

        case SMBUS_UBM_CMD_CCC_RESULT_INDEX:
            ctx->ccc_index = transaction->payload[0];
            ctx->last_command_status = SMBUS_UBM_STATUS_SUCCESS;
            break;

        case SMBUS_UBM_CMD_DFC_DESCRIPTOR:
            if (ctx->dfc_index >= SMBUS_UBM_CONTROLLER_DESCRIPTOR_COUNT)
            {
                ctx->last_command_status = SMBUS_UBM_STATUS_INVALID_INDEX;
            }
            else
            {
                for (index = 0U; index < SMBUS_UBM_CONTROLLER_DFC_DESCRIPTOR_SIZE; index++)
                {
                    ctx->dfc_descriptors[ctx->dfc_index][index] = transaction->payload[index];
                }
                ctx->last_command_status = SMBUS_UBM_STATUS_SUCCESS;
            }
            break;

        case SMBUS_UBM_CMD_FLEX_INDEX:
            ctx->flex_index = transaction->payload[0];
            ctx->last_command_status = SMBUS_UBM_STATUS_SUCCESS;
            break;

        case SMBUS_UBM_CMD_FLEX_DESCRIPTOR:
            for (index = 0U; index < SMBUS_UBM_CONTROLLER_FLEX_DESCRIPTOR_SIZE; index++)
            {
                ctx->flex_descriptor[index] = transaction->payload[index];
            }
            ctx->last_command_status = SMBUS_UBM_STATUS_SUCCESS;
            break;

        default:
            ctx->last_command_status = SMBUS_UBM_STATUS_NOT_IMPLEMENTED;
            break;
    }

    return 1U;
}

uint8_t SMBusUbmController_Execute(SMBUS_UBM_CONTROLLER_CONTEXT_T *ctx,
                                   uint8_t address_7bit,
                                   const struct smbus_transaction_s *transaction,
                                   uint8_t *tx_buffer,
                                   uint8_t *tx_length)
{
    if ((ctx == (SMBUS_UBM_CONTROLLER_CONTEXT_T *)0) ||
        (transaction == (const struct smbus_transaction_s *)0) ||
        (tx_buffer == (uint8_t *)0) ||
        (tx_length == (uint8_t *)0))
    {
        return 0U;
    }

    *tx_length = 0U;
    if (transaction->protocol == SMBUS_TRANSACTION_PROTOCOL_UBM_CONTROLLER_READ)
    {
        return smbus_ubm_prepare_read(ctx, address_7bit, transaction, tx_buffer, tx_length);
    }

    if (transaction->protocol == SMBUS_TRANSACTION_PROTOCOL_UBM_CONTROLLER_WRITE)
    {
        return smbus_ubm_execute_write(ctx, address_7bit, transaction);
    }

    return 0U;
}
