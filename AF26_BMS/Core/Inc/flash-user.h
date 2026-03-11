/*
 * Flash.h
 *
 *  Created on: 2022年7月17日
 *      Author: Ahorn
 */
 //2022.7.26:修改flashread的bug，解决读写函数与扇区擦除函数地址不对应的问题
 
#ifndef __FLASH_H
#define __FLASH_H

#include "main.h"

//操作定义
#define Flash_CS0 HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_RESET)
#define Flash_CS1 HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_SET)
#define Flash_SPI hspi3

#define Flash_Sec_SOC 0		//原2, SOC处于这里没问题
#define Flash_Addr_SOC 0   	//原32	

#define Flash_Sec_Errormute 1			
#define Flash_Addr_Errormute 0x1000      	

#define Flash_Sec_Time 2
#define Flash_Addr_Time 0x2000    		

#define Flash_Sec_Init_Flag 3     		//原1
#define Flash_Addr_Init_Flag 0x3000         //原16

#define Flash_Sec_Threshold 4			//原4
#define Flash_Addr_Threshold 0x4000     	//原64

//指令表
//寄存器位
#define WriteEnable				0x06 
#define WriteDisable			0x04 
#define ReadStatusReg			0x05 
#define WriteStatusReg			0x01 
#define ReadData				0x03 
#define FastReadData			0x0B 
#define FastReadDual			0x3B 
#define PageProgram				0x02 
#define BlockErase				0xD8 
#define SectorErase				0x20 
#define ChipErase				0xC7 
#define PowerDown				0xB9 
#define ReleasePowerDown		0xAB 
#define ElectronicID			0xAB 
#define ManufactDeviceID		0x90 
#define DeviceID				0x9F 
#define ResetEnable 			0x66
#define ResetMemory  			0x99

//常数定义
#define BlockSize         0x10000
#define SectorSize        0x1000
#define PageSize          0x100    //256Bytes
#define Flash_Busy        0x01
#define TimeoutValue      1000


//芯片初始化
void Flash_Reset(void);
void Flash_ReadID(void);

//状态检测
void Flash_Wait_Busy(void);
uint8_t Flash_GetStatus(void);

//使能与禁能
void Flash_WREN(void);
void Flash_WRDI(void);

//扇区擦除函数,擦除第n个扇区
void Flash_SE(uint16_t sector);

//读写
//参数：数组名、起始地址、数据大小
void Flash_Read(uint8_t* data, uint32_t start_addr, uint16_t size);
void Flash_Write(uint8_t* data, uint32_t start_addr, uint16_t size);

//BMS操作
void Flash_Init(void);
uint8_t Flash_Read_Init_Flag(void);
void Flash_Read_Errormute(void);
void Flash_Read_SOC(void);
void Flash_Read_Time(void);

void Flash_Write_Init_Flag(void);
void Flash_Write_Threshold(void);
void Flash_Write_SOC(void);
void Flash_Write_Errormute(void);
void Flash_Write_Time(void);

void Threshold_Update(void);

#endif

