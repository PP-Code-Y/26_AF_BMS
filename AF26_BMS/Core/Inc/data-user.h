/*
 * data.h
 *
 *  Created on: 2022年3月3日
 *      Author: BBBBB
 *      brief: 定义好BMS各个数据
 *      如果更换电池的方案, 需要将数组的各个值修改好
 */

#ifndef DATA_DATA_H_
#define DATA_DATA_H_

/*输出状态宏定义*/
#ifndef TRUE
#define TRUE 1
#endif

#ifndef FAULT
#define FAULT 0
#endif
/*include*/
#include "stdint.h"
/*类型定义*/
typedef volatile uint8_t vu8;
typedef volatile uint16_t vu16;
typedef volatile uint32_t vu32;

typedef  uint8_t u8;
typedef  uint16_t u16;
typedef  uint32_t u32;

typedef volatile int32_t  vs32;
typedef volatile int16_t  vs16;
typedef volatile int8_t   vs8;

#define BMS_START_ID 0X100
#define BMS_PKL 96

/*大结构体定义*/
typedef struct
{
	vs32 TotalCurrent;	//电流,单位mA, 若为正则为放电 为负则为充电
	vu16 TotalVoltage_cal;//累计总压, 单位0.1V/bit, 如435, 即43500mV==43.5V
	vu16 TotalVoltage;	  //霍尔总压, 单位0.1V/bit, 如435, 即43500mV==43.5V
	vu16 PrechargeVoltage;//预充电压, 单位0.1V/bit, 如435, 即43500mV==43.5V
	vu16 SOC;
	vu16 RTC_Time;	//上次下电RTC计时-后续改成实时时钟
	vu8  BMS_State;	//BMS状态
	vu8  Init_flag;	//阈值是否曾经修改
	vu8  Charge_flag; //是否收到充电机CAN数据, 0:无; 1:有。
					  //charge_flag该变量仅作为BMS充电部分内部判别使用，并不会通过CAN发给上位机，也不用担心会影响到CAN发送的地址偏移问题，因为state的发送刚好偏移到上一个byte就结束了，不会到这个来
}BMS_State;

/**/
typedef struct
{
	vu16 Vol[144];	//单体电压, 单位mv, 如4000代表4000mv
	vu16 Temp[48];	//电芯温度, 单位℃, 如25代表25℃
	vu16 MaxVol;	//最高单体电压, 单位mv, 如4000代表4000mv
	vu16 MinVol;	//最低单体电压, 单位℃, 如25代表25℃
	vu16 MaxTemp;	//最高单体温度, 单位℃, 如25代表25℃
	vu16 MinTemp;	//最低单体温度, 单位℃, 如25代表25℃
	vu8  DCC[18];	//均衡控制位, 0为不开启, 1位开启, 如主负往上数的第一和第二块电池开了均衡, 就是DCC[0]=0x03
	vu8  MaxVol_No;	//最高电压序号
	vu8  MinVol_No;	//最低电压序号
	vu8  MaxTemp_No;//最高温度序号
	vu8  MinTemp_No;//最高温度序号
}Cell_State;

/*
READ ONLY
*/
typedef struct
{
	vu8 AIRP_State;
	vu8 AIRN_State;
	vu8 PrechargeAIR_State;
	vu8 Fans_State;
}Relay_State;
/*

*/
typedef struct
{
	vu16 ChargeVol_rx;	//在接收充电机信息时被赋值，在can里发给上位机
	vu16 ChargeCur_rx;
	vu8  ChargeStatus_rx;

	vu16 ChargeVol_tx;	//用来发送给充电机的参数
	vu16 ChargeCur_tx;
}Charge_State;

/*故障状态, 默认值为0, 有问题置1*/
typedef struct
{
	u32 error_mute;					//屏蔽位, OV为最低位, Openwire_Temp整个数组当作一位作为最高位
	vu8 OV;	//单体过压
	vu8 UV;	//单体欠压
	vu8 OT;	//单体高温
	vu8 UT;	//单体低温
	vu8 Vol_Openwires;	//电压采集出现断线
	vu8 Temp_Openwires;	//温度采集出现断线
	vu8 ODV;	//单体压差过大
	vu8 ODT;	//单体温差过大
	vu8 StaticDischarge_OC;	//放电过流
	vu8 StaticCharge_OC;	//充电过流
	vu8 Chargemachine_Error;//充电机故障
	vu8 IMD_Error;			//IMD发生接地故障
	vu8 Hall_Openwires;		//霍尔电流传感器离线
	vu8 Hall_Error;			//霍尔电流传感器故障
	vu8 Daisy_Openwires;	//菊花链断线
	vu8 Precharge_Fail;		//预充失败
	vu8 Relay_Error;	//继电器故障, 未按照对应的状态开闭
	vu8 Vol_Error;	//高压异常, 累计总压和采集总压相差过大
	vu8 Openwire_Vol[19];			//电池断线检测,第十三个字节检测C0断线,位从小到大按照MODULE1-6排列,
									//比如第十一号电池断线了, 则Cell_openwire[1]==XXXXX1XX, 也就是倒数第三位为1
									//比如第3个模组的C0断线了, 则Openwire_Vol[12]==XXXXX1XX, 也就是倒数第三位为1
	vu8 Openwire_Temp[6];			//48位
}Alarm_State;

