#ifdef CANABLE_GS_USB

#include "gs_can.h"
#include "main.h"

/* Linux wire values: include/uapi/linux/can/error.h. Only controller state
 * changes and overruns are reported; per-frame bus-error reporting is absent. */
enum {
    GS_ERR_CRTL = 1U << 2,
    GS_ERR_BUSOFF = 1U << 6,
    GS_ERR_CNT = 1U << 9,
    GS_ERR_RX_WARNING = 1U << 2,
    GS_ERR_TX_WARNING = 1U << 3,
    GS_ERR_RX_PASSIVE = 1U << 4,
    GS_ERR_TX_PASSIVE = 1U << 5,
    GS_ERR_ACTIVE = 1U << 6,
    TX_CAPACITY = 16,
    RX_CAPACITY = 32
};

typedef struct {
    GS_HostFrame frames[TX_CAPACITY];
    uint8_t read;
    uint8_t write;
    uint8_t count;
} TxQueue;

static TxQueue tx_queue;
static TxQueue echo_queue;
static GS_HostFrame rx_queue[RX_CAPACITY];
static uint8_t rx_read;
static uint8_t rx_write;
static uint8_t rx_count;
static GS_HostFrame mailbox_frame[3];
static bool mailbox_active[3];

/* Reserve an echo slot when accepting TX, not after sending it. Thus queued
 * TX + active mailboxes + completed echoes <= TX_CAPACITY, independently of RX. */
static uint8_t tx_outstanding;
static bool running;
static bool bus_off;
static uint32_t mode_flags;
static uint32_t controller_state;
static GS_HostFrame error_frame;
static bool error_pending;
static bool rx_overflow;

static uint32_t irq_lock(void)
{
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    return mask;
}

static void irq_unlock(uint32_t mask)
{
    __set_PRIMASK(mask);
}

/* Queue operations and shared state below are protected by irq_lock. */
static void tx_push(TxQueue *queue, const GS_HostFrame *frame)
{
    queue->frames[queue->write] = *frame;
    queue->write = (queue->write + 1U) % TX_CAPACITY;
    queue->count++;
}

static void tx_pop(TxQueue *queue, GS_HostFrame *frame)
{
    *frame = queue->frames[queue->read];
    queue->read = (queue->read + 1U) % TX_CAPACITY;
    queue->count--;
}

static void purge_queues(void)
{
    tx_queue.read = tx_queue.write = tx_queue.count = 0;
    echo_queue.read = echo_queue.write = echo_queue.count = 0;
    rx_read = rx_write = rx_count = 0;
    tx_outstanding = 0;
    for (unsigned i = 0; i < 3; i++) {
        mailbox_active[i] = false;
    }
    error_pending = false;
    rx_overflow = false;
    controller_state = 0;
}

static bool stop_channel(void)
{
    uint32_t mask = irq_lock();
    bool listening = hcan.State == HAL_CAN_STATE_LISTENING;
    running = false;
    bus_off = false;
    hcan.Instance->IER = 0;
    if (listening || hcan.State == HAL_CAN_STATE_READY) {
        (void)HAL_CAN_AbortTxRequest(&hcan,
            CAN_TX_MAILBOX0 | CAN_TX_MAILBOX1 | CAN_TX_MAILBOX2);
    }
    SET_BIT(hcan.Instance->MCR, CAN_MCR_INRQ);
    purge_queues();
    irq_unlock(mask);

    /* HAL waits for INAK: leave SysTick enabled, including in USB IRQ context. */
    bool success = !listening || HAL_CAN_Stop(&hcan) == HAL_OK;

    mask = irq_lock();
    hcan.Instance->TSR = CAN_TSR_RQCP0 | CAN_TSR_RQCP1 | CAN_TSR_RQCP2;
    /* There are only three hardware RX slots. Flush stale frames on reset. */
    for (unsigned i = 0; i < 3; i++) {
        if ((hcan.Instance->RF0R & CAN_RF0R_FMP0) == 0) {
            break;
        }
        hcan.Instance->RF0R = CAN_RF0R_RFOM0;
    }
    hcan.Instance->RF0R = CAN_RF0R_FULL0 | CAN_RF0R_FOVR0;
    hcan.ErrorCode = HAL_CAN_ERROR_NONE;
    irq_unlock(mask);
    return success;
}

