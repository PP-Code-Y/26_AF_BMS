/*
 *  Created on: 2022年3月4日
 *      Author: BBBBB
 */
#include "ltc681x-user.h"
// #include "main.h"
#include "spi.h"
#include "stdlib.h"

// PEC计算的相关参数
int16_t pec15Table[256];
int16_t CRC15_POLY = 0x4599;
int16_t re, address; // re本来是remainder的，但是VS里面有个remainder函数所以改成re了
// 其他变量

/*spi相关*/
/*片选*/
void cs_low(void)
{
    HAL_GPIO_WritePin(SPI1_NSS_GPIO_Port, SPI1_NSS_Pin, GPIO_PIN_RESET);
}
/*取消片选*/
void cs_high(void)
{
    HAL_GPIO_WritePin(SPI1_NSS_GPIO_Port, SPI1_NSS_Pin, GPIO_PIN_SET);
}
/* brief: 有读有写, 适合于681X芯片的读写, for循环好像优化不掉
 *  	  DMA可能传输会快点
 * param: u8 tx_Data[],	//发送数据
          u8 tx_len,	//发送数据长度
          u8 *rx_data,	//接收数据长度
          u8 rx_len)	//接收数据长度*/
void spi_write_read(u8 *tx_data, u8 tx_len, u8 *rx_data, u8 rx_len)
{
    //	u16 time_out=0;
    //	for(u8 i=0; i<tx_len; i++){
    //		while((HAL_SPI_Transmit_DMA(&hspi1, &tx_data[i], 1)!=HAL_OK) && (time_out<2000)){	//while循环读取处理结果, 保证发送完成, 否则会有丢包
    //			time_out++;	 //超时判断
    //		}
    //	}
    HAL_SPI_Transmit(&hspi1, tx_data, tx_len, 1000);
    //	for(u8 i=0; i<rx_len; i++){
    //		while((HAL_SPI_Receive_DMA(&hspi1, &rx_data[i], 1)!=HAL_OK) && (time_out<2000)){
    //			time_out++;	 //超时判断
    //		}
    //	}
    HAL_SPI_Receive(&hspi1, rx_data, rx_len, 1000);
}
/* brief: 从SPI端口中写入字节数组，并发数组
 * param: 	len: 数组长度
 * 			data：要发的数据数组*/
void spi_write_array(u8 len, u8 data[])
{
    //	u16 time_out=0;
    //	for(u8 i=0; i<len; i++){
    //		while((HAL_SPI_Transmit_DMA(&hspi1, &data[i], 1)!=HAL_OK) && (time_out<2000)){
    //			time_out++;	//超时判断
    //		}
    //	}
    HAL_SPI_Transmit(&hspi1, data, len, 1000);
}
/*brief:最常用的唤醒函数: Wake isoSPI up from idle state
 * 		在调试时需要每条指令之前都要加这句话, 不确定是不是调试的问题还是芯片的问题
 * 		能优化wakeup还是要优化的, 因为太多wake不是太好
 * */
void wakeup_idle(u8 total_ic)
{
    u8 temp = 0xff;
    //	u16 time_out=0;
    for (u8 i = 0; i < total_ic; i++)
    {
        cs_low(); // 片选
        //		while((HAL_SPI_Transmit_DMA(&hspi1, &temp, 1)!=HAL_OK) && (time_out<2000)){	//确保唤醒
        //			time_out++;	//超时判断
        //		}
        HAL_SPI_Transmit(&hspi1, &temp, 1, 1000);
        cs_high(); // 取消片选
    }
}

void wakeup_sleep(u8 total_ic)
{
    for (int i = 0; i < total_ic; i++)
    {
        cs_low();
        HAL_Delay(1); // 至少240ns才是唤醒信号in P53，我估计片选后拉低引脚就是唤醒信号
        cs_high();
    }
}

/* brief: 	PEC计算的init,要在main刚开始的时候init, 这个不用搞懂具体原理, 从数据手册上面抄下来的
 */
void init_PEC15_Table()
{
    int i, bit;
    for (i = 0; i < 256; i++)
    {
        re = i << 7;
        for (bit = 8; bit > 0; --bit)
        {
            if (re & 0x4000)
            {
                re = (re << 1);
                re = (re ^ CRC15_POLY);
            }
            else
            {
                re = (re << 1);
            }
        }
        pec15Table[i] = re & 0xffff;
    }
}
/* brief: 	计算cmd的PEC校验码
 * param: 	data：一般为CMD[]，即包含命令的数组
 * 			len：数组长度*/
u16 pec15_calc(u8 *data, u8 len) // 由数据手册直接得来的PEC校验码计算算法
{
    int i;
    re = 16;
    for (i = 0; i < len; i++)
    {
        address = ((re >> 7) ^ data[i]) & 0xff;
        re = (re << 8) ^ pec15Table[address];
    }
    return (re * 2);
}

/* brief: 	用于wr和rd的函数之后, 一般用于记录PEC的错误位*/
void LTC681x_check_pec(u8 total_ic, u8 reg, cell_asic *ic)
{
    int current_ic, i;
    switch (reg)
    {
    case cfgr:
        for (current_ic = 0; current_ic < total_ic; current_ic++)
        {
            ic[current_ic].crc_count.pec_count = ic[current_ic].crc_count.pec_count + ic[current_ic].config.rx_pec_match;
            ic[current_ic].crc_count.cfgr_pec = ic[current_ic].crc_count.cfgr_pec + ic[current_ic].config.rx_pec_match;
        }
        break;

    case CFGRB:
        for (current_ic = 0; current_ic < total_ic; current_ic++)
        {
            ic[current_ic].crc_count.pec_count = ic[current_ic].crc_count.pec_count + ic[current_ic].configb.rx_pec_match;
            ic[current_ic].crc_count.cfgr_pec = ic[current_ic].crc_count.cfgr_pec + ic[current_ic].configb.rx_pec_match;
        }
        break;

    case CELL:
        for (current_ic = 0; current_ic < total_ic; current_ic++)
        {
            for (i = 0; i < ic[0].ic_reg.num_cv_reg; i++)
            {
                ic[current_ic].crc_count.pec_count = ic[current_ic].crc_count.pec_count + ic[current_ic].cells.pec_match[i];
                ic[current_ic].crc_count.cell_pec[i] = ic[current_ic].crc_count.cell_pec[i] + ic[current_ic].cells.pec_match[i];
            }
        }
        break;
    case AUX:
        for (current_ic = 0; current_ic < total_ic; current_ic++)
        {
            for (i = 0; i < ic[0].ic_reg.num_gpio_reg; i++)
            {
                ic[current_ic].crc_count.pec_count = ic[current_ic].crc_count.pec_count + (ic[current_ic].aux.pec_match[i]);
                ic[current_ic].crc_count.aux_pec[i] = ic[current_ic].crc_count.aux_pec[i] + (ic[current_ic].aux.pec_match[i]);
            }
        }
        break;

    case STAT:
        for (current_ic = 0; current_ic < total_ic; current_ic++)
        {
            for (i = 0; i < ic[0].ic_reg.num_stat_reg - 1; i++)
            {
                ic[current_ic].crc_count.pec_count = ic[current_ic].crc_count.pec_count + ic[current_ic].stat.pec_match[i];
                ic[current_ic].crc_count.stat_pec[i] = ic[current_ic].crc_count.stat_pec[i] + ic[current_ic].stat.pec_match[i];
            }
        }
        break;

    default:
        break;
    }
}

