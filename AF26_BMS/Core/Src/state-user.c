/*
 * state.c
 *
 *  Created on: 2022年3月6日
 *      Author: BBBBB
 */

#include "state-user.h"
#include "os-user.h"
#include "ltc681x-user.h"
#include "relay-user.h"
#include "daisy-user.h"
#include "adc-user.h"
#include "can-user.h"
// #include "rtc-user.h"
#include "tim.h"
#include "flash-user.h"

extern BMS A03;
extern MODULE Module[Total_ic];
extern cell_asic IC[Total_ic];
extern u8 n_Cell[Total_ic];
// extern TASK_COMPONENTS BMS_Tasks[];
extern TaskManager taskManager;
extern vu8 charge_cur_count;

u8 ChargeMachine_Count = 0;
vu8 PreChargeFail_Count = 0;

void BMS_StateMachine(void *parameterNull)
{
    switch (A03.State.BMS_State)
    {
    case BMS_STANDBY:
        BMS_Standby();
        break;
    /*预充属于触发状态*/
    case BMS_PRECHARGE:
        BMS_Precharge();
        break;
    case BMS_RUN:
        BMS_Run();
        break;
    case BMS_SDC_OFF:
        BMS_SDC_Off();
        break;
    case BMS_ERROR:
        BMS_Error();
        break;
    /*充电属于触发状态*/
    case BMS_CHARGE:
        BMS_Charge();
        break;
    case BMS_CHARGEDONE:
        BMS_Charge_Done();
        break;
    /*工装模式*/
    case BMS_Manual_MODE:
        BMS_Manual_mode();
        break;
    }
}

/*main开头执行一次, BMS自检测,如果没有错误就进入standby*/
void BMS_SelfTest()
{
    A03.State.BMS_State = BMS_SELFTEST; // 进入自检状态
    // 通过蜂鸣器鸣叫提示有没有设置错误位屏蔽, 顺便通过这个延时完成IMD的自检等待
    if (A03.Alarm.error_mute)
    {
        HAL_GPIO_WritePin(Error_Beep_GPIO_Port, Error_Beep_Pin, GPIO_PIN_SET); // 蜂鸣器鸣叫50ms, 下同
        HAL_Delay(100);
        HAL_GPIO_WritePin(Error_Beep_GPIO_Port, Error_Beep_Pin, GPIO_PIN_RESET);
        HAL_Delay(50);
        HAL_GPIO_WritePin(Error_Beep_GPIO_Port, Error_Beep_Pin, GPIO_PIN_SET);
        HAL_Delay(100);
        HAL_GPIO_WritePin(Error_Beep_GPIO_Port, Error_Beep_Pin, GPIO_PIN_RESET);
        HAL_Delay(50);
        HAL_GPIO_WritePin(Error_Beep_GPIO_Port, Error_Beep_Pin, GPIO_PIN_SET);
        HAL_Delay(100);
        HAL_GPIO_WritePin(Error_Beep_GPIO_Port, Error_Beep_Pin, GPIO_PIN_RESET);
        HAL_Delay(50);
        HAL_GPIO_WritePin(Error_Beep_GPIO_Port, Error_Beep_Pin, GPIO_PIN_SET);
        HAL_Delay(100);
        HAL_GPIO_WritePin(Error_Beep_GPIO_Port, Error_Beep_Pin, GPIO_PIN_RESET);
        HAL_Delay(50);
        HAL_GPIO_WritePin(Error_Beep_GPIO_Port, Error_Beep_Pin, GPIO_PIN_SET);
        HAL_Delay(100);
        HAL_GPIO_WritePin(Error_Beep_GPIO_Port, Error_Beep_Pin, GPIO_PIN_RESET);
    }
    else
    {
        HAL_GPIO_WritePin(Error_Beep_GPIO_Port, Error_Beep_Pin, GPIO_PIN_SET); // 蜂鸣器鸣叫1s
        HAL_Delay(500);
        HAL_GPIO_WritePin(Error_Beep_GPIO_Port, Error_Beep_Pin, GPIO_PIN_RESET);
    }
    TS_Voltage();  // 采集总压
    PRE_Voltage(); // 先采集一遍预充电压
    Daisy_Init();  // 菊花链初始化
    DaisyVol();    // 采集电压
    DaisyTem();    // 采集温度
    InitSoc();
    // Openwire_check(&A03, Total_ic, n_Cell, Module);	//判断是否采样开路
    // user_CheckRtcBkup();	//给RTC寄存器写值, 使之不会下电重置计数
    /*if ( (A03.Alarm.error_mute == ALL_Error_mute) | (A03.Alarm.error_mute == ALL_Error_mute_Abnormal) ){
        A03.State.BMS_State = BMS_ERROR;
        //ALL_Error_mute_Beep();
    }*/
    if (A03.State.BMS_State != BMS_ERROR)
    {
        /*SOC赋初值, 放在这里以防电压采样断线导致测得的最小电压有问题*/
        // SOC_Init(A03.Cell.MinVol);			//询问SOC是否要做开路电压修正(若已经静止超过10MIN)
        // HAL_TIM_Base_Start_IT(&htim3); 		//定时器3使能, 开始计算SOC
        A03.State.BMS_State = BMS_STANDBY; // 如果没问题就进入待机状态
    }
}

