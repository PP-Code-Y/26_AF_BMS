/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    rtc.h
  * @brief   This file contains all the function prototypes for
  *          the rtc.c file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2022 STMicroelectronics.
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
#ifndef RTC_RTC_H_
#define RTC_RTC_H_

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "data-user.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

extern RTC_HandleTypeDef hrtc;

/* USER CODE BEGIN Private defines */
typedef struct
{
    RTC_TimeTypeDef  OFF_Time;
	RTC_DateTypeDef  OFF_Date;
	RTC_TimeTypeDef	 ON_Time;
	RTC_DateTypeDef	 ON_Date;
}TIME;//存储上下电时间的结构体

/* USER CODE END Private defines */

void MX_RTC_Init(void);

/* USER CODE BEGIN Prototypes */
void user_CheckRtcBkup(void);
void Get_OFF_Time(void);
void Get_ON_Time(void);
void SOC_Init(u16 minvol);
void Save_OFFtime(void);
void SOC_CAL(void);
/* USER CODE END Prototypes */

#endif /* RTC_RTC_H_ */