/* brief: 	输入一个二维数组，写入输出数组的1-2位，
 * 			并为命令计算PEC并附在数组的3-4位，并发送出去到BMS的IC
 * 			将两个两个命令与其PEC码打包在一起，并发送出去
 * param: 	tx_cmd[2] 包含要发送的BMS命令的2字节数组 */
void cmd_68(u8 tx_cmd[2]) // 写2字节命令用
{
    u8 cmd[4];
    u16 cmd_pec;

    cmd[0] = tx_cmd[0];
    cmd[1] = tx_cmd[1];           // 添加数据
    cmd_pec = pec15_calc(cmd, 2); // 对要发送的2字节数组计算PEC
    cmd[2] = (u8)(cmd_pec >> 8);  // 添加PEC
    cmd[3] = (u8)(cmd_pec);
    cs_low();
    spi_write_array(4, cmd); // 发送，调用HAL库的SPI发送 函数通过SPI发送给6820
    cs_high();
}
/* brief: 	写函数, 写入的第一个配置由菊花链中的最后一个IC接收
 * 			将数据数组写入菊花链 ,如果要看菊花链的命令指令传输协议就看P56
 * param: 	total_ic 采样芯片总数
 * 			txcmd 要发送的指令
 * 			data 要发送的数据 */
void write_68(u8 total_ic, u8 tx_cmd[2], u8 data[]) // 写配置寄存器用
{
    vu8 BYTES_IN_REG = 6;                  // 一个寄存器组(6byte)
    const u8 CMD_LEN = 4 + (8 * total_ic); // 4表示命令长度，两位cmd+两位pec。8*totalic表示一个寄存器组（6byte）+PEC（2byte）
    u8 *cmd;
    u16 data_pec, cmd_pec;
    u8 cmd_index;

    cmd = (u8 *)malloc(CMD_LEN * sizeof(u8)); // 动态申请一个cmd长的字节的地址长度
    cmd[0] = tx_cmd[0];
    cmd[1] = tx_cmd[1];
    cmd_pec = pec15_calc(cmd, 2);
    cmd[2] = (u8)(cmd_pec >> 8);
    cmd[3] = (u8)(cmd_pec);
    cmd_index = 4; // 记录当前cmd走到哪个字节用
    /* 对每个LTC681x执行一次，此循环从堆栈上的最后一个IC开始, executes for each LTC681x, this loops starts with the last IC on the stack
     * 写入的第一个配置由菊花链中的最后一个IC接收, The first configuration written is received by the last IC in the daisy chain*/
    for (u8 current_ic = total_ic; current_ic > 0; current_ic--)
    {
        for (u8 current_byte = 0; current_byte < BYTES_IN_REG; current_byte++)
        {
            cmd[cmd_index] = data[((current_ic - 1) * 6) + current_byte];
            /* 举个例子,比如说我要写第二个ic的命令,因此current_ic=2,
             * 所以得到6是为了把写第一个寄存器组的数据跳过,
             * current byte就是为了定位我要写哪个寄存器*/
            cmd_index = cmd_index + 1; // 每写一个byte都应该自增一个1
        }
        data_pec = (u16)pec15_calc(&data[(current_ic - 1) * 6], BYTES_IN_REG); // calculating the PEC for each Iss configuration register data
        cmd[cmd_index] = (u8)(data_pec >> 8);
        cmd[cmd_index + 1] = (u8)data_pec;
        cmd_index = cmd_index + 2; // 算上pec(2byte), 所以index增加2
    }
    cs_low();
    spi_write_array(CMD_LEN, cmd); // 发送数据
    cs_high();
    free(cmd); // 释放申请的数据控件
}
/* brief: 	读函数:读取菊花链上IC的数据然后读回来 6*total_ic 大小的数据储存在rx_data
 * 			这个数组里面会返回一个错误标志, 而且是-1
 * param: 	total_ic 采样芯片总数
 * 			txcmd 要发送的指令
 * 			data 要发送的数据 */
int8_t read_68(u8 total_ic, u8 tx_cmd[2], u8 *rx_data)
{
    vu8 BYTES_IN_REG = 8; // 每个IC读回来的包括6byte的数据以及2byte的PEC, 所以是8
    u8 cmd[4];
    u8 data[256]; // 临时储存
    int8_t pec_error = 0;
    u16 cmd_pec, data_pec, received_pec;
    /*命令计算*/
    cmd[0] = tx_cmd[0];
    cmd[1] = tx_cmd[1];
    cmd_pec = pec15_calc(cmd, 2);
    cmd[2] = (u8)(cmd_pec >> 8);
    cmd[3] = (u8)(cmd_pec);
    cs_low();
    spi_write_read(cmd, 4, data, (BYTES_IN_REG * total_ic)); // 读写数据
    cs_high();
    /*数据储存到*rx_data中*/
    for (u8 current_ic = 0; current_ic < total_ic; current_ic++)
    {
        for (u8 current_byte = 0; current_byte < BYTES_IN_REG; current_byte++)
        {
            rx_data[(current_ic * 8) + current_byte] = data[current_byte + (current_ic * BYTES_IN_REG)];
        }
        received_pec = (rx_data[(current_ic * 8) + 6] << 8) + rx_data[(current_ic * 8) + 7]; // 读回来的数据中pec将它储存在received pec
        data_pec = pec15_calc(&rx_data[current_ic * 8], 6);
        if (received_pec != data_pec)
        {
            pec_error = -1;
        }
    }

    return (pec_error);
}

/*
 * !!!下面定义辅助函数的意思: 只是计算出指令/数据值储存在待发送数组,但是还没有真正发送到IC上
 * */

