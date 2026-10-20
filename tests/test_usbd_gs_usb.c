#include "usbd_gs_usb.h"
#include "gs_can.h"
#include "usbd_core.h"
#include "usbd_ctlreq.h"
#include "usbd_ioreq.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static USBD_HandleTypeDef device;
static USB_TypeDef usb_registers;
static PCD_HandleTypeDef pcd;
static struct {
    uint8_t *out_buffer;
    uint8_t *control_buffer;
    const uint8_t *in_buffer;
    uint8_t control_reply[64];
    uint16_t control_length;
    uint16_t in_length;
    uint32_t rx_length[16];
    unsigned open_count, close_count, arm_count, transmit_count;
    unsigned stall_count, status_count, abort_count, disconnect_count;
    unsigned enqueue_count, dequeue_count, timing_count, mode_count;
    unsigned fail_open_call;
    char endpoint_events[64];
    unsigned endpoint_event_count;
    uint32_t irq_mask;
    bool accept_enqueue, accept_timing, accept_mode;
    USBD_StatusTypeDef arm_result, transmit_result;
    GS_HostFrame enqueued, queued[4];
    unsigned queue_head, queue_tail;
    GS_BitTiming timing;
    GS_DeviceMode mode;
} fake;

static void endpoint_event(char event)
{
    CHECK(fake.endpoint_event_count < sizeof(fake.endpoint_events));
    fake.endpoint_events[fake.endpoint_event_count++] = event;
}

static void clear_backend_queue(void)
{
    fake.queue_head = 0;
    fake.queue_tail = 0;
}

void GS_CAN_Disconnect(void)
{
    ++fake.disconnect_count;
    clear_backend_queue();
}

bool GS_CAN_SetBitTiming(const GS_BitTiming *timing)
{
    ++fake.timing_count;
    fake.timing = *timing;
    return fake.accept_timing;
}

bool GS_CAN_SetMode(uint32_t mode, uint32_t flags)
{
    ++fake.mode_count;
    fake.mode = (GS_DeviceMode) { mode, flags };
    if (fake.accept_mode && mode == GS_CAN_MODE_RESET) {
        clear_backend_queue();
    }
    return fake.accept_mode;
}

bool GS_CAN_Enqueue(const GS_HostFrame *frame)
{
    ++fake.enqueue_count;
    fake.enqueued = *frame;
    return fake.accept_enqueue;
}

bool GS_CAN_Dequeue(GS_HostFrame *frame)
{
    ++fake.dequeue_count;
    if (fake.queue_head == fake.queue_tail) {
        return false;
    }
    *frame = fake.queued[fake.queue_head++];
    return true;
}

uint32_t HAL_RCC_GetPCLK1Freq(void) { return 48000000; }
uint32_t __get_PRIMASK(void) { return fake.irq_mask; }
void __disable_irq(void) { fake.irq_mask = 1; }
void __set_PRIMASK(uint32_t mask) { fake.irq_mask = mask; }

HAL_StatusTypeDef HAL_PCD_EP_Abort(PCD_HandleTypeDef *handle, uint8_t endpoint)
{
    CHECK(handle == &pcd);
    CHECK(endpoint == 0x81 || endpoint == 0x02);
    ++fake.abort_count;
    endpoint_event(endpoint == 0x81 ? 'I' : 'O');
    return HAL_OK;
}

void test_clear_tx_ctr(USB_TypeDef *instance, uint8_t endpoint)
{
    CHECK(instance == &usb_registers && endpoint == 1);
    endpoint_event('i');
}

void test_clear_rx_ctr(USB_TypeDef *instance, uint8_t endpoint)
{
    CHECK(instance == &usb_registers && endpoint == 2);
    endpoint_event('o');
}

USBD_StatusTypeDef USBD_LL_OpenEP(USBD_HandleTypeDef *dev, uint8_t endpoint,
                                 uint8_t type, uint16_t size)
{
    CHECK(dev == &device);
    CHECK(size == 64);
    CHECK((endpoint == 0x81 || endpoint == 0x02) ?
          type == USBD_EP_TYPE_BULK :
          ((endpoint == 0 || endpoint == 0x80) && type == USBD_EP_TYPE_CTRL));
    ++fake.open_count;
    return fake.open_count == fake.fail_open_call ? USBD_FAIL : USBD_OK;
}

USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef *dev, uint8_t endpoint)
{
    CHECK(dev == &device);
    CHECK(endpoint == 0x81 || endpoint == 0x02);
    ++fake.close_count;
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef *dev, uint8_t endpoint)
{
    CHECK(dev == &device && (endpoint == 0 || endpoint == 0x80));
    ++fake.stall_count;
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef *dev,
    uint8_t endpoint, uint8_t *buffer, uint16_t size)
{
    CHECK(dev == &device);
    CHECK(endpoint == 0x02 && size == 64);
    fake.out_buffer = buffer;
    ++fake.arm_count;
    endpoint_event('A');
    return fake.arm_result;
}

uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef *dev, uint8_t endpoint)
{
    CHECK(dev == &device && endpoint < 16);
    return fake.rx_length[endpoint];
}

USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef *dev, uint8_t endpoint,
                                   uint8_t *buffer, uint16_t size)
{
    CHECK(dev == &device && endpoint == 0x81);
    CHECK(size == 20);
    CHECK(fake.irq_mask == 1);
    fake.in_buffer = buffer;
    fake.in_length = size;
    ++fake.transmit_count;
    return fake.transmit_result;
}

USBD_StatusTypeDef USBD_CtlSendData(USBD_HandleTypeDef *dev,
                                   uint8_t *buffer, uint16_t size)
{
    CHECK(dev == &device && size <= sizeof(fake.control_reply));
    memcpy(fake.control_reply, buffer, size);
    fake.control_length = size;
    return USBD_OK;
}

USBD_StatusTypeDef USBD_CtlPrepareRx(USBD_HandleTypeDef *dev,
                                    uint8_t *buffer, uint16_t size)
{
    CHECK(dev == &device && size <= 64);
    fake.control_buffer = buffer;
    fake.control_length = size;
    dev->ep0_state = USBD_EP0_DATA_OUT;
    dev->ep_out[0].rem_length = size;
    dev->ep_out[0].maxpacket = 64;
    return USBD_OK;
}

USBD_StatusTypeDef USBD_CtlContinueRx(USBD_HandleTypeDef *dev,
                                     uint8_t *buffer, uint16_t size)
{
    (void)dev; (void)buffer; (void)size;
    CHECK(false); /* All supported controls fit a single EP0 packet. */
    return USBD_FAIL;
}

USBD_StatusTypeDef USBD_CtlSendStatus(USBD_HandleTypeDef *dev)
{
    CHECK(dev == &device);
    ++fake.status_count;
    dev->ep0_state = USBD_EP0_STATUS_IN;
    return USBD_OK;
}

void USBD_CtlError(USBD_HandleTypeDef *dev, USBD_SetupReqTypedef *request)
{
    CHECK(dev == &device && request != NULL);
    ++fake.stall_count;
    /* ST CtlError does not set ep0_state: the class must do that itself. */
}

static void fixture(void)
{
    memset(&device, 0, sizeof(device));
    memset(&fake, 0, sizeof(fake));
    fake.accept_enqueue = fake.accept_timing = fake.accept_mode = true;
    pcd.Instance = &usb_registers;
    device.dev_speed = USBD_SPEED_FULL;
    device.dev_state = USBD_STATE_CONFIGURED;
    device.pData = &pcd;
    device.pClass = &USBD_GS_USB;
    CHECK(USBD_GS_USB.Init(&device, 1) == USBD_OK);
    CHECK(device.pClassData != NULL);
    CHECK(device.ep_in[1].is_used && device.ep_out[2].is_used);
    CHECK(fake.open_count == 2 && fake.arm_count == 1);
}

static uint8_t setup(USBD_SetupReqTypedef request)
{
    device.request = request;
    device.ep0_state = USBD_EP0_SETUP;
    fake.control_buffer = NULL;
    fake.control_length = 0;
    return USBD_GS_USB.Setup(&device, &device.request);
}

static uint8_t control_out(uint8_t request, uint16_t value,
                           const void *payload, uint16_t size)
{
    CHECK(setup((USBD_SetupReqTypedef){0x41, request, value, 0, size}) == USBD_OK);
    CHECK(fake.control_buffer != NULL && fake.control_length == size);
    memcpy(fake.control_buffer, payload, size);
    fake.rx_length[0] = size;
    return USBD_LL_DataOutStage(&device, 0, fake.control_buffer);
}

