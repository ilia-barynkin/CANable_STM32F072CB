#include "main.h"
#include "gs_can.h"

#include <assert.h>
#include <stdio.h>

static CAN_TypeDef registers;
CAN_HandleTypeDef hcan;
TimerCallbackStruct timerCallback;
static uint32_t irq_mask;
static HAL_StatusTypeDef init_result, start_result, filter_result, notify_result, stop_result;
static CAN_FilterTypeDef configured_filter;
static CAN_RxHeaderTypeDef received_header;
static uint8_t received_data[8];
static unsigned received_count;
static unsigned start_calls, abort_calls;

uint32_t __get_PRIMASK(void) { return irq_mask; }
void __disable_irq(void) { irq_mask = 1; }
void __set_PRIMASK(uint32_t value) { irq_mask = value; }
void __DMB(void) { assert(irq_mask == 1); }

HAL_StatusTypeDef HAL_CAN_Init(CAN_HandleTypeDef *handle)
{
    assert(!irq_mask); /* This HAL operation can wait for SysTick. */
    if (init_result == HAL_OK) handle->State = HAL_CAN_STATE_READY;
    return init_result;
}

HAL_StatusTypeDef HAL_CAN_Start(CAN_HandleTypeDef *handle)
{
    assert(!irq_mask);
    start_calls++;
    if (start_result == HAL_OK) {
        handle->State = HAL_CAN_STATE_LISTENING;
        handle->Instance->MCR &= ~CAN_MCR_INRQ;
        handle->Instance->TSR = CAN_TSR_TME0 | CAN_TSR_TME1 | CAN_TSR_TME2;
        handle->Instance->ESR = 0;
        memset(handle->Instance->sTxMailBox, 0, sizeof(handle->Instance->sTxMailBox));
    }
    return start_result;
}

HAL_StatusTypeDef HAL_CAN_Stop(CAN_HandleTypeDef *handle)
{
    assert(!irq_mask);
    if (stop_result == HAL_OK) handle->State = HAL_CAN_STATE_READY;
    return stop_result;
}

HAL_StatusTypeDef HAL_CAN_ConfigFilter(CAN_HandleTypeDef *handle,
                                     const CAN_FilterTypeDef *filter)
{
    (void)handle;
    configured_filter = *filter;
    return filter_result;
}

HAL_StatusTypeDef HAL_CAN_ActivateNotification(CAN_HandleTypeDef *handle, uint32_t mask)
{
    if (notify_result == HAL_OK) handle->Instance->IER |= mask;
    return notify_result;
}

HAL_StatusTypeDef HAL_CAN_AbortTxRequest(CAN_HandleTypeDef *handle, uint32_t mask)
{
    assert(mask == (CAN_TX_MAILBOX0 | CAN_TX_MAILBOX1 | CAN_TX_MAILBOX2));
    abort_calls++;
    for (unsigned i = 0; i < 3; i++) handle->Instance->sTxMailBox[i].TIR &= ~CAN_TI0R_TXRQ;
    return HAL_OK;
}

uint32_t HAL_CAN_GetRxFifoFillLevel(const CAN_HandleTypeDef *handle, uint32_t fifo)
{
    (void)handle;
    assert(fifo == CAN_RX_FIFO0);
    return received_count;
}

HAL_StatusTypeDef HAL_CAN_GetRxMessage(CAN_HandleTypeDef *handle, uint32_t fifo,
                                     CAN_RxHeaderTypeDef *header, uint8_t data[])
{
    (void)handle;
    assert(irq_mask == 1 && fifo == CAN_RX_FIFO0 && received_count);
    *header = received_header;
    memcpy(data, received_data, sizeof(received_data));
    received_count--;
    return HAL_OK;
}

uint32_t HAL_CAN_GetError(const CAN_HandleTypeDef *handle) { return handle->ErrorCode; }
void LED_Green_Toggle(void) {}
void LED_Blue_Toggle(void) {}
void LED_Green_Off(void) {}
void LED_Blue_Off(void) {}
void TimerCallbackRegisterOnly(TimerCallbackStruct *timer, void (*callback)(void))
{ (void)timer; (void)callback; }
void TimerCallbackRegister2nd(TimerCallbackStruct *timer, void (*first)(void), void (*second)(void))
{ (void)timer; (void)first; (void)second; }
void TimerCallbackRepetitionStart(TimerCallbackStruct *timer, void (*callback)(void),
                                 uint32_t period, uint32_t repetitions)
{ (void)timer; (void)callback; (void)period; (void)repetitions; }