/* brief: 	辅助函数, 用于初始化CFG变量, 将ic[current_ic].config.tx_data[j]全部置为0,
 * 			所以该函数后面要跟一个write函数, 将值真正写入IC之中
 * param: 	total_ic 采样芯片总数
 * 			ic 官方定义的数据存储结构体 */
void LTC681x_init_cfg(u8 total_ic, cell_asic *ic)
{
    u8 current_ic;
    int j;
    for (current_ic = 0; current_ic < total_ic; current_ic++)
    {
        for (j = 1; j < 6; j++)
        {
            ic[current_ic].config.tx_data[j] = 0; // 先给所有状态清个零
        }
        ic[current_ic].config.tx_data[0] = 0xFE; // 配置GPIO1-5, refon均为1
    }
}
/* brief: 	辅助函数, 用于初始化CFGR各个值
 * param: 	nic 采样芯片总数
 * 			下面一大堆的变量都是寄存器组的东西, 详情请看数据手册 */
void LTC681x_set_cfgr(u8 nIC, cell_asic *ic, bool refon, bool adcopt,
                      bool gpio[5], bool dcc[12], bool dcto[4], u16 uv, u16 ov)
{
    LTC681x_set_cfgr_refon(nIC, ic, refon);
    LTC681x_set_cfgr_adcopt(nIC, ic, adcopt);
    LTC681x_set_cfgr_gpio(nIC, ic);
    LTC681x_set_cfgr_dis(nIC, ic, dcc);
    LTC681x_set_cfgr_dcto(nIC, ic, dcto);
    LTC681x_set_cfgr_uv(nIC, ic, uv);
    LTC681x_set_cfgr_ov(nIC, ic, ov);
}

void LTC681x_set_cfgr_refon(u8 nIC, cell_asic *ic, bool refon)
{
    if (refon)
        ic[nIC].config.tx_data[0] = ic[nIC].config.tx_data[0] | 0x04; // 按位与可以准确的把cfgar0的refon位设为1而不影响其他位，下面同理 文件来源：P63
    else
        ic[nIC].config.tx_data[0] = ic[nIC].config.tx_data[0] & 0xFB;
}

void LTC681x_set_cfgr_adcopt(u8 nIC, cell_asic *ic, bool adcopt)
{
    if (adcopt)
        ic[nIC].config.tx_data[0] = ic[nIC].config.tx_data[0] | 0x01;
    else
        ic[nIC].config.tx_data[0] = ic[nIC].config.tx_data[0] & 0xFE;
}

void LTC681x_set_cfgr_gpio(u8 nIC, cell_asic *ic)
{
    int i;
    for (i = 0; i < 5; i++)
    {
        //		if (gpio[i])
        ic[nIC].config.tx_data[0] = ic[nIC].config.tx_data[0] | (0x01 << (i + 3)); // 左移嘛，顾名思义就是方便转一下是设置那个寄存器啦
                                                                                   // 注释掉的原因应该是没必要弄成0
        //		else ic[nIC].config.tx_data[0] = ic[nIC].config.tx_data[0]&(~(0x01<<(i+3)));
    }
}

void LTC681x_set_cfgr_dis(u8 nIC, cell_asic *ic, bool dcc[12])
{
    int i;
    for (i = 0; i < 8; i++)
    {
        if (dcc[i])
            ic[nIC].config.tx_data[4] = ic[nIC].config.tx_data[4] | (0x01 << i);
        else
            ic[nIC].config.tx_data[4] = ic[nIC].config.tx_data[4] & (~(0x01 << i));
    }
    for (i = 0; i < 4; i++)
    {
        if (dcc[i + 8])
            ic[nIC].config.tx_data[5] = ic[nIC].config.tx_data[5] | (0x01 << i);
        else
            ic[nIC].config.tx_data[5] = ic[nIC].config.tx_data[5] & (~(0x01 << i));
    }
}

void LTC681x_set_cfgr_dcto(u8 nIC, cell_asic *ic, bool dcto[4])
{
    int i;
    for (i = 0; i < 4; i++)
    {
        if (dcto[i])
            ic[nIC].config.tx_data[5] = ic[nIC].config.tx_data[5] | (0x01 << (i + 4));
        else
            ic[nIC].config.tx_data[5] = ic[nIC].config.tx_data[5] & (~(0x01 << (i + 4)));
    }
}

void LTC681x_set_cfgr_uv(u8 nIC, cell_asic *ic, u16 uv)
{
    u16 tmp = (uv / 16) - 1;                  // 应该是P68的VUV值，至于100uv去了哪里有待商榷 PS：可能有些问题，网上说所有uv全部置为1。temp除以16会丧失字节长度，导致第二步就全部为0
    ic[nIC].config.tx_data[1] = 0x00FF & tmp; //
    ic[nIC].config.tx_data[2] = ic[nIC].config.tx_data[2] & 0xF0;
    ic[nIC].config.tx_data[2] = ic[nIC].config.tx_data[2] | ((0x0F00 & tmp) >> 8);
}

void LTC681x_set_cfgr_ov(u8 nIC, cell_asic *ic, u16 ov)
{
    u16 tmp = (ov / 16);
    ic[nIC].config.tx_data[3] = 0x00FF & (tmp >> 4);
    ic[nIC].config.tx_data[2] = ic[nIC].config.tx_data[2] & 0x0F;
    ic[nIC].config.tx_data[2] = ic[nIC].config.tx_data[2] | ((0x000F & tmp) << 4);
}

/* brief: 	采集电芯电压指令, 开启电压转换
 * param: 	MD ADC模式, 定义的是采样频率, 一般选7kHZ
 * 			DCP ADC期间是否允许放电,1为允许放电；0为静止放电
 * 			CH 电池通道*/
void LTC681x_adcv(u8 MD, u8 DCP, u8 CH) // 最终要发送0000 001MD[1] MD[0]11DCP 0CH[2]CH[1]CH[0]
{
    u8 cmd[2];
    u8 md_bits;                                // 模式选择位。参照P20有八种工作模式    //采样频率模式的选择取决于命令中两个MD位和CFGARO[0]的两种可能，以此形成八种模式
    md_bits = (MD & 0x02) >> 1;                // 设置第8位的值
    cmd[0] = md_bits + 0x02;                   // 第九位的值为1，跟第八位的值组装起来成为8-10位的值
    md_bits = (MD & 0x01) << 7;                // 设置第七位的值
    cmd[1] = md_bits + 0x60 + (DCP << 4) + CH; // 设置0-7的值
    cmd_68(cmd);
}

/* brief: 	ADAX模式，采集温度传感器电压, 在芯片上表现为测GPIO电压和第二参考电压
 * param: 	MD 模式, 定义的是采样频率
 * 			CHG 选择GPIO通道,P62*/
