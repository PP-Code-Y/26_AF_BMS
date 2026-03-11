/*
 * os.c
 *	BMS的操作系统
 *  Created on: 2022年3月6日
 *      Author: BBBBB
 */

#include "data-user.h"
#include "os-user.h"
#include "state-user.h"
#include "adc-user.h"
#include "daisy-user.h"
#include "can-user.h"
#include "ltc681x-user.h"
#include "relay-user.h"
#include "stm32f1xx_hal_spi.h"

// 声明任务的句柄，所有句柄的名字就是对应函数加上_Handle后缀
Task Precharge_Run_Handle, charge_ing_Handle, BMS_StateMachine_Handle,
    Hall_Timeout_Handle, State_Upgrade_Handle, Slave_Measure_Handle,
    OpenVol_Check_Handle, BMS_Can_Handle, IMD_Detect_Handle,
    Slave_Balance_Handle;

// /*改任务记得修改*/
// #define TASK_NUM sizeof(BMS_Tasks)/sizeof(TASK_COMPONENTS)	//计算任务数, 以前是单纯设置数字, 然后如果注释下面任务就要改数字, 现在用sizeof就方便很多, 自动计算结构体数组里面的元素个数

extern BMS A03;
extern MODULE Module[Total_ic];
extern u8 n_Cell[Total_ic];
vu8 Hall_Count = 0; // 100ms内没有收到霍尔电流数据就自增1, 然后进入task3里面判断几级霍尔离线
vu8 Can_Charge_Count = 0;
vu8 Precharge_Count = 0; // 用于检测计数，预充时持续检测预充电压是否达到阈值，每次间隔1秒，若6次进入检测仍然失败，进报错；同时提供一个预充检测的开始时间，计数达到4后才进行检测判断
vu8 Balance_flag = 0;

vu8 Trigger_True = 0; // 发生熄火信号

/********************************************************************
 * 任务定义
 ********************************************************************/
// TASK_COMPONENTS BMS_Tasks[]=
// {
// 	{0, 0xFFFF, 0xFFFF, Precharge_Run},
// 	{0, 1000, 1000, charge_ing},	//1S, 后续应仿照上一条任务一样, 正常为0xFF, 该运行时改回正常值
// 	{0, 50, 50, BMS_StateMachine},	//50ms
// 	{0, 100, 100, Hall_Timeout},	//100ms
// 	{0, 100, 100, State_Upgrade},	//100ms
// 	{0, 200, 200, Slave_Measure},	//60ms
// 	{0, 250, 250, OpenVol_Check},	//500ms, 断线检测, 累计断线2次才会报错, 符合赛规的0.5s断开原则
// 	{0, 100, 100, BMS_Can},			//100ms, 只负责发BMS的一些状态数据, 不包括充电机CAN
// 	{0, 500, 500, IMD_Detect}		//500ms, IMD的状态监测
// };
// //此函数在TIM中断回调运行, 为了计时用
// void TaskStatusCheck(TASK_COMPONENTS* TaskComps){
// 	u8 i;
// 	for(i=0; i<TASK_NUM; i++){			//逐个任务时间处理
// 		if(TaskComps[i].Timer){			//时间不为0
// 			TaskComps[i].Timer--;		//减去1个节拍
// 			if(!TaskComps[i].Timer){	//时间减完了, 时间到了
// 				TaskComps[i].Timer = TaskComps[i].ItvTime;	//恢复定时器值, 开始下一轮计时
// 				TaskComps[i].Run = 1;	//任务可以去运行了
// 			}
// 		}
// 	}
// }
// //在while里面跑, 为执行每个任务, 同时可以调整上面结构体的顺序以调整优先级
// void TaskSchedule(TASK_COMPONENTS* TaskComps){
// 	u8 i;
// 	for(i=0; i<TASK_NUM; i++){	//逐个任务时间处理
// 		if(TaskComps[i].Run){	//程序可以运行
// 			TaskComps[i].TaskHook();	//运行任务
// 			TaskComps[i].Run = 0;		//标志位清零
// 		}
// 	}
// }

