/*
 * rtc.c
 *
 *  Created on: 2022年3月6日
 *      Author: BBBBB
 */
// 逻辑上，一旦监测到下电，把当前时间存储到时间结构体中。同时依据这个时间设定十分钟以后的闹钟。
// 1.如果达到十分钟，就进入中断，将标志位

//  #include "rtc.h"
#include "data-user.h"
#include "daisy-user.h"
#include "main.h"
#include "state-user.h"
#include "rtc-user.h"
#include "flash-user.h"

// 函数

// 1.获取时间
// 2.监测函数
// 4.SOC初始值函数，依据标志位，赋予初始值
// 5.SOC计算函数

extern BMS A03;
extern float SOC_OCV[11][2];

u8 offFlag = 0;			// 下电标志位，目的是防止重复设定闹钟，这个可能有更好的方法，想到了再说。
u8 SOC_Cycle = 10;		// 计算SOC的间隔，为了方便修改，加一个变量
TIME time;				// 定义存储上下电时间的结构体
int32_t Accumulate = 0; // 定义SOC计算的累计电流乘以时间的积分

void user_CheckRtcBkup()
{
	HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, 1);
}
// 获取当前下电时间, 并将下电时间和SOC存进FLASH里面
void Get_OFF_Time(void)
{
	HAL_RTC_GetTime(&hrtc, &time.OFF_Time, RTC_FORMAT_BIN); // 将时间数据储存在OFF_Time结构体里，结构体结构可查询了解
	HAL_RTC_GetDate(&hrtc, &time.OFF_Date, RTC_FORMAT_BIN); // 日期，没啥用
	Flash_Write_SOC();										// 将SOC进flash
	Flash_Write_Time();										// 将OFFtime存进flash
}
void Get_ON_Time(void) // 获取上电时间
{
	HAL_RTC_GetTime(&hrtc, &time.ON_Time, RTC_FORMAT_BIN);
	HAL_RTC_GetDate(&hrtc, &time.ON_Date, RTC_FORMAT_BIN);
}

void CAL_TIME() // 计算下电时间，修改标志位，以计算SOC，因为用存在flash里面的时间，到时候可以修改一下
{
}

// SOC初始值设定
// 下电超过十分钟，开路电压法测。
void SOC_Init(u16 minvol)
{
	u16 k; // 斜率
	u16 ON_times;
	u16 OFF_times;
	u8 AlarmFlag = 0; // 设个旗子，下电时间标志位，下电超过十分钟为1；否则为0

	Flash_Read_Time(); // 先Flash读出time, 包括OFF和ON, 但是ON在下一个函数中被覆盖, 所以主要读出到OFFTIME的信息
	Get_ON_Time();	   // 上电时间获取, 跟OFFtime比较

	ON_times = time.ON_Time.Hours * 3600 + time.ON_Time.Minutes * 60 + time.ON_Time.Seconds;
	OFF_times = time.OFF_Time.Hours * 3600 + time.OFF_Time.Minutes * 60 + time.OFF_Time.Seconds;
	if (ON_times > OFF_times)
	{
		if (ON_times - OFF_times >= 600)
		{ // 大于10min
			AlarmFlag = 1;
		}
		else
		{
			AlarmFlag = 0;
		}
	}
	else
	{
		AlarmFlag = 1; // 如果上电时间小于下电时间，那么说明至少过去了一天
	}
	if (AlarmFlag == 1) // 如果下电超过十分钟，使用开路电压法,拟合的效果并不好，也采用插值
	{
		if (minvol > 4334)
			A03.State.SOC = 1000;
		else if (minvol <= 3050)
			A03.State.SOC = 0; // 超出数组两端极值
		else				   // 中间部分插值线性计算
		{
			for (u8 n = 0; n < 10; n++) // 循环判断，找到当前开路电压存在的区间
			{
				if (minvol <= SOC_OCV[n][1] && minvol > SOC_OCV[n + 1][1])
				{
					k = 1000 * (SOC_OCV[n][0] - SOC_OCV[n + 1][0]) / (SOC_OCV[n][1] - SOC_OCV[n + 1][1]); // 乘了个1000，避免浮点数运算
					A03.State.SOC = (1000 * SOC_OCV[n + 1][0] + k * SOC_OCV[n + 1][1]) / 1000;
					return; // 找到了SOC并赋值之后调出函数
				}
			}
		}
	}
	else // 如果下电不超过十分钟，依旧保持原来的值
	{
		Flash_Read_SOC();
	}
}