void LTC681x_adax(u8 MD, u8 CHG)
{
    u8 cmd[4];
    u8 md_bits;
    md_bits = (MD & 0x02) >> 1;
    cmd[0] = md_bits + 0x04;
    md_bits = (MD & 0x01) << 7;
    cmd[1] = md_bits + 0x60 + CHG;
    cmd_68(cmd);
}

/* brief: 	ADSTAT模式，测量内部设备参数P26
 * 			可以测量所有单元总和SC，内部芯片温度ITMP，模拟电源VA，数字电源VD
 * 			sc的16位adc值存储在status register group a
 * 			CO和V-的引脚电势差会导致SC测量误差等于该差
 * 			所有单元总和=sc*30*100uv
 * 			芯片温度测量ITMP存储在status register group a, 并附有计算公式in P27
 * 			VA VD 存在status register group b ，并附有计算公式以及正常范围
 * param: 	MD 模式, 定义的是采样频率
 * 			CHST 选择GPIO通道*/
void LTC681x_adstat(u8 MD, u8 CHST)
{
    u8 cmd[4];
    u8 md_bits;
    md_bits = (MD & 0x02) >> 1;
    cmd[0] = md_bits + 0x04;
    md_bits = (MD & 0x01) << 7;
    cmd[1] = md_bits + 0x68 + CHST;
    cmd_68(cmd);
}

/* brief: 	ADOW模式，开路检测命令，见P32, 检查LTC681的ADC与外部单元之间是否有断线
 * param: 	MD ADC Mode
 * 			PUP PULL UP OR DOWN current
 * 			CH Channels
 * 			DCP Discharge Permit*/
void LTC681x_adow(u8 MD, u8 PUP, u8 CH, u8 DCP)
{
    u8 cmd[2];
    u8 md_bits;
    md_bits = (MD & 0x02) >> 1;
    cmd[0] = md_bits + 0x02;
    md_bits = (MD & 0x01) << 7;
    cmd[1] = md_bits + 0x28 + (PUP << 6) + CH + (DCP << 4);
    cmd_68(cmd);
}

/* brief: 	ADCVSC模式，测量电池电压和所有电池的总和P28
 * param: 	MD ADC Mode
 * 			DCP Discharge Permit*/
void LTC681x_adcvsc(u8 MD, u8 DCP)
{
    u8 cmd[2];
    u8 md_bits;
    md_bits = (MD & 0x02) >> 1;
    cmd[0] = md_bits | 0x04;
    md_bits = (MD & 0x01) << 7;
    cmd[1] = md_bits | 0x60 | (DCP << 4) | 0x07;
    cmd_68(cmd);
}

/* brief: 	ADCVAX模式, 测18个电池电压值和两个GPIO1和2的值, 看P25
 * param: 	MD ADC Mode
 * 			DCP Discharge Permit*/
void LTC681x_adcvax(u8 MD, u8 DCP)
{
    u8 cmd[2];
    u8 md_bits;
    md_bits = (MD & 0x02) >> 1;
    cmd[0] = md_bits | 0x04;
    md_bits = (MD & 0x01) << 7;
    cmd[1] = (md_bits | ((DCP & 0x01) << 4)) + 0x6F; // 这个(DCP&0x01)<<4不知道有没有必要
    cmd_68(cmd);
}

/* brief: 	ADOL模式，首先同时使用ADC1,ADC2测量电池7，再同时使用ADC2和ADC3测量电池13
 * param: 	MD ADC Mode
 * 			DCP Discharge Permit*/
void LTC681x_adol(u8 MD, u8 DCP)
{
    u8 cmd[2];
    u8 md_bits;
    md_bits = (MD & 0x02) >> 1;
    cmd[0] = md_bits + 0x02;
    md_bits = (MD & 0x01) << 7;
    cmd[1] = md_bits + (DCP << 4) + 0x01;
    cmd_68(cmd);
}

/* brief: 	cvst,axst,statst模式为寄存器自检测函数,
 * 			结果将存到响应寄存器中,正常的话结果将如数据手册的Table14
 * 			CVST模式,结果将存到CVA,CVB......
 * param: 	MD Mode*/
void LTC681x_cvst(u8 MD, u8 ST)
{
    u8 cmd[2];
    u8 md_bits;
    md_bits = (MD & 0x02) >> 1;
    cmd[0] = md_bits + 0x02;
    md_bits = (MD & 0x01) << 7;
    cmd[1] = md_bits + ((ST) << 5) + 0x07;
    cmd_68(cmd);
}

/* brief: 	Start an Auxiliary Register Self Test Conversion
 * param: 	MD Mode*/
void LTC681x_axst(u8 MD, u8 ST)
{
    u8 cmd[2];
    u8 md_bits;
    md_bits = (MD & 0x02) >> 1;
    cmd[0] = md_bits + 0x04;
    md_bits = (MD & 0x01) << 7;
    cmd[1] = md_bits + ((ST & 0x03) << 5) + 0x07;
    cmd_68(cmd);
}

/* brief: 	Start a Status Register Self Test Conversion
 * param: 	MD Mode*/
void LTC681x_statst(u8 MD, u8 ST)
{
    u8 cmd[2];
    u8 md_bits;

    md_bits = (MD & 0x02) >> 1;
    cmd[0] = md_bits + 0x04;
    md_bits = (MD & 0x01) << 7;
    cmd[1] = md_bits + ((ST & 0x03) << 5) + 0x0F;
    cmd_68(cmd);
}

/* brief: 	该函数会在ADC转换未完成前停止函数往下走
 * 			会返回一个转换时间值
 * param*/
u32 LTC681x_pollAdc() // 轮询ADC转化状态PLADC
{
    u32 counter = 0;
    u8 finished = 0;
    u8 current_time = 0;
    u8 cmd[4];
    u16 cmd_pec;
    //	u16 time_out=0;
    /*计算轮询指令*/
    cmd[0] = 0x07;
    cmd[1] = 0x14;
    cmd_pec = pec15_calc(cmd, 2);
    cmd[2] = (u8)(cmd_pec >> 8);
    cmd[3] = (u8)(cmd_pec);
    /*发送轮询指令*/
    cs_low();
    spi_write_array(4, cmd);
    while ((counter < 200000) && (finished == 0))
    {
        // 接收SPI返回值, 因为不确定RECEIVE函数是否达到同样效果,所以就改成这个
        //		while((HAL_SPI_Receive_DMA(&hspi1, &current_time, 1)!=HAL_OK) && (time_out<2000)){
        //			time_out++;	 //超时判断
        //		}
        HAL_SPI_Receive(&hspi1, &current_time, 1, 1000);
        if (current_time > 0)
        {
            finished = 1;
        }
        else
        {
            counter = counter + 10;
        }
    }
    cs_high();
    return (counter);
}

