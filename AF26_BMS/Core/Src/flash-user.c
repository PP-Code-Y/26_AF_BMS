/*
 * Flash.h
 *
 *  Created on: 2022年7月17日
 *      Author: Ahorn
 */

#include "flash-user.h"
#include "data-user.h"
#include "spi.h"
// #include "stdint.h"
// #include "rtc-user.h"

/*
block：块；sector：扇区；page：页
FLASH芯片只能按扇区、块为单位擦除，或者是全片擦除。写可以1~256字节写，一次最多写256字节
P25Q32H有64个块，每个块有16个扇区，每个扇区有16页；每个页有256Bytes；寻址空间：0x000000~0x3FFFFF，共4MB
1block=65536bytes;1sector=4096bytes;1page=256bytes
芯片SPI时序以QPI的图为准(不看command)
*/

/*
hal.spi参数
*hspi: 选择SPI通道，格式为&hspix
*pData：需要发送或接收的数据存储地址
Size：发送数据的字节数，1 就是发送一个字节数据
Timeout：超时时间，就是执行发送函数最长的时间，超过该时间自动退出函数
*/

extern BMS A03;
// extern TIME time;

// 复位
void Flash_Reset(void)
{
	uint8_t cmd[2] = {ResetEnable, ResetMemory};		// 两个字节
	Flash_CS0;											// 发送数据前的电平拉低
	HAL_SPI_Transmit(&Flash_SPI, cmd, 2, TimeoutValue); // SPI通道；发送的数据；字节数；超时时间
	Flash_CS1;											// 发送数据后的电平拉高
}

// 用于芯片识别验证
void Flash_ReadID(void)
{ // page67 发送四个字节：ID寄存器，两个虚拟位0x00，一个地址位00或01
	uint8_t cmd[4] = {ManufactDeviceID, 0x00, 0x00, 0x00};
	uint8_t ID[2];
	Flash_CS0;
	HAL_SPI_Transmit(&Flash_SPI, cmd, 4, TimeoutValue);
	HAL_SPI_Receive(&Flash_SPI, ID, 2, TimeoutValue); // 接收flash返回的ID
	Flash_CS1;
}

// 状态检测
uint8_t Flash_GetStatus(void)
{
	uint8_t cmd[1] = {ReadStatusReg};
	uint8_t status;
	Flash_CS0;
	HAL_SPI_Transmit(&Flash_SPI, cmd, 1, TimeoutValue);
	HAL_SPI_Receive(&Flash_SPI, &status, 1, TimeoutValue);
	Flash_CS1;
	return status; // 返回0x00不繁忙，0x01繁忙
}
// 等待芯片空闲
void Flash_Wait_Busy(void)
{
	while ((Flash_GetStatus() & 0x01) == 0x01)
		; // 直到flash不繁忙跳出循环
}

// 写使能函数
void Flash_WREN(void)
{
	uint8_t cmd[1] = {WriteEnable};
	Flash_CS0;
	HAL_SPI_Transmit(&Flash_SPI, cmd, 1, TimeoutValue);
	Flash_CS1;
}
// 写禁能函数
void Flash_WRDI(void)
{
	uint8_t cmd[1] = {WriteDisable};
	Flash_CS0;
	HAL_SPI_Transmit(&Flash_SPI, cmd, 1, TimeoutValue);
	Flash_CS1;
}

/**********************************读写函数***************************************/

// 扇区擦除，擦除第n个扇区，全部置1
// 扇区容量4KB，即0001 0000 0000 0000
void Flash_SE(uint16_t sector)
{
	Flash_WREN(); // 使能
	uint8_t cmd[4];
	cmd[0] = SectorErase;		   // 扇区擦除
	cmd[1] = (sector >> 4) & 0xFF; // 取中间八位，最高四位舍去，一般都为0000，所以是中间偏高四位保留		//0xFF为0000000011111111
	cmd[2] = (sector << 4) & 0xF0; // 取最低四位，左移补0
	cmd[3] = 0x00;
	Flash_CS0;
	HAL_SPI_Transmit(&Flash_SPI, cmd, 4, TimeoutValue);
	Flash_CS1;
	Flash_Wait_Busy(); // 等待擦除，即等待芯片运行完后返回不繁忙
}