// 任务管理器
// 声明任务管理器是volatile的，防止编译器优化
volatile TaskManager taskManager = {NULL, NULL};

/**
 * @brief  创建一个任务，也即将一个任务添加到链表中
 * @param  taskHandle PCB指针，指向要创建任务的PCB
 * @param  itvTime 任务运行间隔时间，每次任务的计时器走到零，就使用改变量重新装填定时器的值
 * @param  Task_Function 要运行的任务函数
 * @param  parameter 要传递给任务函数的参数，没有则填NULL
 * @retval 无
 * @author ride_pig
 */
void TaskCreate(Task *taskHandle, u16 itvTime, void (*Task_Function)(void *), void *parameter)
{
    taskHandle->taskInfo.timer = taskHandle->taskInfo.itvTime = itvTime;
    taskHandle->taskInfo.Task_Function = Task_Function;
    taskHandle->taskInfo.parameter = parameter;
    taskHandle->taskInfo.status = TASK_IDLE;
    taskHandle->nextTask = NULL;

    // lastTask为当前链表中最后一个任务，使用for循环找到当前最后一个任务，并将其下一个任务设为新创建的任务
    if (taskManager.taskHead != NULL)
    {
        Task *lastTask = taskManager.taskHead;
        for (; lastTask->nextTask != NULL; lastTask = lastTask->nextTask)
        {
        }
        lastTask->nextTask = taskHandle;
    }
    else
    {
        taskManager.taskHead = taskHandle;
    }
}

/**
 * @brief  从链表中删除一个任务。注意，这里只是将任务从链表中去除，任务句柄仍然还在
 * @param  taskHandle PCB指针，指向要删除任务的PCB
 * @retval 无
 * @author ride_pig
 */
void TaskDelete(Task *taskHandle)
{
    // 如果要删除的任务是链表中的第一个任务，则需要做相应的处理
    if (taskManager.taskHead == taskHandle)
    {
        taskManager.taskHead = taskHandle->nextTask;
        taskHandle->nextTask = NULL;
        taskHandle->taskInfo.status = TASK_DEL;
    }
    else
    {
        // 若要删除的任务不是链表中的第一个任务，则找到要删除任务的前一个任务并作相应的处理
        Task *previousTask = taskManager.taskHead;
        for (; previousTask->nextTask != NULL; previousTask = previousTask->nextTask)
        {
            if (previousTask->nextTask == taskHandle)
            {
                // 把要删除任务前一个任务的nextTask设为要删除任务的nextTask
                previousTask->nextTask = taskHandle->nextTask;
                // 对要删除任务的状态等做相应的更新
                taskHandle->nextTask = NULL;
                taskHandle->taskInfo.status = TASK_DEL;
            }
        }
    }
}

/**
 * @brief  每次调用时更新链表中每个任务的状态
 * @param  无
 * @retval 无
 * @author ride_pig
 */
void TaskStatusUpdate(void)
{
    // 利用任务管理器中储存的链表头信息遍历整个链表
    Task *taskPointer = taskManager.taskHead;
    for (; taskPointer != NULL; taskPointer = taskPointer->nextTask)
    {
        if (taskPointer->taskInfo.status == TASK_IDLE)
        {
            // 使用自减运算符在前的形式，相比自减运算符在后的形式更加节省时间
            --taskPointer->taskInfo.timer;
            // 若是timer减到0，那么任务进入就绪状态（可以运行了）
            if (taskPointer->taskInfo.timer == 0)
            {
                taskPointer->taskInfo.timer = taskPointer->taskInfo.itvTime;
                taskPointer->taskInfo.status = TASK_READY;
            }
        }
    }
}

