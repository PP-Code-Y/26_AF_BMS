/*
 * can.h
 *
 *  Created on: 2022年3月6日
 *      Author: BBBBB
 */

#ifndef CAN_CAN_H_
#define CAN_CAN_H_

#include "main.h"
// #include "can.h"
#include "data-user.h"

//内部CAN定义
void can1_interal_config(void);
//外部CAN定义
void can2_exteral_config(void);
//发送一个信息
void Can_Send_Msg(u8* data,u16 ID,u8 can1_state,u8 can2_state);
//发送一组
void Can_Send_Package(u8* data,u16 ID,u8 num,u8 can1_state,u8 can2_state);
//网上看到的一个发送方法
uint8_t CAN_Transmit(const void* buf, uint32_t len, uint8_t number);
//发送CAN数据, BMS直接调用这个即可, 前面都是铺垫
void Can_Send_Data(u8 can1_state,u8 can2_state);
//发送充电数据
void Can_Send_Charge(u8 Charge_state);

// void KeepChargerCommunicate(void);
void CalcSOC(void);

void CalcAveTemp(void);

#endif /* CAN_CAN_H_ */
