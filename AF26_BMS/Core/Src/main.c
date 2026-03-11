/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "can.h"
#include "spi.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "data-user.h"
#include "adc-user.h"
#include "ltc681x-user.h"
#include "daisy-user.h"
#include "relay-user.h"
#include "state-user.h"
#include "can-user.h"
#include "os-user.h"
#include "flash-user.h"
// #include "rtc-user.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
const u8 Beep_mute = 0; // 定义蜂鸣器禁止鸣叫位, 1:禁止 0:允许 . 可以修改

BMS A03 = {0};           // 最重要的全局变量E03定义
vu16 adc[num_adc] = {0}; // adc[0]为ADC_PRE, adc[1]为ADC_TS, adc[2]为PRE_STATE, adc[3]为AIR+_STATE, adc[4]为AIR-_STATE

extern int16_t pec15Table[256]; /*以下的在LTC681X.c*/
extern int16_t CRC15_POLY;
extern int16_t re, address;
// extern TASK_COMPONENTS BMS_Tasks[];
extern TaskManager taskManager;
extern MODULE Module[Total_ic];
extern u8 n_Cell[Total_ic];
extern vu8 Trigger_True;

// extern TIME time;

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_CAN1_Init();
  MX_CAN2_Init();
  MX_SPI1_Init();
  MX_SPI3_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
  HAL_ADCEx_Calibration_Start(&hadc1); // ADC采样校准

  // 创建任务，越先创建的任务优先级越高，任务创建的顺序及其itvTime变量的值参考以前的设置（参见os-user.c中的BMS_Tasks的定义）
  TaskCreate(&Precharge_Run_Handle, 0xFFFF, Precharge_Run, NULL);
  TaskCreate(&charge_ing_Handle, 1000, charge_ing, NULL);
  TaskCreate(&BMS_StateMachine_Handle, 50, BMS_StateMachine, NULL);
  TaskCreate(&Hall_Timeout_Handle, 100, Hall_Timeout, NULL);
  TaskCreate(&State_Upgrade_Handle, 50, State_Upgrade, NULL);
  TaskCreate(&Slave_Measure_Handle, 200, Slave_Measure, NULL);
  TaskCreate(&BMS_Can_Handle, 100, BMS_Can, NULL); // 发给主控的CAN报文
  TaskCreate(&IMD_Detect_Handle, 500, IMD_Detect, NULL);
  TaskCreate(&Slave_Balance_Handle, 1000, Slave_Balance, NULL);
  Data_Init();    // BMS的初始化
  BMS_SelfTest(); // 自检
  HAL_TIM_Base_Start_IT(&htim2); // 定时器2使能, 开始操作系统计时
  HAL_TIM_Base_Start_IT(&htim3); // 定时器2使能, 开始操作系统计时
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    TaskScheduler(); // 启动操作系统
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.Prediv1Source = RCC_PREDIV1_SOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  RCC_OscInitStruct.PLL2.PLL2State = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the Systick interrupt time
   */
  __HAL_RCC_PLLI2S_ENABLE();
}

/* USER CODE BEGIN 4 */
// 下点火信号的外部中断
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
}
// 定时器中断
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  // 定时器2: 负责BMS主程序操作系统的任务提供基本定时
  if (htim->Instance == htim2.Instance)
  {
    TaskStatusUpdate();
  }
}

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