/**
 * @brief  任务调度器，跑在main.c的主循环中，按照任务创建的先后来依次运行就绪的任务
           先进入链表的任务，也即先创建的任务，拥有更高的优先级
 * @param  无
 * @retval 无
 * @author ride_pig
 */
void TaskScheduler(void)
{
    Task *taskPointer = taskManager.taskHead;
    for (; taskPointer != NULL; taskPointer = taskPointer->nextTask)
    {
        if (taskPointer->taskInfo.status == TASK_READY)
        {
            taskPointer->taskInfo.status = TASK_RUNNING;
            taskManager.taskNow = taskPointer;
            taskPointer->taskInfo.Task_Function(taskPointer->taskInfo.parameter);
            // 运行到这里，说明上一句调用的任务运行结束了，对任务管理器以及被调用的任务的状态做相应的更新
            taskManager.taskNow = NULL;
        }
        // 如果运行到这里的时候任务的状态已经是TASK_DEL，那么就不再对任务的状态做更新了
        if (taskPointer->taskInfo.status == TASK_RUNNING)
        {
            taskPointer->taskInfo.status = TASK_IDLE;
        }
    }
}

/*	task0
 * 	预充判断完成函数, 当从standby状态跳到precharge状态后, 任务的计数值将被修改成正常值,
 * 	到点了就跑到这个函数上面来, 来判断预充是否成功
 * */
