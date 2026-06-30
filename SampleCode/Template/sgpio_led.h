#ifndef __SGPIO_LED_H__
#define __SGPIO_LED_H__

#include <stdint.h>
#include "sgpio_slave.h"

#define SGPIO_LED_GROUP_OD0_3             (0U)
#define SGPIO_LED_GROUP_OD4_7             (1U)

void SGPIO_LED_SetGroupSelect(uint8_t group_select);
void SGPIO_LED_Init(void);
void SGPIO_LED_Process(void);
void SGPIO_LED_OnFrameDecoded(const SGPIO_FRAME_T *frame);

#endif
