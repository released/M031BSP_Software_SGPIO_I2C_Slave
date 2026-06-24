#ifndef __SMBUS_SLAVE_CORE_H__
#define __SMBUS_SLAVE_CORE_H__

#include <stdint.h>
#include "smbus_slave.h"

#define SMBUS_SLAVE_RX_BUFFER_SIZE           (40U)
#define SMBUS_SLAVE_TX_BUFFER_SIZE           (34U)

#define SMBUS_STATUS_BUS_ERROR               (0x00U)
#define SMBUS_STATUS_TIMEOUT                 (0x20U)
#define SMBUS_STATUS_SLA_W_ACK               (0x60U)
#define SMBUS_STATUS_DATA_RX_ACK             (0x80U)
#define SMBUS_STATUS_DATA_RX_NACK            (0x88U)
#define SMBUS_STATUS_STOP_RESTART            (0xA0U)
#define SMBUS_STATUS_SLA_R_ACK               (0xA8U)
#define SMBUS_STATUS_DATA_TX_ACK             (0xB8U)
#define SMBUS_STATUS_DATA_TX_NACK            (0xC0U)
#define SMBUS_STATUS_LAST_TX_ACK             (0xC8U)

typedef struct
{
    volatile uint8_t rx_length;
    volatile uint8_t tx_length;
    volatile uint8_t tx_index;
    volatile uint8_t pending_read;
    volatile uint8_t command_length_without_pec;
    volatile uint8_t ack_next;
    volatile uint8_t event_flags;
    volatile uint8_t last_event_command;
    volatile uint8_t last_event_status;
    volatile uint8_t timeout_count;
    volatile uint8_t recover_pending;

#if PMBUS_DEBUG_ENABLE
    uint8_t debug_rx_command;
    uint8_t debug_rx_raw_length;
    uint8_t debug_rx_payload_length;
    uint8_t debug_rx_protocol;
    uint8_t debug_rx_repeated_start;
    uint8_t debug_rx_pec_present;
    uint8_t debug_rx_pec_valid;
    uint8_t debug_rx_raw[SMBUS_SLAVE_RX_BUFFER_SIZE];
    uint8_t debug_tx_command;
    uint8_t debug_tx_protocol;
    uint8_t debug_tx_length;
    uint8_t debug_tx_raw[SMBUS_SLAVE_TX_BUFFER_SIZE];
#endif

    uint8_t rx_buffer[SMBUS_SLAVE_RX_BUFFER_SIZE];
    uint8_t tx_buffer[SMBUS_SLAVE_TX_BUFFER_SIZE];
    uint16_t status_word;
    uint8_t port_id;
    uint8_t address_7bit;
    const char *port_name;
} SMBUS_SLAVE_CONTEXT_T;

typedef struct
{
    uint8_t events;
    uint8_t command;
    uint8_t status;
    uint8_t timeout_count;
    uint8_t recover_pending;
#if PMBUS_DEBUG_ENABLE
    uint8_t debug_rx_command;
    uint8_t debug_rx_raw_length;
    uint8_t debug_rx_payload_length;
    uint8_t debug_rx_protocol;
    uint8_t debug_rx_repeated_start;
    uint8_t debug_rx_pec_present;
    uint8_t debug_rx_pec_valid;
    uint8_t debug_rx_raw[SMBUS_SLAVE_RX_BUFFER_SIZE];
    uint8_t debug_tx_command;
    uint8_t debug_tx_protocol;
    uint8_t debug_tx_length;
    uint8_t debug_tx_raw[SMBUS_SLAVE_TX_BUFFER_SIZE];
#endif
} SMBUS_SLAVE_EVENT_SNAPSHOT_T;

void SMBusSlave_CoreInit(SMBUS_SLAVE_CONTEXT_T *ctx, uint8_t port_id, uint8_t address_7bit, const char *port_name);
void SMBusSlave_CoreResetBus(SMBUS_SLAVE_CONTEXT_T *ctx);
void SMBusSlave_CoreOnAddressWrite(SMBUS_SLAVE_CONTEXT_T *ctx);
void SMBusSlave_CoreOnReceiveByte(SMBUS_SLAVE_CONTEXT_T *ctx, uint8_t data);
void SMBusSlave_CoreOnReceiveNack(SMBUS_SLAVE_CONTEXT_T *ctx);
void SMBusSlave_CoreOnStopOrRestart(SMBUS_SLAVE_CONTEXT_T *ctx, uint8_t status);
uint8_t SMBusSlave_CoreOnAddressRead(SMBUS_SLAVE_CONTEXT_T *ctx, uint8_t status);
uint8_t SMBusSlave_CoreOnTransmitAck(SMBUS_SLAVE_CONTEXT_T *ctx);
void SMBusSlave_CoreOnTransmitDone(SMBUS_SLAVE_CONTEXT_T *ctx);
void SMBusSlave_CoreOnTimeout(SMBUS_SLAVE_CONTEXT_T *ctx, uint8_t status);
void SMBusSlave_CoreOnBusError(SMBUS_SLAVE_CONTEXT_T *ctx, uint8_t status);
void SMBusSlave_CoreOnIgnoredTimeout(SMBUS_SLAVE_CONTEXT_T *ctx);
uint8_t SMBusSlave_CoreGetAck(const SMBUS_SLAVE_CONTEXT_T *ctx);
void SMBusSlave_CoreTakeEvents(SMBUS_SLAVE_CONTEXT_T *ctx, SMBUS_SLAVE_EVENT_SNAPSHOT_T *snapshot);
void SMBusSlave_CoreSetRecovered(SMBUS_SLAVE_CONTEXT_T *ctx);
void SMBusSlave_CorePrintEvents(uint8_t port_id, const SMBUS_SLAVE_EVENT_SNAPSHOT_T *snapshot);

#endif
