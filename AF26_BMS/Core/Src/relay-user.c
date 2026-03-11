/*
 * relay.c
 *
 *  Created on: 2022年3月6日
 *      Author: BBBBB
 */
#include "main.h"
#include "relay-user.h"
#include "data-user.h"
#include "state-user.h"

extern u8 Beep_mute;
extern BMS A03;

// AIR+开启
void AIRP_ON()
{
	HAL_GPIO_WritePin(AIRP_SW_GPIO_Port, AIRP_SW_Pin, GPIO_PIN_SET);
}
void AIRP_OFF()
{
	HAL_GPIO_WritePin(AIRP_SW_GPIO_Port, AIRP_SW_Pin, GPIO_PIN_RESET);
}

// AIR-开启
void AIRN_ON()
{
	HAL_GPIO_WritePin(AIRN_SW_GPIO_Port, AIRN_SW_Pin, GPIO_PIN_SET);
}
void AIRN_OFF()
{
	HAL_GPIO_WritePin(AIRN_SW_GPIO_Port, AIRN_SW_Pin, GPIO_PIN_RESET);
}
// PRE开启
void PrechargeAIR_ON()
{
	HAL_GPIO_WritePin(PRE_SW_GPIO_Port, PRE_SW_Pin, GPIO_PIN_SET);
}
void PrechargeAIR_OFF()
{
	HAL_GPIO_WritePin(PRE_SW_GPIO_Port, PRE_SW_Pin, GPIO_PIN_RESET);
}
// 开启风扇
void Fans_ON()
{
	HAL_GPIO_WritePin(Fans_SW_GPIO_Port, Fans_SW_Pin, GPIO_PIN_SET);
}
void Fans_OFF()
{
	HAL_GPIO_WritePin(Fans_SW_GPIO_Port, Fans_SW_Pin, GPIO_PIN_RESET);
}
// 关闭所有继电器
void AIRS_OFF()
{
	PrechargeAIR_OFF();
	AIRN_OFF();
	AIRP_OFF();
}
/*
检测点火信号
没有点火信号时为低
有点火信号时光耦开启,为高
*/
u8 ON_State()
{
	return HAL_GPIO_ReadPin(Trigger_GPIO_Port, Trigger_Pin);
}
/*
检测IMD故障信号, IMD正常时候为0, IMD发生接地故障时候为1
*/
u8 IMD_State()
{
	return HAL_GPIO_ReadPin(IMD_IN_GPIO_Port, IMD_IN_Pin);
}
// BMS错误警告, 蜂鸣器鸣叫, 进入故障状态, 关闭所有继电器
void Error()
{
	if (A03.State.BMS_State != BMS_Manual_MODE)
	{ // 注意不要工装模式下检测到东西报错, 然后引发马上退出工装模式的BUG
		if (!(Beep_mute))
		{
			HAL_GPIO_WritePin(Error_Beep_GPIO_Port, Error_Beep_Pin, GPIO_PIN_SET); // 如果禁止位不打开, 则蜂鸣器鸣叫
			// 弄一弄根据不同报警状态发出不同长度叫声的蜂鸣器
		}
		HAL_GPIO_WritePin(BMS_ERROR_GPIO_Port, BMS_ERROR_Pin, GPIO_PIN_RESET); // BMS故障状态输出
		A03.State.BMS_State = BMS_ERROR;									   // BMS进入故障状态
		AIRS_OFF();															   // 关闭所有继电器
	}
}

void ALL_Error_mute_Beep()
{
	if (A03.State.BMS_State == BMS_ERROR)
	{
		if ((A03.Alarm.error_mute == ALL_Error_mute) | (A03.Alarm.error_mute == ALL_Error_mute_Abnormal))
		{
			if (!(Beep_mute))
			{
				HAL_GPIO_WritePin(Error_Beep_GPIO_Port, Error_Beep_Pin, GPIO_PIN_SET); // 如果禁止位不打开, 则蜂鸣器鸣叫
				HAL_Delay(1000);
				HAL_GPIO_WritePin(Error_Beep_GPIO_Port, Error_Beep_Pin, GPIO_PIN_RESET);
				HAL_Delay(500);
				HAL_GPIO_WritePin(Error_Beep_GPIO_Port, Error_Beep_Pin, GPIO_PIN_SET); // 如果禁止位不打开, 则蜂鸣器鸣叫
				HAL_Delay(1000);
				HAL_GPIO_WritePin(Error_Beep_GPIO_Port, Error_Beep_Pin, GPIO_PIN_RESET);
				HAL_Delay(500);
				HAL_GPIO_WritePin(Error_Beep_GPIO_Port, Error_Beep_Pin, GPIO_PIN_SET); // 如果禁止位不打开, 则蜂鸣器鸣叫
				HAL_Delay(1000);
				HAL_GPIO_WritePin(Error_Beep_GPIO_Port, Error_Beep_Pin, GPIO_PIN_RESET);
				HAL_Delay(500);
				HAL_GPIO_WritePin(Error_Beep_GPIO_Port, Error_Beep_Pin, GPIO_PIN_SET); // 如果禁止位不打开, 则蜂鸣器鸣叫
				HAL_Delay(1000);
				HAL_GPIO_WritePin(Error_Beep_GPIO_Port, Error_Beep_Pin, GPIO_PIN_RESET);
				HAL_Delay(500);
				HAL_GPIO_WritePin(Error_Beep_GPIO_Port, Error_Beep_Pin, GPIO_PIN_SET); // 如果禁止位不打开, 则蜂鸣器鸣叫
				HAL_Delay(1000);
				HAL_GPIO_WritePin(Error_Beep_GPIO_Port, Error_Beep_Pin, GPIO_PIN_RESET);
				HAL_Delay(500);
			}
		}
	}
}

/*点火状态下蜂鸣器鸣叫*/
void ON_Beep()
{
	HAL_GPIO_WritePin(Error_Beep_GPIO_Port, Error_Beep_Pin, GPIO_PIN_SET); // 蜂鸣器鸣叫100ms
	HAL_Delay(100);
	HAL_GPIO_WritePin(Error_Beep_GPIO_Port, Error_Beep_Pin, GPIO_PIN_RESET); // 则蜂鸣器关闭100ms
	HAL_Delay(100);
	HAL_GPIO_WritePin(Error_Beep_GPIO_Port, Error_Beep_Pin, GPIO_PIN_SET); // 蜂鸣器鸣叫100ms
	HAL_Delay(100);
	HAL_GPIO_WritePin(Error_Beep_GPIO_Port, Error_Beep_Pin, GPIO_PIN_RESET); // 则蜂鸣器关闭100ms
	HAL_Delay(100);
	HAL_GPIO_WritePin(Error_Beep_GPIO_Port, Error_Beep_Pin, GPIO_PIN_SET); // 蜂鸣器鸣叫100ms
	HAL_Delay(100);
	HAL_GPIO_WritePin(Error_Beep_GPIO_Port, Error_Beep_Pin, GPIO_PIN_RESET); // 则蜂鸣器关闭100ms
	HAL_Delay(100);
	HAL_GPIO_WritePin(Error_Beep_GPIO_Port, Error_Beep_Pin, GPIO_PIN_SET); // 蜂鸣器鸣叫100ms
	HAL_Delay(1000);
	HAL_GPIO_WritePin(Error_Beep_GPIO_Port, Error_Beep_Pin, GPIO_PIN_RESET); // 则蜂鸣器关闭1s
}
