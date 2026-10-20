#ifndef USBD_GS_USB_H
#define USBD_GS_USB_H

#include "usbd_def.h"

#define GS_USB_IN_EP 0x81U
#define GS_USB_OUT_EP 0x02U
#define GS_USB_PACKET_SIZE 64U

extern USBD_ClassTypeDef USBD_GS_USB;

/* Call from the main loop after GS_CAN_Poll(). Never waits for USB or CAN. */
void USBD_GS_USB_Poll(USBD_HandleTypeDef *pdev);

#endif
