/*_____ I N C L U D E S ____________________________________________________*/
#include <stdio.h>
#include <string.h>
#include "NuMicro.h"
#include "smbus_slave_core.h"

/*_____ D E C L A R A T I O N S ____________________________________________*/

typedef enum
{
    SMBUS_PROTOCOL_NONE = 0,
    SMBUS_PROTOCOL_READ,
    SMBUS_PROTOCOL_BLOCK_WRITE,
    SMBUS_PROTOCOL_WRITE_BYTE,
    SMBUS_PROTOCOL_SEND_BYTE
} SMBUS_PROTOCOL_T;

/*_____ D E F I N I T I O N S ______________________________________________*/

#define SMBUS_EVENT_TIMEOUT                  (0x01U)
#define SMBUS_EVENT_BUS_ERROR                (0x02U)
#define SMBUS_EVENT_PEC_ERROR                (0x04U)
#define SMBUS_EVENT_UNSUPPORTED              (0x08U)
#define SMBUS_EVENT_WRITE_DONE               (0x10U)
#define SMBUS_EVENT_RECOVERED                (0x20U)
#if PMBUS_DEBUG_ENABLE
#define SMBUS_EVENT_DEBUG_RX                 (0x40U)
#define SMBUS_EVENT_DEBUG_TX                 (0x80U)
#endif

#define SMBUS_STATUS_WORD_CML                (0x0002U)

#if PMBUS_DEBUG_ENABLE
#define SMBUS_DEBUG_PROTOCOL_NONE            (0U)
#define SMBUS_DEBUG_PROTOCOL_SEND_BYTE       (1U)
#define SMBUS_DEBUG_PROTOCOL_WRITE_BYTE      (2U)
#define SMBUS_DEBUG_PROTOCOL_READ_BYTE       (4U)
#define SMBUS_DEBUG_PROTOCOL_READ_WORD       (5U)
#define SMBUS_DEBUG_PROTOCOL_BLOCK_WRITE     (7U)
#define SMBUS_DEBUG_PROTOCOL_BLOCK_READ      (8U)
#endif

static const char g_smbus_mfr_id[] = "MFR_ID_001";
static const char g_smbus_mfr_model[] = "MFR_MODEL_001";

/*_____ M A C R O S ________________________________________________________*/

#define SMBUS_ADDRESS_WRITE(addr7)           ((uint8_t)((addr7) << 1U))
#define SMBUS_ADDRESS_READ(addr7)            ((uint8_t)(((addr7) << 1U) | 0x01U))

/*_____ F U N C T I O N S __________________________________________________*/

static void smbus_set_event(SMBUS_SLAVE_CONTEXT_T *ctx, uint8_t event_id, uint8_t command, uint8_t status)
{
    ctx->event_flags = (uint8_t)(ctx->event_flags | event_id);
    ctx->last_event_command = command;
    ctx->last_event_status = status;
}

static const char *smbus_get_port_name_from_id(uint8_t port_id)
{
    switch (port_id)
    {
        case SMBUS_SLAVE_PORT_I2C1:
            return "I2C1";

        case SMBUS_SLAVE_PORT_I2C0:
            return "I2C0";

        case SMBUS_SLAVE_PORT_USCI0:
            return "USCI0";

        default:
            break;
    }

    return "?";
}

#if PMBUS_DEBUG_ENABLE
static const char *smbus_debug_get_command_name(uint8_t command)
{
    switch (command)
    {
        case SMBUS_SLAVE_PMBUS_CLEAR_FAULTS:
            return "CLEAR_FAULTS";

        case SMBUS_SLAVE_PMBUS_REVISION:
            return "PMBUS_REVISION";

        case SMBUS_SLAVE_PMBUS_MFR_ID:
            return "MFR_ID";

        case SMBUS_SLAVE_PMBUS_MFR_MODEL:
            return "MFR_MODEL";

        default:
            break;
    }

    return "UNKNOWN";
}

static const char *smbus_debug_get_protocol_name(uint8_t protocol)
{
    switch (protocol)
    {
        case SMBUS_DEBUG_PROTOCOL_SEND_BYTE:
            return "SEND_BYTE";

        case SMBUS_DEBUG_PROTOCOL_WRITE_BYTE:
            return "WRITE_BYTE";

        case SMBUS_DEBUG_PROTOCOL_READ_BYTE:
            return "READ_BYTE";

        case SMBUS_DEBUG_PROTOCOL_READ_WORD:
            return "READ_WORD";

        case SMBUS_DEBUG_PROTOCOL_BLOCK_WRITE:
            return "BLOCK_WRITE";

        case SMBUS_DEBUG_PROTOCOL_BLOCK_READ:
            return "BLOCK_READ";

        default:
            break;
    }

    return "UNKNOWN";
}

