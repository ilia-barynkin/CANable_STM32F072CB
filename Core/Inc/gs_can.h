#ifndef GS_CAN_H
#define GS_CAN_H

#include <stdbool.h>
#include "gs_usb_protocol.h"

/* Init runs before USB connects. Poll runs in the main loop. CAN starts only
 * after a host MODE START request. Control calls also run in USB IRQ context;
 * SysTick must have higher priority so HAL timeouts can expire. */
void GS_CAN_Init(void);
void GS_CAN_Poll(void);
bool GS_CAN_SetBitTiming(const GS_BitTiming *timing);
bool GS_CAN_SetMode(uint32_t mode, uint32_t flags);
void GS_CAN_Disconnect(void);

/* False means backpressure: keep the USB packet and retry from the main loop.
 * Invalid/stopped-channel packets are consumed without sending onto the bus. */
bool GS_CAN_Enqueue(const GS_HostFrame *frame);
/* Returns either a received CAN frame or a completed TX echo, with echo
 * completions taking priority. The caller owns a copy until USB completes. */
bool GS_CAN_Dequeue(GS_HostFrame *frame);

#endif
