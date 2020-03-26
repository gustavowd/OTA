/*
 * driver_uart.c
 *
 *  Created on: 26 de mar de 2020
 *      Author: gustavo
 */
#include <string.h>
#include "driver_uart.h"
#include "stm32f7xx_hal_gpio.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

typedef enum
{
  COM1 = 0,
  COM2 = 1
}COM_TypeDef;

// Manipulador de configuração da UART da biblioteca da ST
UART_HandleTypeDef huart1;

// Declares a queue structure for the UART
xQueueHandle qUART;

// Declares a semaphore structure for the UART
xSemaphoreHandle sUART;

// Declares a mutex structure for the UART
xSemaphoreHandle mutexTx;

USART_TypeDef* COM_USART[COMn] = {DISCOVERY_COM1};

GPIO_TypeDef* COM_TX_PORT[COMn] = {DISCOVERY_COM1_TX_GPIO_PORT};

GPIO_TypeDef* COM_RX_PORT[COMn] = {DISCOVERY_COM1_RX_GPIO_PORT};

const uint16_t COM_TX_PIN[COMn] = {DISCOVERY_COM1_TX_PIN};

const uint16_t COM_RX_PIN[COMn] = {DISCOVERY_COM1_RX_PIN};

const uint16_t COM_TX_AF[COMn] = {DISCOVERY_COM1_TX_AF};

const uint16_t COM_RX_AF[COMn] = {DISCOVERY_COM1_RX_AF};

/**
  * @brief  Configures COM port.
  * @param  COM: COM port to be configured.
  *          This parameter can be one of the following values:
  *            @arg  COM1
  *            @arg  COM2
  * @param  huart: Pointer to a UART_HandleTypeDef structure that contains the
  *                configuration information for the specified USART peripheral.
  * @retval None
  */
void BSP_COM_Init(COM_TypeDef COM, UART_HandleTypeDef *huart)
{
  GPIO_InitTypeDef gpio_init_structure;

  /* Enable GPIO clock */
  DISCOVERY_COMx_TX_GPIO_CLK_ENABLE(COM);
  DISCOVERY_COMx_RX_GPIO_CLK_ENABLE(COM);

  /* Enable USART clock */
  DISCOVERY_COMx_CLK_ENABLE(COM);

  /* Configure USART Tx as alternate function */
  gpio_init_structure.Pin = COM_TX_PIN[COM];
  gpio_init_structure.Mode = GPIO_MODE_AF_PP;
  gpio_init_structure.Speed = GPIO_SPEED_FAST;
  gpio_init_structure.Pull = GPIO_PULLUP;
  gpio_init_structure.Alternate = COM_TX_AF[COM];
  HAL_GPIO_Init(COM_TX_PORT[COM], &gpio_init_structure);

  /* Configure USART Rx as alternate function */
  gpio_init_structure.Pin = COM_RX_PIN[COM];
  gpio_init_structure.Mode = GPIO_MODE_AF_PP;
  gpio_init_structure.Alternate = COM_RX_AF[COM];
  HAL_GPIO_Init(COM_RX_PORT[COM], &gpio_init_structure);

  /* USART configuration */
  huart->Instance = COM_USART[COM];
  HAL_UART_Init(huart);

  HAL_NVIC_SetPriority(USART1_IRQn, 15, 0);
  HAL_NVIC_EnableIRQ(USART1_IRQn);
}

/**
  * @brief  DeInit COM port.
  * @param  COM: COM port to be configured.
  *          This parameter can be one of the following values:
  *            @arg  COM1
  *            @arg  COM2
  * @param  huart: Pointer to a UART_HandleTypeDef structure that contains the
  *                configuration information for the specified USART peripheral.
  * @retval None
  */
void BSP_COM_DeInit(COM_TypeDef COM, UART_HandleTypeDef *huart)
{
  /* USART configuration */
  huart->Instance = COM_USART[COM];
  HAL_UART_DeInit(huart);

  /* Enable USART clock */
  DISCOVERY_COMx_CLK_DISABLE(COM);

  /* DeInit GPIO pins can be done in the application
     (by surcharging this __weak function) */

  /* GPIO pins clock, DMA clock can be shut down in the application
     by surcharging this __weak function */
}