/* brief: diagn命令，P31: diagn命令保证每一个多路信道（multiplexer channel）操作。
 * 		  遍历所有通道，如果通道解码失败，会把Status Register Group B 的 MUXFAIL 置 1。
 *		  如果通过，置0。
 * param*/
void LTC681x_diagn()
{
    u8 cmd[2] = {0x07, 0x15};
    cmd_68(cmd);
}

/* brief: 	解码"函数:将读回来的数据整合处理为有效的单体电池电压数据
 * 			每一个寄存器组(Register Group)有6个子寄存器，两个寄存器16位存储一个电池的电压。
 * 			第一个for循环把一个寄存器组的数据处理成3个电池的电压，存储在cell_codes[]中。
 * 			cell_data[]中的最后两位是PEC码，自己再计算一次pec，再与提取出来的pec对比。
 * param: 	current_ic	从0开始, 代表目前的IC序号
 * 			cell_reg	每个IC的6个CV寄存器组其中一个
 * 			cell_data[]	通过ADCV读回来的数据
 * 			u16 *cell_codes	 也就是&ic[c_ic].cells.c_codes[0], 其中c_codes[18],  记录为18个电池的电压数据
 * 			u8 *ic_pec	也就是&ic[c_ic].cells.pec_match[0], 其中pec_match[6],记录为6个寄存器组的PEC标志位，如果PEC匹配错误返回1
 */
int8_t parse_cells(u8 current_ic, u8 cell_reg, u8 cell_data[], u16 *cell_codes, u8 *ic_pec)
{
    vu8 BYT_IN_REG = 6;
    vu8 CELL_IN_REG = 3; // 一个寄存器存储3个电芯的电压值
    int8_t pec_error = 0;
    u16 parsed_cell;  // 中间变量
    u16 received_pec; // 中间变量
    u16 data_pec;
    u8 current_cell;
    u8 data_counter = current_ic * NUM_RX_BYT; // 记忆数组已经移了多少位,目前是0
    // 一个寄存器组对应三个电池，每次循环对应一个单体电池电压
    for (current_cell = 0; current_cell < CELL_IN_REG; current_cell++)
    {
        parsed_cell = cell_data[data_counter] + (cell_data[data_counter + 1] << 8); // 因为读回来的一个电压值是两个8位的数而且需要组合，现在将两个数组合成一个数才是电压值
        cell_codes[current_cell + ((cell_reg - 1) * CELL_IN_REG)] = parsed_cell;    // 括号里面的算法其实就是按照C1，C2……的顺序储存,而且C1中有三个电压数据。例如cell_reg=2，表示第二个寄存器组, 里面的C4V应该存在第3位的元素中
        data_counter = data_counter + 2;                                            // 每个电池电压都是2个8位数，所以每次循环要跳两位
    }
    received_pec = (cell_data[data_counter] << 8) | cell_data[data_counter + 1]; // 将第7,8位的PEC组合起来
    data_pec = pec15_calc(&cell_data[(current_ic)*NUM_RX_BYT], BYT_IN_REG);      // 将ADCV得到的数据来计算PEC，将与收到的PEC进行比较
    if (received_pec != data_pec)
    {
        pec_error = 1;
        ic_pec[cell_reg - 1] = 1; // 错误位设1
    }
    else
    {
        ic_pec[cell_reg - 1] = 0;
    }
    data_counter = data_counter + 2;

    return (pec_error);
}

/* brief: 	读取CVG的某一个组的值
 * param:   reg 寄存器
 * 			total_ic IC总数
 * 			u8* data 储存数据的数组*/
void LTC681x_rdcv_reg(u8 reg, u8 total_ic, u8 *data)
{
    vu8 REG_LEN = 8; // number of bytes in each ICs register + 2 bytes for the PEC = 8
    u8 cmd[4];
    u16 cmd_pec;
    // 以下是输入读不同寄存器组的命令
    if (reg == 1)
    { // 1: RDCVA
        cmd[1] = 0x04;
        cmd[0] = 0x00;
    }
    else if (reg == 2)
    { // 2: RDCVB
        cmd[1] = 0x06;
        cmd[0] = 0x00;
    }
    else if (reg == 3)
    { // 3: RDCVC
        cmd[1] = 0x08;
        cmd[0] = 0x00;
    }
    else if (reg == 4)
    { // 4: RDCVD
        cmd[1] = 0x0A;
        cmd[0] = 0x00;
    }
    else if (reg == 5)
    { // 5: RDCVE
        cmd[1] = 0x09;
        cmd[0] = 0x00;
    }
    else if (reg == 6)
    { // 4: RDCVF
        cmd[1] = 0x0B;
        cmd[0] = 0x00;
    }
    cmd_pec = pec15_calc(cmd, 2); // 计算PEC
    cmd[2] = (u8)(cmd_pec >> 8);
    cmd[3] = (u8)(cmd_pec);

    cs_low();
    spi_write_read(cmd, 4, data, (REG_LEN * total_ic)); // 有读有写，前两个是写，后两个是读
    cs_high();
}

/* brief: 	该函数读取单个GPIO电压寄存器，并将读取的数据作为字节数组存储在* data中。
 *			在LTC6811_rdaux（）命令之外很少使用此函数。
 *			这些寄存器似乎是存储欠压或者过压的标志位的(P24),B存放ADAX命令测量结果
 * param:   reg 寄存器
 * 			total_ic IC总数
 * 			u8* data 储存数据的数组*/
void LTC681x_rdaux_reg(u8 reg, u8 total_ic, u8 *data)
{
    const u8 REG_LEN = 8; // number of bytes in the register + 2 bytes for the PEC
    u8 cmd[4];
    u16 cmd_pec;

    if (reg == 1)
    { // AUX A
        cmd[1] = 0x0C;
        cmd[0] = 0x00;
    }
    else if (reg == 2)
    { // AUX B
        cmd[1] = 0x0E;
        cmd[0] = 0x00;
    }
    else if (reg == 3)
    { // AUX C
        cmd[1] = 0x0D;
        cmd[0] = 0x00;
    }
    else if (reg == 4)
    { // AUX D
        cmd[1] = 0x0F;
        cmd[0] = 0x00;
    }
    else
    { // AUX A, 输入错了就是输入AUX A
        cmd[1] = 0x0C;
        cmd[0] = 0x00;
    }
    cmd_pec = pec15_calc(cmd, 2);
    cmd[2] = (u8)(cmd_pec >> 8);
    cmd[3] = (u8)(cmd_pec);

    cs_low();
    spi_write_read(cmd, 4, data, (REG_LEN * total_ic));
    cs_high();
}