void Precharge_Run(void *parameterNull)
{
    // 虽然很长时间才跑进来一次, 但是要判断到底是不是预充状态
    if (A03.State.BMS_State == BMS_PRECHARGE)
    {
        A03.State.TotalVoltage = TS_Voltage();
        A03.State.PrechargeVoltage = PRE_Voltage();
        Precharge_Count++;                              // 预充计时自增
        if (A03.Alarm.error_mute & Precharge_Fail_mute) // 按下预充失败屏蔽后强上，防止在按下预充失败屏蔽强上后点火直接接通主正主负，进行适当延时
        {
            // if(Precharge_Count >= ( A03.Threshold.Precharge_Time * 2)){
            if (Precharge_Count >= (6 * 2))
            { // A03.Threshold.Precharge_Time=6
                if ((100.0 * A03.State.PrechargeVoltage / A03.State.TotalVoltage) > 90 && A03.State.PrechargeVoltage > 5000)
                { // 防止预充失败屏蔽下压差过大，闭合瞬间产生大电流，使继电器粘连
                    if (A03.State.Charge_flag)
                    { // 此时是充电状态, charge_flag==1
                        for (u8 i = 0; i < 100; i++)
                        {
                            AIRP_ON(); // 打开AIRP
                        }
                        for (u8 i = 0; i < 100; i++)
                        {
                            AIRN_ON();
                        }
                        for (u8 i = 0; i < 100; i++)
                        {
                            PrechargeAIR_OFF(); // 关闭PREAIR
                        }
                        HAL_Delay(100);
                        for (u8 i = 0; i < 100; i++)
                        {
                            AIRP_ON(); // 打开AIRP
                        }
                        for (u8 i = 0; i < 100; i++)
                        {
                            AIRN_ON();
                        }
                        if (Charge_detect() && (A03.Alarm.Chargemachine_Error == 0))
                        {                                     // 主要是进行继电器状态的检测, 如有问题会报错返回0, 跳过if, 无问题向下执行
                            A03.State.BMS_State = BMS_CHARGE; // 下一步进入charge状态, 在charge_ing()里面一直发CAN充电函数, 并在statemachine一直检查状态
                        }
                    }
                    else
                    { // 此时上车了, 属于(Run)放电状态, charge_flag==0
                        //				Run_relay_open();	//进行Run下的继电器打开或关闭
                        for (u8 i = 0; i < 100; i++)
                        {
                            AIRP_ON(); // 打开AIRP
                        }
                        for (u8 i = 0; i < 100; i++)
                        {
                            AIRN_ON();
                        }
                        for (u8 i = 0; i < 100; i++)
                        {
                            PrechargeAIR_OFF(); // 关闭PREAIR
                        }
                        HAL_Delay(100);
                        for (u8 i = 0; i < 100; i++)
                        {
                            AIRP_ON(); // 打开AIRP
                        }
                        for (u8 i = 0; i < 100; i++)
                        {
                            AIRN_ON();
                        }
                        if (Run_detect())
                        { // 主要是进行继电器状态的检测, 如有问题会报错, 无问题向下执行

                            A03.State.BMS_State = BMS_RUN; // 下一步进入run状态, 在statemachine一直检查状态
                        }
                    }
                    Precharge_Count = 0;
                }
                else
                {
                    A03.Alarm.Precharge_Fail = 1;
                    A03.State.BMS_State = BMS_ERROR; // 虽然此时屏蔽了预充失败，但是处于危险情况，BMS也进入故障状态
                    Precharge_Count = 0;
                }
            }
            // } else if ((100.0 * A03.State.PrechargeVoltage / A03.State.TotalVoltage) > A03.Threshold.Precharge_End_Vol_Percent) { // 大于95%, 算预充成功 (A03.Threshold.Precharge_End_Vol_Percent=95)
        }
        else if ((100.0 * A03.State.PrechargeVoltage / A03.State.TotalVoltage) > 95 && A03.State.PrechargeVoltage > 5000)
        { // 大于95%, 算预充成功 (A03.Threshold.Precharge_End_Vol_Percent=95)
            if (Precharge_Count >= 8)
            {
                if (A03.State.Charge_flag)
                { // 此时是充电状态, charge_flag==1
                    for (u8 i = 0; i < 100; i++)
                    {
                        AIRP_ON(); // 打开AIRP
                        AIRN_ON();
                        PrechargeAIR_OFF();
                    }
                    HAL_Delay(100);
                    for (u8 i = 0; i < 100; i++)
                    {
                        AIRP_ON(); // 打开AIRP
                    }
                    for (u8 i = 0; i < 100; i++)
                    {
                        AIRN_ON();
                    }
                    for (u8 i = 0; i < 100; i++)
                    {
                        PrechargeAIR_OFF(); // 关闭PREAIR
                    }
                    HAL_Delay(100);
                    for (u8 i = 0; i < 100; i++)
                    {
                        AIRP_ON(); // 打开AIRP
                    }
                    for (u8 i = 0; i < 100; i++)
                    {
                        AIRN_ON();
                    }
                    if (Charge_detect() && (A03.Alarm.Chargemachine_Error == 0))
                    {                                     // 主要是进行继电器状态的检测, 如有问题会报错返回0, 跳过if, 无问题向下执行
                        A03.State.BMS_State = BMS_CHARGE; // 下一步进入charge状态, 在charge_ing()里面一直发CAN充电函数, 并在statemachine一直检查状态
                    }
                }
                else
                { // 此时上车了, 属于(Run)放电状态, charge_flag==0
                    for (u8 i = 0; i < 100; i++)
                    {
                        AIRP_ON(); // 打开AIRP
                        AIRN_ON();
                        PrechargeAIR_OFF();
                    }
                    HAL_Delay(100);
                    for (u8 i = 0; i < 100; i++)
                    {
                        AIRP_ON(); // 打开AIRP
                    }
                    for (u8 i = 0; i < 100; i++)
                    {
                        AIRN_ON();
                    }
                    for (u8 i = 0; i < 100; i++)
                    {
                        PrechargeAIR_OFF(); // 关闭PREAIR
                    }
                    HAL_Delay(100);
                    for (u8 i = 0; i < 100; i++)
                    {
                        AIRP_ON(); // 打开AIRP
                    }
                    for (u8 i = 0; i < 100; i++)
                    {
                        AIRN_ON();
                    }
                    if (Run_detect())
                    {                                  // 主要是进行继电器状态的检测, 如有问题会报错, 无问题向下执行
                        A03.State.BMS_State = BMS_RUN; // 下一步进入run状态, 在statemachine一直检查状态
                    }
                }
                Precharge_Count = 0;
            }
        }
        else
        {
            // if((Precharge_Count >= (A03.Threshold.Precharge_Time * 2)) && (A03.State.Charge_flag == 0)){ // 上车情况下的预充达不到阈值, 报故障
            if ((Precharge_Count >= (10 * 2)) && (A03.State.Charge_flag == 0))
            {                                 // 上车情况下的预充达不到阈值, 报故障
                A03.Alarm.Precharge_Fail = 1; // 预充失败告警
                if (!(A03.Alarm.error_mute & Precharge_Fail_mute))
                {            // 屏蔽位判断,如果没有设置屏蔽位则可以报警, 其实也就是判断相关位有没有置为1,如果是1那不管左移多少位都是大于0的
                    Error(); // 输出故障信号-关闭继电器-修改状态为error
                }
                Precharge_Count = 0;
            }
            else if ((Precharge_Count >= 40) && (A03.State.Charge_flag == 1))
            {                                 // 充电情况下的预充达不到阈值，报故障
                A03.Alarm.Precharge_Fail = 1; // 预充失败告警
                if (!(A03.Alarm.error_mute & Precharge_Fail_mute))
                {            // 屏蔽位判断,如果没有设置屏蔽位则可以报警, 其实也就是判断相关位有没有置为1,如果是1那不管左移多少位都是大于0的
                    Error(); // 输出故障信号-关闭继电器-修改状态为error
                }
                Precharge_Count = 0;
            }
        }
    }
    else
    {
        // BMS_Tasks[Pre_NO].ItvTime=0xFFFF;	//此时还不是预充状态, 所以关闭预充计时, 可以理解为将值调的特别大就代表关闭了. 然后如果跑完了上面的if之后被修改了状态, 下一次跑进这个函数也是会修改阈值
        Task *taskPointer = taskManager.taskHead;
        // 此时还不是预充状态, 所以关闭预充计时, 可以理解为将值调的特别大就代表关闭了. 然后如果跑完了上面的if之后被修改了状态, 下一次跑进这个函数也是会修改阈值
        for (; taskPointer != NULL; taskPointer = taskPointer->nextTask)
        {
            if (taskPointer->taskInfo.Task_Function == Precharge_Run_Handle.taskInfo.Task_Function)
            {
                taskPointer->taskInfo.itvTime = 0xFFFF;
            }
        }
    }
}
// task1 充电状态下发送CAN数据给充电机
void charge_ing(void *parameterNull)
{
    if (A03.State.BMS_State == BMS_CHARGE)
        Can_Send_Charge(ENABLE); // 充电状态下才能发充电机CAN数据, 启动充电机
    else if (A03.State.BMS_State == BMS_CHARGEDONE)
        Can_Send_Charge(DISABLE); // 充电完成, 关闭充电机输出
    else if ((A03.State.BMS_State == BMS_ERROR) && (A03.State.Charge_flag == 1))
        Can_Send_Charge(DISABLE); // 充电过程中出现了错误, 也要发送关闭充电机输出
}
/*	task2 BMS_StateMachine()
 * 状态机任务, 为50ms一次判断当前bms状态, 进入的函数是判断该状态下的继电器状态是否正常
 * */

