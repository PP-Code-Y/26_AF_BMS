/*
 * adc.h
 *
 *  Created on: 2022年3月3日
 *      Author: BBBBB
 */

#ifndef ADC_ADC_H_
#define ADC_ADC_H_

#include "data-user.h"
#include "adc.h"
/*继电器状态*/
#define REALY_OFF 0
#define REALY_ON  1
/*定义使用ADC数量, 以便以后修改*/
#define num_adc 5
/*定义adc数组DMA的顺序, 具体请看adc.c*/
#define adc_pre    0
#define adc_ts     1
#define pre_state  2
#define airp_state 3
#define airn_state 4

/*计算adc的平均值*/
u16 adc_read_average(uint32_t Channel, u8 times);
u16 adc_average(u8 adc_ch, u8 times);
/*读取总电压, 单位100mV*/
float TS_Voltage(void);
/*读取预充电压, 单位100mV*/
float PRE_Voltage(void);
/*获取继电器状态*/
u8 AIRP_State(void);	//获取AIR+状态
u8 AIRN_State(void);	//获取AIR-状态
u8 PREAIR_State(void);	//获取预充继电器状态
/*
 * 检查所有继电器是否关闭
 * 输出TRUE/FAULT
 * */
u8 verify_relay_OFF(void);

#endif /* ADC_ADC_H_ */