static GS_HostFrame frame(uint32_t id)
{
    GS_HostFrame result = {.echo_id = GS_ECHO_ID_RX, .can_id = id, .can_dlc = 8};
    for (unsigned i = 0; i < 8; ++i) result.data[i] = (uint8_t)(id + i);
    return result;
}

static void queue_in(GS_HostFrame value)
{
    CHECK(fake.queue_tail < 4);
    fake.queued[fake.queue_tail++] = value;
}

static void receive_out(const void *payload, uint16_t length)
{
    CHECK(length <= 64 && fake.out_buffer != NULL);
    memcpy(fake.out_buffer, payload, length);
    fake.rx_length[2] = length;
    CHECK(USBD_GS_USB.DataOut(&device, 2) == USBD_OK);
}

static void test_descriptors(void)
{
    uint16_t size = 0;
    uint8_t *desc = USBD_GS_USB.GetFSConfigDescriptor(&size);
    CHECK(size == 32 && desc[0] == 9 && desc[1] == USB_DESC_TYPE_CONFIGURATION);
    CHECK(desc[2] == size && desc[3] == 0 && desc[4] == 1);
    CHECK(desc[9] == 9 && desc[10] == USB_DESC_TYPE_INTERFACE);
    CHECK(desc[11] == 0 && desc[12] == 0 && desc[13] == 2 && desc[14] == 0xff);
    unsigned endpoints = 0;
    for (unsigned offset = 18; offset < size; offset += desc[offset]) {
        CHECK(desc[offset] == 7 && desc[offset + 1] == USB_DESC_TYPE_ENDPOINT);
        CHECK(desc[offset + 3] == USBD_EP_TYPE_BULK);
        CHECK(desc[offset + 4] == 64 && desc[offset + 5] == 0);
        CHECK(desc[offset + 2] == 0x81 || desc[offset + 2] == 0x02);
        endpoints |= desc[offset + 2] == 0x81 ? 1U : 2U;
    }
    CHECK(endpoints == 3);
}

static void test_discovery_controls(void)
{
    fixture();
    const uint32_t format = 0xbeef;
    CHECK(control_out(GS_USB_BREQ_HOST_FORMAT, 1, &format, 4) == USBD_OK);
    CHECK(fake.status_count == 1 && fake.stall_count == 0);
    CHECK(setup((USBD_SetupReqTypedef){0xc1, 5, 1, 0, 12}) == USBD_OK);
    CHECK(fake.control_length == 12);
    CHECK(fake.control_reply[0] == 0 && fake.control_reply[1] == 0);
    CHECK(fake.control_reply[2] == 0 && fake.control_reply[3] == 0);
    CHECK(setup((USBD_SetupReqTypedef){0xc1, 4, 0, 0, 40}) == USBD_OK);
    GS_BitTimingConst limits;
    memcpy(&limits, fake.control_reply, sizeof(limits));
    CHECK(limits.feature == 3 && limits.fclk_can == 48000000);
    CHECK(limits.tseg1_min == 1 && limits.tseg1_max == 16);
    CHECK(limits.tseg2_min == 1 && limits.tseg2_max == 8 && limits.sjw_max == 4);
    CHECK(limits.brp_min == 1 && limits.brp_max == 1024 && limits.brp_inc == 1);
    CHECK(setup((USBD_SetupReqTypedef){0x81, USB_REQ_GET_INTERFACE, 0, 0, 1}) == USBD_OK);
    CHECK(fake.control_length == 1 && fake.control_reply[0] == 0);
}

static void test_malformed_controls_and_core_stall(void)
{
    fixture();
    const USBD_SetupReqTypedef invalid[] = {
        {0x41, 0, 0, 0, 4}, {0xc1, 5, 0, 0, 12}, /* per-device value is 1 */
        {0xc1, 4, 1, 0, 40}, {0xc1, 4, 0, 1, 40},
        {0x41, 4, 0, 0, 40}, {0x21, 0, 1, 0, 4},
        {0x41, 1, 0, 0, 19}, {0x41, 2, 0, 0, 64},
        {0xc1, 5, 1, 0, 13}, {0xc1, 0xff, 0, 0, 0},
        {0x01, USB_REQ_SET_INTERFACE, 1, 0, 0}
    };
    for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        unsigned count = fake.stall_count;
        CHECK(setup(invalid[i]) == USBD_FAIL);
        CHECK(fake.stall_count == count + 1 && device.ep0_state == USBD_EP0_STALL);
        CHECK(fake.control_buffer == NULL);
    }
    uint32_t wrong_format = 0xdeadbeef;
    CHECK(control_out(0, 1, &wrong_format, 4) == USBD_FAIL);
    CHECK(fake.status_count == 0 && device.ep0_state == USBD_EP0_STALL);
    CHECK(setup((USBD_SetupReqTypedef){0x41, 0, 1, 0, 4}) == USBD_OK);
    fake.rx_length[0] = 3;
    CHECK(USBD_LL_DataOutStage(&device, 0, fake.control_buffer) == USBD_FAIL);
    CHECK(fake.status_count == 0);
    /* A new SETUP must cancel an unfinished OUT control transaction. */
    CHECK(setup((USBD_SetupReqTypedef){0x41, 2, 0, 0, 8}) == USBD_OK);
    CHECK(setup((USBD_SetupReqTypedef){0xc1, 5, 1, 0, 12}) == USBD_OK);
    CHECK(USBD_GS_USB.EP0_RxReady(&device) == USBD_FAIL && fake.mode_count == 0);
}

