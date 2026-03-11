/*
 * daisy.h
 *
 *  Created on: 2022年3月5日
 *      Author: BBBBB
 */
#ifndef DAISY_DAISY_H_
#define DAISY_DAISY_H_

#include "ltc681x-user.h"
#include "data-user.h"

//IC总数
#define Total_ic 8
//电芯数量最大的模组的电芯数, 当模组变化时候记得改
#define Max_ncell 18

//每个模组需要检测的温度数量, 最多只能测量9个GPIO
#define nGPIO	 6
//模块的单体电池数, 需要移植时修改其中的值
#define MODULE1  18		//16
#define MODULE2  18		//16
#define MODULE3  18
#define MODULE4  18
#define MODULE5  18
#define MODULE6  18
#define MODULE7  18
#define MODULE8  18
//GPIO外部额定电压,也就是Vref2,单位:V
#define GPIO_Vol 3
//风扇开启/关闭
#define FAN_ON   1
#define FAN_OFF  0

//下面是简单的ic，拿下面几个参数用就足够了
typedef struct
{
	u16    CellVol[Max_ncell];				//单位mV
	u16    CellTem[nGPIO];					//单位°C
	u32    ModuleVol;				 		//单位mV .模组总压
	u32    DCC;								//被动均衡标志位, 最低位为1号电池，直接接收来自ic读取到的寄存器值
	u8	   single_DCC[Max_ncell];			//单个DCC位拿出来处理,似乎只是用来作为中间量方便处理，后续传递给E03
	double Tem_Res[nGPIO];					//热敏电阻值, 查找用
	u8	   Cell_openwire[Max_ncell];		//断线检测位, 监测用
	u8	   Cell_openwire_aux[Max_ncell];	//上面电压断线检测的计数位,超过3次就将上面的从0变成1
	u8	   GPIO_openwire[nGPIO];			//GPIO断线检测位
	u8 	   C0_openwire;						//判断C0是否断线
	u16	   MinCellVoltage_Module;			//某一个模组内的最低单体电压，用于均衡判断
}MODULE;
//最高最低单体电池电压参数
extern u16 MaxCellVoltage;					//最高单体电压
extern u16 MaxVoltageCellNO;				//最高单体电压单体序号
extern u16 MinCellVoltage;					//最低单体电压
extern u16 MinVoltageCellNO;				//最低单体电压单体序号
//过压,欠压标志位:高4位表示过压标志位,低4位表示欠压标志位
extern u8 Symbol_OV_UV;
//高温,低温标志位:高4位表示高温标志位,低4位表示低温标志位
extern u8 Symbol_OT_UT;
//单体电池最高低温参数
extern u16 MaxCellTem;						//最高单体温度
extern u16 MaxTempCellNO;					//最高单体温度单体序号
extern u16 MinCellTem;						//最低单体温度
extern u16 MinTempCellNO;					//最低单体温度单体序号

////实验平台用的数组
//extern MODULE Module[Total_ic];
//extern cell_asic IC[Total_ic];
///*储存每个模块的电芯数量,注意这里的顺序是从离主控最近开始算*/
//extern u8 n_Cell[Total_ic];
////热敏电阻参数
////extern float Standard_Res[100];					//递减标准电阻值,跟下面温度值一一对应
////extern u16 Tem[100];							//10°-80°
//extern float Standard_Res[71];				//递减标准电阻值,跟下面温度值一一对应
//extern u16 Tem[71];							//10°-80°

/*记录转换的时间*/
extern float conv_time_Vol;					/*ADCV命令时长*/
extern float conv_time_Temp;				/*ADAX命令时长*/
extern float conv_time_Openwire;			/*Openwire_check命令时长*/

/*
将Module[nic].CellVol[i]中的数据初始化为FA0(4V)，为的是防止之后有个断线检测麻烦。
将Module[nic].CellTem[i]中的数据初始化为26°
初始化模组总压为64V
初始化DCC位均为0
*/
void Module_Init(MODULE *ic, u8 total_ic);
/*读取所有菊花链内所有电压,储存在详细版的ic[c_ic].cells.c_codes之中*/
void CV_Measure(u8 total_ic, cell_asic *ic);
/*
将ic的ic[c_ic].cells.c_codes转移到Module中的CellVol，也计算模组总压ModuleVol
也通过累加计算了模组总压
*/
void CV_Transfer(u8 total_ic, u8 *nCell, cell_asic *ic, MODULE *module);
/*
断线检测:判断某个电压低于阈值, 进行中断报警
后期可以加上打印哪里出现了断线
*/
void Openwire_check(BMS* bms,
					uint8_t total_ic,
					u8 *nCell,
					MODULE *module);