static uint8_t smbus_debug_read_protocol_from_flags(uint8_t flags)
{
    if ((flags & SMBUS_SLAVE_COMMAND_FLAG_READ_BYTE) != 0U)
    {
        return SMBUS_DEBUG_PROTOCOL_READ_BYTE;
    }

    if ((flags & SMBUS_SLAVE_COMMAND_FLAG_READ_WORD) != 0U)
    {
        return SMBUS_DEBUG_PROTOCOL_READ_WORD;
    }

    if ((flags & SMBUS_SLAVE_COMMAND_FLAG_BLOCK_READ) != 0U)
    {
        return SMBUS_DEBUG_PROTOCOL_BLOCK_READ;
    }

    return SMBUS_DEBUG_PROTOCOL_NONE;
}

static uint8_t smbus_debug_protocol_from_core(uint8_t protocol, uint8_t flags)
{
    switch (protocol)
    {
        case SMBUS_PROTOCOL_READ:
            return smbus_debug_read_protocol_from_flags(flags);

        case SMBUS_PROTOCOL_BLOCK_WRITE:
            return SMBUS_DEBUG_PROTOCOL_BLOCK_WRITE;

        case SMBUS_PROTOCOL_WRITE_BYTE:
            return SMBUS_DEBUG_PROTOCOL_WRITE_BYTE;

        case SMBUS_PROTOCOL_SEND_BYTE:
            return SMBUS_DEBUG_PROTOCOL_SEND_BYTE;

        default:
            break;
    }

    return SMBUS_DEBUG_PROTOCOL_NONE;
}

static void smbus_debug_capture_rx(SMBUS_SLAVE_CONTEXT_T *ctx,
                                   uint8_t command,
                                   uint8_t raw_length,
                                   uint8_t payload_length,
                                   uint8_t protocol,
                                   uint8_t repeated_start,
                                   uint8_t pec_present,
                                   uint8_t pec_valid)
{
#if PMBUS_DEBUG_PRINT_RX_FRAME
    uint8_t index;
    uint8_t capped_length;

    capped_length = raw_length;
    if (capped_length > SMBUS_SLAVE_RX_BUFFER_SIZE)
    {
        capped_length = SMBUS_SLAVE_RX_BUFFER_SIZE;
    }

    ctx->debug_rx_command = command;
    ctx->debug_rx_raw_length = capped_length;
    ctx->debug_rx_payload_length = payload_length;
    ctx->debug_rx_protocol = protocol;
    ctx->debug_rx_repeated_start = repeated_start;
    ctx->debug_rx_pec_present = pec_present;
    ctx->debug_rx_pec_valid = pec_valid;

    for (index = 0U; index < capped_length; index++)
    {
        ctx->debug_rx_raw[index] = ctx->rx_buffer[index];
    }

    ctx->event_flags = (uint8_t)(ctx->event_flags | SMBUS_EVENT_DEBUG_RX);
#else
    ctx = ctx;
    command = command;
    raw_length = raw_length;
    payload_length = payload_length;
    protocol = protocol;
    repeated_start = repeated_start;
    pec_present = pec_present;
    pec_valid = pec_valid;
#endif
}

static void smbus_debug_capture_tx(SMBUS_SLAVE_CONTEXT_T *ctx, uint8_t command, uint8_t protocol)
{
#if PMBUS_DEBUG_PRINT_TX_READY
    uint8_t index;
    uint8_t capped_length;

    capped_length = ctx->tx_length;
    if (capped_length > SMBUS_SLAVE_TX_BUFFER_SIZE)
    {
        capped_length = SMBUS_SLAVE_TX_BUFFER_SIZE;
    }

    ctx->debug_tx_command = command;
    ctx->debug_tx_protocol = protocol;
    ctx->debug_tx_length = capped_length;

    for (index = 0U; index < capped_length; index++)
    {
        ctx->debug_tx_raw[index] = ctx->tx_buffer[index];
    }

    ctx->event_flags = (uint8_t)(ctx->event_flags | SMBUS_EVENT_DEBUG_TX);
#else
    ctx = ctx;
    command = command;
    protocol = protocol;
#endif
}

static void smbus_debug_print_raw_bytes(const uint8_t *data, uint8_t length)
{
    uint8_t index;

    for (index = 0U; index < length; index++)
    {
        if (index != 0U)
        {
            printf(" ");
        }
        printf("%02X", (unsigned int)data[index]);
    }
}
#endif

static void smbus_set_ack(SMBUS_SLAVE_CONTEXT_T *ctx)
{
    ctx->ack_next = 1U;
}

static void smbus_clear_ack(SMBUS_SLAVE_CONTEXT_T *ctx)
{
    ctx->ack_next = 0U;
}

static void smbus_reset_rx(SMBUS_SLAVE_CONTEXT_T *ctx)
{
    ctx->rx_length = 0U;
    ctx->pending_read = 0U;
    ctx->command_length_without_pec = 0U;
}

static void smbus_reset_tx(SMBUS_SLAVE_CONTEXT_T *ctx)
{
    ctx->tx_length = 0U;
    ctx->tx_index = 0U;
}

static uint8_t smbus_pec_update(uint8_t crc, uint8_t data_byte)
{
    uint8_t bit_index;

    crc = (uint8_t)(crc ^ data_byte);
    for (bit_index = 0U; bit_index < 8U; bit_index++)
    {
        if ((crc & 0x80U) != 0U)
        {
            crc = (uint8_t)((crc << 1) ^ 0x07U);
        }
        else
        {
            crc = (uint8_t)(crc << 1);
        }
    }

    return crc;
}

