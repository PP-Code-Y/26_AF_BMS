/*
 * LTC681X.h
 *
 *  Created on: 2022年3月4日
 *      Author: BBBBB
 *      caution: 需要更换芯片的话得修改的函数:rdaux
 */

#ifndef LTC681X_LTC681X_H_
#define LTC681X_LTC681X_H_

#include "data-user.h"

//c语言没有定义bool类型
typedef enum {False = 0, True = 1} bool;
/*LTC681X芯片参数修改, 现在是6813的参数, 如果用6811需要修改*/
#define chip_cell_channels 18
#define chip_stat_channels 4
#define chip_aux_channels  9
#define chip_num_cv_reg   6
#define chip_num_gpio_reg 4
#define chip_num_stat_reg 2
/*ADC mode*/
#define MD_422HZ_1KHZ  0
#define MD_27KHZ_14KHZ 1
#define MD_7KHZ_3KHZ   2			//这个最常用
#define MD_26HZ_2KHZ   3
/*ADCOPT*/
#define ADC_OPT_ENABLED  1
#define ADC_OPT_DISABLED 0
/*ADCV通道选择*/
#define CH_ALL       0
#define CH_1_7_13    1
#define CH_2_8_14    2
#define CH_3_9_15    3
#define CH_4_10_16   4
#define CH_5_11_17   5
#define CH_6_12_18   6
/*自测*/
#define SELFTEST_1 1
#define SELFTEST_2 2
/*GPIO的选择*/
#define AUX_CH_ALL 	   0
#define AUX_CH_GPIO1_6 1
#define AUX_CH_GPIO2_7 2
#define AUX_CH_GPIO3_8 3
#define AUX_CH_GPIO4_9 4
#define AUX_CH_GPIO5   5
#define AUX_CH_VREF2   6
/*状态寄存器*/
#define STAT_CH_ALL   0
#define STAT_CH_SOC   1
#define STAT_CH_ITEMP 2
#define STAT_CH_VREGA 3
#define STAT_CH_VREGD 4
/*寄存器组选择*/
#define REG_ALL 0
#define REG_1 1
#define REG_2 2
#define REG_3 3
#define REG_4 4
#define REG_5 5
#define REG_6 6
/*ADC时是否允许均衡*/
#define DCP_DISABLED 0
#define DCP_ENABLED 1
/*PUP*/
#define PULL_UP_CURRENT 1
#define PULL_DOWN_CURRENT 0

#define NUM_RX_BYT 8
#define CELL 1
#define AUX  2
#define STAT 3
#define cfgr 0
#define CFGRB 4
#define CS_PIN 10

/*PEC计算相关参数*/
extern int16_t pec15Table[256];
extern int16_t CRC15_POLY;
extern int16_t re, address;						//re本来是remainder的，但是VS里面有个remainder函数所以改成re了
void init_PEC15_Table(void);					//这个主要是跟着pec15_calc函数的
u16 pec15_calc( u8* data, u8 len);				//根据cmd计算pec校验码

/*以下是ltc681x系列芯片官方给的结构体,
 *后面会提取出比较关键的参数组合成更简单的结构体*/
typedef struct{
  u16 c_codes[18];// Cell Voltage Codes 单位mV
  u8 pec_match[6];// If a PEC error was detected during most recent read cmd
} cv;

// AUX Reg Voltage Data
typedef struct{
  u16 a_codes[12];// Aux Voltage Codes
  u8 pec_match[4];// If a PEC error was detected during most recent read cmd
} ax;

//Status Reg data structure.
typedef struct{
  u16 stat_codes[4];// A two dimensional array of the stat voltage codes.
  u8 flags[3]; // byte array that contains the uv/ov flag data
  u8 mux_fail[1]; // Mux self test status flag
  u8 thsd[1]; // Thermal shutdown status
  u8 pec_match[2];// If a PEC error was detected during most recent read cmd
} st;

typedef struct{
  u8 tx_data[6];
  u8 rx_data[8];
  u8 rx_pec_match;//如果在最近一次读取的cmd期间检测到PEC错误
} ic_register;

typedef struct{
  u16 pec_count;
  u16 cfgr_pec;
  u16 cell_pec[6];
  u16 aux_pec[4];
  u16 stat_pec[2];
} pec_counter;