static GS_BitTiming timing(void)
{
    return (GS_BitTiming){.prop_seg = 6, .phase_seg1 = 7, .phase_seg2 = 2, .sjw = 1, .brp = 6};
}

static GS_HostFrame frame(uint32_t echo)
{
    GS_HostFrame result = {.echo_id = echo, .can_id = 0x123, .can_dlc = 3};
    for (unsigned i = 0; i < 8; i++) result.data[i] = (uint8_t)(i + 1);
    return result;
}

static void fresh(void)
{
    memset(&registers, 0, sizeof(registers));
    memset(&hcan, 0, sizeof(hcan));
    hcan.Instance = &registers;
    irq_mask = received_count = start_calls = abort_calls = 0;
    init_result = start_result = filter_result = notify_result = stop_result = HAL_OK;
    GS_CAN_Init();
    assert(!irq_mask && hcan.Init.AutoRetransmission == ENABLE);
    assert(hcan.Init.AutoBusOff == DISABLE && registers.IER == 0);
}

static void start(uint32_t flags)
{
    GS_BitTiming value = timing();
    assert(GS_CAN_SetBitTiming(&value));
    assert(GS_CAN_SetMode(GS_CAN_MODE_START, flags));
    assert(!irq_mask && hcan.State == HAL_CAN_STATE_LISTENING);
    assert(configured_filter.FilterMaskIdHigh == 0 && configured_filter.FilterMaskIdLow == 0);
    assert(configured_filter.FilterActivation == ENABLE);
    assert(registers.IER & CAN_IT_TX_MAILBOX_EMPTY);
}

static void complete(unsigned index)
{
    registers.sTxMailBox[index].TIR &= ~CAN_TI0R_TXRQ;
    registers.TSR |= CAN_TSR_TME0 << index;
    void (*const callbacks[])(CAN_HandleTypeDef *) = {
        HAL_CAN_TxMailbox0CompleteCallback, HAL_CAN_TxMailbox1CompleteCallback,
        HAL_CAN_TxMailbox2CompleteCallback
    };
    callbacks[index](&hcan);
}

static void receive(uint32_t id, uint32_t ide, uint32_t rtr, uint32_t dlc)
{
    received_header = (CAN_RxHeaderTypeDef){.StdId = id, .ExtId = id,
        .IDE = ide, .RTR = rtr, .DLC = dlc};
    memset(received_data, 0xA5, sizeof(received_data));
    received_count = 1;
    HAL_CAN_RxFifo0MsgPendingCallback(&hcan);
    assert(received_count == 0 && !irq_mask);
}

static void assert_empty(void)
{
    GS_HostFrame output;
    assert(!GS_CAN_Dequeue(&output));
    assert(!irq_mask);
}