/*故障等级判断*/
typedef struct
{
	vu16 OTV_LV1;
	vu16 OTV_LV2;
	vu16 OTV_LV3;
	vu16 OTV_LV4;

	vu16 UTV_LV1;
	vu16 UTV_LV2;
	vu16 UTV_LV3;
	vu16 UTV_LV4;

	vu16 StaticDischarge_OC_LV1;
	vu16 StaticDischarge_OC_LV2;
	vu16 StaticDischarge_OC_LV3;
	vu16 StaticDischarge_OC_LV4;

	vu16 StaticCharge_OC_LV1;
	vu16 StaticCharge_OC_LV2;
	vu16 StaticCharge_OC_LV3;
	vu16 StaticCharge_OC_LV4;

	vu16 SOC_High_LV1;
	vu16 SOC_High_LV2;
	vu16 SOC_High_LV3;
	vu16 SOC_High_LV4;

	vu16 SOC_Low_LV1;
	vu16 SOC_Low_LV2;
	vu16 SOC_Low_LV3;
	vu16 SOC_Low_LV4;

	vu16 Cell_OV_LV1;
	vu16 Cell_OV_LV2;
	vu16 Cell_OV_LV3;
	vu16 Cell_OV_LV4;

	vu16 Cell_UV_LV1;
	vu16 Cell_UV_LV2;
	vu16 Cell_UV_LV3;
	vu16 Cell_UV_LV4;

	vu16 Cell_ODV_LV1;
	vu16 Cell_ODV_LV2;
	vu16 Cell_ODV_LV3;
	vu16 Cell_ODV_LV4;

	vu8 Cell_OT_LV1;
	vu8 Cell_OT_LV2;
	vu8 Cell_OT_LV3;
	vu8 Cell_OT_LV4;

	vu8 Cell_UT_LV1;
	vu8 Cell_UT_LV2;
	vu8 Cell_UT_LV3;
	vu8 Cell_UT_LV4;

	vu8 Cell_ODT_LV1;
	vu8 Cell_ODT_LV2;
	vu8 Cell_ODT_LV3;
	vu8 Cell_ODT_LV4;

	vu8 Daisy_Openwires_LV1;
	vu8 Daisy_Openwires_LV2;
	vu8 Daisy_Openwires_LV3;
	vu8 Daisy_Openwires_LV4;

	vu8 Cell_Vol_Openwires_LV1;
	vu8 Cell_Vol_Openwires_LV2;
	vu8 Cell_Vol_Openwires_LV3;
	vu8 Cell_Vol_Openwires_LV4;

	vu8 Cell_Temp_Openwires_LV1;
	vu8 Cell_Temp_Openwires_LV2;
	vu8 Cell_Temp_Openwires_LV3;
	vu8 Cell_Temp_Openwires_LV4;

	vu8 Hall_Openwires_LV1;
	vu8 Hall_Openwires_LV2;
	vu8 Hall_Openwires_LV3;
	vu8 Hall_Openwires_LV4;

	vu8 Vol_Error_LV1;
	vu8 Vol_Error_LV2;
	vu8 Vol_Error_LV3;
	vu8 Vol_Error_LV4;

	vu8 Fans_Start_OT;
	vu8 Fans_End_OT;
	vu8 Fans_Start_ODT;
	vu8 Fans_End_ODT;

	vu8 Balance_Start_DV;
	vu8 Balance_End_DV;
	vu8 Precharge_End_Vol_Percent;
	vu8 Precharge_Time;
}Threshold_Value;

typedef struct
{
	vu8 AIRP_Ctrl;
	vu8 AIRN_Ctrl;
	vu8 PrechargeAIR_Ctrl;
	vu8 Error_Ctrl;
	vu8 Fans_Ctrl;
	vu8 Manual_Enable;
	vu8 Active_onHV;
}Tooling_Mode;

typedef struct
{
	u16 A_CODES_GPIO[48];
	u16 A_CODES_REF[8];
	u16 B_CODES_DELTA[144];
} CODES;

typedef struct
{
	BMS_State 			State;
	Cell_State 			Cell;
	Relay_State 		Relay;
	Charge_State 		charge;
	Alarm_State		 	Alarm;
	Threshold_Value Threshold;
	Tooling_Mode    Tooling;
	CODES Codes;
}BMS;

