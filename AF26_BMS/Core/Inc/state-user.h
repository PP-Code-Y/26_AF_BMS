/*
 * state.h
 *
 *  Created on: 2022年3月6日
 *      Author: BBBBB
 */

#ifndef BMS_STATE_STATE_H_
#define BMS_STATE_STATE_H_

// #include "main.h"
#include "data-user.h"

#define BMS_INIT 		 1
#define BMS_SELFTEST 	 2	//没有用上, 不是很必要
#define BMS_STANDBY 	 3
#define BMS_PRECHARGE 	 4
#define BMS_RUN 		 5
#define BMS_SDC_OFF 	 6
#define BMS_ERROR 		 7
#define BMS_CHARGE 		 8
#define BMS_CHARGEDONE   9
#define BMS_Manual_MODE 10

#define PRE_COUNT 30 			 //预充时间（ms）/100
extern u8 ChargeMachine_Count; //充电机离线时间(ms):5s
/*工装模式*/
#define Manual_RELAY_ON  1
#define Manual_RELAY_OFF 2
#define Manual_ERROR_ON  1
#define Manual_ERROR_OFF 2
#define Manual_FANS_ON   1
#define Manual_FANS_OFF  2
#define Manual_ENABLE  	 1
#define Manual_DISABLE	 2
#define Manual_Active_ON 1
#define Manual_Active_OFF 2

//状态判断
void BMS_StateMachine(void *parameterNull);
//自检模式
void BMS_SelfTest(void);
//待驶状态
void BMS_Standby(void);
u8 Standby_detect(void);	//检测standby下继电器状态是否正常
//预充状态
void BMS_Precharge(void);
void Precharge_relay_open(void);	//定义开启precharge下的继电器函数
u8 Precharge_detect(void);	//检测Precharge下继电器状态是否正常
//运行状态
void BMS_Run(void);
void Run_relay_open(void);	//定义开启Run下的继电器函数
u8 Run_detect(void);		//检测Run下继电器状态是否正常
//故障模式
void BMS_Error(void);
void ALL_Error_mute_Beep(void);
//充电模式
void BMS_Charge(void);
void Charge_relay_open(void);	//定义开启Charge下的继电器函数
u8 Charge_detect(void);			//检测Charge下继电器状态是否正常
//下高压
void BMS_SDC_Off(void);
//充电结束模式
void BMS_Charge_Done(void);
//工装模式
void BMS_Manual_mode(void);
#endif /* BMS_STATE_STATE_H_ */
