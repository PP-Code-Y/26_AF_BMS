/*
 * adc.c
 *
 *  Created on: 2022年3月3日
 *      Author: BBBBB
 *      brief:  关于预充和电压的计算
 *				注意ADC不能开连续转换模式, 会导致adc数组混乱
 */

#include "data-user.h"
#include "adc-user.h"
#include "main.h"

extern BMS A03;
/*adc[0]为ADC_PRE, adc[1]为ADC_TS,
 *adc[2]为PRE_STATE, adc[3]为AIRP_STATE, adc[4]为AIRN_STATE
 */
extern vu16 adc[num_adc];
extern ADC_HandleTypeDef hadc1;

u16 adc_read_average(uint32_t Channel, u8 times)
{
	u32 sum = 0;
	ADC_ChannelConfTypeDef sConfig = {0};
	sConfig.Channel = Channel;
	sConfig.Rank = ADC_REGULAR_RANK_1;
	sConfig.SamplingTime = ADC_SAMPLETIME_55CYCLES_5; // 不采用dma，多通道采样的话其实有两种方式，这里只开了一个通道，所以就是每次调用修改一下初始设定
	if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
	{
		Error_Handler();
	}
	for (u8 i = 0; i < times; i++)
	{
		HAL_ADC_Start(&hadc1);					// 开启adc
		HAL_ADC_PollForConversion(&hadc1, 500); // 等待采集结束
		if (HAL_IS_BIT_SET(HAL_ADC_GetState(&hadc1), HAL_ADC_STATE_REG_EOC))
		{
			sum += HAL_ADC_GetValue(&hadc1);
		}
		else
		{
			i = i - 1;
		}
	}
	return sum / times;
}

u16 adc_average(u8 adc_ch, u8 times)
{
	u32 vol;
	switch (adc_ch)
	{
	case adc_ts:
		vol = adc[1] = adc_read_average(ADC_CHANNEL_1, times);
		break;
	case adc_pre:
		vol = adc[0] = adc_read_average(ADC_CHANNEL_0, times);
		break;
	}
	return vol;
}

/*
 *通过霍尔电压器,ADC计算预充电压
 * 单位100mV
 * */
float PRE_Voltage()
{
	u16 AD_PRE;
	float temp;
	AD_PRE = adc_average(adc_pre, 20);
	temp = AD_PRE / 4096.0 * 3.3 / 0.5 * 1000; // 单位0.1V/bit, 也就是说4171为417.1V //计算得到总电压, AD_TS/4096.0*3.3得到ADC采样电压, /0.5为22和33电阻分压, *1000转换单位为0.1V/bit
	A03.State.PrechargeVoltage = temp;		   // 进行状态更新
	return temp;
}
/*
 *通过霍尔电压器,ADC计算总压
 * 单位100mV
 * */
float TS_Voltage(void)
{
	u16 AD_TS;
	float temp;
	AD_TS = adc_average(adc_ts, 20);
	temp = AD_TS / 4096.0 * 3.3 / 0.5 * 1000; // 计算得到总电压, 源公式AD_TS/4095.0*3.3/0.6*1000// 计算得到总电压, AD_TS/4096.0*3.3得到ADC采样电压, /0.6为22和33电阻分压, *1000转换单位为0.1V/bit
	A03.State.TotalVoltage = temp;			  // 进行状态更新
	return temp;
}

uint8_t AIRP_State()
{
	if (HAL_GPIO_ReadPin(AIRP_State_GPIO_Port, AIRP_State_Pin) == GPIO_PIN_RESET)
	{
		return REALY_OFF;
	}
	else
	{
		return REALY_ON;
	}
}

uint8_t AIRN_State()
{
	if (HAL_GPIO_ReadPin(AIRN_State_GPIO_Port, AIRN_State_Pin) == GPIO_PIN_RESET)
	{
		return REALY_OFF;
	}
	else
	{
		return REALY_ON;
	}
}

uint8_t PREAIR_State()
{
	if (HAL_GPIO_ReadPin(PRE_State_GPIO_Port, PRE_State_Pin) == GPIO_PIN_RESET)
	{
		return REALY_OFF;
	}
	else
	{
		return REALY_ON;
	}
}

/*
 * brief: 检测函数,判断如果三个继电器均关断则返回true
 */
u8 verify_relay_OFF()
{
	if (AIRN_State() == REALY_OFF)
	{ // 里面跑了一次AIRN_State()来判断AIR-的状态, 如果确实是OFF则进入if函数,
		A03.Relay.AIRN_State = REALY_OFF;
		if (AIRP_State() == REALY_OFF)
		{
			A03.Relay.AIRP_State = REALY_OFF;
			if (PREAIR_State() == REALY_OFF)
			{
				A03.Relay.PrechargeAIR_State = REALY_OFF;
				return TRUE;
			}
		}
	}
	return FAULT;
}

/*
 * 读到电压有三种状态:2.5/5
 * 2.5V:开启
 * 5V:  关闭
目前为了稳妥一点, 就是暂定读到
*/

/*检测继电器AIR+状态函数*/
// u8 AIRP_State(){
// 	u16 ad=adc_average(airp_state, 20);
// 	if(ad<1862){	//1862代表输入3V, 源公式为3.3*ad/4096*2=3, 3.3*ad/4096为ADC采集电压, *2为电阻分压
// 		A03.Relay.AIRP_State=REALY_ON;				//继电器开启
// 		return REALY_ON;
// 	}
// 	else{
// 		A03.Relay.AIRP_State=REALY_OFF;				//输入5V, 继电器关闭
// 		return REALY_OFF;
// 	}
// }
/*检测继电器PREAIR状态的函数*/
// u8 PREAIR_State(){
// 	u16 ad=adc_average(pre_state, 10);
// 	if(ad<1862){	//1862代表输入3V, 源公式为3.3*ad/4096*2=3, 3.3*ad/4096为ADC采集电压, *2为电阻分压
// 		A03.Relay.PrechargeAIR_State=REALY_ON;
// 		return REALY_ON;
// 	}
// 	else{
// 		A03.Relay.PrechargeAIR_State=REALY_OFF;
// 		return REALY_OFF;
// 	}
// }
/*检测继电器AIR-状态函数*/
// u8 AIRN_State(){
// 	u16 ad=adc_average(airn_state, 10);
// 	if(ad<1862){	//1862代表输入3V, 源公式为3.3*ad/4096*2=3, 3.3*ad/4096为ADC采集电压, *2为电阻分压
// 		A03.Relay.AIRN_State=REALY_ON;
// 		return REALY_ON;
// 	}
// 	else{
// 		A03.Relay.AIRN_State=REALY_OFF;				//输入5V, 继电器关闭
// 		return REALY_OFF;
// 	}