// OCV单位为mV
static double SOC_OCV[11][2] = {{0, 3600},
                                {100, 3684},
                                {200, 3724},
                                {300, 3762},
                                {400, 3780},
                                {500, 3798},
                                {600, 3836},
                                {700, 3905},
                                {800, 3960},
                                {900, 4033},
                                {1000, 4150}};

// 调用此函数前一定要已经采集过一次总压
void InitSoc(void)
{
    double k, b;             // 线性插值所用直线的斜率以及截距
    uint32_t averageVoltage; // 平均电芯电压
    averageVoltage = A03.State.TotalVoltage_cal * 100.0 / 144;
    if (averageVoltage > 4150)
    {
        A03.State.SOC = 1000;
    }
    else if (averageVoltage < 3600)
    {
        A03.State.SOC = 0;
    }
    else
    {
        for (uint8_t i = 0; i < 10; i++)
        {
            if (averageVoltage >= SOC_OCV[i][1] && averageVoltage <= SOC_OCV[i + 1][1])
            {
                k = (SOC_OCV[i + 1][0] - SOC_OCV[i][0]) / (SOC_OCV[i + 1][1] - SOC_OCV[i][1]);
                b = SOC_OCV[i][0] - k * SOC_OCV[i][1]; // 代入一个点求截距
                A03.State.SOC = (uint16_t)(k * averageVoltage + b);
            }
        }
    }
}