// PS:1block=65536bytes;1sector=4096bytes;1page=256bytes
// 在指定页地址开始读写指定长度的数据，注意start_addr是页地址
// 参数：数组名、开始读写的页地址、读写字节数
// 由于寄存器操作只能在一页内读写，因此如果超过了一页，则需要加页
// 2022.7.28 被注释掉的版本在主控板上不可用，问题未知
void Flash_Read(uint8_t *data, uint32_t start_addr, uint16_t size)
{
	//	uint8_t cmd[4];
	//	uint16_t page_count=0;
	//	uint32_t continue_addr=0;
	//	for(;page_count<size/256;page_count++)//计算数据一共需要多少页，读取整页的数据
	//	{
	//		cmd[0] = ReadData;//读数寄存器
	//		cmd[1] = 0x00;
	//		cmd[2] = (uint8_t)(start_addr + page_count);
	//		cmd[3] = 0x00;
	//		Flash_CS0;
	//		HAL_SPI_Transmit(&Flash_SPI, cmd, 4, TimeoutValue);
	//		HAL_SPI_Receive(&Flash_SPI, data, size, TimeoutValue);//接收data发来的数据
	//		Flash_CS1;
	//	}
	//	HAL_Delay(10);
	//	if(size%256)//读取最后不足整页的数据
	//	{
	//		continue_addr =page_count * PageSize +  start_addr;
	//		cmd[0] = ReadData;//读数寄存器
	//		cmd[1] = 0x00;
	//		cmd[2] = (uint8_t)(continue_addr);
	//		cmd[3] = 0x00;
	//		Flash_CS0;
	//		HAL_SPI_Transmit(&Flash_SPI, cmd, 4, TimeoutValue);
	//		HAL_SPI_Receive(&Flash_SPI, data, size, TimeoutValue);
	//		Flash_CS1;
	//	}
	uint8_t cmd[4];
	cmd[0] = ReadData;					  // 读数寄存器
	cmd[1] = (uint8_t)(start_addr >> 16); // 从右数第三组8位
	cmd[2] = (uint8_t)(start_addr >> 8);  // 第二组
	cmd[3] = (uint8_t)(start_addr);		  // 第一组
	Flash_CS0;
	HAL_SPI_Transmit(&Flash_SPI, cmd, 4, TimeoutValue);
	HAL_SPI_Receive(&Flash_SPI, data, size, TimeoutValue); // 接收data发来的数据
	Flash_CS1;
}

void Flash_Write(uint8_t *data, uint32_t start_addr, uint16_t size)
{
	//	uint8_t cmd[4];
	//	uint16_t page_count=0;
	//	uint32_t continue_addr=0;
	//	for(;page_count<size/256;page_count++)//计算数据一共需要多少页，写入整页的数据
	//	{
	//		Flash_WREN();//写使能
	//		cmd[0] = PageProgram;
	//		cmd[1] = 0x00;
	//		cmd[2] = (uint8_t)(start_addr + page_count);
	//		cmd[3] = 0x00;
	//		Flash_CS0;
	//		HAL_SPI_Transmit(&Flash_SPI, cmd, 4, TimeoutValue);
	//		HAL_SPI_Transmit(&Flash_SPI, data, size, TimeoutValue);
	//		Flash_CS1;
	//	}
	//	Flash_Wait_Busy();
	//
	//	if(size%256)//写入最后不足整页的数据
	//	{
	//		Flash_WREN();//写使能
	//		continue_addr =page_count * PageSize +  start_addr;
	//		cmd[0] = PageProgram;
	//		cmd[1] = 0x00;
	//		cmd[2] = (uint8_t)(continue_addr);
	//		cmd[3] = 0x00;
	//		Flash_CS0;
	//		HAL_SPI_Transmit(&Flash_SPI, cmd, 4, TimeoutValue);
	//		HAL_SPI_Transmit(&Flash_SPI, data, size, TimeoutValue);
	//		Flash_CS1;
	//	}
	//	Flash_Wait_Busy();
	uint8_t cmd[4];
	uint16_t page_count = 0;
	uint32_t continue_addr = 0;
	for (; page_count < size / 256; page_count++) // 计算数据一共需要多少页，写入整页的数据，一页至多存储256个字节
	{
		Flash_WREN(); // 写使能
		cmd[0] = PageProgram;
		cmd[1] = (uint8_t)(start_addr >> 16);
		cmd[2] = (uint8_t)(start_addr >> 8);
		cmd[3] = (uint8_t)(start_addr);
		Flash_CS0;
		HAL_SPI_Transmit(&Flash_SPI, cmd, 4, TimeoutValue);
		HAL_SPI_Transmit(&Flash_SPI, data, size, TimeoutValue);
		Flash_CS1;
	}
	Flash_Wait_Busy();

	if (size % 256) // 写入最后不足整页的数据
	{
		Flash_WREN(); // 写使能
		continue_addr = page_count * PageSize + start_addr;
		cmd[0] = PageProgram;
		cmd[1] = (uint8_t)(continue_addr >> 16);
		cmd[2] = (uint8_t)(continue_addr >> 8);
		cmd[3] = (uint8_t)(continue_addr);
		Flash_CS0;
		HAL_SPI_Transmit(&Flash_SPI, cmd, 4, TimeoutValue);
		HAL_SPI_Transmit(&Flash_SPI, data, size, TimeoutValue); // 若写入数据超出本页大小，那么超出的第一个字节将会从本页的首位地址开始写
		Flash_CS1;
	}
	Flash_Wait_Busy();
}