void GS_CAN_Init(void)
{
    (void)stop_channel();
    hcan.Init.AutoRetransmission = ENABLE;
    hcan.Init.AutoBusOff = DISABLE;

    uint32_t mask = irq_lock();
    LED_Green_Off();
    LED_Blue_Off();
    TimerCallbackRegisterOnly(&timerCallback, LED_Green_Toggle);
    TimerCallbackRegisterOnly(&timerCallback, LED_Blue_Toggle);
    TimerCallbackRegister2nd(&timerCallback, LED_Green_Toggle, LED_Green_Off);
    TimerCallbackRegister2nd(&timerCallback, LED_Blue_Toggle, LED_Blue_Off);
    TimerCallbackRepetitionStart(&timerCallback, LED_Green_Toggle, 100, 6);
    irq_unlock(mask);
}

void GS_CAN_Disconnect(void)
{
    (void)stop_channel();
}

bool GS_CAN_SetBitTiming(const GS_BitTiming *timing)
{
    /* Bound each operand before addition to reject wrapped hostile values. */
    if (timing == NULL || timing->prop_seg > 16 || timing->phase_seg1 > 16 ||
        timing->prop_seg + timing->phase_seg1 < 1 ||
        timing->prop_seg + timing->phase_seg1 > 16 ||
        timing->phase_seg2 < 1 || timing->phase_seg2 > 8 ||
        timing->sjw < 1 || timing->sjw > 4 || timing->sjw > timing->phase_seg2 ||
        timing->brp < 1 || timing->brp > 1024) {
        return false;
    }

    uint32_t mask = irq_lock();
    if (running) {
        irq_unlock(mask);
        return false;
    }
    hcan.Init.Prescaler = timing->brp;
    hcan.Init.TimeSeg1 = (timing->prop_seg + timing->phase_seg1 - 1U) << CAN_BTR_TS1_Pos;
    hcan.Init.TimeSeg2 = (timing->phase_seg2 - 1U) << CAN_BTR_TS2_Pos;
    hcan.Init.SyncJumpWidth = (timing->sjw - 1U) << CAN_BTR_SJW_Pos;
    irq_unlock(mask);
    return HAL_CAN_Init(&hcan) == HAL_OK;
}

bool GS_CAN_SetMode(uint32_t mode, uint32_t flags)
{
    if ((flags & ~GS_CAN_FEATURES) != 0 ||
        (mode != GS_CAN_MODE_RESET && mode != GS_CAN_MODE_START)) {
        return false;
    }
    if (mode == GS_CAN_MODE_RESET) {
        return flags == 0 && stop_channel();
    }

    uint32_t mask = irq_lock();
    if (running && !bus_off) {
        bool unchanged = mode_flags == flags;
        irq_unlock(mask);
        return unchanged;
    }
    irq_unlock(mask);
    if (!stop_channel()) {
        return false;
    }

    hcan.Init.Mode = CAN_MODE_NORMAL;
    if (flags & GS_CAN_MODE_LISTEN_ONLY) {
        hcan.Init.Mode |= CAN_MODE_SILENT;
    }
    if (flags & GS_CAN_MODE_LOOP_BACK) {
        hcan.Init.Mode |= CAN_MODE_LOOPBACK;
    }
    hcan.Init.AutoRetransmission = ENABLE;
    hcan.Init.AutoBusOff = DISABLE;
    hcan.Init.TransmitFifoPriority = ENABLE;

    CAN_FilterTypeDef filter = {0};
    filter.FilterMode = CAN_FILTERMODE_IDMASK;
    filter.FilterScale = CAN_FILTERSCALE_32BIT;
    filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    filter.FilterActivation = ENABLE;
    if (HAL_CAN_Init(&hcan) != HAL_OK ||
        HAL_CAN_ConfigFilter(&hcan, &filter) != HAL_OK ||
        HAL_CAN_Start(&hcan) != HAL_OK) {
        (void)stop_channel();
        return false;
    }

    mask = irq_lock();
    mode_flags = flags;
    running = true;
    bool success = HAL_CAN_ActivateNotification(&hcan,
        CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_RX_FIFO0_OVERRUN |
        CAN_IT_TX_MAILBOX_EMPTY | CAN_IT_ERROR_WARNING | CAN_IT_ERROR_PASSIVE |
        CAN_IT_BUSOFF | CAN_IT_ERROR) == HAL_OK;
    irq_unlock(mask);
    if (!success) {
        (void)stop_channel();
    }
    return success;
}