/*
辅助函数, 计算出现最大最小电压/温度时候的序号
参数: nic,当前第几个IC
	 n,当前第几个电池
返回: 计算出来的序号值
*/
u8 NO_cal(	u8 nic,
			u8 n,
			u8* nCell);

/*
寻找最大最小单体电压值
之后判断是否超阈值变量
*/
void CV_Find_Max_Min(u8 total_ic,
					u8 *nCell,
					MODULE* module,
					u16 *MaxVol,
					u16 *Mark_Max,
					u16 *MinVol,
					u16 *Mark_Min);
/*
将单体电压跟最低电压的1.05倍(5%)比较.
如果超过则将对应的DCC位置1
*/
void DCC_detect(u8 total_ic,
				u8 *nCell,
				cell_asic *ic,
				MODULE* module,
				u16 MinVol);
/*
将cfga和cfgb的状态值配置好, 同时设置好DCC位
之后发送命令修改DCC位, 开启均衡
*/
void Balance_Open(u8 total_ic,
				  u8 *nCell,
				  cell_asic *ic,
				  MODULE* module,
				  u16 MinVol);
/*
将cfga和cfgb的状态值配置好, 同时设置好DCC位
之后发送命令修改DCC位, 关闭均衡
*/
void Balance_Close(u8 total_ic, 
				   cell_asic *ic);
/*
该函数将DCC位全部整理在MODULE的U32的DCC之中
*/
void DCC_Transfer(u8 total_ic,
				  cell_asic *ic,
				  u8 *nCell,
				  MODULE *module);

/*读取所有菊花链内所有的温度电压,储存在详细版的ic[c_ic].aux.a_codes之中*/
void Tem_Measure(u8 total_ic,
				 cell_asic *ic);
/*
通过计算出热敏电阻值,并转移数据到module中
*/
void TemRes_Cal(BMS *bms,
				u8 total_ic,
				u8 total_GPIO,
				cell_asic *ic,
				MODULE *module);
/*
通过查表得出热敏电阻温度,并转移到module中
*/
void Tem_search(u8 total_ic,
				u8 total_GPIO,
				MODULE *module);

/*
寻找最大最小单体温度值
Mark_Max:最大单体电压对应的编号, 高8位表示第几个模块, 低8位表示第几个电池
之后判断是否超阈值变量
*/
void TemFind_Max_Min(MODULE* module,
					u8 total_ic,
					u8 total_GPIO,
					u16 *MaxTem,
					u16  *Mark_Max,
					u16 *MinTem,
					u16  *Mark_Min);
/*设置告警位储存在Symbol_OV_UV之中*/
void set_Volwarning(u16 *MaxVol,
					u16 *MinVol,
					BMS* bms);
/*设置告警位储存在Symbol_OT_UT之中*/
void set_Temwarning(u16 *MaxTem,
					u16 *MinTem,
					BMS* bms);
/*
保存以下信息到E03大结构体中
断线电池序号
*/
void save_Openwire_Vol(	BMS* bms,
						u8 total_ic,
						u8 *nCell,
						MODULE* module);
/*
保存以下信息到E03大结构体中
断线电池序号
*/
void save_Openwire_Temp(BMS* bms,
						u8 total_ic,
						u8 *nCell,
						MODULE* module);

/*
保存以下信息到E03大结构体中
电压/累计总压/最高电压/最高压序号/最低压/最低序号/
*/
void save_Voldata(	BMS* bms,
					MODULE* module,
					u8 total_ic,
					u8 *nCell,
					u16 *MaxVol,
					u16 *Mark_Max,
					u16 *MinVol,
					u16 *Mark_Min);
/*
保存以下信息到E03大结构体中
温度/最高温度/最高温序号/最低温度/最低温序号/
*/
void save_Tempdata(	BMS* bms,
					MODULE* module,
					u8 total_ic,
					u16 *MaxTem,
					u16 *Mark_Max,
					u16 *MinTem,
					u16 *Mark_Min);
/*
保存以下信息到E03大结构体中
DCC
*/
void save_DCCdata(	BMS* bms,
					u8 total_ic,
					u8 *nCell,
					MODULE* module);

void DaisyVol(void);
void Balance(void);
void DaisyTem(void);
void Daisy_Init(void);

void InitSoc(void);

#endif /* DAISY_DAISY_H_ */
