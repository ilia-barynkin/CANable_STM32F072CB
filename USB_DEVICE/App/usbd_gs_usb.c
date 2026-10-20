#ifdef CANABLE_GS_USB

#include "usbd_gs_usb.h"
#include "gs_can.h"
#include "usbd_core.h"
#include "usbd_ctlreq.h"
#include "usbd_ioreq.h"

#include <string.h>

#define GS_USB_CONFIG_SIZE 32U
#define GS_USB_VENDOR_OUT 0x41U
#define GS_USB_VENDOR_IN 0xC1U
#define GS_USB_HOST_BYTE_ORDER 0x0000BEEFU

/* Aligned storage also leaves room for a malformed full-size EP0 packet. */
typedef union {
    uint32_t words[USB_MAX_EP0_SIZE / sizeof(uint32_t)];
    GS_BitTiming timing;
    GS_DeviceMode mode;
    GS_DeviceConfig config;
    GS_BitTimingConst timing_const;
} GS_ControlBuffer;

typedef struct {
    GS_ControlBuffer control;
    USBD_SetupReqTypedef control_request;
    uint32_t out_buffer[GS_USB_PACKET_SIZE / sizeof(uint32_t)];
    GS_HostFrame out_frame;
    GS_HostFrame in_frame;
    uint8_t control_pending;
    uint8_t out_pending;
    uint8_t out_armed;
    uint8_t in_pending;
    uint8_t in_busy;
} GS_USB_State;

static GS_USB_State state;

/* Older Linux gs_usb drivers use fixed endpoint numbers 1 IN and 2 OUT. */
static uint8_t configuration[GS_USB_CONFIG_SIZE] = {
    9, USB_DESC_TYPE_CONFIGURATION, GS_USB_CONFIG_SIZE, 0,
    1, 1, 0, 0x80, 50,
    9, USB_DESC_TYPE_INTERFACE, 0, 0, 2, 0xFF, 0, 0, 0,
    7, USB_DESC_TYPE_ENDPOINT, GS_USB_OUT_EP, USBD_EP_TYPE_BULK,
    GS_USB_PACKET_SIZE, 0, 0,
    7, USB_DESC_TYPE_ENDPOINT, GS_USB_IN_EP, USBD_EP_TYPE_BULK,
    GS_USB_PACKET_SIZE, 0, 0
};

static uint8_t arm_out(USBD_HandleTypeDef *pdev)
{
    uint8_t status = USBD_LL_PrepareReceive(pdev, GS_USB_OUT_EP,
        (uint8_t *)state.out_buffer, sizeof(state.out_buffer));
    state.out_armed = (status == USBD_OK);
    return status;
}

static uint8_t control_error(USBD_HandleTypeDef *pdev,
                             USBD_SetupReqTypedef *request)
{
    state.control_pending = 0;
    pdev->ep0_state = USBD_EP0_STALL;
    USBD_CtlError(pdev, request);
    return USBD_FAIL;
}

static uint8_t gs_init(USBD_HandleTypeDef *pdev, uint8_t configuration_index)
{
    (void)configuration_index;
    GS_CAN_Disconnect();
    memset(&state, 0, sizeof(state));
    if (pdev->dev_speed != USBD_SPEED_FULL) {
        return USBD_FAIL;
    }
    if (USBD_LL_OpenEP(pdev, GS_USB_IN_EP, USBD_EP_TYPE_BULK,
                      GS_USB_PACKET_SIZE) != USBD_OK) {
        return USBD_FAIL;
    }
    if (USBD_LL_OpenEP(pdev, GS_USB_OUT_EP, USBD_EP_TYPE_BULK,
                      GS_USB_PACKET_SIZE) != USBD_OK) {
        USBD_LL_CloseEP(pdev, GS_USB_IN_EP);
        return USBD_FAIL;
    }
    pdev->ep_in[GS_USB_IN_EP & 0x0FU].is_used = 1;
    pdev->ep_out[GS_USB_OUT_EP & 0x0FU].is_used = 1;
    pdev->pClassData = &state;
    if (arm_out(pdev) != USBD_OK) {
        USBD_LL_CloseEP(pdev, GS_USB_IN_EP);
        USBD_LL_CloseEP(pdev, GS_USB_OUT_EP);
        pdev->ep_in[GS_USB_IN_EP & 0x0FU].is_used = 0;
        pdev->ep_out[GS_USB_OUT_EP & 0x0FU].is_used = 0;
        pdev->pClassData = NULL;
        return USBD_FAIL;
    }
    return USBD_OK;
}

