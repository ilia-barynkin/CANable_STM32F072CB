#ifndef TEST_USBD_CONF_H
#define TEST_USBD_CONF_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Hardware boundary only; USB structures come from the production ST headers. */
typedef struct { unsigned unused; } USB_TypeDef;
typedef struct { USB_TypeDef *Instance; } PCD_HandleTypeDef;
typedef enum { HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
HAL_StatusTypeDef HAL_PCD_EP_Abort(PCD_HandleTypeDef *pcd, uint8_t endpoint);
uint32_t HAL_RCC_GetPCLK1Freq(void);
uint32_t __get_PRIMASK(void);
void __disable_irq(void);
void __set_PRIMASK(uint32_t mask);
void test_clear_tx_ctr(USB_TypeDef *instance, uint8_t endpoint);
void test_clear_rx_ctr(USB_TypeDef *instance, uint8_t endpoint);
#define PCD_CLEAR_TX_EP_CTR(instance, endpoint) test_clear_tx_ctr(instance, endpoint)
#define PCD_CLEAR_RX_EP_CTR(instance, endpoint) test_clear_rx_ctr(instance, endpoint)

#define USBD_UsrLog(...) ((void)0)
#define USBD_ErrLog(...) ((void)0)
#define USBD_DbgLog(...) ((void)0)
#define UNUSED(value) ((void)(value))

#endif