static uint8_t smbus_compute_write_pec(SMBUS_SLAVE_CONTEXT_T *ctx, uint8_t frame_length_without_pec)
{
    uint8_t crc;
    uint8_t index;

    crc = 0U;
    crc = smbus_pec_update(crc, SMBUS_ADDRESS_WRITE(ctx->address_7bit));
    for (index = 0U; index < frame_length_without_pec; index++)
    {
        crc = smbus_pec_update(crc, ctx->rx_buffer[index]);
    }

    return crc;
}

static void smbus_append_read_pec(SMBUS_SLAVE_CONTEXT_T *ctx, uint8_t command_length)
{
    uint8_t crc;
    uint8_t index;

    if (ctx->tx_length >= SMBUS_SLAVE_TX_BUFFER_SIZE)
    {
        return;
    }

    crc = 0U;
    crc = smbus_pec_update(crc, SMBUS_ADDRESS_WRITE(ctx->address_7bit));
    for (index = 0U; index < command_length; index++)
    {
        crc = smbus_pec_update(crc, ctx->rx_buffer[index]);
    }

    crc = smbus_pec_update(crc, SMBUS_ADDRESS_READ(ctx->address_7bit));
    for (index = 0U; index < ctx->tx_length; index++)
    {
        crc = smbus_pec_update(crc, ctx->tx_buffer[index]);
    }

    ctx->tx_buffer[ctx->tx_length] = crc;
    ctx->tx_length = (uint8_t)(ctx->tx_length + 1U);
}

static uint8_t smbus_get_builtin_command_flags(uint8_t command)
{
    uint8_t flags;

    flags = 0U;
    switch (command)
    {
        case SMBUS_SLAVE_PMBUS_CLEAR_FAULTS:
            flags = SMBUS_SLAVE_COMMAND_FLAG_SEND_BYTE;
            break;

        case SMBUS_SLAVE_PMBUS_REVISION:
            flags = SMBUS_SLAVE_COMMAND_FLAG_READ_BYTE;
            break;

        case SMBUS_SLAVE_PMBUS_MFR_ID:
        case SMBUS_SLAVE_PMBUS_MFR_MODEL:
            flags = SMBUS_SLAVE_COMMAND_FLAG_BLOCK_READ;
            break;

        default:
            break;
    }

    return flags;
}

static uint8_t smbus_get_command_flags(SMBUS_SLAVE_CONTEXT_T *ctx, uint8_t command)
{
    uint8_t flags;
    uint8_t user_flags;

    flags = smbus_get_builtin_command_flags(command);
    user_flags = 0U;
    if (SMBusSlave_UserGetCommandFlags(ctx->port_id, command, &user_flags) != 0U)
    {
        flags = (uint8_t)(flags | user_flags);
    }

    return flags;
}

static uint8_t smbus_length_matches_protocol(SMBUS_SLAVE_CONTEXT_T *ctx, uint8_t flags, uint8_t length, uint8_t *protocol)
{
    uint8_t count;

    *protocol = (uint8_t)SMBUS_PROTOCOL_NONE;

    if ((length == 1U) &&
        ((flags & (SMBUS_SLAVE_COMMAND_FLAG_READ_BYTE |
                   SMBUS_SLAVE_COMMAND_FLAG_READ_WORD |
                   SMBUS_SLAVE_COMMAND_FLAG_BLOCK_READ)) != 0U))
    {
        *protocol = (uint8_t)SMBUS_PROTOCOL_READ;
        return 1U;
    }

    if ((length == 1U) && ((flags & SMBUS_SLAVE_COMMAND_FLAG_SEND_BYTE) != 0U))
    {
        *protocol = (uint8_t)SMBUS_PROTOCOL_SEND_BYTE;
        return 1U;
    }

    if ((length == 2U) && ((flags & SMBUS_SLAVE_COMMAND_FLAG_WRITE_BYTE) != 0U))
    {
        *protocol = (uint8_t)SMBUS_PROTOCOL_WRITE_BYTE;
        return 1U;
    }

    if ((length >= 2U) && ((flags & SMBUS_SLAVE_COMMAND_FLAG_BLOCK_WRITE) != 0U))
    {
        count = ctx->rx_buffer[1];
        if ((count <= SMBUS_SLAVE_MAX_BLOCK_SIZE) &&
            (length == (uint8_t)(count + 2U)))
        {
            *protocol = (uint8_t)SMBUS_PROTOCOL_BLOCK_WRITE;
            return 1U;
        }
    }

    return 0U;
}