static void test_backend_control_errors(void)
{
    fixture();
    GS_BitTiming timing = {6, 7, 2, 1, 6};
    CHECK(control_out(1, 0, &timing, sizeof(timing)) == USBD_OK);
    CHECK(fake.timing_count == 1 && memcmp(&fake.timing, &timing, sizeof(timing)) == 0);
    fake.accept_timing = false;
    CHECK(control_out(1, 0, &timing, sizeof(timing)) == USBD_FAIL);
    CHECK(fake.status_count == 1);
    GS_DeviceMode mode = {GS_CAN_MODE_START, GS_CAN_MODE_LISTEN_ONLY};
    CHECK(control_out(2, 0, &mode, sizeof(mode)) == USBD_OK);
    CHECK(fake.mode_count == 1 && fake.mode.mode == 1 && fake.mode.flags == 1);
    fake.accept_mode = false;
    CHECK(control_out(2, 0, &mode, sizeof(mode)) == USBD_FAIL);
    CHECK(fake.status_count == 2 && fake.abort_count == 0);
}

static void test_out_backpressure_and_invalid_lengths(void)
{
    fixture();
    GS_HostFrame value = frame(0x123);
    value.echo_id = 4;
    value.flags = value.reserved = 0xff; /* Old Linux transmits uninitialized bytes. */
    fake.accept_enqueue = false;
    receive_out(&value, sizeof(value));
    CHECK(fake.enqueue_count == 1 && fake.arm_count == 1);
    CHECK(fake.enqueued.echo_id == 4 && fake.enqueued.can_id == 0x123);
    CHECK(fake.enqueued.flags == 0 && fake.enqueued.reserved == 0);
    USBD_GS_USB_Poll(&device);
    CHECK(fake.enqueue_count == 2 && fake.arm_count == 1);
    fake.accept_enqueue = true;
    USBD_GS_USB_Poll(&device);
    CHECK(fake.enqueue_count == 3 && fake.arm_count == 2);
    CHECK(fake.enqueued.can_id == 0x123 && fake.enqueued.data[7] == value.data[7]);
    CHECK(fake.irq_mask == 0);
    uint8_t malformed[64] = {0};
    const uint16_t lengths[] = {0, 1, 19, 21, 64};
    for (unsigned i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
        unsigned arms = fake.arm_count;
        receive_out(malformed, lengths[i]);
        CHECK(fake.enqueue_count == 3 && fake.arm_count == arms + 1);
    }
    receive_out(&value, sizeof(value));
    CHECK(fake.enqueue_count == 4);
}

static void test_in_lifetime_retry_and_irq_restore(void)
{
    fixture();
    GS_HostFrame first = frame(0x123), second = frame(0x456);
    queue_in(first);
    queue_in(second);
    fake.transmit_result = USBD_BUSY;
    USBD_GS_USB_Poll(&device);
    CHECK(fake.transmit_count == 1 && fake.dequeue_count == 1);
    CHECK(fake.in_length == 20 && memcmp(fake.in_buffer, &first, 20) == 0);
    fake.transmit_result = USBD_OK;
    fake.irq_mask = 1;
    USBD_GS_USB_Poll(&device);
    CHECK(fake.transmit_count == 2 && fake.dequeue_count == 1 && fake.irq_mask == 1);
    fake.irq_mask = 0;
    const uint8_t *in_flight = fake.in_buffer;
    USBD_GS_USB_Poll(&device);
    CHECK(fake.transmit_count == 2 && fake.dequeue_count == 1);
    CHECK(memcmp(in_flight, &first, 20) == 0);
    CHECK(USBD_GS_USB.DataIn(&device, 2) == USBD_FAIL);
    CHECK(USBD_GS_USB.DataIn(&device, 1) == USBD_OK);
    USBD_GS_USB_Poll(&device);
    CHECK(fake.transmit_count == 3 && fake.dequeue_count == 2);
    CHECK(memcmp(fake.in_buffer, &second, 20) == 0);
}

