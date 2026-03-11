/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
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
#include "stm32f1xx_hal.h"

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
#define MCU_RUN_Pin GPIO_PIN_0
#define MCU_RUN_GPIO_Port GPIOC
#define ADC_PRE_Pin GPIO_PIN_0
#define ADC_PRE_GPIO_Port GPIOA
#define ADC_TS_Pin GPIO_PIN_1
#define ADC_TS_GPIO_Port GPIOA
#define BMS_ERROR_Pin GPIO_PIN_2
#define BMS_ERROR_GPIO_Port GPIOA
#define SPI1_NSS_Pin GPIO_PIN_4
#define SPI1_NSS_GPIO_Port GPIOA
#define AIRP_State_Pin GPIO_PIN_14
#define AIRP_State_GPIO_Port GPIOB
#define AIRN_State_Pin GPIO_PIN_15
#define AIRN_State_GPIO_Port GPIOB
#define PRE_State_Pin GPIO_PIN_6
#define PRE_State_GPIO_Port GPIOC
#define Error_Beep_Pin GPIO_PIN_7
#define Error_Beep_GPIO_Port GPIOC
#define Trigger_Pin GPIO_PIN_8
#define Trigger_GPIO_Port GPIOC
#define Trigger_EXTI_IRQn EXTI9_5_IRQn
#define AIRP_SW_Pin GPIO_PIN_9
#define AIRP_SW_GPIO_Port GPIOC
#define PRE_SW_Pin GPIO_PIN_8
#define PRE_SW_GPIO_Port GPIOA
#define AIRN_SW_Pin GPIO_PIN_9
#define AIRN_SW_GPIO_Port GPIOA
#define Fans_SW_Pin GPIO_PIN_10
#define Fans_SW_GPIO_Port GPIOA
#define FLASH_CS_Pin GPIO_PIN_2
#define FLASH_CS_GPIO_Port GPIOD
#define IMD_IN_Pin GPIO_PIN_7
#define IMD_IN_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