typedef struct{
  u8 cell_channels;
  u8 stat_channels;
  u8 aux_channels;
  u8 num_cv_reg;
  u8 num_gpio_reg;
  u8 num_stat_reg;
} register_cfg;

typedef struct{
  ic_register config;
  ic_register configb;
  cv   cells;           //单位0.1mV
  ax   aux;
  st   stat;
  ic_register com;
  ic_register pwm;
  ic_register pwmb;
  ic_register sctrl;
  ic_register sctrlb;
  bool isospi_reverse;
  pec_counter crc_count;
  register_cfg ic_reg;
  long system_open_wire;
}  cell_asic;

/*片选引脚*/
void cs_low(void);
/*取消片选*/
void cs_high(void);
/*spi读写*/
// void spi_write_read(u8 tx_data[], u8 tx_len, u8 *rx_data, u8 rx_len);
void spi_write_array(u8 len, u8 data[]);
/*唤醒函数*/
void wakeup_idle(u8 total_ic);
void wakeup_sleep(u8 total_ic);

/*PEC计算相关*/
void init_PEC15_Table(void);
u16 pec15_calc(u8* data, u8 len);
void LTC681x_check_pec(u8 total_ic, u8 reg,  cell_asic *ic);
//发送指令函数,跟write68的区别大概是这个可以是发一些clear命令之类的
void cmd_68(u8 tx_cmd[2]);
//写命令,包括把指令和数据一起写入
void write_68(u8 total_ic, u8 tx_cmd[2], u8 data[]);
/*读命令, 发送指令并进行读取*/
int8_t read_68( u8 total_ic, u8 tx_cmd[2], u8 *rx_data);
//用来初始化,但是IC只是存放一个数组的，初始化还需要write68函数来写到寄存器
void LTC681x_init_cfg(u8 total_ic, cell_asic *ic);
//将cfgr寄存器组数据全部测好放在定义好的结构数组内,到时候再write68进去.
void LTC681x_set_cfgr(u8 nIC, cell_asic *ic, bool refon, bool adcopt,
					bool gpio[5], bool dcc[12], bool dcto[4],
					u16 uv, u16  ov);
//以下一系列函数包含在上面函数
//设置CFGAR0的值
void LTC681x_set_cfgr_refon(u8 nIC, cell_asic *ic, bool refon);
//设置ADCOPT位的值
void LTC681x_set_cfgr_adcopt(u8 nIC, cell_asic *ic, bool adcopt);
//设置GPIO位
void LTC681x_set_cfgr_gpio(u8 nIC, cell_asic *ic);
//设置CFGRB的DCC位,Helper function to control discharge
void LTC681x_set_cfgr_dis(u8 nIC, cell_asic *ic,bool dcc[12]);
//设置CFGRA的DCTO位,control discharge time value
void LTC681x_set_cfgr_dcto(u8 nIC, cell_asic *ic,bool dcto[4]);
//设置欠压比较电压值,set uv value in CFG register
void LTC681x_set_cfgr_uv(u8 nIC, cell_asic *ic,u16 uv);
//设置过压比较电压值，set OV value in CFG register
void LTC681x_set_cfgr_ov(u8 nIC, cell_asic *ic,u16 ov);
/*采集电压指令*/
void LTC681x_adcv(u8 MD, u8 DCP, u8 CH);
//采集温度传感器电压
void LTC681x_adax(u8 MD, u8 CHG);
/* ADAXD先不写
 * ADSTAT模式,有计算公式P27,测量所有单元总和SC,内部芯片温度ITMP(热关断),模拟电源VA 数字电源VD*/
void LTC681x_adstat(u8 MD, u8 CHST);
/* ADSTATD先不写
 * ADOW模式，开路检测命令，见P32,检查LTC681的ADC与外部单元之间是否有断线*/
void LTC681x_adow(u8 MD, u8 PUP, u8 CH, u8 DCP);
/*ADCVSC模式，测量电池电压和所有电池的总和P28*/
void LTC681x_adcvsc(u8 MD, u8 DCP);
/*ADCVAX模式,测18个电池电压值和两个GPIO1和2的值,看P25*/
void LTC681x_adcvax(u8 MD, u8 DCP);
/*ADOL模式, 见P30, 首先同时使用ADC1,ADC2测量电池7, 再同时使用ADC2和ADC3测量电池13,两者相比较以确定正确性*/
void LTC681x_adol(u8 MD, u8 DCP);
/*cvst,axst,statst模式为寄存器自检测函数*/
void LTC681x_cvst(u8 MD, u8 ST);
void LTC681x_axst(u8 MD, u8 ST);
void LTC681x_statst(u8 MD, u8 ST);
//轮询函数
u32 LTC681x_pollAdc(void);
/*diagn命令，P31.*/
void LTC681x_diagn(void);
//解码函数:将读回来的数据整合处理为有效的单体电池电压数据
int8_t parse_cells(u8 current_ic, u8 cell_reg, u8 cell_data[],
					u16 *cell_codes, u8 *ic_pec);