static uint8_t smbus_select_effective_length(SMBUS_SLAVE_CONTEXT_T *ctx,
                                             uint8_t flags,
                                             uint8_t *effective_length,
                                             uint8_t *pec_present,
                                             uint8_t *pec_valid)
{
    uint8_t candidate_length;
    uint8_t protocol;
    uint8_t computed_pec;
    uint8_t last_byte;

    *effective_length = ctx->rx_length;
    *pec_present = 0U;
    *pec_valid = 1U;

    if (ctx->rx_length <= 1U)
    {
        return 1U;
    }

    candidate_length = (uint8_t)(ctx->rx_length - 1U);
    if (smbus_length_matches_protocol(ctx, flags, candidate_length, &protocol) == 0U)
    {
        return 1U;
    }

    computed_pec = smbus_compute_write_pec(ctx, candidate_length);
    last_byte = ctx->rx_buffer[ctx->rx_length - 1U];
    if (last_byte == computed_pec)
    {
        *effective_length = candidate_length;
        *pec_present = 1U;
        return 1U;
    }

    if (smbus_length_matches_protocol(ctx, flags, ctx->rx_length, &protocol) == 0U)
    {
        *effective_length = candidate_length;
        *pec_present = 1U;
        *pec_valid = 0U;
        return 0U;
    }

    return 1U;
}

static uint8_t smbus_builtin_read_byte(uint8_t command, uint8_t *value)
{
    if (command == SMBUS_SLAVE_PMBUS_REVISION)
    {
        *value = 0x33U;
        return 1U;
    }

    return 0U;
}

static uint8_t smbus_copy_block_string(const char *text, uint8_t *data, uint8_t *length)
{
    uint8_t copy_length;
    uint8_t index;

    copy_length = (uint8_t)strlen(text);
    if (copy_length > SMBUS_SLAVE_MAX_BLOCK_SIZE)
    {
        copy_length = SMBUS_SLAVE_MAX_BLOCK_SIZE;
    }

    for (index = 0U; index < copy_length; index++)
    {
        data[index] = (uint8_t)text[index];
    }

    *length = copy_length;
    return 1U;
}

static uint8_t smbus_builtin_block_read(uint8_t command, uint8_t *data, uint8_t *length)
{
    if (command == SMBUS_SLAVE_PMBUS_MFR_ID)
    {
        return smbus_copy_block_string(g_smbus_mfr_id, data, length);
    }

    if (command == SMBUS_SLAVE_PMBUS_MFR_MODEL)
    {
        return smbus_copy_block_string(g_smbus_mfr_model, data, length);
    }

    return 0U;
}

static uint8_t smbus_builtin_send_byte(SMBUS_SLAVE_CONTEXT_T *ctx, uint8_t command)
{
    if (command == SMBUS_SLAVE_PMBUS_CLEAR_FAULTS)
    {
        ctx->status_word = 0U;
        return 1U;
    }

    return 0U;
}

static void smbus_prepare_error_response(SMBUS_SLAVE_CONTEXT_T *ctx, uint8_t command, uint8_t status)
{
    ctx->status_word = (uint16_t)(ctx->status_word | SMBUS_STATUS_WORD_CML);
    ctx->tx_buffer[0] = 0x00U;
    ctx->tx_length = 1U;
    ctx->tx_index = 0U;
    smbus_set_event(ctx, SMBUS_EVENT_UNSUPPORTED, command, status);
}

static void smbus_prepare_read_response(SMBUS_SLAVE_CONTEXT_T *ctx, uint8_t status)
{
    uint8_t command;
    uint8_t flags;
    uint8_t length;
    uint8_t value;
    uint16_t word_value;
    uint8_t handled;
#if PMBUS_DEBUG_ENABLE
    uint8_t debug_protocol;
#endif

    command = ctx->rx_buffer[0];
    flags = smbus_get_command_flags(ctx, command);
    ctx->tx_length = 0U;
    ctx->tx_index = 0U;
    handled = 0U;
#if PMBUS_DEBUG_ENABLE
    debug_protocol = smbus_debug_read_protocol_from_flags(flags);
#endif

    if ((flags & SMBUS_SLAVE_COMMAND_FLAG_READ_BYTE) != 0U)
    {
        if ((smbus_builtin_read_byte(command, &value) != 0U) ||
            (SMBusSlave_UserReadByte(ctx->port_id, command, &value) != 0U))
        {
            ctx->tx_buffer[0] = value;
            ctx->tx_length = 1U;
            handled = 1U;
        }
    }

    if ((handled == 0U) && ((flags & SMBUS_SLAVE_COMMAND_FLAG_READ_WORD) != 0U))
    {
        if (SMBusSlave_UserReadWord(ctx->port_id, command, &word_value) != 0U)
        {
            ctx->tx_buffer[0] = (uint8_t)(word_value & 0x00FFU);
            ctx->tx_buffer[1] = (uint8_t)((word_value >> 8) & 0x00FFU);
            ctx->tx_length = 2U;
            handled = 1U;
        }
    }

    if ((handled == 0U) && ((flags & SMBUS_SLAVE_COMMAND_FLAG_BLOCK_READ) != 0U))
    {
        length = 0U;
        if ((smbus_builtin_block_read(command, &ctx->tx_buffer[1], &length) != 0U) ||
            (SMBusSlave_UserBlockRead(ctx->port_id, command, &ctx->tx_buffer[1], &length) != 0U))
        {
            if (length > SMBUS_SLAVE_MAX_BLOCK_SIZE)
            {
                length = SMBUS_SLAVE_MAX_BLOCK_SIZE;
            }
            ctx->tx_buffer[0] = length;
            ctx->tx_length = (uint8_t)(length + 1U);
            handled = 1U;
        }
    }

    if (handled == 0U)
    {
        smbus_prepare_error_response(ctx, command, status);
    }
    else
    {
        smbus_append_read_pec(ctx, ctx->command_length_without_pec);
    }

#if PMBUS_DEBUG_ENABLE
    smbus_debug_capture_tx(ctx, command, debug_protocol);
#endif
}