bool GS_CAN_Enqueue(const GS_HostFrame *frame)
{
    if (frame == NULL || frame->channel != 0 || frame->can_dlc > 8 ||
        frame->echo_id == GS_ECHO_ID_RX || (frame->can_id & GS_CAN_ERR_FLAG) ||
        (!(frame->can_id & GS_CAN_EFF_FLAG) &&
         (frame->can_id & ~(GS_CAN_RTR_FLAG | GS_CAN_SFF_MASK)))) {
        return true;
    }

    uint32_t mask = irq_lock();
    if (!running || bus_off ||
        ((mode_flags & GS_CAN_MODE_LISTEN_ONLY) && !(mode_flags & GS_CAN_MODE_LOOP_BACK))) {
        irq_unlock(mask);
        return true;
    }
    if (tx_outstanding == TX_CAPACITY) {
        irq_unlock(mask);
        return false;
    }

    GS_HostFrame normalized = *frame;
    /* Older gs_usb hosts leave these bytes uninitialized. */
    normalized.flags = 0;
    normalized.reserved = 0;
    unsigned used = (frame->can_id & GS_CAN_RTR_FLAG) ? 0 : frame->can_dlc;
    memset(normalized.data + used, 0, sizeof(normalized.data) - used);
    tx_push(&tx_queue, &normalized);
    tx_outstanding++;
    irq_unlock(mask);
    return true;
}

/* Keep the latest controller state outside the RX ring: RX saturation must
 * never hide bus-off from the host. A bus-off stays latched until host reset. */
static void update_controller_state(uint32_t esr)
{
    uint32_t state = esr & (CAN_ESR_EWGF | CAN_ESR_EPVF | CAN_ESR_BOFF);
    if (!running || bus_off || state == controller_state) {
        return;
    }
    controller_state = state;
    error_frame = (GS_HostFrame){
        .echo_id = GS_ECHO_ID_RX,
        .can_id = GS_CAN_ERR_FLAG | GS_ERR_CNT,
        .can_dlc = 8
    };
    uint8_t tx_error = (esr & CAN_ESR_TEC) >> CAN_ESR_TEC_Pos;
    uint8_t rx_error = (esr & CAN_ESR_REC) >> CAN_ESR_REC_Pos;
    error_frame.data[6] = tx_error;
    error_frame.data[7] = rx_error;
    if (state & CAN_ESR_BOFF) {
        error_frame.can_id |= GS_ERR_BUSOFF;
        bus_off = true;
        hcan.Instance->IER = 0;
        (void)HAL_CAN_AbortTxRequest(&hcan,
            CAN_TX_MAILBOX0 | CAN_TX_MAILBOX1 | CAN_TX_MAILBOX2);
        SET_BIT(hcan.Instance->MCR, CAN_MCR_INRQ);
    } else {
        error_frame.can_id |= GS_ERR_CRTL;
        if (state & CAN_ESR_EPVF) {
            if (rx_error >= 128) error_frame.data[1] |= GS_ERR_RX_PASSIVE;
            if (tx_error >= 128) error_frame.data[1] |= GS_ERR_TX_PASSIVE;
        } else if (state & CAN_ESR_EWGF) {
            if (rx_error >= 96) error_frame.data[1] |= GS_ERR_RX_WARNING;
            if (tx_error >= 96) error_frame.data[1] |= GS_ERR_TX_WARNING;
        } else {
            error_frame.data[1] = GS_ERR_ACTIVE;
        }
    }
    error_pending = true;
}