/*
这个函数读stat  register and存储在 *data 作为一个字节长度的数据.
除了在 LTC6811_rdstat() command，其余地方很少用到
*/
/* brief: 	这个函数读stat  register and存储在 *data 作为一个字节长度的数据.
 * 			除了在 LTC6811_rdstat() command，其余地方很少用到
 * param:   reg 寄存器
 * 			total_ic IC总数
 * 			u8* data 储存数据的数组*/
void LTC681x_rdstat_reg(u8 reg, u8 total_ic, u8 *data)
{
    vu8 REG_LEN = 8; // 6byte寄存器 + 2bytes PEC
    u8 cmd[4];
    u16 cmd_pec;

    if (reg == 1)
    { // Read back statiliary group A
        cmd[1] = 0x10;
        cmd[0] = 0x00;
    }
    else if (reg == 2)
    { // Read back statiliary group B
        cmd[1] = 0x12;
        cmd[0] = 0x00;
    }
    else
    { // Read back statiliary group A
        cmd[1] = 0x10;
        cmd[0] = 0x00;
    }
    cmd_pec = pec15_calc(cmd, 2);
    cmd[2] = (u8)(cmd_pec >> 8);
    cmd[3] = (u8)(cmd_pec);

    cs_low();
    spi_write_read(cmd, 4, data, (REG_LEN * total_ic));
    cs_high();
}

/* brief: 	该命令清除电池电压寄存器并将所有值初始化为1。
 * 			发送命令后，寄存器将读回十六进制0xFF*/
void LTC681x_clrcell()
{
    u8 cmd[2] = {0x07, 0x11}; // 对应61页命令代码：清除单元电压寄存器组：111 0001 0001
    cmd_68(cmd);
}

/* brief: 	该命令清除辅助寄存器（ the Auxiliary registers）并将所有值初始化为1。
 * 			发送命令后，该寄存器将回读十六进制的0xFF。*/
void LTC681x_clraux()
{
    u8 cmd[2] = {0x07, 0x12};
    cmd_68(cmd);
}

/* brief: 	该命令清除状态寄存器（the Stat registers）并将所有值初始化为1。
 * 			发送命令后，该寄存器将回读十六进制的0xFF。*/
void LTC681x_clrstat()
{
    u8 cmd[2] = {0x07, 0x13};
    cmd_68(cmd);
}

/* brief: 	Reads and parses the LTC681x cell voltage registers.根据想要读取的电池串参数reg， 去读取电压。
 * 			逻辑：每一次循环循环一个寄存器组(Register Group)，通过parse_cells函数，把电压数据处理打包好放入ic[]结构体中。
 * 			数据保存在: ic[c_ic].cells.c_codes之中
 * 			返回: ERROR数
 * param:   reg 寄存器
 * 			total_ic IC总数
 * 			u8* data 储存数据的数组*/
u8 LTC681x_rdcv(u8 reg, u8 total_ic, cell_asic *ic)
{
    int8_t pec_error = 0;
    u8 *cell_data;
    u8 c_ic = 0;
    cell_data = (u8 *)malloc((NUM_RX_BYT * total_ic) * sizeof(u8)); // 每次寄存器组读取中一个IC需要存储的大小是8个byte，8个IC就是64个byte大小
    if (reg == REG_ALL)
    {
        for (u8 cell_reg = 1; cell_reg < (chip_num_cv_reg + 1); cell_reg++)
        { // 所有IC的六个寄存器进行写命令+读值
            //			wakeup_idle(total_ic);
            LTC681x_rdcv_reg(cell_reg, total_ic, cell_data);
            for (int current_ic = 0; current_ic < total_ic; current_ic++)
            {
                c_ic = current_ic;
                pec_error = pec_error + parse_cells(current_ic, cell_reg, cell_data,
                                                    &ic[c_ic].cells.c_codes[0], // 把首位地址传过去，后续指针偏移
                                                    &ic[c_ic].cells.pec_match[0]);
            }
        }
    }
    else
    { // 单独读取某一个寄存器, 好像也不至于用这个
        LTC681x_rdcv_reg(reg, total_ic, cell_data);
        for (int current_ic = 0; current_ic < total_ic; current_ic++)
        {
            c_ic = current_ic;
            pec_error = pec_error + parse_cells(current_ic, reg, &cell_data[8 * c_ic],
                                                &ic[c_ic].cells.c_codes[0],
                                                &ic[c_ic].cells.pec_match[0]);
        }
    }
    LTC681x_check_pec(total_ic, CELL, ic);
    free(cell_data);
    return (pec_error);
}

/* brief: 	读取解码后的GPIO电压数据, 如果更换芯片可能得根据AUX寄存器做进行函数的变动
 * 			将整理好的电压数据储存在&ic[c_ic].aux.a_codes以及&ic[c_ic].aux.pec_match之中
 * param:   reg 寄存器
 * 			total_ic IC总数
 * 			u8* data 储存数据的数组*/
int8_t LTC681x_rdaux(u8 reg, u8 total_ic, cell_asic *ic)
{
    u8 *data;
    int8_t pec_error = 0;
    u8 c_ic = 0;
    data = (u8 *)malloc((NUM_RX_BYT * total_ic) * sizeof(u8)); // 比如total_ic=2, data数组就有16byte的空间,
                                                               // 也就是每次通过LTC681x_rdaux_reg都能读出所有ic在该寄存器的数据,
                                                               // 之后换寄存器读数据循环覆盖data
    /* 可以看数据手册P65, 由于AUX寄存器组不像CV组那样是连续的GPIO的电压,
     * 在GPIO5和GPIO6之间夹了一个VREF, 所以以后需要特殊处理*/
    if (reg == REG_ALL)
    {
        for (uint8_t gpio_reg = 1; gpio_reg < (chip_num_gpio_reg + 1); gpio_reg++)
        {                                                // 寄存器组的选择
            LTC681x_rdaux_reg(gpio_reg, total_ic, data); // Reads the raw auxiliary register data into the data[] array
            // LTC681x_clraux();                            // 清除GPIO电压寄存器值, 不这么做的话只能读回来auxA,B,C,D其中一个, 如果加了这条就可以全部寄存器读回来
            for (int current_ic = 0; current_ic < total_ic; current_ic++)
            {
                c_ic = current_ic;
                pec_error = parse_cells(current_ic, gpio_reg, data,  // 可以理解为一个解码过程,因为读回来是分开的两个16位数据,需要该函数组合成一个合适的数据
                                        &ic[c_ic].aux.a_codes[0],    // a_codes[12],储存了12个AUX寄存器数据
                                        &ic[c_ic].aux.pec_match[0]); // pec_match[4], 储存了4个PEC数据
            }
        }
    }
    else
    { // 单独读取某一个寄存器, 好像也不至于用这个
        LTC681x_rdaux_reg(reg, total_ic, data);
        for (int current_ic = 0; current_ic < total_ic; current_ic++)
        {
            c_ic = current_ic;
            pec_error = parse_cells(current_ic, reg, data,
                                    &ic[c_ic].aux.a_codes[0],
                                    &ic[c_ic].aux.pec_match[0]);
        }
    }
    LTC681x_check_pec(total_ic, AUX, ic);
    free(data);
    return (pec_error);
}