/*
extern u8 data[768]; //数据数组

#define MODULE_NUM 6
#define CELL_NUM 96
#define MODULE1_NUM
#define MODULE2_NUM
#define MODULE3_NUM
#define MODULE4_NUM
#define MODULE5_NUM
#define MODULE6_NUM
*/

/*BMS整体状态*/
#define CAN_ID_BMS_state 0X100
#define CAN_PL_BMS_state 2
/*电芯状态*/
#define CAN_ID_CELL_Vol 	0X104
#define CAN_PL_CELL_Vol 	36
#define CAN_ID_CELL_Tem 	0x128
#define CAN_PL_CELL_Tem 	12
#define CAN_ID_CELL_MVol 	0X134
#define CAN_PL_CELL_MVol 	1
#define CAN_ID_CELL_Dcc 	0X135
#define CAN_PL_CELL_Dcc 	3
#define CAN_ID_CELL_MVolNo 	0x138
#define CAN_PL_CELL_MVolNo 	1
/*继电器状态*/
#define ADDR_RELAY_STATE 400
#define CAN_ID_RELAY_STATE 0X139
#define CAN_PL_RELAY_STATE 1
/*充电状态*/
#define ADDR_CHARGE_STATE 408
#define CAN_ID_CHARGE_STATE 0X140
#define CAN_PL_CHARGE_STATE 1
/*告警*/
#define CAN_ID_ALARM 0X141
#define CAN_PL_ALARM 3
#define CAN_ID_OpenVol 0x144
#define CAN_PL_OpenVol 3
#define CAN_ID_OpenTem 0x147
#define CAN_PL_OpenTem 1
/*阈值*/
#define CAN_ID_THRESHOLD 0x150
#define CAN_PL_THRESHOLD 14
/*充电参数*/
#define CAN_ID_CHARGE_PARA 0x15F  					//更新充电参数时发给上位机的ID
#define CAN_PL_CHARGE_PARA 1
/*手动模式*/
#define CAN_ID_TOOLING 0x160
#define CAN_PL_TOOLING 1
/*错误屏蔽位设置. 主要用于不想检查某一项的报错使用*/
#define OV_mute (1<<0)								//最大电压过压
#define UV_mute (1<<1)								//最低电压欠压
#define OT_mute (1<<2)								//最大温度过温
#define UT_mute (1<<3)								//最低温度欠温
#define Vol_Openwires_mute  (1<<4)					//电压采集断线（包括菊花链断线）
#define Temp_Openwires_mute (1<<5)					//温度采集断线（包括菊花链断线）
#define ODV_mute (1<<6)								//最大最小压差过大
#define ODT_mute (1<<7)								//最大最小温差过大
#define StaticDischarge_OC_mute 	(1<<8)
#define StaticCharge_OC_mute 		(1<<9)
#define Chargemachine_Error_mute  	(1<<10)
#define IMD_Error_mute 				(1<<11)			//IMD故障
#define Hall_Openwires_mute  		(1<<12)			//霍尔电流计断线
#define Hall_Error_mute 	 		(1<<13)			
#define Daisy_Openwires_mute 		(1<<14)
#define Precharge_Fail_mute  		(1<<15)			//预充失败
#define Relay_Error_mute 	 		(1<<16)			//继电器状态不符合
#define Vol_Error_mute 		 		(1<<17)			//累计总压与总压相差过大
#define Openwire_Vol_mute  	 		(1<<18)			//电池断线检测,第十三位检测C0断线,位从小到大按照MODULE1-6排列
#define Openwire_Temp_mute 	 		(1<<19)			//温度采样线断线检测,其实只有36位,多出了4位
#define ALL_Error_mute 0x3dfff
#define ALL_Error_mute_Abnormal 0xffffffff
/*充电参数*/
#define Charge_Vol 6000	//0.1V/bit, 充电电压417V
#define Charge_Cur_State1 15		//0.1A/bit, 2A
#define Charge_Cur_State2 25		//0.1A/bit, 3.2A
#define Charge_Cur_State3 35		//0.1A/bit, 4A
#define Charge_Cur_State4 40		//0.1A/bit, 5A
// #define Charge_Cur_State1 20		//0.1A/bit, 2A
// #define Charge_Cur_State2 30		//0.1A/bit, 3.2A
// #define Charge_Cur_State3 40		//0.1A/bit, 4A
// #define Charge_Cur_State4 50		//0.1A/bit, 5A

#define Charge_Trip_Cur 1	//0.1A/bit, 关闭充电时设置最小电流

/*
内部变量
*/


extern vu8 Charge_Flag;
extern volatile float SOC;

void Data_Init(void);


#endif /* DATA_DATA_H_ */