/*task3: 霍尔电流超时判断, 判断超时时间
 * 如果没有收到霍尔电流CAN信号, Hall_Count会一直自增, 而当收到CAN会在中断服务函数里面清零
 * */
void Hall_Timeout(void *parameterNull)
{
    Hall_Count++;
    if (Hall_Count > 2)
    { // 10s离线 现在并未考虑更低等级的霍尔电流计离线报错，而是只要断线计数大于2就直接进报错
        A03.Alarm.Hall_Openwires = 1;
        if (!(A03.Alarm.error_mute & Hall_Openwires_mute))
        { // 判断相应的位是否被禁止告警
            Error();
        }
        return;
    }
    else if (Hall_Count > A03.Threshold.Hall_Openwires_LV2)
    { // 8s离线
        A03.Alarm.Hall_Openwires = 2;
        return;
    }
    else if (Hall_Count > A03.Threshold.Hall_Openwires_LV3)
    { // 5s离线
        A03.Alarm.Hall_Openwires = 3;
        return;
    }
    else if (Hall_Count > A03.Threshold.Hall_Openwires_LV4)
    { // 1s离线
        A03.Alarm.Hall_Openwires = 4;
    }
}

/*	task4 状态更新
 * 	采集并更新预充电压和总压, 各个继电器状态
 * 	判断是否发生高压异常: 累积总压和总压相差太多?
 * */
