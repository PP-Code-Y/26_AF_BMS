/*
 * relay.h
 *
 *  Created on: 2022年3月6日
 *      Author: BBBBB
 */

#ifndef RELAY_RELAY_H_
#define RELAY_RELAY_H_

#include "data-user.h"
//主正继电器控制
void AIRP_ON(void);
void AIRP_OFF(void);
//主负继电器控制
void AIRN_ON(void);
void AIRN_OFF(void);
//预充继电器控制
void PrechargeAIR_ON(void);
void PrechargeAIR_OFF(void);
//风扇控制
void Fans_ON(void);
void Fans_OFF(void);
//关闭所有继电器
void AIRS_OFF(void);
//检测点火信号
u8 ON_State(void);
//检测IMD故障信号
u8 IMD_State(void);
//BMS错误警告, 蜂鸣器鸣叫, 进入故障状态, 关闭所有继电器
void Error(void);
void ALL_Error_mute_Beep(void);
//点火蜂鸣器鸣叫
void ON_Beep(void);

#endif /* RELAY_RELAY_H_ */