static void create_pending_transfers(void)
{
    GS_HostFrame value = frame(0x321);
    fake.accept_enqueue = false;
    receive_out(&value, sizeof(value));
    queue_in(value);
    USBD_GS_USB_Poll(&device);
    CHECK(fake.transmit_count == 1);
}

static void test_mode_reset_and_usb_lifecycle(void)
{
    fixture();
    create_pending_transfers();
    GS_DeviceMode reset = {GS_CAN_MODE_RESET, 0};
    unsigned event_start = fake.endpoint_event_count;
    CHECK(control_out(2, 0, &reset, sizeof(reset)) == USBD_OK);
    CHECK(fake.abort_count == 2 && fake.arm_count == 2);
    /* Both NAKs, then both pending completions cleared, then OUT rearmed. */
    CHECK(fake.endpoint_event_count == event_start + 5);
    CHECK(memcmp(&fake.endpoint_events[event_start], "IOioA", 5) == 0);
    unsigned enqueues = fake.enqueue_count;
    USBD_GS_USB_Poll(&device);
    CHECK(fake.enqueue_count == enqueues && fake.transmit_count == 1);
    queue_in(frame(0x456));
    USBD_GS_USB_Poll(&device);
    CHECK(fake.transmit_count == 2);

    CHECK(USBD_LL_Reset(&device) == USBD_OK);
    CHECK(device.pClassData == NULL && device.dev_state == USBD_STATE_DEFAULT);
    CHECK(fake.disconnect_count == 2 && fake.close_count == 2);
    USBD_GS_USB_Poll(&device);
    CHECK(fake.transmit_count == 2);
    device.dev_state = USBD_STATE_CONFIGURED;
    CHECK(USBD_GS_USB.Init(&device, 1) == USBD_OK);
    USBD_GS_USB_Poll(&device);
    CHECK(fake.transmit_count == 2 && fake.enqueue_count == enqueues);
    CHECK(USBD_LL_DevDisconnected(&device) == USBD_OK);
    CHECK(device.pClassData == NULL && fake.disconnect_count == 4);
    CHECK(!device.ep_in[1].is_used && !device.ep_out[2].is_used);
}

static void test_endpoint_failures_and_suspension(void)
{
    fixture();
    CHECK(USBD_GS_USB.DeInit(&device, 1) == USBD_OK);
    fake.fail_open_call = fake.open_count + 2;
    unsigned closes = fake.close_count;
    CHECK(USBD_GS_USB.Init(&device, 1) == USBD_FAIL);
    CHECK(fake.close_count == closes + 1 && device.pClassData == NULL);
    fake.fail_open_call = 0;
    fake.arm_result = USBD_FAIL;
    CHECK(USBD_GS_USB.Init(&device, 1) == USBD_FAIL);
    CHECK(device.pClassData == NULL && !device.ep_in[1].is_used && !device.ep_out[2].is_used);
    fake.arm_result = USBD_OK;
    CHECK(USBD_GS_USB.Init(&device, 1) == USBD_OK);
    queue_in(frame(0x111));
    device.dev_state = USBD_STATE_SUSPENDED;
    USBD_GS_USB_Poll(&device);
    CHECK(fake.transmit_count == 0 && fake.irq_mask == 0);
    device.dev_state = USBD_STATE_CONFIGURED;
    USBD_GS_USB_Poll(&device);
    CHECK(fake.transmit_count == 1);
}

int main(void)
{
    test_descriptors();
    test_discovery_controls();
    test_malformed_controls_and_core_stall();
    test_backend_control_errors();
    test_out_backpressure_and_invalid_lengths();
    test_in_lifetime_retry_and_irq_restore();
    test_mode_reset_and_usb_lifecycle();
    test_endpoint_failures_and_suspension();
    puts("gs_usb class/core: 8 test groups passed");
    return EXIT_SUCCESS;
}