// void State_Upgrade(){
// 	//预充电压&&总压采集
// 	A03.State.TotalVoltage=TS_Voltage();
// 	A03.State.PrechargeVoltage=PRE_Voltage();
// 	//AIRS状态采集
// 	AIRP_State();
// 	AIRN_State();
// 	PREAIR_State();
// 	//高压异常判断
// 	Vol_deviance();
// 	//充放电过流判断
// 	Current_detect();
// 	//根据温度判断风扇是否开启
// 	Fans_control();
// }

void State_Upgrade(void *parameterNull)
{
    // 预充电压&&总压采集
    A03.State.TotalVoltage = TS_Voltage();
    A03.State.PrechargeVoltage = PRE_Voltage();
    // AIRS状态采集
    A03.Relay.AIRP_State = AIRP_State();
    A03.Relay.AIRN_State = AIRN_State();
    A03.Relay.PrechargeAIR_State = PREAIR_State();
    // 高压异常判断 累积电压与总压差值
    Vol_deviance();
    // 充放电过流判断 充电判断
    Current_detect();
    // 根据温度判断风扇是否开启
    // Fans_control();
}

// task4从属函数:  高压异常判断,如果累积总压和总压相差太多要报故障
void Vol_deviance()
{
    static vu8 Vol_deverror = 0;
    vu16 Delta = 0;
    u8 precent;
    // 绝对值计算
    if (A03.State.TotalVoltage > A03.State.TotalVoltage_cal)
    {
        Delta = A03.State.TotalVoltage - A03.State.TotalVoltage_cal;
        precent = 100.0f * (Delta / A03.State.TotalVoltage); // 单位: %
    }
    else
    {
        Delta = A03.State.TotalVoltage_cal - A03.State.TotalVoltage;
        precent = 100.0f * (Delta / A03.State.TotalVoltage_cal); // 单位: %
    }

    if (precent > A03.Threshold.Vol_Error_LV4)
        Vol_deverror++; // 缓冲一段，避免采样有误差或者突然有个大偏差导致的误判
    else
        Vol_deverror = 0;
    if (Vol_deverror > 2)
    {
        if (precent > A03.Threshold.Vol_Error_LV1)
        { // 一级故障并需要报警
            A03.Alarm.Vol_Error = 1;
            if ((A03.Alarm.error_mute & Vol_Error_mute) == 0)
            { // 判断相应的位是否被禁止告警
                Error();
            }
        }
        else if (precent > A03.Threshold.Vol_Error_LV2)
            A03.Alarm.Vol_Error = 2;
        else if (precent > A03.Threshold.Vol_Error_LV3)
            A03.Alarm.Vol_Error = 3;
        else if (precent > A03.Threshold.Vol_Error_LV4)
            A03.Alarm.Vol_Error = 4;
    }
}
// task4从属函数: 电流判断, 判断在充电和放电时候电流是否过流
void Current_detect()
{
    vs16 temp_cur = A03.State.TotalCurrent / 100; // 将电流单位从mA转换成0.1A/bit, 如6000mA被转化为60, 也就是6A
    if (temp_cur < 0)
        temp_cur = (-1) * temp_cur; // 取个绝对值
    if (A03.State.BMS_State == BMS_RUN)
    { // 此时电流为正值
        if (temp_cur > A03.Threshold.StaticDischarge_OC_LV1)
        {
            A03.Alarm.StaticDischarge_OC = 1;
            if (!(A03.Alarm.error_mute & StaticDischarge_OC_mute))
            { // 判断相应的位是否被禁止告警
                Error();
            }
            return;
        }
        else if (temp_cur > A03.Threshold.StaticDischarge_OC_LV2)
        {
            A03.Alarm.StaticDischarge_OC = 2;
            if (!(A03.Alarm.error_mute & StaticDischarge_OC_mute))
            { // 判断相应的位是否被禁止告警
                Error();
            }
            return;
        }
        else if (temp_cur > A03.Threshold.StaticDischarge_OC_LV3)
        {
            A03.Alarm.StaticDischarge_OC = 3;
            if (!(A03.Alarm.error_mute & StaticDischarge_OC_mute))
            { // 判断相应的位是否被禁止告警
                Error();
            }
            return;
        }
        else if (temp_cur > A03.Threshold.StaticDischarge_OC_LV4)
        {
            A03.Alarm.StaticDischarge_OC = 4;
            if (!(A03.Alarm.error_mute & StaticDischarge_OC_mute))
            { // 判断相应的位是否被禁止告警
                Error();
            }
            return;
        }
    }
    else if (A03.State.BMS_State == BMS_CHARGE)
    { // 此时电流为负值
        if (temp_cur > A03.Threshold.StaticCharge_OC_LV1)
        {
            A03.Alarm.StaticCharge_OC = 1;
            if (!(A03.Alarm.error_mute & StaticCharge_OC_mute))
            { // 判断相应的位是否被禁止告警
                Error();
            }
            return;
        }
        else if (temp_cur > A03.Threshold.StaticCharge_OC_LV2)
        {
            A03.Alarm.StaticCharge_OC = 2;
            return;
        }
        else if (temp_cur > A03.Threshold.StaticCharge_OC_LV3)
        {
            A03.Alarm.StaticCharge_OC = 3;
            return;
        }
        else if (temp_cur > A03.Threshold.StaticCharge_OC_LV4)
        {
            A03.Alarm.StaticCharge_OC = 4;
        }
    }
}
// task4从属函数: 判断风扇是否达到条件开启
void Fans_control()
{
    /*超过开风扇阈值要开风扇, 还需要判断BMS是否处于工装模式, 否则会引起继电器一直开合*/
    if (((A03.Cell.MaxTemp > A03.Threshold.Fans_Start_OT) || ((A03.Cell.MaxTemp - A03.Cell.MinTemp) > A03.Threshold.Fans_Start_ODT)) && (A03.State.BMS_State != BMS_Manual_MODE))
    {
        Fans_ON();
    }
    /*要同时达到两个关风扇的阈值条件才能关风扇 还需要判断BMS是否处于工装模式, 否则会引起继电器一直开合*/
    else if (((A03.Cell.MaxTemp < A03.Threshold.Fans_End_OT) && ((A03.Cell.MaxTemp - A03.Cell.MinTemp) < A03.Threshold.Fans_End_ODT)) && (A03.State.BMS_State != BMS_Manual_MODE))
    {
        Fans_OFF();
    }
}
/*	task5 电压/温度采集
 * 	采集单体电压和单体温度
 * */