/* brief: 	辅助函数,清除所有电池的DCC位,并关闭DTM
 * 			注意还要调用其他函数发过去*/
void LTC681x_clear_discharge(u8 total_ic, cell_asic *ic)
{
    for (int i = 0; i < total_ic; i++)
    {
        ic[i].config.tx_data[4] = 0;                                  // DCC1-8
        ic[i].config.tx_data[5] = 0;                                  // DCC9-12, DCTO[0-3]
        ic[i].configb.tx_data[0] = ic[i].configb.tx_data[0] & (0x0F); // DCC13-16==0
        ic[i].configb.tx_data[1] = ic[i].configb.tx_data[1] & (0xF0); // DTMEN==0, DCC0,17,18==0
    }
}

/* brief: 	辅助函数,当IC在工作时候refon应该为1, 而GPIO位全部为0*/
void LTC6813_init_cfgb(u8 total_ic, cell_asic *ic)
{
    for (u8 current_ic = 0; current_ic < total_ic; current_ic++)
    {
        for (int j = 0; j < 6; j++)
        {
            ic[current_ic].configb.tx_data[j] = 0;
        }
        ic[current_ic].configb.tx_data[0] = 0X0F;
    }
}

/* brief: 	辅助函数, 设置CFGA的值, 应该跟在上面init函数的后面*/
void LTC6813_set_cfgr(u8 nIC, cell_asic *ic, bool refon)
{
    LTC681x_set_cfgr_refon(nIC, ic, refon);
    LTC681x_set_cfgr_adcopt(nIC, ic, False);
    LTC681x_set_cfgr_gpio(nIC, ic);
    LTC681x_set_cfgr_uv(nIC, ic, 0); // 没必要用里面的过压欠压
    LTC681x_set_cfgr_ov(nIC, ic, 0); // 没必要用里面的过压欠压
}
/* brief: 	Helper Function to set the configuration register B*/
void LTC6813_set_cfgrb(u8 nIC, cell_asic *ic, bool dtmen, bool gpiobits[4], bool dccbits[7])
{
    LTC6813_set_cfgrb_fdrf(nIC, ic, False);    // 设置成0: 强制 ADC 转换的数字冗余比较失败
    LTC6813_set_cfgrb_dtmen(nIC, ic, dtmen);   // DTMEN位,放电定时器监控器开启位
    LTC6813_set_cfgrb_ps(nIC, ic, 0);          // Digital Redundancy Path Selection. 这里不想用,故设置成默认值
    LTC6813_set_cfgrb_gpio_b(nIC, ic);         // 设置CFGB的GPIO6,7,8,9
    LTC6813_set_cfgrb_dcc_b(nIC, ic, dccbits); // 设置CFGB的DCC0,13,14,15,16,17,18
}

// Helper function to set the FDRF bit
void LTC6813_set_cfgrb_fdrf(u8 nIC, cell_asic *ic, bool fdrf)
{
    if (fdrf)
        ic[nIC].configb.tx_data[1] = ic[nIC].configb.tx_data[1] | 0x40;
    else
        ic[nIC].configb.tx_data[1] = ic[nIC].configb.tx_data[1] & 0xBF;
}

// Helper function to set the DTMEN bit
void LTC6813_set_cfgrb_dtmen(u8 nIC, cell_asic *ic, bool dtmen)
{
    if (dtmen)
        ic[nIC].configb.tx_data[1] = ic[nIC].configb.tx_data[1] | 0x08;
    else
        ic[nIC].configb.tx_data[1] = ic[nIC].configb.tx_data[1] & 0xF7;
}

// Helper function to set the PATH SELECT bit
void LTC6813_set_cfgrb_ps(u8 nIC, cell_asic *ic, bool ps[])
{
    for (int i = 0; i < 2; i++)
    {
        if (ps[i])
            ic[nIC].configb.tx_data[1] = ic[nIC].configb.tx_data[1] | (0x01 << (i + 4));
        else
            ic[nIC].configb.tx_data[1] = ic[nIC].configb.tx_data[1] & (~(0x01 << (i + 4)));
    }
}
// 设置CFGB的GPIO6,7,8,9
void LTC6813_set_cfgrb_gpio_b(u8 nIC, cell_asic *ic)
{
    for (int i = 0; i < 4; i++)
    {
        //      if(gpiobits[i])
        ic[nIC].configb.tx_data[0] = ic[nIC].configb.tx_data[0] | (0x01 << i);
        //      else ic[nIC].configb.tx_data[0] = ic[nIC].configb.tx_data[0]&(~(0x01<<i));
    }
}
// 设置CFGB的DCC0,13,14,15,16,17,18
void LTC6813_set_cfgrb_dcc_b(u8 nIC, cell_asic *ic, bool dccbits[])
{
    for (int i = 0; i < 7; i++)
    {
        if (i == 0)
        { // DCC0
            if (dccbits[i])
                ic[nIC].configb.tx_data[1] = ic[nIC].configb.tx_data[1] | 0x04;
            else
                ic[nIC].configb.tx_data[1] = ic[nIC].configb.tx_data[1] & 0xFB;
        }
        if (i > 0 && i < 5)
        { // DCC13,14,15,16
            if (dccbits[i])
                ic[nIC].configb.tx_data[0] = ic[nIC].configb.tx_data[0] | (0x01 << (i + 3));
            else
                ic[nIC].configb.tx_data[0] = ic[nIC].configb.tx_data[0] & (~(0x01 << (i + 3)));
        }
        if (i > 4 && i < 7)
        { // DCC17, 18
            if (dccbits[i])
                ic[nIC].configb.tx_data[1] = ic[nIC].configb.tx_data[1] | (0x01 << (i - 5));
            else
                ic[nIC].configb.tx_data[1] = ic[nIC].configb.tx_data[1] & (~(0x01 << (i - 5)));
        }
    }
}