static void smbus_execute_write_protocol(SMBUS_SLAVE_CONTEXT_T *ctx,
                                         uint8_t command,
                                         uint8_t protocol,
                                         uint8_t effective_length,
                                         uint8_t pec_present,
                                         uint8_t pec_valid,
                                         uint8_t status)
{
    uint8_t handled;
    uint8_t count;

    handled = 0U;

    switch (protocol)
    {
        case SMBUS_PROTOCOL_SEND_BYTE:
            handled = smbus_builtin_send_byte(ctx, command);
            if (handled == 0U)
            {
                handled = SMBusSlave_UserSendByte(ctx->port_id, command, pec_present, pec_valid);
            }
            break;

        case SMBUS_PROTOCOL_WRITE_BYTE:
            handled = SMBusSlave_UserWriteByte(ctx->port_id, command, ctx->rx_buffer[1], pec_present, pec_valid);
            break;

        case SMBUS_PROTOCOL_BLOCK_WRITE:
            count = ctx->rx_buffer[1];
            if (effective_length == (uint8_t)(count + 2U))
            {
                handled = SMBusSlave_UserBlockWrite(ctx->port_id,
                                                    command,
                                                    &ctx->rx_buffer[2],
                                                    count,
                                                    pec_present,
                                                    pec_valid);
            }
            break;

        default:
            break;
    }

    if (handled == 0U)
    {
        smbus_prepare_error_response(ctx, command, status);
    }
    else
    {
        smbus_set_event(ctx, SMBUS_EVENT_WRITE_DONE, command, status);
    }
}

static void smbus_process_stop_or_restart(SMBUS_SLAVE_CONTEXT_T *ctx, uint8_t status)
{
    uint8_t command;
    uint8_t flags;
    uint8_t effective_length;
    uint8_t pec_present;
    uint8_t pec_valid;
    uint8_t protocol;
#if PMBUS_DEBUG_ENABLE
    uint8_t debug_payload_length;
    uint8_t debug_protocol;
    uint8_t debug_repeated_start;
#endif

    if (ctx->rx_length == 0U)
    {
        return;
    }

    command = ctx->rx_buffer[0];
    flags = smbus_get_command_flags(ctx, command);
    if (smbus_select_effective_length(ctx, flags, &effective_length, &pec_present, &pec_valid) == 0U)
    {
#if PMBUS_DEBUG_ENABLE
        debug_payload_length = 0U;
        if (effective_length > 0U)
        {
            debug_payload_length = (uint8_t)(effective_length - 1U);
        }
        smbus_debug_capture_rx(ctx,
                               command,
                               ctx->rx_length,
                               debug_payload_length,
                               SMBUS_DEBUG_PROTOCOL_NONE,
                               0U,
                               pec_present,
                               pec_valid);
#endif
        ctx->status_word = (uint16_t)(ctx->status_word | SMBUS_STATUS_WORD_CML);
        smbus_set_event(ctx, SMBUS_EVENT_PEC_ERROR, command, status);
        smbus_reset_rx(ctx);
        smbus_reset_tx(ctx);
        return;
    }

    if (smbus_length_matches_protocol(ctx, flags, effective_length, &protocol) == 0U)
    {
#if PMBUS_DEBUG_ENABLE
        debug_payload_length = 0U;
        if (effective_length > 0U)
        {
            debug_payload_length = (uint8_t)(effective_length - 1U);
        }
        smbus_debug_capture_rx(ctx,
                               command,
                               ctx->rx_length,
                               debug_payload_length,
                               SMBUS_DEBUG_PROTOCOL_NONE,
                               0U,
                               pec_present,
                               pec_valid);
#endif
        smbus_prepare_error_response(ctx, command, status);
        smbus_reset_rx(ctx);
        return;
    }

#if PMBUS_DEBUG_ENABLE
    debug_protocol = smbus_debug_protocol_from_core(protocol, flags);
    debug_payload_length = 0U;
    if (effective_length > 0U)
    {
        debug_payload_length = (uint8_t)(effective_length - 1U);
    }
    debug_repeated_start = 0U;
    if ((protocol == (uint8_t)SMBUS_PROTOCOL_READ) && (effective_length == 1U))
    {
        debug_repeated_start = 1U;
    }
    smbus_debug_capture_rx(ctx,
                           command,
                           ctx->rx_length,
                           debug_payload_length,
                           debug_protocol,
                           debug_repeated_start,
                           pec_present,
                           pec_valid);
#endif

    if ((protocol == (uint8_t)SMBUS_PROTOCOL_READ) && (effective_length == 1U))
    {
        ctx->pending_read = 1U;
        ctx->command_length_without_pec = effective_length;
        return;
    }

    smbus_execute_write_protocol(ctx, command, protocol, effective_length, pec_present, pec_valid, status);
    smbus_reset_rx(ctx);
}