/* Submit a fixed software-free mailbox. HAL_CAN_AddTxMessage chooses any
 * hardware-empty mailbox, including one whose completion IRQ is still pending.
 * It can therefore overwrite the old echo association even with IRQs masked.
 * Here the old callback must have released the slot, and the new association
 * is published before TXRQ, using the same register layout as the HAL driver. */
static void submit_mailbox(unsigned index)
{
    GS_HostFrame *frame = &mailbox_frame[index];
    tx_pop(&tx_queue, frame);
    mailbox_active[index] = true;
    uint32_t identifier;
    if (frame->can_id & GS_CAN_EFF_FLAG) {
        identifier = ((frame->can_id & GS_CAN_EFF_MASK) << CAN_TI0R_EXID_Pos) | CAN_ID_EXT;
    } else {
        identifier = (frame->can_id & GS_CAN_SFF_MASK) << CAN_TI0R_STID_Pos;
    }
    if (frame->can_id & GS_CAN_RTR_FLAG) {
        identifier |= CAN_RTR_REMOTE;
    }

    CAN_TxMailBox_TypeDef *mailbox = &hcan.Instance->sTxMailBox[index];
    mailbox->TIR = identifier;
    mailbox->TDTR = frame->can_dlc;
    mailbox->TDLR = (uint32_t)frame->data[0] | ((uint32_t)frame->data[1] << 8) |
        ((uint32_t)frame->data[2] << 16) | ((uint32_t)frame->data[3] << 24);
    mailbox->TDHR = (uint32_t)frame->data[4] | ((uint32_t)frame->data[5] << 8) |
        ((uint32_t)frame->data[6] << 16) | ((uint32_t)frame->data[7] << 24);
    __DMB();
    mailbox->TIR = identifier | CAN_TI0R_TXRQ;
}

void GS_CAN_Poll(void)
{
    uint32_t mask = irq_lock();
    update_controller_state(hcan.Instance->ESR);
    if (running && !bus_off) {
        for (unsigned i = 0; i < 3 && tx_queue.count != 0; i++) {
            if (!mailbox_active[i] && (hcan.Instance->TSR & (CAN_TSR_TME0 << i))) {
                submit_mailbox(i);
            }
        }
    }
    irq_unlock(mask);
}

bool GS_CAN_Dequeue(GS_HostFrame *frame)
{
    if (frame == NULL) return false;
    uint32_t mask = irq_lock();
    if (echo_queue.count != 0) {
        tx_pop(&echo_queue, frame);
        tx_outstanding--;
    } else if (error_pending) {
        *frame = error_frame;
        error_pending = false;
    } else if (rx_count != 0) {
        *frame = rx_queue[rx_read];
        rx_read = (rx_read + 1U) % RX_CAPACITY;
        rx_count--;
    } else if (rx_overflow) {
        *frame = (GS_HostFrame){
            .echo_id = GS_ECHO_ID_RX,
            .can_id = GS_CAN_ERR_FLAG | GS_ERR_CNT,
            .can_dlc = 8
        };
        uint32_t esr = hcan.Instance->ESR;
        frame->data[6] = (esr & CAN_ESR_TEC) >> CAN_ESR_TEC_Pos;
        frame->data[7] = (esr & CAN_ESR_REC) >> CAN_ESR_REC_Pos;
    } else {
        irq_unlock(mask);
        return false;
    }
    if (rx_overflow) {
        frame->flags |= GS_CAN_FLAG_OVERFLOW;
        rx_overflow = false;
    }
    irq_unlock(mask);
    return true;
}

static void tx_complete(CAN_HandleTypeDef *handle, unsigned index)
{
    if (handle != &hcan) return;
    uint32_t mask = irq_lock();
    if (mailbox_active[index]) {
        /* The credit reserved by Enqueue guarantees space in this ring. */
        tx_push(&echo_queue, &mailbox_frame[index]);
        mailbox_active[index] = false;
        TimerCallbackRepetitionStart(&timerCallback, LED_Green_Toggle, 20, 2);
    }
    irq_unlock(mask);
}