/* brief: 	辅助函数, 设置DCC位
 * 			直接设置某个电池将对应DCC位设置为1. 因此不需要DCC数组
 * param:   i: 第几个模组的意思*/
void LTC6813_set_discharge(u8 i, u8 Cell, cell_asic *ic)
{
    if (Cell < 8)
    {
        ic[i].config.tx_data[4] = ic[i].config.tx_data[4] | (1 << (Cell)); // 在不改变除DCC外的其他配置寄存器位的前提下对DCC位进行修改
    }
    else if (Cell < 12)
    {
        ic[i].config.tx_data[5] = ic[i].config.tx_data[5] | (1 << (Cell - 8));
    }
    else if (Cell < 16)
    {
        ic[i].configb.tx_data[0] = ic[i].configb.tx_data[0] | (1 << (Cell - 8));
    }
    else if (Cell < 18)
    {
        ic[i].configb.tx_data[1] = ic[i].configb.tx_data[1] | (1 << (Cell - 16));
    }
}

/* brief: 	禁止均衡打开, 沉默掉DCC位, Mutes the LTC6813 discharge transistors*/
void LTC6813_mute()
{
    u8 cmd[2];
    cmd[0] = 0x00; // 对应mute命令：000 0010 1000
    cmd[1] = 0x28;
    cmd_68(cmd);
}
/* brief: 	清除禁止均衡打开, Clears the LTC6813 Mute Discharge*/
void LTC6813_unmute()
{
    u8 cmd[2];
    cmd[0] = 0x00;
    cmd[1] = 0x29;
    cmd_68(cmd);
}

/* brief: 	该命令将写入连接在菊花链堆栈中的 LTC681xs 的配置寄存器。
 * 			配置按降序写入，因此最先写入最后一个设备的配置。
 * 			记得配置好:ic[c_ic].config.tx_data[data]*/
void LTC6813_wrcfg(u8 total_ic, cell_asic ic[])
{
    u8 cmd[2] = {0x00, 0x01}; // 写配置寄存器组A的命令
    u8 write_buffer[256];
    u8 write_count = 0;
    u8 c_ic = 0;
    for (u8 current_ic = 0; current_ic < total_ic; current_ic++)
    {
        c_ic = current_ic; // 原函数有升序还是降序的选择, 但实测还是不能降序写, 否则会出问题
        for (u8 data = 0; data < 6; data++)
        {
            write_buffer[write_count] = ic[c_ic].config.tx_data[data];
            write_count++;
        }
    }
    write_68(total_ic, cmd, write_buffer); // 向所有IC发送
}

/* brief: 	使用LTC6813_set_cfgrb配置好数组
 * 			记得配置好:ic[c_ic].configb.tx_data[data];因为这是要写进去的数据*/
void LTC6813_wrcfgb(u8 total_ic, cell_asic ic[])
{
    u8 cmd[2] = {0x00, 0x24};
    u8 write_buffer[256];
    u8 write_count = 0;
    u8 c_ic = 0;
    for (u8 current_ic = 0; current_ic < total_ic; current_ic++)
    {
        c_ic = current_ic;
        for (u8 data = 0; data < 6; data++)
        {
            write_buffer[write_count] = ic[c_ic].configb.tx_data[data];
            write_count++;
        }
    }
    write_68(total_ic, cmd, write_buffer);
}

/* brief: 	读取到的数据被存到ic[c_ic].config.rx_data[byte]中
            出现PEC错误会将错误一次上传在ic[c_ic].config.rx_pec_match之中*/
int8_t LTC6813_rdcfg(u8 total_ic, cell_asic ic[])
{
    u8 cmd[2] = {0x00, 0x02};
    u8 read_buffer[256]; // 临时存储数组
    int8_t pec_error = 0;
    u16 data_pec;
    u16 calc_pec;
    u8 c_ic = 0;
    pec_error = read_68(total_ic, cmd, read_buffer);
    for (u8 current_ic = 0; current_ic < total_ic; current_ic++)
    {
        c_ic = current_ic;
        for (int byte = 0; byte < 8; byte++)
        {
            ic[c_ic].config.rx_data[byte] = read_buffer[byte + (8 * current_ic)];
        }
        calc_pec = pec15_calc(&read_buffer[8 * current_ic], 6);                                  // 这是根据读回来数据而计算的PEC
        data_pec = read_buffer[7 + (8 * current_ic)] | (read_buffer[6 + (8 * current_ic)] << 8); // 这是读回来数据的PEC,注意一个细节,6的数据被左移8个单位.
        if (calc_pec != data_pec)
        { // 出错可以给IC相关的位加一位
            ic[c_ic].config.rx_pec_match = 1;
        }
        else
            ic[c_ic].config.rx_pec_match = 0;
    }
    LTC681x_check_pec(total_ic, cfgr, ic); // 该函数用于上传PEC错误位.
    return (pec_error);
}

/*Reads CFGB
读取到的数据被存到ic[c_ic].configb.rx_data[byte]中
出现PEC错误会将错误一次上传在ic[c_ic].configb.rx_pec_match = 1之中
返回值:读取时是否接受异常,异常出现PECerror==1, 正常则返回0
*/
/* brief: 	读取到的数据被存到ic[c_ic].configb.rx_data[byte]中
 * 			出现PEC错误会将错误一次上传在ic[c_ic].configb.rx_pec_match = 1之中
 * 			返回值:读取时是否接受异常,异常出现PECerror==1, 正常则返回0*/
int8_t LTC681x_rdcfgb(u8 total_ic, cell_asic ic[])
{
    u8 cmd[2] = {0x00, 0x26};
    u8 read_buffer[256];
    int8_t pec_error = 0;
    u16 data_pec;
    u16 calc_pec;
    u8 c_ic = 0;
    pec_error = read_68(total_ic, cmd, read_buffer);
    for (u8 current_ic = 0; current_ic < total_ic; current_ic++)
    {
        c_ic = current_ic;
        for (int byte = 0; byte < 8; byte++)
        {
            ic[c_ic].configb.rx_data[byte] = read_buffer[byte + (8 * current_ic)];
        }
        calc_pec = pec15_calc(&read_buffer[8 * current_ic], 6);
        data_pec = read_buffer[7 + (8 * current_ic)] | (read_buffer[6 + (8 * current_ic)] << 8);
        if (calc_pec != data_pec)
        {
            ic[c_ic].configb.rx_pec_match = 1;
        }
        else
            ic[c_ic].configb.rx_pec_match = 0;
    }
    LTC681x_check_pec(total_ic, CFGRB, ic);
    return (pec_error);
}
