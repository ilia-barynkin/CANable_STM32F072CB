#ifndef TEST_CAN_MAIN_H
#define TEST_CAN_MAIN_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

typedef enum { HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
typedef enum { DISABLE, ENABLE } FunctionalState;
typedef enum {
    HAL_CAN_STATE_RESET, HAL_CAN_STATE_READY, HAL_CAN_STATE_LISTENING,
    HAL_CAN_STATE_SLEEP_PENDING, HAL_CAN_STATE_SLEEP_ACTIVE, HAL_CAN_STATE_ERROR
} HAL_CAN_StateTypeDef;

typedef struct {
    uint32_t TIR, TDTR, TDLR, TDHR;
} CAN_TxMailBox_TypeDef;

typedef struct {
    uint32_t MCR, MSR, TSR, RF0R, RF1R, IER, ESR, BTR;
    CAN_TxMailBox_TypeDef sTxMailBox[3];
} CAN_TypeDef;

typedef struct {
    uint32_t Prescaler, Mode, SyncJumpWidth, TimeSeg1, TimeSeg2;
    FunctionalState TimeTriggeredMode, AutoBusOff, AutoWakeUp;
    FunctionalState AutoRetransmission, ReceiveFifoLocked, TransmitFifoPriority;
} CAN_InitTypeDef;

typedef struct {
    CAN_TypeDef *Instance;
    CAN_InitTypeDef Init;
    HAL_CAN_StateTypeDef State;
    uint32_t ErrorCode;
} CAN_HandleTypeDef;

typedef struct {
    uint32_t FilterIdHigh, FilterIdLow, FilterMaskIdHigh, FilterMaskIdLow;
    uint32_t FilterFIFOAssignment, FilterBank, FilterMode, FilterScale;
    uint32_t FilterActivation, SlaveStartFilterBank;
} CAN_FilterTypeDef;

typedef struct {
    uint32_t StdId, ExtId, IDE, RTR, DLC;
    FunctionalState TransmitGlobalTime;
} CAN_TxHeaderTypeDef;

typedef struct {
    uint32_t StdId, ExtId, IDE, RTR, DLC, Timestamp, FilterMatchIndex;
} CAN_RxHeaderTypeDef;

#define CAN_MODE_NORMAL 0U
#define CAN_MODE_LOOPBACK (1U << 30)
#define CAN_MODE_SILENT (1U << 31)
#define CAN_MODE_SILENT_LOOPBACK (CAN_MODE_SILENT | CAN_MODE_LOOPBACK)
#define CAN_ID_STD 0U
#define CAN_ID_EXT 4U
#define CAN_RTR_DATA 0U
#define CAN_RTR_REMOTE 2U
#define CAN_RX_FIFO0 0U
#define CAN_FILTER_FIFO0 0U
#define CAN_FILTERMODE_IDMASK 0U
#define CAN_FILTERSCALE_32BIT 1U
#define CAN_TX_MAILBOX0 1U
#define CAN_TX_MAILBOX1 2U
#define CAN_TX_MAILBOX2 4U
#define CAN_BTR_TS1_Pos 16U
#define CAN_BTR_TS2_Pos 20U
#define CAN_BTR_SJW_Pos 24U
#define CAN_TI0R_EXID_Pos 3U
#define CAN_TI0R_STID_Pos 21U
#define CAN_TI0R_TXRQ 1U
#define CAN_MCR_INRQ 1U
#define CAN_RF0R_FMP0 3U
#define CAN_RF0R_FULL0 (1U << 3)
#define CAN_RF0R_FOVR0 (1U << 4)
#define CAN_RF0R_RFOM0 (1U << 5)
#define CAN_TSR_CODE_Pos 24U
#define CAN_TSR_CODE (3U << CAN_TSR_CODE_Pos)
#define CAN_TSR_TME0 (1U << 26)
#define CAN_TSR_TME1 (1U << 27)
#define CAN_TSR_TME2 (1U << 28)
#define CAN_TSR_RQCP0 (1U << 0)
#define CAN_TSR_RQCP1 (1U << 8)
#define CAN_TSR_RQCP2 (1U << 16)
#define CAN_TSR_TXOK0 (1U << 1)
#define CAN_TSR_TXOK1 (1U << 9)
#define CAN_TSR_TXOK2 (1U << 17)
#define CAN_ESR_BOFF (1U << 2)
#define CAN_ESR_EWGF (1U << 0)
#define CAN_ESR_EPVF (1U << 1)
#define CAN_ESR_TEC_Pos 16U
#define CAN_ESR_TEC (0xFFU << CAN_ESR_TEC_Pos)
#define CAN_ESR_REC_Pos 24U
#define CAN_ESR_REC (0xFFU << CAN_ESR_REC_Pos)
#define CAN_IT_TX_MAILBOX_EMPTY (1U << 0)
#define CAN_IT_RX_FIFO0_MSG_PENDING (1U << 1)
#define CAN_IT_RX_FIFO0_FULL (1U << 2)
#define CAN_IT_RX_FIFO0_OVERRUN (1U << 3)
#define CAN_IT_ERROR_WARNING (1U << 8)
#define CAN_IT_ERROR_PASSIVE (1U << 9)
#define CAN_IT_BUSOFF (1U << 10)
#define CAN_IT_LAST_ERROR_CODE (1U << 11)
#define CAN_IT_ERROR (1U << 15)
#define HAL_CAN_ERROR_NONE 0U
#define HAL_CAN_ERROR_BOF (1U << 2)
#define HAL_CAN_ERROR_RX_FOV0 (1U << 9)
#define HAL_CAN_ERROR_RX_FOV1 (1U << 10)
#define HAL_CAN_ERROR_TX_ALST0 (1U << 11)
#define HAL_CAN_ERROR_TX_TERR0 (1U << 12)
#define HAL_CAN_ERROR_TX_ALST1 (1U << 13)
#define HAL_CAN_ERROR_TX_TERR1 (1U << 14)
#define HAL_CAN_ERROR_TX_ALST2 (1U << 15)
#define HAL_CAN_ERROR_TX_TERR2 (1U << 16)
#define CEC_CAN_IRQn 30
#define UNUSED(value) ((void)(value))
#define SET_BIT(reg, bit) ((reg) |= (bit))

extern CAN_HandleTypeDef hcan;
uint32_t __get_PRIMASK(void);
void __disable_irq(void);
void __set_PRIMASK(uint32_t value);
void __DMB(void);
void HAL_NVIC_DisableIRQ(int irq);
void HAL_NVIC_EnableIRQ(int irq);
void HAL_NVIC_ClearPendingIRQ(int irq);
uint32_t HAL_GetTick(void);
HAL_StatusTypeDef HAL_CAN_Init(CAN_HandleTypeDef *handle);
HAL_StatusTypeDef HAL_CAN_DeInit(CAN_HandleTypeDef *handle);
HAL_StatusTypeDef HAL_CAN_Start(CAN_HandleTypeDef *handle);
HAL_StatusTypeDef HAL_CAN_Stop(CAN_HandleTypeDef *handle);
HAL_CAN_StateTypeDef HAL_CAN_GetState(const CAN_HandleTypeDef *handle);
HAL_StatusTypeDef HAL_CAN_ConfigFilter(CAN_HandleTypeDef *handle,
                                     const CAN_FilterTypeDef *filter);
HAL_StatusTypeDef HAL_CAN_ActivateNotification(CAN_HandleTypeDef *handle, uint32_t mask);
HAL_StatusTypeDef HAL_CAN_DeactivateNotification(CAN_HandleTypeDef *handle, uint32_t mask);
HAL_StatusTypeDef HAL_CAN_AbortTxRequest(CAN_HandleTypeDef *handle, uint32_t mask);
uint32_t HAL_CAN_GetTxMailboxesFreeLevel(const CAN_HandleTypeDef *handle);
HAL_StatusTypeDef HAL_CAN_AddTxMessage(CAN_HandleTypeDef *handle,
    const CAN_TxHeaderTypeDef *header, const uint8_t data[], uint32_t *mailbox);
uint32_t HAL_CAN_GetRxFifoFillLevel(const CAN_HandleTypeDef *handle, uint32_t fifo);
HAL_StatusTypeDef HAL_CAN_GetRxMessage(CAN_HandleTypeDef *handle, uint32_t fifo,
    CAN_RxHeaderTypeDef *header, uint8_t data[]);
uint32_t HAL_CAN_GetError(const CAN_HandleTypeDef *handle);
HAL_StatusTypeDef HAL_CAN_ResetError(CAN_HandleTypeDef *handle);
void LED_Green_Toggle(void);
void LED_Blue_Toggle(void);
void LED_Green(bool state);
void LED_Blue(bool state);
void LED_Green_Off(void);
void LED_Blue_Off(void);

typedef struct { unsigned unused; } TimerCallbackStruct;
extern TimerCallbackStruct timerCallback;
void TimerCallbackRegisterOnly(TimerCallbackStruct *timer, void (*callback)(void));
void TimerCallbackRegister2nd(TimerCallbackStruct *timer, void (*first)(void),
                              void (*second)(void));
void TimerCallbackRepetitionStart(TimerCallbackStruct *timer, void (*callback)(void),
                                  uint32_t period, uint32_t repetitions);

void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef *handle);
void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef *handle);
void HAL_CAN_TxMailbox2CompleteCallback(CAN_HandleTypeDef *handle);
void HAL_CAN_TxMailbox0AbortCallback(CAN_HandleTypeDef *handle);
void HAL_CAN_TxMailbox1AbortCallback(CAN_HandleTypeDef *handle);
void HAL_CAN_TxMailbox2AbortCallback(CAN_HandleTypeDef *handle);
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *handle);
void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *handle);

#endif