//读取CVG的某一个组的值
void LTC681x_rdcv_reg(u8 reg, u8 total_ic, u8 *data);
//读取单个GPIO电压寄存器
void LTC681x_rdaux_reg(u8 reg, u8 total_ic, u8 *data);
//读stat寄存器
void LTC681x_rdstat_reg(u8 reg, u8 total_ic, u8 *data);
//该命令清除电池电压寄存器并将所有值初始化为1
void LTC681x_clrcell(void);
//清除寄存器(the Auxiliary registers)并将所有值初始化为1
void LTC681x_clraux(void);
//该命令清除状态寄存器（the Stat registers）并将所有值初始化为1。
void LTC681x_clrstat(void);
//该命令清除Sctrl寄存器并将所有值初始化为0。
void LTC681x_clrsctrl(void);
//读取单体电芯数据的函数
u8 LTC681x_rdcv(u8 reg, u8 total_ic, cell_asic *ic);
//读取解码后的GPIO电压数据, 目的是算出温度
int8_t LTC681x_rdaux(u8 reg, u8 total_ic, cell_asic *ic);
//辅助函数, 清除所有电池的DCC位,并关闭DTM
void LTC681x_clear_discharge(u8 total_ic, cell_asic *ic);
/*LTC6813部分, 此处不敢保证对其他系列芯片适用*/
//辅助函数, 初始化cfgrb寄存器组的值
void LTC6813_init_cfgb(u8 total_ic,cell_asic *ic);
//辅助函数, 设置CFGA的值
void LTC6813_set_cfgr(u8 nIC, cell_asic *ic, bool refon);
/* Helper Function to set the configuration register B */
void LTC6813_set_cfgrb(u8 nIC, cell_asic *ic,bool dtmen,bool gpiobits[4],bool dccbits[7]);
/*! Helper function to turn the fdrf bit HIGH or LOW*/
void LTC6813_set_cfgrb_fdrf(u8 nIC, cell_asic *ic, bool fdrf);
/*! Helper function to turn the DTMEN bit HIGH or LOW*/
void LTC6813_set_cfgrb_dtmen(u8 nIC, cell_asic *ic, bool dtmen);
/*! Helper function to turn the Path Select bit HIGH or LOW*/
void LTC6813_set_cfgrb_ps(u8 nIC, cell_asic *ic, bool ps[]);
/*! Helper function to turn the GPIO bit HIGH or LOW*/
void LTC6813_set_cfgrb_gpio_b(u8 nIC, cell_asic *ic);
/*! Helper function to turn the dccbit bit HIGH or LOW*/
void LTC6813_set_cfgrb_dcc_b(u8 nIC, cell_asic *ic, bool dccbits[]);
//辅助函数,设置DCC位
void LTC6813_set_discharge(u8 i, u8 Cell, cell_asic *ic);
/*! Mutes the LTC6813 discharge transistors */
void LTC6813_mute(void);
/*! Clears the LTC6813 Mute Discharge */
void LTC6813_unmute(void);

void LTC6813_wrcfg(u8 total_ic, cell_asic ic[]);

void LTC6813_wrcfgb(u8 total_ic, cell_asic ic[]);

int8_t LTC6813_rdcfg(u8 total_ic, cell_asic ic[]);

int8_t LTC681x_rdcfgb(u8 total_ic, cell_asic ic[]);




/*	LTC681x_rdstat LTC681x_wrpwm LTC681x_rdpwm LTC681x_run_cell_adc_st
 *  LTC681x_wrcomm LTC681x_rdcomm LTC681x_stcomm LTC681x_st_lookup
 *  LTC681x_reset_crc_count
 *  这些函数均未写入, 因为目前版本不需要使用, 以后可以研究一下*/

#endif /* LTC681X_LTC681X_H_ */
