/*
 * os.h
 *
 *  Created on: 2022年3月6日
 *      Author: BBBBB
 */

#ifndef OS_OS_H_
#define OS_OS_H_

#include "data-user.h"

/*下面定义一些任务在任务堆的排序, 如果注释掉了一些其他任务同时又要跑这些任务记得修改数字*/
// #define Pre_NO 0			//预充在操作系统的优先级
// #define CHarge_done_NO 1	//充电完成在操作系统的优先级
// #define anchor_NO 8			//下点火在操作系统的优先级

// 定义任务的几个状态
typedef enum
{
	TASK_IDLE = 0,	// 空闲
	TASK_RUNNING,	// 运行
	TASK_READY,		// 就绪
	TASK_DEL		// 删除
}TaskStatus;

// 任务信息
/*
	注意，若要为任务增加返回参数的接口，只需将结构体中任务函数一项的返回值
	改为void*即可，void*可以返回任意类型的值。
	此处因为没有必要返回参数所以就没有留出接口
*/
typedef struct
{
	TaskStatus status;				// 任务状态
	u16 timer;						// 计时器
	u16 itvTime;					// Interval Time 任务运行间隔时间，这个一般设定好就不再改动
	void (*Task_Function)(void*);	// 任务函数
	void *parameter;				// 任务函数参数，有需要可以使用，没有参数令此参数等于NULL即可，期望使用参数以减少全局变量的使用
									// void*可以接受任意类型的变量，但使用时注意要做转换，不会用上网查
}TaskInfo;

// PCB（Process Control Block）
// 进程控制块
typedef struct Task_
{
	TaskInfo taskInfo;
	struct Task_ *nextTask;
}Task;

extern Task Precharge_Run_Handle, charge_ing_Handle, BMS_StateMachine_Handle,
			Hall_Timeout_Handle, State_Upgrade_Handle, Slave_Measure_Handle,
			OpenVol_Check_Handle, BMS_Can_Handle, IMD_Detect_Handle,
			Slave_Balance_Handle;

// 任务管理器
// 记录了现在正在运行的任务以及链表中的第一个任务
typedef struct
{
	Task *taskNow;
	Task *taskHead;
}TaskManager;

// typedef struct _TASK_COMPONENTS{
// 	u8 Run;					//程序运行标记: 0不运行 1运行
// 	u16 Timer;				//计时器
// 	u16 ItvTime;			//任务运行间隔时间, 这个一般设定好就不再改动
// 	void (*TaskHook)(void);	//任务定义
// }TASK_COMPONENTS; 			//任务定义


// 任务相关的操作有两种思路
/*
	1、为每一个函数创建一个句柄，这样会占用更多内存，但是对任务的管理相对较为方便和快速，编
	程也相对较为简单；
	2、使用malloc和free组合来管理任务，这样可以动态管理内存，只需要一个Task的指针就可以了，
	但缺点是对任务的管理较为复杂，删除一个任务后要手动释放内存，否则可能造成内存泄漏。
	总体考量下，整个BMS主控所占用的内存还没有到芯片内存不够用的程度，因此采用方法1，若是有需求，
	可以改为方法2，此时应在TaskInfo中多加一个记录任务名字（字符串）的变量，用于精确找到某个任务
	并进行一些操作。这个功能在方法1中由各任务的句柄完成。
*/

// 创建一个新任务，也即将一个任务加入链表中
void TaskCreate(Task *taskHandle, u16 itvTime,
				void (*Task_Function)(void *), void *parameter);

// 删除一个任务，需要注意的是，此处的删除只是将任务从链表中移除，任务的句柄还存在
void TaskDelete(Task *taskHandle);

// 各个任务计时用
// 在每一次调用时更新链表中任务的状态
void TaskStatusUpdate(void);

// 任务调度
void TaskScheduler(void);

// void Precharge_Run(void);

// void charge_ing(void);

// void Hall_Timeout(void);

// void State_Upgrade(void);
// void Vol_deviance(void);
// void Current_detect(void);
// void TotalVol_detect(void);
// void Fans_control(void);

// void Slave_Measure(void);

// void OpenVol_Check(void);

// void BMS_Can(void);

// void BMS_anchor(void);
// //IMD故障检测
// void IMD_Detect(void);

// 由于PCB中的Task_Function的类型为void (*)(void *)，因此就算是没有函数的参数也要声明一个类型为void*的参数，这里统一用parameterNull作为参数的名字，以表明该函数实际上是没有参数的
// 有一些函数是被其他函数调用的，这些函数的参数不用按规范来
void Precharge_Run(void *parameterNull);
void charge_ing(void *parameterNull);
void Hall_Timeout(void *parameterNull);
void State_Upgrade(void *parameterNull);
void Vol_deviance(void);
void Current_detect(void);
void TotalVol_detect(void);
void Fans_control(void);
void Slave_Measure(void *parameterNull);
void OpenVol_Check(void *parameterNull);
void BMS_Can(void *parameterNull);
void BMS_anchor(void);
//IMD故障检测
void IMD_Detect(void *parameterNull);
void Slave_Balance(void *parameterNull);
#endif /* OS_OS_H_ */