void InitUART(void){
	  sUART = xSemaphoreCreateBinary();

		if( sUART == NULL )
		{
			/* There was insufficient FreeRTOS heap available for the semaphore to
			be created successfully. */
			vTaskSuspend(NULL);
		}
		else
		{
			mutexTx = xSemaphoreCreateMutex();
			if( mutexTx == NULL )
			{
				/* There was insufficient FreeRTOS heap available for the semaphore to
				be created successfully. */
				vSemaphoreDelete( sUART);
				vTaskSuspend(NULL);
			}else
			{
				qUART = xQueueCreate(128, sizeof(char));
				if( qUART == NULL )
				{
					/* There was insufficient FreeRTOS heap available for the queue to
					be created successfully. */
					vSemaphoreDelete( sUART);
					vSemaphoreDelete( mutexTx);
					vTaskSuspend(NULL);
				}
			}
		}

	  // Inicializa o hardware da UART
	  huart1.Instance = USART1;
	  huart1.Init.BaudRate = 115200;
	  huart1.Init.WordLength = UART_WORDLENGTH_8B;
	  huart1.Init.StopBits = UART_STOPBITS_1;
	  huart1.Init.Parity = UART_PARITY_NONE;
	  huart1.Init.Mode = UART_MODE_TX_RX;
	  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
	  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
	  BSP_COM_Init(COM1, &huart1);
	  /* Habilita interrupções de erro da UART */
	  SET_BIT(huart1.Instance->CR3, USART_CR3_EIE);

	  /* Habilita as interrupções de erro de paridade e buffer vazio */
	  SET_BIT(huart1.Instance->CR1, USART_CR1_PEIE | USART_CR1_RXNEIE);
}

/*
 * Função para receber um caracter pela porta serial.
 */
portBASE_TYPE UARTGetChar(char *data){
	return xQueueReceive(qUART, data, portMAX_DELAY);
}


/* Função chamada dentro da interrupção de UART.
 * Copia dado recebido para uma fila.
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart){
	BaseType_t pxHigherPriorityTaskWokenRX = pdFALSE;
	uint16_t uhdata = (uint16_t) READ_REG(huart->Instance->RDR);
	char data = (uint8_t)(uhdata & 0xFF);
	xQueueSendToBackFromISR(qUART, &data, &pxHigherPriorityTaskWokenRX);

	if (pxHigherPriorityTaskWokenRX == pdTRUE){
		portYIELD();
	}
}


/* Função chamada dentro da interrupção de UART.
 * Informa que um próximo caracter pode ser transmitido.
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart){
	(void)huart;
	BaseType_t pxHigherPriorityTaskWokenTX = pdFALSE;
	xSemaphoreGiveFromISR(sUART, &pxHigherPriorityTaskWokenTX);
	if (pxHigherPriorityTaskWokenTX == pdTRUE){
		portYIELD();
	}
}

/*
 * Função para transmitir um caracter pela porta serial.
 */
char UARTPutChar(char ucData)
{
    // Adquire a porta serial
	if (mutexTx != NULL){
		if (xSemaphoreTake(mutexTx, portMAX_DELAY) == pdTRUE){
			// Envia um caracter
			HAL_UART_Transmit_IT(&huart1,(uint8_t *)&ucData,1);
			// Espera por uma interrupção da UART
			xSemaphoreTake(sUART, portMAX_DELAY);
			// Libera a porta serial
			xSemaphoreGive(mutexTx);
		}
	}
	return '\0';
}


/*
 * Função para transmitir uma string pela porta serial.
 */
void UARTPutString(char *string, uint16_t size){
	// Adquire a porta serial
	if (mutexTx != NULL){
		if (xSemaphoreTake(mutexTx, portMAX_DELAY) == pdTRUE){
			// Descobre o tamanho da string, caso não informado
			if (!size){
				uint8_t *tmp = (uint8_t *)string;

				while(*tmp++){
					size++;
				}
			}

			/* Transmite uma sequencia de dados, com fluxo controlado pela interrupção */
			HAL_UART_Transmit_IT(&huart1,(uint8_t *)string,size);

			// Espera pelo fim da transmissão
			xSemaphoreTake(sUART, portMAX_DELAY);

			xSemaphoreGive(mutexTx);
		}
	}
}