/*BMS待机函数, 将等待点火信号或者充电信号以进入新的状态*/
void BMS_Standby()
{
    Standby_detect(); // 进行standby的继电器状态监测, 这个程序跑完后如果继电器状态正确就还是保持standby状态, 否则进error, 所以下面还要判断BMS状态是有说法的
    // 无论是放电还是充电进预充都是如此判断
    uint8_t ONSignal = ON_State();
    if (ONSignal && (A03.State.BMS_State == BMS_STANDBY))
    {              //"点火"+"进入standby状态"="进入预充状态"
        ON_Beep(); // 蜂鸣器鸣叫提醒点火, 顺便完成延时消抖, 但是蜂鸣器延时比较长, 可能会影响预充时间的判断, 所以建议以后研究蜂鸣器鸣叫可不可以独立
        if (ON_State() && (A03.State.BMS_State == BMS_STANDBY))
        {
            // SOC_Init(A03.Cell.MinVol);	//询问SOC是否要做开路电压修正(若已经静止超过10MIN)
            Precharge_relay_open(); // 进行预充相应继电器的打开
            if (Precharge_detect())
            {                                        // 以及判断是否按照正确的方式打开
                A03.State.BMS_State = BMS_PRECHARGE; // 如果都没有进error则进入预充状态
                // BMS_Tasks[Pre_NO].Run=0;								//其实就是做个保险, 防止函数刚好变成1而导致错误直接进入预充函数
                // BMS_Tasks[Pre_NO].Timer=A03.Threshold.Precharge_Time*1000;		//从0xFF修改成真正的预充计数值, 开始预充计时
                // BMS_Tasks[Pre_NO].ItvTime=A03.Threshold.Precharge_Time*1000;	//开始预充计时
                Task *taskPointer = taskManager.taskHead;
                for (; taskPointer != NULL; taskPointer = taskPointer->nextTask)
                {
                    if (taskPointer->taskInfo.Task_Function == Precharge_Run_Handle.taskInfo.Task_Function)
                    {
                        taskPointer->taskInfo.status = TASK_IDLE;                          // 防止函数刚好变为TASK_READY而直接进入Precharge_Run函数
                        taskPointer->taskInfo.timer = taskPointer->taskInfo.itvTime = 500; // 从0xFF修改成真正的预充计数值, 开始预充计时
                        break;                                                             // 找到目标任务后就跳出循环，节省时间
                    }
                }
            } // 下一步看Precharge_Run();
        }
    }
}
/*进行standby的继电器状态监测函数, 状态不对就报错
return 1:没问题; 0:出问题
*/
u8 Standby_detect()
{
    AIRS_OFF(); // standby下应关闭所有继电器
    if (AIRP_State() != REALY_OFF)
    {
        A03.Alarm.Relay_Error = 1; // 报错标志位不管有没有屏蔽错误, 先置一
        if ((A03.Alarm.error_mute & Relay_Error_mute) == 0)
        { // 检查是否设置了继电器错误屏蔽, 如果是设置了屏蔽结果应是1XXXXXXX...., 不为0, 所以不触发error
            Error();
            return 0; // 屏蔽错误后, 就不返回0了,最终结果返回1
        }
    }
    if (AIRN_State() != REALY_OFF)
    {
        A03.Alarm.Relay_Error = 1; // 报错标志位不管有没有屏蔽错误, 先置一
        if ((A03.Alarm.error_mute & Relay_Error_mute) == 0)
        { // 检查是否设置了继电器错误屏蔽
            Error();
            return 0; // 屏蔽错误后, 就不返回0了,最终结果返回1
        }
    }
    if (PREAIR_State() != REALY_OFF)
    {
        A03.Alarm.Relay_Error = 1; // 报错标志位不管有没有屏蔽错误, 先置一
        if ((A03.Alarm.error_mute & Relay_Error_mute) == 0)
        { // 检查是否设置了继电器错误屏蔽
            Error();
            return 0; // 屏蔽错误后, 就不返回0了,最终结果返回1
        }
    }
    return 1; // 没有任何错误或者出错但是屏蔽了返回1
}

/*BMS预充函数, 将检查继电器时候按照预充时刻正常打开*/
void BMS_Precharge()
{
    Precharge_detect();
}
/*开启或者关闭prechg下相应的继电器*/
void Precharge_relay_open()
{
    for (u8 i = 0; i < 100; i++)
    {
        AIRP_OFF();
    }
    for (u8 i = 0; i < 100; i++)
    {
        AIRN_ON();
    }
    // HAL_Delay(10);
    for (u8 i = 0; i < 100; i++)
    {
        PrechargeAIR_ON(); // PREAIR
    }
    HAL_Delay(100);
}
/*进行Precharge的继电器状态监测函数, 状态不对就报错
return 1:没问题; 0:出问题
*/
u8 Precharge_detect()
{
    if (AIRP_State() != REALY_OFF)
    {
        A03.Alarm.Relay_Error = 1; // 报错标志位不管有没有屏蔽错误, 先置一
        if ((A03.Alarm.error_mute & Relay_Error_mute) == 0)
        { // 检查是否设置了继电器错误屏蔽, 如果是设置了屏蔽结果应是1XXXXXXX...., 不为0, 所以不触发error
            Error();
            return 0; // 屏蔽错误后, 就不返回0了,最终结果返回1
        }
    }
    if (AIRN_State() != REALY_ON)
    {
        A03.Alarm.Relay_Error = 1; // 报错标志位不管有没有屏蔽错误, 先置一
        if ((A03.Alarm.error_mute & Relay_Error_mute) == 0)
        { // 检查是否设置了继电器错误屏蔽, 如果是设置了屏蔽结果应是1XXXXXXX...., 不为0, 所以不触发error
            Error();
            return 0; // 屏蔽错误后, 就不返回0了,最终结果返回1
        }
    }
    if (PREAIR_State() != REALY_ON)
    {
        A03.Alarm.Relay_Error = 1; // 报错标志位不管有没有屏蔽错误, 先置一
        if ((A03.Alarm.error_mute & Relay_Error_mute) == 0)
        { // 检查是否设置了继电器错误屏蔽, 如果是设置了屏蔽结果应是1XXXXXXX...., 不为0, 所以不触发error
            Error();
            return 0; // 屏蔽错误后, 就不返回0了,最终结果返回1
        }
    }
    return 1; // 没有任何错误或者出错但是屏蔽了返回1
}