static uint8_t smbus_load_next_tx_byte(SMBUS_SLAVE_CONTEXT_T *ctx)
{
    uint8_t next_byte;

    if (ctx->tx_index < ctx->tx_length)
    {
        next_byte = ctx->tx_buffer[ctx->tx_index];
        ctx->tx_index = (uint8_t)(ctx->tx_index + 1U);
    }
    else
    {
        next_byte = 0x00U;
    }

    return next_byte;
}

void SMBusSlave_CoreInit(SMBUS_SLAVE_CONTEXT_T *ctx, uint8_t port_id, uint8_t address_7bit, const char *port_name)
{
    memset(ctx->rx_buffer, 0, sizeof(ctx->rx_buffer));
    memset(ctx->tx_buffer, 0, sizeof(ctx->tx_buffer));

    ctx->rx_length = 0U;
    ctx->tx_length = 0U;
    ctx->tx_index = 0U;
    ctx->pending_read = 0U;
    ctx->command_length_without_pec = 0U;
    ctx->ack_next = 1U;
    ctx->event_flags = 0U;
    ctx->last_event_command = 0U;
    ctx->last_event_status = 0U;
    ctx->timeout_count = 0U;
    ctx->recover_pending = 0U;
#if PMBUS_DEBUG_ENABLE
    ctx->debug_rx_command = 0U;
    ctx->debug_rx_raw_length = 0U;
    ctx->debug_rx_payload_length = 0U;
    ctx->debug_rx_protocol = 0U;
    ctx->debug_rx_repeated_start = 0U;
    ctx->debug_rx_pec_present = 0U;
    ctx->debug_rx_pec_valid = 0U;
    memset(ctx->debug_rx_raw, 0, sizeof(ctx->debug_rx_raw));
    ctx->debug_tx_command = 0U;
    ctx->debug_tx_protocol = 0U;
    ctx->debug_tx_length = 0U;
    memset(ctx->debug_tx_raw, 0, sizeof(ctx->debug_tx_raw));
#endif
    ctx->status_word = 0U;
    ctx->port_id = port_id;
    ctx->address_7bit = address_7bit;
    ctx->port_name = port_name;
}

void SMBusSlave_CoreResetBus(SMBUS_SLAVE_CONTEXT_T *ctx)
{
    smbus_reset_rx(ctx);
    smbus_reset_tx(ctx);
    smbus_set_ack(ctx);
}

void SMBusSlave_CoreOnAddressWrite(SMBUS_SLAVE_CONTEXT_T *ctx)
{
    smbus_reset_rx(ctx);
    smbus_reset_tx(ctx);
    smbus_set_ack(ctx);
}

void SMBusSlave_CoreOnReceiveByte(SMBUS_SLAVE_CONTEXT_T *ctx, uint8_t data)
{
    if (ctx->rx_length < SMBUS_SLAVE_RX_BUFFER_SIZE)
    {
        ctx->rx_buffer[ctx->rx_length] = data;
        ctx->rx_length = (uint8_t)(ctx->rx_length + 1U);
        smbus_set_ack(ctx);
    }
    else
    {
        smbus_clear_ack(ctx);
    }
}

void SMBusSlave_CoreOnReceiveNack(SMBUS_SLAVE_CONTEXT_T *ctx)
{
    smbus_reset_rx(ctx);
    smbus_reset_tx(ctx);
    smbus_set_ack(ctx);
}

void SMBusSlave_CoreOnStopOrRestart(SMBUS_SLAVE_CONTEXT_T *ctx, uint8_t status)
{
    smbus_process_stop_or_restart(ctx, status);
    smbus_set_ack(ctx);
}

uint8_t SMBusSlave_CoreOnAddressRead(SMBUS_SLAVE_CONTEXT_T *ctx, uint8_t status)
{
    if ((ctx->pending_read != 0U) || (ctx->rx_length > 0U))
    {
        if (ctx->command_length_without_pec == 0U)
        {
            ctx->command_length_without_pec = ctx->rx_length;
        }
        smbus_prepare_read_response(ctx, status);
    }
    else
    {
        ctx->tx_buffer[0] = 0x00U;
        ctx->tx_length = 1U;
        ctx->tx_index = 0U;
    }

    smbus_set_ack(ctx);
    return smbus_load_next_tx_byte(ctx);
}

uint8_t SMBusSlave_CoreOnTransmitAck(SMBUS_SLAVE_CONTEXT_T *ctx)
{
    smbus_set_ack(ctx);
    return smbus_load_next_tx_byte(ctx);
}

void SMBusSlave_CoreOnTransmitDone(SMBUS_SLAVE_CONTEXT_T *ctx)
{
    smbus_reset_rx(ctx);
    smbus_reset_tx(ctx);
    smbus_set_ack(ctx);
}