static void test_timing_and_lifecycle(void)
{
    fresh();
    GS_BitTiming value = timing(), bad = value;
    assert(!GS_CAN_SetBitTiming(NULL));
    bad.prop_seg = UINT32_MAX; assert(!GS_CAN_SetBitTiming(&bad));
    bad = value; bad.brp = 0; assert(!GS_CAN_SetBitTiming(&bad));
    bad = value; bad.brp = 1025; assert(!GS_CAN_SetBitTiming(&bad));
    bad = value; bad.sjw = 3; assert(!GS_CAN_SetBitTiming(&bad));
    bad = value; bad.phase_seg2 = 9; assert(!GS_CAN_SetBitTiming(&bad));
    bad = value; bad.prop_seg = bad.phase_seg1 = 0; assert(!GS_CAN_SetBitTiming(&bad));
    bad = value; bad.prop_seg = bad.phase_seg1 = 16; assert(!GS_CAN_SetBitTiming(&bad));
    assert(GS_CAN_SetBitTiming(&value));
    assert(hcan.Init.Prescaler == 6 && hcan.Init.TimeSeg1 == (12U << CAN_BTR_TS1_Pos));
    assert(hcan.Init.TimeSeg2 == (1U << CAN_BTR_TS2_Pos));
    assert(!GS_CAN_SetMode(99, 0));
    assert(!GS_CAN_SetMode(GS_CAN_MODE_START, 1U << 8));
    assert(!GS_CAN_SetMode(GS_CAN_MODE_RESET, GS_CAN_MODE_LOOP_BACK));
    GS_HostFrame tx = frame(1);
    assert(GS_CAN_Enqueue(&tx)); /* Stopped-channel frames are consumed safely. */
    GS_CAN_Poll(); assert_empty();
    assert(!(registers.sTxMailBox[0].TIR & CAN_TI0R_TXRQ));
    start(0);
    assert(!GS_CAN_SetBitTiming(&value));
    assert(GS_CAN_SetMode(GS_CAN_MODE_START, 0));
    assert(start_calls == 1);
    assert(!GS_CAN_SetMode(GS_CAN_MODE_START, GS_CAN_MODE_LOOP_BACK));
    assert(GS_CAN_Enqueue(&tx)); GS_CAN_Poll();
    assert(registers.sTxMailBox[0].TIR & CAN_TI0R_TXRQ);
    assert(GS_CAN_SetMode(GS_CAN_MODE_RESET, 0));
    complete(0); assert_empty(); /* Late completion after reset has no echo. */
    assert(registers.IER == 0 && abort_calls != 0);
    start(0);
    assert(GS_CAN_Enqueue(&tx)); GS_CAN_Poll(); complete(0);
    GS_CAN_Disconnect(); assert_empty();
}

static void test_start_failures(void)
{
    fresh();
    GS_BitTiming value = timing();
    init_result = HAL_ERROR; assert(!GS_CAN_SetBitTiming(&value));
    init_result = HAL_OK;
    filter_result = HAL_ERROR; assert(!GS_CAN_SetMode(GS_CAN_MODE_START, 0));
    filter_result = HAL_OK;
    start_result = HAL_ERROR; assert(!GS_CAN_SetMode(GS_CAN_MODE_START, 0));
    start_result = HAL_OK;
    notify_result = HAL_ERROR; assert(!GS_CAN_SetMode(GS_CAN_MODE_START, 0));
    assert(registers.IER == 0 && !irq_mask);
    notify_result = HAL_OK; start(0);
    stop_result = HAL_TIMEOUT;
    assert(!GS_CAN_SetMode(GS_CAN_MODE_RESET, 0));
    assert(registers.IER == 0 && !irq_mask);
}

static void test_tx_mailboxes_and_wire_fields(void)
{
    fresh(); start(0);
    GS_HostFrame tx[4] = {frame(10), frame(11), frame(12), frame(13)}, output;
    tx[0].flags = tx[0].reserved = 0xFF;
    tx[1].can_id = GS_CAN_EFF_FLAG | 0x1ABCDEFFU;
    tx[2].can_id = GS_CAN_RTR_FLAG | 0x456;
    for (unsigned i = 0; i < 4; i++) assert(GS_CAN_Enqueue(&tx[i]));
    GS_CAN_Poll();
    for (unsigned i = 0; i < 3; i++) assert(registers.sTxMailBox[i].TIR & CAN_TI0R_TXRQ);
    assert(registers.sTxMailBox[0].TIR == ((0x123U << CAN_TI0R_STID_Pos) | CAN_TI0R_TXRQ));
    assert(registers.sTxMailBox[0].TDLR == 0x00030201 && registers.sTxMailBox[0].TDHR == 0);
    assert(registers.sTxMailBox[1].TIR == ((0x1ABCDEFFU << CAN_TI0R_EXID_Pos) | CAN_ID_EXT | CAN_TI0R_TXRQ));
    assert(registers.sTxMailBox[2].TIR & CAN_RTR_REMOTE);
    assert(registers.sTxMailBox[2].TDTR == 3 && registers.sTxMailBox[2].TDLR == 0);
    assert_empty(); /* Enqueue and writing a mailbox are not TX completion. */

    /* Model hardware completion before the IRQ callback: TME is already set.
     * Another poll must preserve the old software echo association. */
    uint32_t old_identifier = registers.sTxMailBox[0].TIR;
    registers.TSR |= CAN_TSR_TME0;
    GS_CAN_Poll();
    assert(registers.sTxMailBox[0].TIR == old_identifier);
    complete(0);
    assert(GS_CAN_Dequeue(&output) && output.echo_id == 10);
    assert(output.flags == 0 && output.reserved == 0 && output.data[3] == 0);
    GS_CAN_Poll();
    complete(2); complete(1); complete(0);
    assert(GS_CAN_Dequeue(&output) && output.echo_id == 12);
    assert(GS_CAN_Dequeue(&output) && output.echo_id == 11);
    assert(GS_CAN_Dequeue(&output) && output.echo_id == 13);
    assert_empty();
}

