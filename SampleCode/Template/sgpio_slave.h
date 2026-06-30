#ifndef __SGPIO_SLAVE_H__
#define __SGPIO_SLAVE_H__

#include <stdint.h>
#include "NuMicro.h"

/*
 * SGPIO wiring:
 *   SCLOCK    -> PA2 GPIO input, shared GPIO ISR rising sampler
 *   SDATA OUT -> PA0 GPIO input, sampled by SCLOCK
 *   SLOAD     -> PA3 GPIO input, sampled by SCLOCK
 */
#define SGPIO_SLAVE_SLOAD_PORT           (PA)
#define SGPIO_SLAVE_SLOAD_PIN_NUM        (3UL)
#define SGPIO_SLAVE_SLOAD_PIN_MASK       (BIT3)

#define SGPIO_SLAVE_SDOUT_PORT           (PA)
#define SGPIO_SLAVE_SDOUT_PIN_NUM        (0UL)
#define SGPIO_SLAVE_SDOUT_PIN_MASK       (BIT0)

#define SGPIO_SLAVE_SCLK_PORT            (PA)
#define SGPIO_SLAVE_SCLK_PIN_NUM         (2UL)
#define SGPIO_SLAVE_SCLK_PIN_MASK        (BIT2)

#define SGPIO_SLAVE_GPIO_IRQn            (GPIO_PAPBPGPH_IRQn)
#define SGPIO_SLAVE_GPIO_IRQHandler      GPABGH_IRQHandler

#define SGPIO_SLAVE_SLOAD_PIN_NAME       "PA3"
#define SGPIO_SLAVE_SDOUT_PIN_NAME       "PA0"
#define SGPIO_SLAVE_SCLK_PIN_NAME        "PA2"

#define SGPIO_SLAVE_SLOAD_IO             ((uint8_t)(((GPIO_GET_IN_DATA(SGPIO_SLAVE_SLOAD_PORT) & SGPIO_SLAVE_SLOAD_PIN_MASK) != 0UL) ? 1U : 0U))
#define SGPIO_SLAVE_SDOUT_IO             ((uint8_t)(((GPIO_GET_IN_DATA(SGPIO_SLAVE_SDOUT_PORT) & SGPIO_SLAVE_SDOUT_PIN_MASK) != 0UL) ? 1U : 0U))

#define SGPIO_SLAVE_MAX_SLOTS            (16U)
#define SGPIO_SLAVE_RX_MAX_BYTES         (8U)
#define SGPIO_SLAVE_DATA_BITS_PER_SLOT   (3U)

typedef struct
{
    uint32_t frame_count;
    uint32_t dropped_frames;
    uint16_t bit_count;
    uint16_t act_mask;
    uint16_t locate_mask;
    uint16_t fail_mask;
    uint8_t raw[SGPIO_SLAVE_RX_MAX_BYTES];
    uint8_t raw_len;
    uint8_t sload_raw;
    uint8_t sload_raw_valid;
    uint8_t low_sync_count;
    uint8_t overflow;
    uint8_t valid;
} SGPIO_FRAME_T;

typedef void (*SGPIO_FRAME_DECODED_CALLBACK_T)(const SGPIO_FRAME_T *frame);

void SGPIO_SetFrameDecodedCallback(SGPIO_FRAME_DECODED_CALLBACK_T callback);
void SGPIO_Init(void);
void SGPIO_Process(void);
void SGPIO_OnClockRisingSampledIrq(uint8_t sload_value, uint8_t sdata_value);

#endif