/************************以下是FLASH的BMS函数**********************/
uint8_t Flash_Read_Init_Flag(void)
{
	Flash_Read((u8 *)&A03.State.Init_flag, Flash_Addr_Init_Flag, sizeof(A03.State.Init_flag) / sizeof(vu8)); // 读取Flash的Init_Flag，1个字节
	return A03.State.Init_flag;																				 // 将最新的值储存并返回
}

void Flash_Write_Init_Flag(void)
{
	Flash_SE(Flash_Sec_Init_Flag); // 向Flash存进最新的flag
	Flash_Write((u8 *)&A03.State.Init_flag, Flash_Addr_Init_Flag, sizeof(A03.State.Init_flag) / sizeof(vu8));
}

// 这里的读阈值跟Init中的读不同的是，Init中是Flag是直接读取Flash里面的值，而这里的Flag由上位机的指令决定，并将其存入Flash
void Flash_Read_Errormute(void)
{
	Flash_Read((uint8_t *)&A03.Alarm.error_mute, Flash_Addr_Errormute, sizeof(A03.Alarm.error_mute) / sizeof(vu8));
}

void Flash_Write_Errormute(void)
{
	Flash_SE(Flash_Sec_Errormute);
	Flash_Write((uint8_t *)&A03.Alarm.error_mute, Flash_Addr_Errormute, sizeof(A03.Alarm.error_mute) / sizeof(vu8));
}

// 读取SOC不需要InitFlag判断
void Flash_Read_SOC(void)
{
	vu16 temp = 0;
	Flash_Read((uint8_t *)&temp, Flash_Addr_SOC, sizeof(A03.State.SOC) / sizeof(vu8));
	A03.State.SOC = temp;
}

void Flash_Write_SOC(void)
{
	vu16 temp = 0;
	temp = A03.State.SOC;
	Flash_SE(Flash_Sec_SOC);
	Flash_Write((uint8_t *)&temp, Flash_Addr_SOC, sizeof(A03.State.SOC) / sizeof(vu8));
}

// void Flash_Read_Time(void){
// 	Flash_Read((uint8_t*)&time, Flash_Addr_Time, sizeof(time));
// }

// void Flash_Write_Time(void){
// 	Flash_SE(Flash_Sec_Time);
// 	Flash_Write((uint8_t*)&time, Flash_Addr_Time, sizeof(time));
// }

void Flash_Write_Threshold(void)
{
	Flash_SE(Flash_Sec_Threshold);
	Flash_Write((uint8_t *)&A03.Threshold, Flash_Addr_Threshold, sizeof(A03.Threshold));
}

