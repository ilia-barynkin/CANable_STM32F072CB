/*
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 Karl Yamashita
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
 */

/*
 * GPIO_Handler.c
 *
 *  Created on: Jul 30, 2025
 *      Author: karl.yamashita
 */


#include "main.h"

#ifdef BLUEPILL
/* TX and RX share one LED. Keep independent states so overlapping flashes
 * cannot cancel each other out on the physical pin. */
static bool txLedOn;
static bool rxLedOn;

static void LED_Activity_Update(void)
{
	HAL_GPIO_WritePin(LED_ACTIVITY_GPIO_Port, LED_ACTIVITY_Pin,
			(txLedOn || rxLedOn) ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void LED_Green(bool state)
{
	txLedOn = state;
	LED_Activity_Update();
}

void LED_Blue(bool state)
{
	rxLedOn = state;
	LED_Activity_Update();
}

void LED_Green_Toggle(void)
{
	LED_Green(!txLedOn);
}

void LED_Blue_Toggle(void)
{
	LED_Blue(!rxLedOn);
}

void LED_Green_Off(void)
{
	LED_Green(false);
}

void LED_Blue_Off(void)
{
	LED_Blue(false);
}

void LED_InnomakerStatus(void)
{
}
#else
// TX
void LED_Green_Toggle(void)
{
	HAL_GPIO_TogglePin(LED_PA0_GPIO_Port, LED_PA0_Pin);
	HAL_GPIO_TogglePin(LED_B0_GPIO_Port, LED_B0_Pin); // Jhoinrch
	HAL_GPIO_TogglePin(LED_B2_GPIO_Port, LED_B2_Pin); // Innomaker
}

// RX
void LED_Blue_Toggle(void)
{
	HAL_GPIO_TogglePin(LED_PA1_GPIO_Port, LED_PA1_Pin);
	HAL_GPIO_TogglePin(LED_B1_GPIO_Port, LED_B1_Pin); // Jhoinrch
	HAL_GPIO_TogglePin(LED_B10_GPIO_Port, LED_B10_Pin); // Innomaker
}

void LED_InnomakerStatus(void)
{
	HAL_GPIO_TogglePin(LED_B11_GPIO_Port, LED_B11_Pin); // Innomaker status LED
}

void LED_Green(bool state)
{
#if defined (P_CAN_07e) || defined (CANABLE_V1_0_PRO) || defined (JHOINRCH)
	if(state)
	{
		HAL_GPIO_WritePin(LED_PA0_GPIO_Port, LED_PA0_Pin, GPIO_PIN_RESET); // On
		HAL_GPIO_WritePin(LED_B0_GPIO_Port, LED_B0_Pin, GPIO_PIN_RESET); // On // Jhoinrch
	}
	else
	{
		HAL_GPIO_WritePin(LED_PA0_GPIO_Port, LED_PA0_Pin, GPIO_PIN_SET); // Off
		HAL_GPIO_WritePin(LED_B0_GPIO_Port, LED_B0_Pin, GPIO_PIN_SET); // Off // Jhoinrch
	}
#endif
}

void LED_Blue(bool state)
{
#if defined (P_CAN_07e) || defined (CANABLE_V1_0_PRO) || defined (JHOINRCH)
	if(state)
	{
		HAL_GPIO_WritePin(LED_PA1_GPIO_Port, LED_PA1_Pin, GPIO_PIN_RESET); // On
		HAL_GPIO_WritePin(LED_B1_GPIO_Port, LED_B1_Pin, GPIO_PIN_RESET); // On // Jhoinrch
	}
	else
	{
		HAL_GPIO_WritePin(LED_PA1_GPIO_Port, LED_PA1_Pin, GPIO_PIN_SET); // Off
		HAL_GPIO_WritePin(LED_B1_GPIO_Port, LED_B1_Pin, GPIO_PIN_SET); // Off // Jhoinrch
	}
#endif
}

void LED_Green_Off(void)
{
#if defined (P_CAN_07e) || defined (CANABLE_V1_0_PRO) || defined (JHOINRCH)
	HAL_GPIO_WritePin(LED_PA0_GPIO_Port, LED_PA0_Pin, GPIO_PIN_SET); // Off
	HAL_GPIO_WritePin(LED_B0_GPIO_Port, LED_B0_Pin, GPIO_PIN_SET); // Off // Off // Jhoinrch
#else
	HAL_GPIO_WritePin(LED_PA0_GPIO_Port, LED_PA0_Pin, GPIO_PIN_RESET); // Off
#endif
}

void LED_Blue_Off(void)
{
#if defined (P_CAN_07e) || defined (CANABLE_V1_0_PRO) || defined (JHOINRCH)
	HAL_GPIO_WritePin(LED_PA1_GPIO_Port, LED_PA1_Pin, GPIO_PIN_SET); // Off
	HAL_GPIO_WritePin(LED_B1_GPIO_Port, LED_B1_Pin, GPIO_PIN_SET); // Off // Off // Jhoinrch
#else
	HAL_GPIO_WritePin(LED_PA1_GPIO_Port, LED_PA1_Pin, GPIO_PIN_RESET); // Off
#endif
}
#endif