/*BMS运行函数, 将检查继电器时候按照运行时刻的正状态*/
void BMS_Run()
{
    Run_detect();
    // 判断如果预充电压/采集总压小于可调的阈值之后, 进入下高压状态
    // 出现进入if的情况有:
    // 1. 如果下点火信号了，安回断开，放电继电器起动，所以预充电压会下去;
    // 2. 如果手动上高压， 操作员断开三个继电器的供电，也会导致预充电压下去
    if (PreChargeFail_Count < 5)
    {
        PreChargeFail_Count++; // 防止因为短暂异常导致立即进入预充失败状态。
    }
    if (A03.Alarm.error_mute & Precharge_Fail_mute) // 按下预充失败屏蔽后强上，预充失败屏蔽强上的情况下不进行预充电压检测
    {
        AIRN_ON();
        //		HAL_Delay(10);
        AIRP_ON(); // 打开AIRP
        //		HAL_Delay(10);
        PrechargeAIR_OFF(); // 关闭PREAIR
    }
    // else if ((100.0 * A03.State.PrechargeVoltage / A03.State.TotalVoltage) < A03.Threshold.Precharge_End_Vol_Percent) {
    else if ((100.0 * A03.State.PrechargeVoltage / A03.State.TotalVoltage) < 95)
    {
        HAL_Delay(200);
        A03.State.TotalVoltage = TS_Voltage();
        A03.State.PrechargeVoltage = PRE_Voltage();
        if ((100.0 * A03.State.PrechargeVoltage / A03.State.TotalVoltage) < 95)
            A03.State.BMS_State = BMS_SDC_OFF;
    }
}
/*开启或者关闭Run下相应的继电器*/
void Run_relay_open()
{
    AIRP_ON();          // 打开AIRP
    HAL_Delay(50);      // 因为继电器
    PrechargeAIR_OFF(); // 关闭PREAIR
    HAL_Delay(50);
}
/*进行Run的继电器状态监测函数, 状态不对就报错
return 1:没问题; 0:出问题
*/
u8 Run_detect()
{
    if (AIRP_State() != REALY_ON)
    {
        A03.Alarm.Relay_Error = 1; // 报错标志位不管有没有屏蔽错误, 先置一
        // if ((A03.Alarm.error_mute & Relay_Error_mute) == 0) { // 检查是否设置了继电器错误屏蔽, 如果是设置了屏蔽结果应是1XXXXXXX...., 不为0, 所以不触发error
        //     Error();
        //     return 0; // 屏蔽错误后, 就不返回0了,最终结果返回1
        // }
    }
    if (AIRN_State() != REALY_ON)
    {
        A03.Alarm.Relay_Error = 1; // 报错标志位不管有没有屏蔽错误, 先置一
        // if ((A03.Alarm.error_mute & Relay_Error_mute) == 0) { // 检查是否设置了继电器错误屏蔽, 如果是设置了屏蔽结果应是1XXXXXXX...., 不为0, 所以不触发error
        //     Error();
        //     return 0; // 屏蔽错误后, 就不返回0了,最终结果返回1
        // }
    }
    if (PREAIR_State() != REALY_OFF)
    {
        A03.Alarm.Relay_Error = 1; // 报错标志位不管有没有屏蔽错误, 先置一
        // if ((A03.Alarm.error_mute & Relay_Error_mute) == 0) { // 检查是否设置了继电器错误屏蔽, 如果是设置了屏蔽结果应是1XXXXXXX...., 不为0, 所以不触发error
        //     Error();
        //     return 0; // 屏蔽错误后, 就不返回0了,最终结果返回1
        // }
    }
    return 1; // 没有任何错误或者出错但是屏蔽了返回1
}
/*
下高压函数
*/
void BMS_SDC_Off()
{
    AIRS_OFF();
    // Save_OFFtime();	//将SOC和下电时间存进FLASH
    Standby_detect(); // 先保证Standby前继电器状态正常
    A03.State.BMS_State = BMS_STANDBY;
    if (PreChargeFail_Count < 5)
    {
        A03.Alarm.Precharge_Fail = 1; // 预充失败告警
        if (!(A03.Alarm.error_mute & Precharge_Fail_mute))
        {            // 屏蔽位判断,如果没有设置屏蔽位则可以报警, 其实也就是判断相关位有没有置为1,如果是1那不管左移多少位都是大于0的
            Error(); // 输出故障信号-关闭继电器-修改状态为error
        }
    }
    PreChargeFail_Count = 0;
}