void Slave_Measure(void *parameterNull)
{
    wakeup_sleep(Total_ic);
    DaisyVol();                                         // 采集单体电压
    DaisyTem();                                         // 采集单体温度
    Openwire_check(&A03, Total_ic, n_Cell, Module);     // 电压采集断线检测
    save_Openwire_Temp(&A03, Total_ic, n_Cell, Module); // 温度采集断线检测
}

/*	task6 断线检测
 * 	检测单体电压采集是否断线. 单体温度断线采集在DaisyTem已经包含, 目前已经加入到这里来, 将DaisyTem的注释掉了
 * */
void OpenVol_Check(void *parameterNull)
{
    //已经放在slave——measure里面防止spi抢占
}

/*	task7 CAN发送
 * 	充电只发送外部CAN, 即只发给充电机
 * 	不充电内部CAN和外部CAN均发送, 内部CAN发给上位机, 外部CAN发给整车
 * */
void BMS_Can(void *parameterNull)
{
    // 发送Can数据 //实际上上位机can始终使能，整车can只发送整车状态始终使能
    if (A03.State.Charge_flag)
    {                                   // BMS接上了充电机
        Can_Send_Data(DISABLE, ENABLE); // 充电CAN只发拓展帧充电数据, BDUCAN发BMS数据
    }
    else
    {                                  // 不处于充电状态下的BMS
        Can_Send_Data(ENABLE, ENABLE); // 前一位是上位机使能，后一位是整车使能
    }
}
/*	task8 下点火(熄火==抛锚)判断
 * 	充电只发送外部CAN, 即只发给充电机
 * 	不充电内部CAN和外部CAN均发送, 内部CAN发给上位机, 外部CAN发给整车
 * */
