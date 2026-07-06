/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define KEY2_Pin GPIO_PIN_2
#define KEY2_GPIO_Port GPIOE
#define KEY1_Pin GPIO_PIN_3
#define KEY1_GPIO_Port GPIOE
#define KEY0_Pin GPIO_PIN_4
#define KEY0_GPIO_Port GPIOE
#define OV7670_D6_Pin GPIO_PIN_5
#define OV7670_D6_GPIO_Port GPIOE
#define OV7670_D7_Pin GPIO_PIN_6
#define OV7670_D7_GPIO_Port GPIOE
#define ESP8266_CH_Pin GPIO_PIN_6
#define ESP8266_CH_GPIO_Port GPIOF
#define BEEP_Pin GPIO_PIN_8
#define BEEP_GPIO_Port GPIOF
#define LED0_Pin GPIO_PIN_9
#define LED0_GPIO_Port GPIOF
#define LED1_Pin GPIO_PIN_10
#define LED1_GPIO_Port GPIOF
#define ESP8266_RAT_Pin GPIO_PIN_0
#define ESP8266_RAT_GPIO_Port GPIOC
#define KEY_UP_Pin GPIO_PIN_0
#define KEY_UP_GPIO_Port GPIOA
#define OV7670_RRST_Pin GPIO_PIN_4
#define OV7670_RRST_GPIO_Port GPIOA
#define OV7670_RCK_Pin GPIO_PIN_6
#define OV7670_RCK_GPIO_Port GPIOA
#define TP_INT_Pin GPIO_PIN_1
#define TP_INT_GPIO_Port GPIOB
#define FLASH_CS_Pin GPIO_PIN_14
#define FLASH_CS_GPIO_Port GPIOB
#define LCD_BL_Pin GPIO_PIN_15
#define LCD_BL_GPIO_Port GPIOB
#define NRF_CS_Pin GPIO_PIN_7
#define NRF_CS_GPIO_Port GPIOG
#define OV7670_D0_Pin GPIO_PIN_6
#define OV7670_D0_GPIO_Port GPIOC
#define OV7670_D1_Pin GPIO_PIN_7
#define OV7670_D1_GPIO_Port GPIOC
#define OV7670_D2_Pin GPIO_PIN_8
#define OV7670_D2_GPIO_Port GPIOC
#define OV7670_D3_Pin GPIO_PIN_9
#define OV7670_D3_GPIO_Port GPIOC
#define OV7670_VSYNC_Pin GPIO_PIN_8
#define OV7670_VSYNC_GPIO_Port GPIOA
#define OV7670_VSYNC_EXTI_IRQn EXTI9_5_IRQn
#define OV7670_D4_Pin GPIO_PIN_11
#define OV7670_D4_GPIO_Port GPIOC
#define ETH_RST_Pin GPIO_PIN_3
#define ETH_RST_GPIO_Port GPIOD
#define SCCB_SCL_Pin GPIO_PIN_6
#define SCCB_SCL_GPIO_Port GPIOD
#define OV7670_WREN_Pin GPIO_PIN_9
#define OV7670_WREN_GPIO_Port GPIOG
#define OV7670_CS_Pin GPIO_PIN_15
#define OV7670_CS_GPIO_Port GPIOG
#define OV7670_D5_Pin GPIO_PIN_6
#define OV7670_D5_GPIO_Port GPIOB
#define OV7670_WRST_Pin GPIO_PIN_7
#define OV7670_WRST_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