void BMS_Error()
{
    if ((A03.Alarm.error_mute == ALL_Error_mute) | (A03.Alarm.error_mute == ALL_Error_mute_Abnormal))
    {
        ALL_Error_mute_Beep();
    }
    else
    {
        Error();
    }

    // Save_OFFtime();	//将SOC和下电时间存进FLASH, 由于有OFF_FLAG, 所以只会存一次下电时间
}
/*主要进行一些充电状态的判断*/
void BMS_Charge()
{
    Charge_detect();
    ChargeMachine_Count++; // 100ms为周期, 因此达到50次之后就是5秒, 在CAN接收中断那里进行清零
                           // 充电电流是否过流判断在Current_detect()中
}
/*开启或者关闭Charge下相应的继电器*/
void Charge_relay_open()
{
    AIRP_ON(); // 打开AIRP
    HAL_Delay(100);
    PrechargeAIR_OFF(); // 关闭PREAIR
    HAL_Delay(100);
}
/*进行Charge的继电器状态监测函数, 状态不对就报错
return 1:没问题; 0:出问题
*/
u8 Charge_detect()
{
    // 电压到达590V且充电电流小于0.5A
    if (A03.State.TotalCurrent >= -500 && A03.State.TotalVoltage_cal >= 6000)
    {
        A03.State.BMS_State = BMS_CHARGEDONE; // 进入充电完成状态
    }

    if (ChargeMachine_Count > 100)
    {                                      // 充电机离线超过10S, 清零将在can2中断函数里面
        A03.Alarm.Chargemachine_Error = 1; // 充电机离线, 报故障
        if ((A03.Alarm.error_mute & Chargemachine_Error_mute) == 0)
        {
            Error(); // 报错
        }
    }
    else if (AIRP_State() != REALY_ON)
    {
        A03.Alarm.Relay_Error = 1; // 报错标志位不管有没有屏蔽错误, 先置一
        if ((A03.Alarm.error_mute & Relay_Error_mute) == 0)
        { // 检查是否设置了继电器错误屏蔽, 如果是设置了屏蔽结果应是1XXXXXXX...., 不为0, 所以不触发error
            Error();
        }
        // 用else if是有说法的, 如果前面报错了, 但是后面又单体电压充满了, 就会出现刚进error又改回来的BUG
    }
    else if (AIRN_State() != REALY_ON)
    {
        // A03.Alarm.Relay_Error = 1;                            // 报错标志位不管有没有屏蔽错误, 先置一
        if ((A03.Alarm.error_mute & Relay_Error_mute) == 0)
        { // 检查是否设置了继电器错误屏蔽, 如果是设置了屏蔽结果应是1XXXXXXX...., 不为0, 所以不触发error
            Error();
        }
    }
    else if (PREAIR_State() != REALY_OFF)
    {
        // A03.Alarm.Relay_Error = 1;                            // 报错标志位不管有没有屏蔽错误, 先置一
        if ((A03.Alarm.error_mute & Relay_Error_mute) == 0)
        { // 检查是否设置了继电器错误屏蔽, 如果是设置了屏蔽结果应是1XXXXXXX...., 不为0, 所以不触发error
            Error();
        }
    }
    else if ((A03.Alarm.OT == 1) && ((A03.Alarm.error_mute & OT_mute) == 0))
    { // 温度达到一级故障下高压, Slave_MEASURE()的函数中会将E03.Alarm.OV的值设置成1,代表超出了4350这个阈值, 单体电芯充满了
        if ((A03.Alarm.error_mute & OT_mute) == 0)
        { // 检查是否设置了继电器错误屏蔽, 如果是设置了屏蔽结果应是1XXXXXXX...., 不为0, 所以不触发error
            Error();
        }
    }
    else if ((A03.Alarm.OV == 1) && (A03.Alarm.error_mute & Precharge_Fail_mute))
    {
        AIRN_ON();
        AIRP_ON();          // 打开AIRP
        PrechargeAIR_OFF(); // 关闭PREAIR
        // 电池充满了, 达到一级阈值
    }
    else if ((A03.Alarm.OV == 1) && ((A03.Alarm.error_mute & OV_mute) == 0))
    {                     // 在Slave_MEASURE()的函数中的set_Volwarning()会将E03.Alarm.OV的值设置成1,代表超出了4350这个阈值, 单体电芯充满了
        AIRS_OFF();       // 先关闭所有继电器
        Standby_detect(); // 其实standby和chargedone关的继电器都一样, 这边偷懒直接用这个
        if (A03.State.BMS_State != BMS_ERROR)
        {                                         // 一定要防止进了error之后又被改回来的情况发生
            A03.State.BMS_State = BMS_CHARGEDONE; // 进入充电完成状态
            // Save_OFFtime();	//将SOC和下电时间存进FLASH
            if ((A03.State.TotalVoltage_cal - 415) > 0)
                A03.State.SOC = 100;
            // 此处应该还有一个开路电压测SOC
            // Flash_Write_SOC();    // 存Flash
            charge_cur_count = 0; // 方便重新充电时需要重新发电流数据, 其实应该清零是没用的, 因为这个变量也只会在charge状态下才会自增, 而chargedone已经不会再返回charge了
        }
    }

    // 跑完上面一连串if之后, 一定会进入下面的if else判断, 进行return输出
    if (A03.State.BMS_State != BMS_ERROR)
        return 1; // 如无问题则返回1, 但是还有需要商议的是是否会被某些中断修改成error使得函数不正常输出
    else
        return 0; // 有问题输出为0
}