void SMBusSlave_CoreOnTimeout(SMBUS_SLAVE_CONTEXT_T *ctx, uint8_t status)
{
    ctx->timeout_count = (uint8_t)(ctx->timeout_count + 1U);
    ctx->recover_pending = 1U;
    ctx->status_word = (uint16_t)(ctx->status_word | SMBUS_STATUS_WORD_CML);
    smbus_set_event(ctx, SMBUS_EVENT_TIMEOUT, 0U, status);
    smbus_reset_rx(ctx);
    smbus_reset_tx(ctx);
    smbus_set_ack(ctx);
}

void SMBusSlave_CoreOnBusError(SMBUS_SLAVE_CONTEXT_T *ctx, uint8_t status)
{
    ctx->recover_pending = 1U;
    ctx->status_word = (uint16_t)(ctx->status_word | SMBUS_STATUS_WORD_CML);
    smbus_set_event(ctx, SMBUS_EVENT_BUS_ERROR, 0U, status);
    smbus_reset_rx(ctx);
    smbus_reset_tx(ctx);
    smbus_set_ack(ctx);
}

void SMBusSlave_CoreOnIgnoredTimeout(SMBUS_SLAVE_CONTEXT_T *ctx)
{
    smbus_set_ack(ctx);
}

uint8_t SMBusSlave_CoreGetAck(const SMBUS_SLAVE_CONTEXT_T *ctx)
{
    return ctx->ack_next;
}

void SMBusSlave_CoreTakeEvents(SMBUS_SLAVE_CONTEXT_T *ctx, SMBUS_SLAVE_EVENT_SNAPSHOT_T *snapshot)
{
#if PMBUS_DEBUG_ENABLE
    uint8_t index;
#endif

    snapshot->events = ctx->event_flags;
    snapshot->command = ctx->last_event_command;
    snapshot->status = ctx->last_event_status;
    snapshot->timeout_count = ctx->timeout_count;
    snapshot->recover_pending = ctx->recover_pending;
#if PMBUS_DEBUG_ENABLE
    snapshot->debug_rx_command = ctx->debug_rx_command;
    snapshot->debug_rx_raw_length = ctx->debug_rx_raw_length;
    if (snapshot->debug_rx_raw_length > SMBUS_SLAVE_RX_BUFFER_SIZE)
    {
        snapshot->debug_rx_raw_length = SMBUS_SLAVE_RX_BUFFER_SIZE;
    }
    snapshot->debug_rx_payload_length = ctx->debug_rx_payload_length;
    snapshot->debug_rx_protocol = ctx->debug_rx_protocol;
    snapshot->debug_rx_repeated_start = ctx->debug_rx_repeated_start;
    snapshot->debug_rx_pec_present = ctx->debug_rx_pec_present;
    snapshot->debug_rx_pec_valid = ctx->debug_rx_pec_valid;
    for (index = 0U; index < snapshot->debug_rx_raw_length; index++)
    {
        snapshot->debug_rx_raw[index] = ctx->debug_rx_raw[index];
    }

    snapshot->debug_tx_command = ctx->debug_tx_command;
    snapshot->debug_tx_protocol = ctx->debug_tx_protocol;
    snapshot->debug_tx_length = ctx->debug_tx_length;
    if (snapshot->debug_tx_length > SMBUS_SLAVE_TX_BUFFER_SIZE)
    {
        snapshot->debug_tx_length = SMBUS_SLAVE_TX_BUFFER_SIZE;
    }
    for (index = 0U; index < snapshot->debug_tx_length; index++)
    {
        snapshot->debug_tx_raw[index] = ctx->debug_tx_raw[index];
    }
#endif
    ctx->event_flags = 0U;
    ctx->recover_pending = 0U;
}

void SMBusSlave_CoreSetRecovered(SMBUS_SLAVE_CONTEXT_T *ctx)
{
    smbus_set_event(ctx, SMBUS_EVENT_RECOVERED, 0U, 0U);
}