static uint8_t gs_deinit(USBD_HandleTypeDef *pdev, uint8_t configuration_index)
{
    (void)configuration_index;
    pdev->pClassData = NULL;
    USBD_LL_CloseEP(pdev, GS_USB_IN_EP);
    USBD_LL_CloseEP(pdev, GS_USB_OUT_EP);
    pdev->ep_in[GS_USB_IN_EP & 0x0FU].is_used = 0;
    pdev->ep_out[GS_USB_OUT_EP & 0x0FU].is_used = 0;
    memset(&state, 0, sizeof(state));
    /* USB reset and disconnect reach this callback too. Keep SysTick enabled
     * while the backend waits for CAN to enter its reset state. */
    GS_CAN_Disconnect();
    return USBD_OK;
}

static uint8_t gs_setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *request)
{
    uint16_t length = 0;
    uint16_t value = 0;
    uint8_t direction = GS_USB_VENDOR_OUT;

    /* A new SETUP packet cancels any previous unfinished control transfer. */
    state.control_pending = 0;
    if (pdev->pClassData != &state ||
        pdev->dev_state != USBD_STATE_CONFIGURED || request->wIndex != 0) {
        return control_error(pdev, request);
    }

    if ((request->bmRequest & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_STANDARD) {
        state.control.words[0] = 0;
        if (request->bmRequest == 0x81U && request->wValue == 0) {
            if (request->bRequest == USB_REQ_GET_STATUS && request->wLength == 2) {
                return USBD_CtlSendData(pdev, (uint8_t *)&state.control, 2);
            }
            if (request->bRequest == USB_REQ_GET_INTERFACE && request->wLength == 1) {
                return USBD_CtlSendData(pdev, (uint8_t *)&state.control, 1);
            }
        }
        if (request->bmRequest == 0x01U &&
            request->bRequest == USB_REQ_SET_INTERFACE &&
            request->wValue == 0 && request->wLength == 0) {
            return USBD_OK; /* The core sends the status stage. */
        }
        return control_error(pdev, request);
    }

    switch (request->bRequest) {
    case GS_USB_BREQ_HOST_FORMAT:
        length = sizeof(uint32_t);
        value = 1; /* Device-wide requests use 1, not the channel index. */
        break;
    case GS_USB_BREQ_BITTIMING:
        length = sizeof(GS_BitTiming);
        break;
    case GS_USB_BREQ_MODE:
        length = sizeof(GS_DeviceMode);
        break;
    case GS_USB_BREQ_BT_CONST:
        length = sizeof(GS_BitTimingConst);
        direction = GS_USB_VENDOR_IN;
        break;
    case GS_USB_BREQ_DEVICE_CONFIG:
        length = sizeof(GS_DeviceConfig);
        value = 1;
        direction = GS_USB_VENDOR_IN;
        break;
    default:
        return control_error(pdev, request);
    }
    if (request->bmRequest != direction || request->wValue != value ||
        request->wLength != length) {
        return control_error(pdev, request);
    }

    memset(&state.control, 0, sizeof(state.control));
    if (direction == GS_USB_VENDOR_OUT) {
        state.control_request = *request;
        state.control_pending = 1;
        return USBD_CtlPrepareRx(pdev, (uint8_t *)&state.control, length);
    }
    if (request->bRequest == GS_USB_BREQ_DEVICE_CONFIG) {
        state.control.config.icount = 0; /* One channel, numbered zero. */
        state.control.config.sw_version = 1;
        state.control.config.hw_version = 1;
    } else {
        state.control.timing_const = (GS_BitTimingConst) {
            .feature = GS_CAN_FEATURES,
            .fclk_can = HAL_RCC_GetPCLK1Freq(),
            .tseg1_min = 1, .tseg1_max = 16,
            .tseg2_min = 1, .tseg2_max = 8,
            .sjw_max = 4,
            .brp_min = 1, .brp_max = 1024, .brp_inc = 1
        };
    }
    return USBD_CtlSendData(pdev, (uint8_t *)&state.control, length);
}

static void reset_transfers(USBD_HandleTypeDef *pdev)
{
    PCD_HandleTypeDef *hpcd = (PCD_HandleTypeDef *)pdev->pData;
    /* Abort preserves data toggles; reopening endpoints here would lose USB
     * synchronization. On F072, FlushEP alone does not cancel a PMA transfer. */
    HAL_PCD_EP_Abort(hpcd, GS_USB_IN_EP);
    HAL_PCD_EP_Abort(hpcd, GS_USB_OUT_EP);
    /* Abort only sets NAK. Discard completions from before MODE RESET before
     * rearming OUT; clearing CTR also preserves the endpoint data toggles. */
    PCD_CLEAR_TX_EP_CTR(hpcd->Instance, GS_USB_IN_EP & 0x0FU);
    PCD_CLEAR_RX_EP_CTR(hpcd->Instance, GS_USB_OUT_EP & 0x0FU);
    state.out_pending = 0;
    state.out_armed = 0;
    state.in_pending = 0;
    state.in_busy = 0;
    memset(&state.out_frame, 0, sizeof(state.out_frame));
    memset(&state.in_frame, 0, sizeof(state.in_frame));
    arm_out(pdev);
}

static uint8_t gs_ep0_rx_ready(USBD_HandleTypeDef *pdev)
{
    bool valid = false;
    if (pdev->pClassData != &state || !state.control_pending) {
        return control_error(pdev, &pdev->request);
    }
    state.control_pending = 0;
    if (USBD_LL_GetRxDataSize(pdev, 0) != state.control_request.wLength) {
        return control_error(pdev, &state.control_request);
    }
    switch (state.control_request.bRequest) {
    case GS_USB_BREQ_HOST_FORMAT:
        valid = (state.control.words[0] == GS_USB_HOST_BYTE_ORDER);
        break;
    case GS_USB_BREQ_BITTIMING:
        valid = GS_CAN_SetBitTiming(&state.control.timing);
        break;
    case GS_USB_BREQ_MODE:
        valid = GS_CAN_SetMode(state.control.mode.mode, state.control.mode.flags);
        if (valid && state.control.mode.mode == GS_CAN_MODE_RESET) {
            reset_transfers(pdev);
        }
        break;
    default:
        break;
    }
    if (!valid) {
        /* The core must honor this return value instead of sending status. */
        return control_error(pdev, &state.control_request);
    }
    return USBD_OK;
}

static uint8_t gs_data_out(USBD_HandleTypeDef *pdev, uint8_t endpoint)
{
    if (pdev->pClassData != &state || endpoint != (GS_USB_OUT_EP & 0x0FU)) {
        return USBD_FAIL;
    }
    state.out_armed = 0;
    if (USBD_LL_GetRxDataSize(pdev, endpoint) == sizeof(GS_HostFrame)) {
        memcpy(&state.out_frame, state.out_buffer, sizeof(state.out_frame));
        /* Old Linux drivers leave these two bytes uninitialized on TX. */
        state.out_frame.flags = 0;
        state.out_frame.reserved = 0;
        if (!GS_CAN_Enqueue(&state.out_frame)) {
            /* Leave OUT NAKed until Poll can put this frame in the CAN queue. */
            state.out_pending = 1;
            return USBD_OK;
        }
    }
    /* A malformed USB packet must not wedge subsequent transfers. */
    return arm_out(pdev);
}

static uint8_t gs_data_in(USBD_HandleTypeDef *pdev, uint8_t endpoint)
{
    if (pdev->pClassData != &state || endpoint != (GS_USB_IN_EP & 0x0FU)) {
        return USBD_FAIL;
    }
    state.in_busy = 0;
    state.in_pending = 0;
    return USBD_OK;
}

static uint8_t *gs_configuration(uint16_t *length)
{
    *length = sizeof(configuration);
    return configuration;
}

USBD_ClassTypeDef USBD_GS_USB = {
    .Init = gs_init,
    .DeInit = gs_deinit,
    .Setup = gs_setup,
    .EP0_RxReady = gs_ep0_rx_ready,
    .DataIn = gs_data_in,
    .DataOut = gs_data_out,
    .GetFSConfigDescriptor = gs_configuration
};

void USBD_GS_USB_Poll(USBD_HandleTypeDef *pdev)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    /* Serialize the short queue copies and endpoint arms with USB reset and
     * callbacks. No timeout-based HAL operation runs with interrupts masked. */
    if (pdev->pClassData == &state && pdev->dev_state == USBD_STATE_CONFIGURED) {
        if (state.out_pending && GS_CAN_Enqueue(&state.out_frame)) {
            state.out_pending = 0;
        }
        if (!state.out_pending && !state.out_armed) {
            arm_out(pdev);
        }
        if (!state.in_busy) {
            if (!state.in_pending) {
                state.in_pending = GS_CAN_Dequeue(&state.in_frame);
            }
            if (state.in_pending) {
                state.in_busy = 1;
                if (USBD_LL_Transmit(pdev, GS_USB_IN_EP,
                    (uint8_t *)&state.in_frame, sizeof(state.in_frame)) != USBD_OK) {
                    state.in_busy = 0;
                }
                /* Keep in_frame untouched until the completion callback. */
            }
        }
    }
    __set_PRIMASK(primask);
}

#endif /* CANABLE_GS_USB */