/************************************************************************************/
void Threshold_Update(void)
{
	// if(Flash_Read_Init_Flag()){	//如果阈值有修改过(flag==1), 则直接存入FLASH里面修改了的阈值
	// 	Flash_Read((u8*)&A03.Threshold, Flash_Addr_Threshold, sizeof(A03.Threshold)/sizeof(vu8));
	// }
	if (Flash_Read_Init_Flag())
	{ // 如果阈值有修改过(flag==1), 则直接存入FLASH里面修改了的阈值
		Flash_Read((u8 *)&A03.Threshold, Flash_Addr_Threshold, sizeof(A03.Threshold) / sizeof(vu8));
	}
	else
	{ // 否则恢复默认出厂设置
		A03.Threshold.OTV_LV1 = 5200;
		A03.Threshold.OTV_LV2 = 5000;
		A03.Threshold.OTV_LV3 = 4700;
		A03.Threshold.OTV_LV4 = 4500;

		A03.Threshold.UTV_LV1 = 3000;
		A03.Threshold.UTV_LV2 = 3500;
		A03.Threshold.UTV_LV3 = 3700;
		A03.Threshold.UTV_LV4 = 3800;

		A03.Threshold.StaticDischarge_OC_LV1 = 2300; // 0.1A/bit
		A03.Threshold.StaticDischarge_OC_LV2 = 2080;
		A03.Threshold.StaticDischarge_OC_LV3 = 1800;
		A03.Threshold.StaticDischarge_OC_LV4 = 1500;

		A03.Threshold.StaticCharge_OC_LV1 = 100; // 0.1A/bit, 10A
		A03.Threshold.StaticCharge_OC_LV2 = 80;	 // 8A
		A03.Threshold.StaticCharge_OC_LV3 = 63;
		A03.Threshold.StaticCharge_OC_LV4 = 62;

		A03.Threshold.SOC_High_LV1 = 1000; // 千分比, 900为90%
		A03.Threshold.SOC_High_LV2 = 900;
		A03.Threshold.SOC_High_LV3 = 850;
		A03.Threshold.SOC_High_LV4 = 800;

		A03.Threshold.SOC_Low_LV1 = 50;
		A03.Threshold.SOC_Low_LV2 = 100;
		A03.Threshold.SOC_Low_LV3 = 150;
		A03.Threshold.SOC_Low_LV4 = 200;

		A03.Threshold.Cell_OV_LV1 = 5200;
		A03.Threshold.Cell_OV_LV2 = 5000;
		A03.Threshold.Cell_OV_LV3 = 4700;
		A03.Threshold.Cell_OV_LV4 = 4500;

		A03.Threshold.Cell_UV_LV1 = 3000;
		A03.Threshold.Cell_UV_LV2 = 3100;
		A03.Threshold.Cell_UV_LV3 = 3300;
		A03.Threshold.Cell_UV_LV4 = 3400;

		A03.Threshold.Cell_ODV_LV1 = 1000;
		A03.Threshold.Cell_ODV_LV2 = 500;
		A03.Threshold.Cell_ODV_LV3 = 300;
		A03.Threshold.Cell_ODV_LV4 = 150;

		A03.Threshold.Cell_OT_LV1 = 60;
		A03.Threshold.Cell_OT_LV2 = 50;
		A03.Threshold.Cell_OT_LV3 = 45;
		A03.Threshold.Cell_OT_LV4 = 40;

		A03.Threshold.Cell_UT_LV1 = 0;
		A03.Threshold.Cell_UT_LV2 = 0;
		A03.Threshold.Cell_UT_LV3 = 0;
		A03.Threshold.Cell_UT_LV4 = 0;

		A03.Threshold.Cell_ODT_LV1 = 10;
		A03.Threshold.Cell_ODT_LV2 = 8;
		A03.Threshold.Cell_ODT_LV3 = 6;
		A03.Threshold.Cell_ODT_LV4 = 5;

		A03.Threshold.Cell_Temp_Openwires_LV1 = 100;
		A03.Threshold.Cell_Temp_Openwires_LV2 = 80;
		A03.Threshold.Cell_Temp_Openwires_LV3 = 50;
		A03.Threshold.Cell_Temp_Openwires_LV4 = 10;

		A03.Threshold.Daisy_Openwires_LV1 = 100;
		A03.Threshold.Daisy_Openwires_LV2 = 80;
		A03.Threshold.Daisy_Openwires_LV3 = 50;
		A03.Threshold.Daisy_Openwires_LV4 = 10;

		A03.Threshold.Cell_Vol_Openwires_LV1 = 100; // 离线10s
		A03.Threshold.Cell_Vol_Openwires_LV2 = 80;
		A03.Threshold.Cell_Vol_Openwires_LV3 = 50;
		A03.Threshold.Cell_Vol_Openwires_LV4 = 10;

		A03.Threshold.Cell_Temp_Openwires_LV1 = 100;
		A03.Threshold.Cell_Temp_Openwires_LV2 = 80;
		A03.Threshold.Cell_Temp_Openwires_LV3 = 50;
		A03.Threshold.Cell_Temp_Openwires_LV4 = 10;

		A03.Threshold.Hall_Openwires_LV1 = 6; // 离线10s
		A03.Threshold.Hall_Openwires_LV2 = 5;
		A03.Threshold.Hall_Openwires_LV3 = 4;
		A03.Threshold.Hall_Openwires_LV4 = 3;

		A03.Threshold.Vol_Error_LV1 = 10; // 10%
		A03.Threshold.Vol_Error_LV2 = 8;
		A03.Threshold.Vol_Error_LV3 = 6;
		A03.Threshold.Vol_Error_LV4 = 3;

		A03.Threshold.Precharge_Time = 6;			  // 6秒
		A03.Threshold.Precharge_End_Vol_Percent = 95; // 预充电压达到上高压阈值

		A03.Threshold.Balance_Start_DV = 50;
		A03.Threshold.Balance_End_DV = 30;

		A03.Threshold.Fans_Start_OT = 32;
		A03.Threshold.Fans_End_OT = 30;

		A03.Threshold.Fans_Start_ODT = 8;
		A03.Threshold.Fans_End_ODT = 3;
	}
}
