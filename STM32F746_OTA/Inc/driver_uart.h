/*
 * driver_uart.h
 *
 *  Created on: 26 de mar de 2020
 *      Author: gustavo
 */

#ifndef DRIVER_UART_H_
#define DRIVER_UART_H_

#include <stdint.h>
#include "stm32f7xx_hal.h"
#include "stm32f7xx_hal_uart_ex.h"
#include "printf_lib.h"


void InitUART(void);
long UARTGetChar(char *data);
char UARTPutChar(char ucData);
void UARTPutString(char *string, uint16_t size);

/** @addtogroup STM32746G_DISCOVERY_LOW_LEVEL_COM
  * @{
  */
#define COMn                             ((uint8_t)1)

/**
 * @brief Definition for COM port1, connected to USART1
 */
#define DISCOVERY_COM1                          USART1
#define DISCOVERY_COM1_CLK_ENABLE()             __HAL_RCC_USART1_CLK_ENABLE()
#define DISCOVERY_COM1_CLK_DISABLE()            __HAL_RCC_USART1_CLK_DISABLE()

#define DISCOVERY_COM1_TX_PIN                   GPIO_PIN_9
#define DISCOVERY_COM1_TX_GPIO_PORT             GPIOA
#define DISCOVERY_COM1_TX_GPIO_CLK_ENABLE()     __HAL_RCC_GPIOA_CLK_ENABLE()
#define DISCOVERY_COM1_TX_GPIO_CLK_DISABLE()    __HAL_RCC_GPIOA_CLK_DISABLE()
#define DISCOVERY_COM1_TX_AF                    GPIO_AF7_USART1

#define DISCOVERY_COM1_RX_PIN                   GPIO_PIN_7
#define DISCOVERY_COM1_RX_GPIO_PORT             GPIOB
#define DISCOVERY_COM1_RX_GPIO_CLK_ENABLE()     __HAL_RCC_GPIOB_CLK_ENABLE()
#define DISCOVERY_COM1_RX_GPIO_CLK_DISABLE()    __HAL_RCC_GPIOB_CLK_DISABLE()
#define DISCOVERY_COM1_RX_AF                    GPIO_AF7_USART1

#define DISCOVERY_COM1_IRQn                     USART1_IRQn

#define DISCOVERY_COMx_CLK_ENABLE(__INDEX__)            do { if((__INDEX__) == COM1) DISCOVERY_COM1_CLK_ENABLE(); } while(0)
#define DISCOVERY_COMx_CLK_DISABLE(__INDEX__)           (((__INDEX__) == 0) ? DISCOVERY_COM1_CLK_DISABLE() : 0)

#define DISCOVERY_COMx_TX_GPIO_CLK_ENABLE(__INDEX__)    do { if((__INDEX__) == COM1) DISCOVERY_COM1_TX_GPIO_CLK_ENABLE(); } while(0)
#define DISCOVERY_COMx_TX_GPIO_CLK_DISABLE(__INDEX__)   (((__INDEX__) == 0) ? DISCOVERY_COM1_TX_GPIO_CLK_DISABLE() : 0)

#define DISCOVERY_COMx_RX_GPIO_CLK_ENABLE(__INDEX__)    do { if((__INDEX__) == COM1) DISCOVERY_COM1_RX_GPIO_CLK_ENABLE(); } while(0)
#define DISCOVERY_COMx_RX_GPIO_CLK_DISABLE(__INDEX__)   (((__INDEX__) == 0) ? DISCOVERY_COM1_RX_GPIO_CLK_DISABLE() : 0)

#endif /* DRIVER_UART_H_ */
