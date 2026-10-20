#ifndef GS_USB_PROTOCOL_H
#define GS_USB_PROTOCOL_H

#include <stdint.h>

/* Wire ABI shared with drivers/net/can/usb/gs_usb.c in the Linux kernel.
 * All multibyte values are little endian; echo_id is returned unchanged. */
enum {
    GS_USB_BREQ_HOST_FORMAT = 0,
    GS_USB_BREQ_BITTIMING = 1,
    GS_USB_BREQ_MODE = 2,
    GS_USB_BREQ_BT_CONST = 4,
    GS_USB_BREQ_DEVICE_CONFIG = 5
};

#define GS_CAN_MODE_RESET 0U
#define GS_CAN_MODE_START 1U
#define GS_CAN_MODE_LISTEN_ONLY (1U << 0)
#define GS_CAN_MODE_LOOP_BACK (1U << 1)
#define GS_CAN_FEATURES (GS_CAN_MODE_LISTEN_ONLY | GS_CAN_MODE_LOOP_BACK)
#define GS_CAN_FLAG_OVERFLOW 1U
#define GS_ECHO_ID_RX UINT32_MAX
#define GS_CAN_EFF_FLAG 0x80000000U
#define GS_CAN_RTR_FLAG 0x40000000U
#define GS_CAN_ERR_FLAG 0x20000000U
#define GS_CAN_EFF_MASK 0x1FFFFFFFU
#define GS_CAN_SFF_MASK 0x7FFU

typedef struct {
    uint32_t echo_id;
    uint32_t can_id;
    uint8_t can_dlc;
    uint8_t channel;
    uint8_t flags;
    uint8_t reserved;
    uint8_t data[8];
} GS_HostFrame;

typedef struct {
    uint32_t prop_seg;
    uint32_t phase_seg1;
    uint32_t phase_seg2;
    uint32_t sjw;
    uint32_t brp;
} GS_BitTiming;

typedef struct {
    uint32_t mode;
    uint32_t flags;
} GS_DeviceMode;

typedef struct {
    uint8_t reserved[3];
    uint8_t icount; /* Last channel index, not the number of channels. */
    uint32_t sw_version;
    uint32_t hw_version;
} GS_DeviceConfig;

typedef struct {
    uint32_t feature;
    uint32_t fclk_can;
    uint32_t tseg1_min;
    uint32_t tseg1_max;
    uint32_t tseg2_min;
    uint32_t tseg2_max;
    uint32_t sjw_max;
    uint32_t brp_min;
    uint32_t brp_max;
    uint32_t brp_inc;
} GS_BitTimingConst;

_Static_assert(sizeof(GS_HostFrame) == 20, "gs_usb Classic CAN frame ABI");
_Static_assert(sizeof(GS_BitTiming) == 20, "gs_usb bit timing ABI");
_Static_assert(sizeof(GS_DeviceMode) == 8, "gs_usb mode ABI");
_Static_assert(sizeof(GS_DeviceConfig) == 12, "gs_usb device configuration ABI");
_Static_assert(sizeof(GS_BitTimingConst) == 40, "gs_usb timing limits ABI");

#endif