void SMBusSlave_CorePrintEvents(uint8_t port_id, const SMBUS_SLAVE_EVENT_SNAPSHOT_T *snapshot)
{
    const char *port_name;
#if PMBUS_DEBUG_ENABLE
    const char *command_name;
    const char *protocol_name;
#endif

    port_name = smbus_get_port_name_from_id(port_id);

#if PMBUS_DEBUG_ENABLE && PMBUS_DEBUG_PRINT_RX_FRAME
    if ((snapshot->events & SMBUS_EVENT_DEBUG_RX) != 0U)
    {
        command_name = smbus_debug_get_command_name(snapshot->debug_rx_command);
        protocol_name = smbus_debug_get_protocol_name(snapshot->debug_rx_protocol);
        printf("PMBus RX cmd=0x%02X ", (unsigned int)snapshot->debug_rx_command);
        printf("(%s) ", command_name);
        printf("bus=%s\r\n", port_name);
        printf("PMBus RX info raw=%u payload=%u proto=%u\r\n",
               (unsigned int)snapshot->debug_rx_raw_length,
               (unsigned int)snapshot->debug_rx_payload_length,
               (unsigned int)snapshot->debug_rx_protocol);
        printf("PMBus RX pec rs=%u pec=%u valid=%u\r\n",
               (unsigned int)snapshot->debug_rx_repeated_start,
               (unsigned int)snapshot->debug_rx_pec_present,
               (unsigned int)snapshot->debug_rx_pec_valid);
        printf("PMBus RX raw=");
        smbus_debug_print_raw_bytes(snapshot->debug_rx_raw, snapshot->debug_rx_raw_length);
        printf(" proto=%s bus=%s\r\n", protocol_name, port_name);
    }
#endif

#if PMBUS_DEBUG_ENABLE && PMBUS_DEBUG_PRINT_TX_READY
    if ((snapshot->events & SMBUS_EVENT_DEBUG_TX) != 0U)
    {
        command_name = smbus_debug_get_command_name(snapshot->debug_tx_command);
        protocol_name = smbus_debug_get_protocol_name(snapshot->debug_tx_protocol);
        printf("PMBus TX cmd=0x%02X ", (unsigned int)snapshot->debug_tx_command);
        printf("(%s) ", command_name);
        printf("proto=%u ", (unsigned int)snapshot->debug_tx_protocol);
        printf("(%s) ", protocol_name);
        printf("len=%u ", (unsigned int)snapshot->debug_tx_length);
        printf("bus=%s raw=", port_name);
        smbus_debug_print_raw_bytes(snapshot->debug_tx_raw, snapshot->debug_tx_length);
        printf("\r\n");
    }
#endif

    if ((snapshot->events & SMBUS_EVENT_TIMEOUT) != 0U)
    {
        printf("[SMBus %s] timeout, SCL low/stuck count=%u status=0x%02X\r\n",
               port_name,
               (unsigned int)snapshot->timeout_count,
               (unsigned int)snapshot->status);
    }
    if ((snapshot->events & SMBUS_EVENT_BUS_ERROR) != 0U)
    {
        printf("[SMBus %s] bus error status=0x%02X\r\n",
               port_name,
               (unsigned int)snapshot->status);
    }
    if ((snapshot->events & SMBUS_EVENT_PEC_ERROR) != 0U)
    {
        printf("[SMBus %s] PEC error cmd=0x%02X status=0x%02X\r\n",
               port_name,
               (unsigned int)snapshot->command,
               (unsigned int)snapshot->status);
    }
    if ((snapshot->events & SMBUS_EVENT_UNSUPPORTED) != 0U)
    {
        printf("[SMBus %s] unsupported cmd=0x%02X status=0x%02X\r\n",
               port_name,
               (unsigned int)snapshot->command,
               (unsigned int)snapshot->status);
    }
    if ((snapshot->events & SMBUS_EVENT_WRITE_DONE) != 0U)
    {
        printf("[SMBus %s] write done cmd=0x%02X\r\n",
               port_name,
               (unsigned int)snapshot->command);
    }
    if ((snapshot->events & SMBUS_EVENT_RECOVERED) != 0U)
    {
        printf("[SMBus %s] slave recovered\r\n", port_name);
    }
}

__WEAK uint8_t SMBusSlave_UserGetCommandFlags(uint8_t port_id, uint8_t command, uint8_t *flags)
{
    port_id = port_id;
    command = command;
    if (flags != 0)
    {
        *flags = 0U;
    }
    return 0U;
}

__WEAK uint8_t SMBusSlave_UserReadByte(uint8_t port_id, uint8_t command, uint8_t *value)
{
    port_id = port_id;
    command = command;
    value = value;
    return 0U;
}

__WEAK uint8_t SMBusSlave_UserReadWord(uint8_t port_id, uint8_t command, uint16_t *value)
{
    port_id = port_id;
    command = command;
    value = value;
    return 0U;
}

__WEAK uint8_t SMBusSlave_UserBlockRead(uint8_t port_id, uint8_t command, uint8_t *data, uint8_t *length)
{
    port_id = port_id;
    command = command;
    data = data;
    length = length;
    return 0U;
}

__WEAK uint8_t SMBusSlave_UserSendByte(uint8_t port_id, uint8_t command, uint8_t pec_present, uint8_t pec_valid)
{
    port_id = port_id;
    command = command;
    pec_present = pec_present;
    pec_valid = pec_valid;
    return 0U;
}

__WEAK uint8_t SMBusSlave_UserWriteByte(uint8_t port_id, uint8_t command, uint8_t value, uint8_t pec_present, uint8_t pec_valid)
{
    port_id = port_id;
    command = command;
    value = value;
    pec_present = pec_present;
    pec_valid = pec_valid;
    return 0U;
}

__WEAK uint8_t SMBusSlave_UserBlockWrite(uint8_t port_id,
                                         uint8_t command,
                                         const uint8_t *data,
                                         uint8_t length,
                                         uint8_t pec_present,
                                         uint8_t pec_valid)
{
    port_id = port_id;
    command = command;
    data = data;
    length = length;
    pec_present = pec_present;
    pec_valid = pec_valid;
    return 0U;
}

__WEAK void SMBusSlave_UserTimeoutError(uint8_t port_id)
{
    port_id = port_id;
}