static void test_credit_and_rx_saturation(void)
{
    fresh(); start(0);
    GS_HostFrame tx = frame(0), output;
    unsigned accepted = 0;
    while (accepted < 100) {
        tx.echo_id = accepted;
        if (!GS_CAN_Enqueue(&tx)) break;
        accepted++;
    }
    assert(accepted >= 10 && accepted < 100);
    for (unsigned i = 0; i < 100; i++) receive(0x321, CAN_ID_STD, CAN_RTR_DATA, 8);
    unsigned completed = 0;
    while (completed < accepted) {
        GS_CAN_Poll();
        unsigned batch = accepted - completed;
        if (batch > 3) batch = 3;
        for (unsigned i = 0; i < batch; i++) complete(i);
        for (unsigned i = 0; i < batch; i++) {
            assert(GS_CAN_Dequeue(&output));
            assert(output.echo_id == completed++); /* RX cannot consume echo slots. */
            if (completed == 1) assert(output.flags & GS_CAN_FLAG_OVERFLOW);
        }
    }
    unsigned rx_count = 0;
    while (GS_CAN_Dequeue(&output)) {
        assert(output.echo_id == GS_ECHO_ID_RX && output.can_id == 0x321);
        rx_count++;
    }
    assert(rx_count > 0 && rx_count < 100);
    tx.echo_id = 123; assert(GS_CAN_Enqueue(&tx)); /* All completion credits returned. */
}

static void test_rx_and_invalid_tx(void)
{
    fresh(); start(0);
    GS_HostFrame output, tx = frame(4);
    receive(0x123, CAN_ID_STD, CAN_RTR_DATA, 2);
    assert(GS_CAN_Dequeue(&output) && output.echo_id == GS_ECHO_ID_RX);
    assert(output.can_id == 0x123 && output.can_dlc == 2 && output.data[2] == 0);
    receive(0x123456, CAN_ID_EXT, CAN_RTR_REMOTE, 6);
    assert(GS_CAN_Dequeue(&output));
    assert(output.can_id == (GS_CAN_EFF_FLAG | GS_CAN_RTR_FLAG | 0x123456));
    for (unsigned i = 0; i < 8; i++) assert(output.data[i] == 0);
    assert(output.can_dlc == 6 && output.channel == 0 && output.reserved == 0);
    tx.can_dlc = 9; assert(GS_CAN_Enqueue(&tx));
    tx = frame(4); tx.channel = 1; assert(GS_CAN_Enqueue(&tx));
    tx = frame(GS_ECHO_ID_RX); assert(GS_CAN_Enqueue(&tx));
    tx = frame(4); tx.can_id = GS_CAN_ERR_FLAG; assert(GS_CAN_Enqueue(&tx));
    tx.can_id = 0x800; assert(GS_CAN_Enqueue(&tx));
    assert(GS_CAN_Enqueue(NULL));
    GS_CAN_Poll(); assert_empty();
    for (unsigned i = 0; i < 3; i++) assert(!(registers.sTxMailBox[i].TIR & CAN_TI0R_TXRQ));
    hcan.ErrorCode = HAL_CAN_ERROR_RX_FOV0;
    HAL_CAN_ErrorCallback(&hcan);
    assert(GS_CAN_Dequeue(&output) && (output.flags & GS_CAN_FLAG_OVERFLOW));
    assert(hcan.ErrorCode == 0); assert_empty();
}

