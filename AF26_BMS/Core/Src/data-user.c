/*
 * data.c
 *
 *  Created on: Mar 9, 2022
 *      Author: BBBBB
 */

#include "adc-user.h"
#include "can-user.h"
#include "data-user.h"
#include "state-user.h"
#include "ltc681x-user.h"
#include "daisy-user.h"
#include "flash-user.h"
// #include "rtc-user.h"
#include "main.h"

extern BMS A03;
extern cell_asic IC[Total_ic];
extern MODULE Module[Total_ic];
extern u8 n_Cell[Total_ic];
extern CAN_HandleTypeDef hcan1;
extern CAN_HandleTypeDef hcan2;

void Data_Init()
{								   /*采集板相关的初始化*/
	init_PEC15_Table();			   /*必用, 从控板PEC计算相关参数初始化*/
	Module_Init(Module, Total_ic); /*MODULE数组的初始化*/

	for (u8 nic = 0; nic < Total_ic; nic++)
	{ // 定义好采集的数量
		switch (nic)
		{
		case 0:
			n_Cell[nic] = MODULE1;
			break;
		case 1:
			n_Cell[nic] = MODULE2;
			break;
		case 2:
			n_Cell[nic] = MODULE3;
			break;
		case 3:
			n_Cell[nic] = MODULE4;
			break;
		case 4:
			n_Cell[nic] = MODULE5;
			break;
		case 5:
			n_Cell[nic] = MODULE6;
			break;
		case 6:
			n_Cell[nic] = MODULE7;
			break;
		case 7:
			n_Cell[nic] = MODULE8;
			break;
		}
	}
	/*配置状态*/
	LTC681x_init_cfg(Total_ic, IC);	 /*辅助函数, 将ic的CFGA相关的全部初始化*/
	LTC6813_init_cfgb(Total_ic, IC); /*辅助函数, 将ic的CFGB相关的全部初始化*/
	wakeup_idle(Total_ic);
	LTC6813_wrcfg(Total_ic, IC);  /*发送命令,修改DCC位*/
	LTC6813_wrcfgb(Total_ic, IC); /*发送命令,修改DCC位*/

	/*CAN相关的初始化*/
	can1_interal_config(); // CAN1使能
	can2_exteral_config();
	// CAN2使能
	/*以下为阈值的配置*/
	// State部分初始化
	A03.State.BMS_State = BMS_INIT;
	A03.State.SOC = 888; // 随便设置的初值
	A03.State.Charge_flag = 0;
	// cell部分清零
	for (u8 i = 0; i < (sizeof(A03.Cell.Vol) / sizeof(vu16)); i++)
		A03.Cell.Vol[i] = 0;
	for (u8 i = 0; i < (sizeof(A03.Cell.Temp) / sizeof(vu16)); i++)
		A03.Cell.Temp[i] = 0;
	for (u8 i = 0; i < (sizeof(A03.Cell.DCC) / sizeof(vu8)); i++)
		A03.Cell.DCC[i] = 0;
	A03.Cell.MaxVol = 0;
	A03.Cell.MinVol = 0;
	A03.Cell.MaxTemp = 0;
	A03.Cell.MinTemp = 0;
	A03.Cell.MaxVol_No = 0;
	A03.Cell.MinVol_No = 0;
	A03.Cell.MaxTemp_No = 0;
	A03.Cell.MinTemp_No = 0;
	// Relay部分清零
	A03.Relay.AIRP_State = REALY_OFF;
	A03.Relay.AIRN_State = REALY_OFF;
	A03.Relay.PrechargeAIR_State = REALY_OFF;
	// Alarm部分清零
	A03.Alarm.OV = 0;
	A03.Alarm.UV = 0;
	A03.Alarm.OT = 0;
	A03.Alarm.UT = 0;
	A03.Alarm.ODV = 0;
	A03.Alarm.ODT = 0;
	A03.Alarm.Vol_Openwires = 0;
	A03.Alarm.Temp_Openwires = 0;
	A03.Alarm.StaticDischarge_OC = 0;
	A03.Alarm.StaticCharge_OC = 0;
	A03.Alarm.Chargemachine_Error = 0;
	A03.Alarm.IMD_Error = 0;
	A03.Alarm.Hall_Openwires = 0;
	A03.Alarm.Hall_Error = 0;
	A03.Alarm.Daisy_Openwires = 0; // 没有引用
	A03.Alarm.Precharge_Fail = 0;
	A03.Alarm.Relay_Error = 0;
	A03.Alarm.Vol_Error = 0;
	for (u8 i = 0; i < (sizeof(A03.Alarm.Openwire_Vol) / sizeof(vu8)); i++)
		A03.Alarm.Openwire_Vol[i] = 0;
	for (u8 i = 0; i < (sizeof(A03.Alarm.Openwire_Temp) / sizeof(vu8)); i++)
		A03.Alarm.Openwire_Temp[i] = 0;
	// 阈值部分读取
	A03.State.Init_flag = 0; // 防止数据乱跑, 先给个0, 最终结果要看下面函数FLASH读回来的
	HAL_Delay(350);
	Threshold_Update(); // 该函数还会在CAN中出现, 都是为了修改阈值的函数
	// 充电参数初始化，即默认值
	A03.charge.ChargeVol_tx = 6000;
	A03.charge.ChargeCur_tx = 30; // 有时候充电电流会过大导致跳闸，因此设置电流为3A，充电电流可在上位机调
	// 读取上一次关机的屏蔽位
	/*A03.Alarm.error_mute=0;
	for(int i = 0; i < 32; i++){
		if(i != 11)
		A03.Alarm.error_mute |= (1 << i);
	}
	Flash_Write_Errormute();*/
	Flash_Read_Errormute();
}