// 检测函数，这部分应该可以融合进对应的BMS状态的几个函数中
void Save_OFFtime()
{
	switch (A03.State.BMS_State)
	{
	case BMS_SDC_OFF:
		Get_OFF_Time();
		break;
	case BMS_CHARGEDONE:
		Get_OFF_Time();
		break;
	case BMS_ERROR:
		if (offFlag == 0)
		{
			Get_OFF_Time();
			offFlag = 1;
		}
		break;
	}
}

// SOC计算程序
// 计算soc 事实上应该还有一个效率的，本来打算按朱队的数据算一下，但是好像朱队的数据就是按照1处理的。暂时定为1吧
// 然后，安时积分法，积分就算了，就是时间乘以电流，目前是定时器10ms，不知道行不行。
// 电流值，目前认为正值为放电，所以中间是个加号，错了再改
void SOC_CAL()
{
	//	A03.State.SOC = A03.State.SOC - A03.State.TotalCurrent * SOC_Cycle / (22*60*60*1000);	//这是源公式, 仅作方便展示用, 电流是mA，SOC是千分值，22容量单位为Ah，
	// 代码讲解: 因为用整数运算快,但是怕丢失精度(比如900/1000实际上等于0), 所以根据电流*时间积分值是否达到SOC变化千分之一所需要的值(也就是上面的22*60*60*1000)来判断SOC是否需要自增1
	// 采用10ms为Δt, 根据电流累加算积分: 其实可以根据这条公式理解E03.State.SOC = A03.State.SOC - A03.State.TotalCurrent * SOC_Cycle / (22*60*60*1000)
	// 当SOC变化1时, 需要E03.State.TotalCurrent * SOC_Cycle==(16.8*60*60*1000), 也就是79200000
	Accumulate += (A03.State.TotalCurrent * SOC_Cycle);
	if (Accumulate > 60480000)
	{
		Accumulate = Accumulate - 60480000; // SOC已经要变化了, 积分值保留一下, 如果直接清零的话会随着时间推移丢失太多, 不如保留好剩下部分
		A03.State.SOC -= 1;					// 放电了千分之一, SOC自减1
	}
	else if (Accumulate < (-60480000))
	{
		Accumulate = Accumulate + 60480000;
		A03.State.SOC += 1; // 充电了千分之一, SOC自增1
	}
}

// 朱队给的数据，开路电压与SOC的关系，SOC是千分数；电压是毫伏
float SOC_OCV[11][2] = {
	{1000, 4334},
	{900, 4219},
	{800, 4103},
	{700, 4002},
	{600, 3906},
	{500, 3832},
	{400, 3789},
	{300, 3761},
	{200, 3717},
	{100, 3677},
	{0, 3050}}; // SOC是千分数；电压是毫伏

// RTC_TimeTypeDef OFF_Time; //定义获取时间的结构体
// RTC_DateTypeDef OFF_Date; //定义获取日期的结构体  这个没用，不过配置的时候顺带着配置上了。
// RTC_AlarmTypeDef Set_Alarm;  //定义闹钟的结构体

// 设定闹钟
// void Set_alarm()
//{
//	  Set_Alarm.Alarm = RTC_ALARM_A; //选闹钟，实际上F1也只有一个闹钟
//	  Set_Alarm.AlarmTime.Seconds = OFF_Time.Seconds; //秒无所谓
//	  Set_Alarm.AlarmTime.Minutes = OFF_Time.Minutes + 10; //闹钟设在下电时间后10分钟
//	  if(Set_Alarm.AlarmTime.Minutes >= 60) //要考虑进位问题,满60进1
//	  {
//			   if(Set_Alarm.AlarmTime.Hours == 24) //24点到一点
//				 {
//		           Set_Alarm.AlarmTime.Hours = 1;
//		           Set_Alarm.AlarmTime.Minutes = Set_Alarm.AlarmTime.Minutes - 60;
//				 }
//				 else
//				 {
//					 		 Set_Alarm.AlarmTime.Hours = OFF_Time.Hours + 1;
//		           Set_Alarm.AlarmTime.Minutes = Set_Alarm.AlarmTime.Minutes - 60;
//				 }
//	  }
//	  HAL_RTC_SetAlarm_IT(&hrtc, &Set_Alarm, RTC_FORMAT_BIN);
// }

// 闹钟中断函数
// void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *hrtc)//闹钟中断
//{
//     AlarmFlag = 1; //标志位改为1，代表下电超过十分钟
// }