void BMS_anchor()
{
    // 更改了下电逻辑在BMS_RUN()里面, 所以这里注释掉了
    //	if(Trigger_True){	//下点火信号中断以后经过50ms, 尚且存疑, 所以需要进行if判断
    //		if((!ON_State())&&(A03.State.BMS_State!=BMS_ERROR)){	//失去点火信号, 同时BMS正常下点火(不是error状态)
    //			A03.State.BMS_State=BMS_SDC_OFF;
    //			BMS_SDC_Off();
    //			Trigger_True=0; 	//跑完后,标志位清零
    //		}
    //		else{	//消抖后发现满足不了条件,说明不是真正的下点火信号, 或者说因为BMS故障导致了下点火, 所以重置
    //			Trigger_True=0; //标志位清零
    //			BMS_Tasks[anchor_NO].ItvTime=0xFFFF;
    //			BMS_Tasks[anchor_NO].Timer=0xFFFF;
    //		}
    //	}
    //	//这个else是没有触发下点火信号中断正常跑进来的, 属于无意义状态, 所以重置
    //	else{
    //		Trigger_True=0; //标志位清零
    //		BMS_Tasks[anchor_NO].ItvTime=0xFFFF;
    //		BMS_Tasks[anchor_NO].Timer=0xFFFF;
    //	}
}
/*	task9 IMD状态判断
 * 	读取到了报错
 * */
void IMD_Detect(void *parameterNull)
{
    static u8 first_in = 1;
    static u8 delay_sec = 0;
    // 在3秒后才进行IMD报故障
    if (delay_sec >= 6)
    {
        if (IMD_State() && first_in)
        {                 // 如果检查到IMD出现故障(IMD_State函数返回1), 会进入该函数
            HAL_Delay(5); // 消抖
            if (IMD_State())
            {
                A03.Alarm.IMD_Error = 1; // 设置故障位
                first_in = 0;            // 不要再第二次进入这个函数了, 因为我觉得持续地让单片机每次进这个函数都有存在delay个5ms不是太好
                if ((A03.Alarm.error_mute & IMD_Error_mute) == 0)
                {
                    Error();
                }
            }
            else
            {
                first_in = 1; // 说明只是一点小小的扰动, 应该让这个函数继续跑
            }
        }
        if (IMD_State() == 0)
        {
            A03.Alarm.IMD_Error = 0;
        }
    }
    else
    {
        delay_sec++;
    }
}

void Slave_Balance(void *parameterNull)
{
    Balance(); // 均衡函数，函数原型在daisy-user
}