static void test_modes_and_abort_retry(void)
{
    fresh(); start(GS_CAN_MODE_LISTEN_ONLY);
    GS_HostFrame tx = frame(7), output;
    assert(hcan.Init.Mode == CAN_MODE_SILENT);
    assert(GS_CAN_Enqueue(&tx)); GS_CAN_Poll(); assert_empty();
    assert(!(registers.sTxMailBox[0].TIR & CAN_TI0R_TXRQ));
    assert(GS_CAN_SetMode(GS_CAN_MODE_RESET, 0));
    start(GS_CAN_MODE_LOOP_BACK | GS_CAN_MODE_LISTEN_ONLY);
    assert(hcan.Init.Mode == CAN_MODE_SILENT_LOOPBACK);
    assert(GS_CAN_Enqueue(&tx)); GS_CAN_Poll();
    registers.sTxMailBox[0].TIR &= ~CAN_TI0R_TXRQ;
    HAL_CAN_TxMailbox0AbortCallback(&hcan); assert_empty();
    GS_CAN_Poll();
    assert(registers.sTxMailBox[0].TIR & CAN_TI0R_TXRQ);
    registers.sTxMailBox[0].TIR &= ~CAN_TI0R_TXRQ;
    hcan.ErrorCode = HAL_CAN_ERROR_TX_TERR0;
    HAL_CAN_ErrorCallback(&hcan); assert_empty();
    GS_CAN_Poll(); complete(0);
    assert(GS_CAN_Dequeue(&output) && output.echo_id == 7); assert_empty();
    assert(GS_CAN_SetMode(GS_CAN_MODE_RESET, 0));
    start(GS_CAN_MODE_LOOP_BACK);
    assert(hcan.Init.Mode == CAN_MODE_LOOPBACK);
}

static void test_bus_off_and_recovery(void)
{
    fresh(); start(0);
    GS_HostFrame tx = frame(9), output;
    assert(GS_CAN_Enqueue(&tx)); GS_CAN_Poll();
    for (unsigned i = 0; i < 100; i++) receive(0x111, CAN_ID_STD, CAN_RTR_DATA, 8);
    registers.ESR = CAN_ESR_BOFF | (255U << CAN_ESR_TEC_Pos);
    hcan.ErrorCode = HAL_CAN_ERROR_BOF;
    HAL_CAN_ErrorCallback(&hcan);
    assert(GS_CAN_Dequeue(&output));
    assert(output.echo_id == GS_ECHO_ID_RX && (output.can_id & GS_CAN_ERR_FLAG));
    assert(output.can_id & (1U << 6)); /* Bus-off has priority over a full RX ring. */
    assert(output.data[6] == 255 && output.can_dlc == 8);
    assert(registers.IER == 0 && (registers.MCR & CAN_MCR_INRQ));
    assert(GS_CAN_Enqueue(&tx)); GS_CAN_Poll();
    for (unsigned i = 0; i < 3; i++) assert(!(registers.sTxMailBox[i].TIR & CAN_TI0R_TXRQ));
    /* A reset purges old RX, unsent TX and outstanding echo ownership. */
    assert(GS_CAN_SetMode(GS_CAN_MODE_RESET, 0)); assert_empty();
    start(0); tx.echo_id = 10;
    assert(GS_CAN_Enqueue(&tx)); GS_CAN_Poll(); complete(0);
    assert(GS_CAN_Dequeue(&output) && output.echo_id == 10); assert_empty();
}

static void test_nested_irq_mask(void)
{
    fresh(); start(0);
    GS_HostFrame tx = frame(1), output;
    irq_mask = 1;
    assert(GS_CAN_Enqueue(&tx) && irq_mask == 1);
    GS_CAN_Poll(); assert(irq_mask == 1);
    complete(0); assert(irq_mask == 1);
    assert(GS_CAN_Dequeue(&output) && irq_mask == 1);
    irq_mask = 0;
}

int main(void)
{
    test_timing_and_lifecycle();
    test_start_failures();
    test_tx_mailboxes_and_wire_fields();
    test_credit_and_rx_saturation();
    test_rx_and_invalid_tx();
    test_modes_and_abort_retry();
    test_bus_off_and_recovery();
    test_nested_irq_mask();
    puts("gs_can backend tests passed");
    return 0;
}