void BMS_Charge_Done()
{
    AIRS_OFF();
    Standby_detect(); // 检测继电器状态, 这里偷懒用状态相同的standby进行检测
    // if ((A03.State.TotalVoltage_cal - 415) > 0) A03.State.SOC = 100;
    // 此处应该还有一个开路电压测SOC
    // Flash_Write_SOC(); // 存Flash
    // 开RTC
}

/*	brief: A03.Tooling的任一个变量: 0:不控制, 默认关闭; 1:开启; 2:关闭
 * */
void BMS_Manual_mode(void)
{
    if (A03.Tooling.AIRP_Ctrl)
    {
        switch (A03.Tooling.AIRP_Ctrl)
        {
        case Manual_RELAY_ON:
            AIRP_ON();
            break;
        case Manual_RELAY_OFF:
            AIRP_OFF();
            break;
        }
    }
    if (A03.Tooling.AIRN_Ctrl)
    {
        switch (A03.Tooling.AIRN_Ctrl)
        {
        case Manual_RELAY_ON:
            AIRN_ON();
            break;
        case Manual_RELAY_OFF:
            AIRN_OFF();
            break;
        }
    }
    /*做个工装模式的保护，预充和主正继电器不能同时打开*/
    if ((A03.Tooling.AIRP_Ctrl == 1) && (A03.Tooling.PrechargeAIR_Ctrl == 1))
    {
        A03.Alarm.Relay_Error = 1;       // 报错标志位不管有没有屏蔽错误, 先置一
        A03.State.BMS_State = BMS_ERROR; // 进入错误模式
        A03.Tooling.Manual_Enable = 0;   // 退出工装
        return;                          // 强制退出
    }

    if (A03.Tooling.PrechargeAIR_Ctrl)
    {
        switch (A03.Tooling.PrechargeAIR_Ctrl)
        {
        case Manual_RELAY_ON:
            PrechargeAIR_ON();
            break;
        case Manual_RELAY_OFF:
            PrechargeAIR_OFF();
            break;
        }
    }
    if (A03.Tooling.Error_Ctrl)
    {
        switch (A03.Tooling.Error_Ctrl)
        {
        case Manual_ERROR_ON:
            HAL_GPIO_WritePin(Error_Beep_GPIO_Port, Error_Beep_Pin, GPIO_PIN_SET);
            break;
        case Manual_ERROR_OFF:
            HAL_GPIO_WritePin(Error_Beep_GPIO_Port, Error_Beep_Pin, GPIO_PIN_RESET);
            break;
        }
    }
    if (A03.Tooling.Fans_Ctrl)
    {
        switch (A03.Tooling.Fans_Ctrl)
        {
        case Manual_FANS_ON:
            Fans_ON();
            break;
        case Manual_FANS_OFF:
            Fans_OFF();
            break;
        }
    }
    if (A03.Tooling.Manual_Enable)
    {
        switch (A03.Tooling.Manual_Enable)
        {
        case Manual_ENABLE:
            A03.State.BMS_State = BMS_Manual_MODE;
            break;
        case Manual_DISABLE:
            A03.State.BMS_State = BMS_STANDBY;
            AIRS_OFF();
            Fans_OFF();
            HAL_GPIO_WritePin(Error_Beep_GPIO_Port, Error_Beep_Pin, GPIO_PIN_RESET);
            break; // 退出工装模式后要关继电器, 确保安全
        }
    }
    if (A03.Tooling.Active_onHV)
    {
        switch (A03.Tooling.Active_onHV)
        {
        case Manual_Active_ON:
            A03.State.BMS_State = BMS_RUN;
            break;
        case Manual_Active_OFF:
            break;
        }
    }
    /*做个防上位机bug的保护，不能同时进行下工装调试和工装上高压*/
    if ((A03.Tooling.Manual_Enable == Manual_DISABLE) && (A03.Tooling.Active_onHV == Manual_Active_ON))
    {
        A03.State.BMS_State = BMS_ERROR; // 进入错误模式, 不用error()是因为在工装模式下不应该进故障报错
        A03.Tooling.Manual_Enable = 0;   // 退出工装
        A03.Tooling.Active_onHV = 0;     // 不能让其有机会进入运行状态
        return;                          // 强制退出
    }
    /*做个防上位机bug的保护，不能在要工装上高压时候不打开继电器供电正和主负继电器,而且也不能不关闭预充继电器*/
    if (A03.Tooling.Active_onHV == Manual_Active_ON)
    {
        if ((A03.Tooling.AIRP_Ctrl != Manual_RELAY_ON) || (A03.Tooling.AIRN_Ctrl != Manual_RELAY_ON) || (A03.Tooling.PrechargeAIR_Ctrl != Manual_RELAY_OFF))
        {
            A03.State.BMS_State = BMS_ERROR; // 进入错误模式
            A03.Tooling.Manual_Enable = 0;   // 退出工装
            A03.Tooling.Active_onHV = 0;     // 不能让其有机会进入运行状态
            return;                          // 强制退出
        }
    }
}