static void tx_aborted(CAN_HandleTypeDef *handle, unsigned index)
{
    if (handle != &hcan) return;
    uint32_t mask = irq_lock();
    if (mailbox_active[index]) {
        mailbox_active[index] = false;
        if (running && !bus_off) {
            /* A failed attempt is not a successful TX echo. Retry the frame. */
            tx_push(&tx_queue, &mailbox_frame[index]);
        } else {
            tx_outstanding--;
        }
    }
    irq_unlock(mask);
}

void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef *handle) { tx_complete(handle, 0); }
void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef *handle) { tx_complete(handle, 1); }
void HAL_CAN_TxMailbox2CompleteCallback(CAN_HandleTypeDef *handle) { tx_complete(handle, 2); }
void HAL_CAN_TxMailbox0AbortCallback(CAN_HandleTypeDef *handle) { tx_aborted(handle, 0); }
void HAL_CAN_TxMailbox1AbortCallback(CAN_HandleTypeDef *handle) { tx_aborted(handle, 1); }
void HAL_CAN_TxMailbox2AbortCallback(CAN_HandleTypeDef *handle) { tx_aborted(handle, 2); }

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *handle)
{
    if (handle != &hcan) return;
    uint32_t mask = irq_lock();
    for (unsigned i = 0; i < 3 && HAL_CAN_GetRxFifoFillLevel(handle, CAN_RX_FIFO0); i++) {
        CAN_RxHeaderTypeDef header;
        GS_HostFrame frame = {.echo_id = GS_ECHO_ID_RX};
        if (HAL_CAN_GetRxMessage(handle, CAN_RX_FIFO0, &header, frame.data) != HAL_OK) {
            break;
        }
        if (!running || bus_off) continue;
        frame.can_id = header.IDE == CAN_ID_EXT ?
            ((header.ExtId & GS_CAN_EFF_MASK) | GS_CAN_EFF_FLAG) :
            (header.StdId & GS_CAN_SFF_MASK);
        frame.can_dlc = header.DLC > 8 ? 8 : header.DLC;
        if (header.RTR == CAN_RTR_REMOTE) {
            frame.can_id |= GS_CAN_RTR_FLAG;
        }
        unsigned used = header.RTR == CAN_RTR_REMOTE ? 0 : frame.can_dlc;
        memset(frame.data + used, 0, sizeof(frame.data) - used);
        if (rx_count == RX_CAPACITY) {
            rx_overflow = true;
        } else {
            rx_queue[rx_write] = frame;
            rx_write = (rx_write + 1U) % RX_CAPACITY;
            rx_count++;
        }
        TimerCallbackRepetitionStart(&timerCallback, LED_Blue_Toggle, 20, 2);
    }
    irq_unlock(mask);
}

void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *handle)
{
    if (handle != &hcan) return;
    uint32_t mask = irq_lock();
    uint32_t errors = HAL_CAN_GetError(handle);
    if (running) {
        update_controller_state(handle->Instance->ESR);
        if (errors & (HAL_CAN_ERROR_RX_FOV0 | HAL_CAN_ERROR_RX_FOV1)) {
            rx_overflow = true;
        }
        if (errors & (HAL_CAN_ERROR_TX_ALST0 | HAL_CAN_ERROR_TX_TERR0)) tx_aborted(handle, 0);
        if (errors & (HAL_CAN_ERROR_TX_ALST1 | HAL_CAN_ERROR_TX_TERR1)) tx_aborted(handle, 1);
        if (errors & (HAL_CAN_ERROR_TX_ALST2 | HAL_CAN_ERROR_TX_TERR2)) tx_aborted(handle, 2);
    }
    handle->ErrorCode = HAL_CAN_ERROR_NONE;
    irq_unlock(mask);
}

#endif /* CANABLE_GS_USB */
