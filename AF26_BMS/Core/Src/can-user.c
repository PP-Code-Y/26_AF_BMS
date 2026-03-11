/*
 * can.c
 *
 *  Created on: 2022年3月6日
 *      Author: BBBBB
 */

#include "can-user.h"
#include "os-user.h"
#include "state-user.h"
#include "relay-user.h"
#include "flash-user.h"

extern CAN_HandleTypeDef hcan1;
extern CAN_HandleTypeDef hcan2;
extern BMS A03;
extern vu8 Hall_Count;
extern vu8 Balance_flag;
vu8 charge_cur_count = 0; // 初值为0, 用于统计充电电流CAN数据发了多少次, 当单体电压充满后会清零

void can1_interal_config()
{
    HAL_StatusTypeDef HAL_Status;                   // 状态
    CAN_FilterTypeDef CAN_Filter;                   // 定义接受过滤器的结构体
    CAN_Filter.FilterBank = 0;                      // 第一个滤波器
    CAN_Filter.FilterMode = CAN_FILTERMODE_IDMASK;  // 过滤器模式选择，当下选择掩码模式
    CAN_Filter.FilterScale = CAN_FILTERSCALE_16BIT; // 开启16位掩码模式
    CAN_Filter.FilterIdHigh = 0xFFFF << 5;          // 电流传感器CAN信号
    CAN_Filter.FilterIdLow = 0x3c3 << 5;            // ID唯一, 必须筛出来
    CAN_Filter.FilterMaskIdHigh = 0;                // 对上位机发进来的先不进行滤波了，置1位为要求一致的位
    CAN_Filter.FilterMaskIdLow = 0;
    CAN_Filter.FilterFIFOAssignment = CAN_FILTER_FIFO0; // 本成员用于设置当报文通过筛选器的匹配后，该报文会被存储到哪一个接收FIFO
    CAN_Filter.FilterActivation = ENABLE;               // 使能过滤器

    if (HAL_OK != HAL_CAN_ConfigFilter(&hcan1, &CAN_Filter))
    { // 配置CAN接收过滤器
        Error_Handler();
    }
    HAL_Status = HAL_CAN_Start(&hcan1); // 开启CAN
    if (HAL_Status != HAL_OK)
    {
        Error_Handler();
    }
    // Activate CAN RX notification (HERE WE attach FIFO0 to CAN1) called "CAN0" in manual
    if (HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK)
    {
        Error_Handler();
    }
}

void can2_exteral_config()
{
    HAL_StatusTypeDef HAL_Status;
    CAN_FilterTypeDef CAN_Filter;
    CAN_Filter.FilterBank = 14; // 第14个滤波器, 对于CAN2
    CAN_Filter.FilterMode = CAN_FILTERMODE_IDLIST;
    CAN_Filter.FilterScale = CAN_FILTERSCALE_32BIT; // 开启32位列表模式
    CAN_Filter.FilterIdHigh = 0xC7FA;               // 充电信号:0x18FF50E5, HIGH有公式((0x18FF50E5<<3)>>16)&0xffff
    CAN_Filter.FilterIdLow = 0x872C;                // LOW有公式(0x18FF50E5<<3)&0xffff|CAN_ID_EXT, 其中CAN_ID_EXT=100(二进制)
    CAN_Filter.FilterMaskIdHigh = 0;                // 对上位机发进来的先不进行滤波了
    CAN_Filter.FilterMaskIdLow = 0;
    CAN_Filter.FilterFIFOAssignment = CAN_FILTER_FIFO1;
    CAN_Filter.FilterActivation = ENABLE; // 使能过滤器
    CAN_Filter.SlaveStartFilterBank = 14;

    if (HAL_OK != HAL_CAN_ConfigFilter(&hcan2, &CAN_Filter))
    {
        Error_Handler();
    }
    HAL_Status = HAL_CAN_Start(&hcan2);
    if (HAL_Status != HAL_OK)
    {
        Error_Handler();
    }
    // Activate CAN RX notification (HERE WE attach FIFO1 to CAN2) called "CAN0" in manual
    if (HAL_CAN_ActivateNotification(&hcan2, CAN_IT_RX_FIFO1_MSG_PENDING) != HAL_OK)
    {
        Error_Handler();
    }
}



// CAN1中断回调函数
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    uint8_t aRxData[8];
    CAN_RxHeaderTypeDef hCAN1_RxHeader;                 // 接收句柄，用于记录接收到的数据信息内的各个帧的数据，包括ID用于判断指令
    static vu8 *temp_threshold = (vu8 *)&A03.Threshold; // 阈值的中间变量，赋予E03中的阈值数据的地址
    static vu16 threshold_id = 0x140;
    vu8 temp_charge[8] = {0};

    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &hCAN1_RxHeader, aRxData) == HAL_OK)
    { // 一次接收到的最多只有8个字节
        if (hcan == &hcan1)
        {
            /*阈值恢复默认值指令*/
            if (hCAN1_RxHeader.StdId == 0x101)
            {
                A03.State.Init_flag = 0;
                Flash_Write_Init_Flag();                                                                     // 往Flash里面存Init_Flag值
                Threshold_Update();                                                                          // 更新BMS的阈值
                Can_Send_Package((u8 *)&A03.Threshold, CAN_ID_THRESHOLD, CAN_PL_THRESHOLD, ENABLE, DISABLE); // 向上位机传回阈值
                // A03.State.Charge_flag   = 1;

            } // 0x140 - 0x14D   14组
            /*BDU更改错误位屏蔽*/
            if (hCAN1_RxHeader.StdId == 0x132)
            {
                A03.Alarm.error_mute = aRxData[0] + (aRxData[1] << 8) + (aRxData[2] << 16) + (aRxData[3] << 24);
                Flash_Write_Errormute(); // 更改开机时的默认屏蔽位
            }
            /*BDU修改阈值*/
            if (hCAN1_RxHeader.StdId == threshold_id)
            {
                for (u8 i = 0; i < 8; i++)
                { // 8个字节的分组读取
                    *temp_threshold = aRxData[i];
                    temp_threshold++; // 地址自增来实现对结构体内不同成员(一个字节一个字节的偏移，偏移字节数决定于指针强制类型转化的类型)进行选择
                }
                threshold_id++;
                if (threshold_id > 0X14D)
                {                         // 修改到threshold数组的最后一个值, 则进行指针的复位, 需要结合BMS的CAN协议来看
                    threshold_id = 0x140; // 复位
                    temp_threshold = (vu8 *)&A03.Threshold;
                    A03.State.Init_flag = 1;       // 修改了阈值则伴随修改标志位为1
                    Flash_SE(Flash_Sec_Init_Flag); // 向Flash存进最新的flag
                    Flash_Write((u8 *)&A03.State.Init_flag, Flash_Addr_Init_Flag, sizeof(A03.State.Init_flag) / sizeof(vu8));
                    Flash_Write_Threshold(); // 向Flash存进最新的Threshold
                }
            }
            if (hCAN1_RxHeader.StdId == 0x14E)
            { /*收到该指令发送阈值数据*/ // 对应上位机得到更新阈值指令
                Can_Send_Package((u8 *)&A03.Threshold, CAN_ID_THRESHOLD, CAN_PL_THRESHOLD, ENABLE, DISABLE);
            }
            /*进入工装模式*/
            if (hCAN1_RxHeader.StdId == 0x150)
            {
                A03.Tooling.AIRP_Ctrl = aRxData[0]; // 更改工装模式配置
                A03.Tooling.AIRN_Ctrl = aRxData[1];
                A03.Tooling.PrechargeAIR_Ctrl = aRxData[2];
                A03.Tooling.Error_Ctrl = aRxData[3];
                A03.Tooling.Fans_Ctrl = aRxData[4];
                A03.Tooling.Manual_Enable = aRxData[5];
                A03.Tooling.Active_onHV = aRxData[6];
                A03.State.BMS_State = BMS_Manual_MODE; // 进入工装模式
            }
            // 充电相关   0x151 - 0x153 恢复默认，写入，更新充电阈值
            if (hCAN1_RxHeader.StdId == 0x151)
            { // 恢复默认
                A03.charge.ChargeVol_tx = 6000;
                A03.charge.ChargeCur_tx = 30;
                temp_charge[0] = (A03.charge.ChargeVol_tx & 0xFF); // 低8位
                temp_charge[1] = (A03.charge.ChargeVol_tx >> 8);   // 高8位
                temp_charge[2] = (A03.charge.ChargeCur_tx & 0xFF);
                temp_charge[3] = (A03.charge.ChargeCur_tx >> 8);
                Can_Send_Package((u8 *)&temp_charge, CAN_ID_CHARGE_PARA, CAN_PL_CHARGE_PARA, ENABLE, DISABLE);
            }
            if (hCAN1_RxHeader.StdId == 0x152)
            {                                                             // 写入
                A03.charge.ChargeVol_tx = aRxData[0] + (aRxData[1] << 8); // 电压
                A03.charge.ChargeCur_tx = aRxData[2] + (aRxData[3] << 8); // 电流
            }
            if (hCAN1_RxHeader.StdId == 0x153)
            {                                                      // 更新
                temp_charge[0] = (A03.charge.ChargeVol_tx & 0xFF); // 低8位
                temp_charge[1] = (A03.charge.ChargeVol_tx >> 8);   // 高8位
                temp_charge[2] = (A03.charge.ChargeCur_tx & 0xFF);
                temp_charge[3] = (A03.charge.ChargeCur_tx >> 8);
                Can_Send_Package((u8 *)&temp_charge, CAN_ID_CHARGE_PARA, CAN_PL_CHARGE_PARA, ENABLE, DISABLE);
            }
            if (hCAN1_RxHeader.StdId == 0x154)
            { // 清除故障位
                for (u8 i = 4; i < 22; i++)
                {
                    *((vu8 *)&A03.Alarm + i) = 0;
                }
            }
            if (hCAN1_RxHeader.StdId == 0x155)
            {
                Balance_flag = 1;
            }
            if (hCAN1_RxHeader.StdId == 0x156)
            {
                Balance_flag = 0;
            }
            /*霍尔can*/
            if (hCAN1_RxHeader.StdId == 0x03c2)
            {
                if ((aRxData[0] << 24) + (aRxData[1] << 16) + (aRxData[2] << 8) + aRxData[3] == 0xffff)
                {
                    A03.Alarm.Hall_Error = 1;
                }
                else
                {
                    A03.State.TotalCurrent = (aRxData[0] << 24) + (aRxData[1] << 16) + (aRxData[2] << 8) + aRxData[3] - 0x80000000;
                    Hall_Count = 0; // 接收到霍尔电流信息, 清除计数，该计数用于霍尔电流计断线判断
                    if (A03.State.BMS_State == BMS_RUN || A03.State.BMS_State == BMS_CHARGE)
                    {
                        CalcSOC(); // 计算SOC
                    }
                }
            }
        }
    }
    else
    {
        Error_Handler();
    }
}

/**
 * @brief 计算第一个模组的平均温度信息并通过整车can（can2）发送，CAN ID为401，温度当次的最大变化值为2摄氏度
 *
 */
void CalcAveTemp(void)
{
    uint16_t sumOfTemp = 0, aveTemp;
    for (int i = 0; i < 5; i++)
    {
        sumOfTemp += A03.Cell.Temp[i];
    }
    aveTemp = (uint16_t)(sumOfTemp / 10.0 * 5.0); // 保留一位小数
    uint8_t data[8] = {0};
    data[0] = aveTemp & 0x00FF;
    data[1] = aveTemp & 0xFF00;
    Can_Send_Msg(data, 0x401, DISABLE, ENABLE);
}

/**
 * @brief 计算SOC的函数，SOC变化量达到千分之一时才发生变化
 */
void CalcSOC(void)
{
    static int32_t electricChargeDelta = 0;
    electricChargeDelta += A03.State.TotalCurrent * 16;
    if (electricChargeDelta >= 50400000)
    {
        electricChargeDelta = electricChargeDelta - 50400000;
        A03.State.SOC -= 1;
    }
    else if (electricChargeDelta <= -50400000)
    {
        electricChargeDelta = electricChargeDelta + 50400000;
        A03.State.SOC += 1;
    }
}

// CAN2中断回调函数
void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    uint8_t aRxData[8];
    static u8 ChargeCan_count = 0;
    CAN_RxHeaderTypeDef hCAN2_RxHeader;
    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO1, &hCAN2_RxHeader, aRxData) == HAL_OK)
    {
        if (hcan == &hcan2)
        {
            if (hCAN2_RxHeader.ExtId == 0x18FF50E5)
            { // 收到充电机发送目前自己的电压和电流
                // KeepChargerCommunicate();
                A03.charge.ChargeVol_rx = (aRxData[0] << 8) + aRxData[1];
                A03.charge.ChargeCur_rx = (aRxData[2] << 8) + aRxData[3];
                A03.charge.ChargeStatus_rx = aRxData[4]; // 充电机当前状态
                A03.State.Charge_flag = 1;               // flag等于1代表收到充电机信号, 可以进行预充
                if (ChargeCan_count <= 20)
                {                      // 接收充电机CAN计数，开始时充电机并未进入充电，防止在此时直接进报错
                    ChargeCan_count++; // 超过20次后计数停留在21，保证后续故障检测报错
                }
                if (A03.charge.ChargeStatus_rx == 0x10)
                { // 表示充电机没有错误, 详情看铁城充电协议
                  // 充电程序的下一步要看statemachine();
                }
                else if (A03.charge.ChargeStatus_rx == 0x00)
                {
                    // 此时正常充电, 空走一下
                }
                else if (ChargeCan_count >= 20)
                {                                      // 表示充电机有错误, 详情看铁城充电协议
                    A03.Alarm.Chargemachine_Error = 1; // 充电机故障标志位置1
                    if ((A03.Alarm.error_mute & Chargemachine_Error_mute) == 0)
                    {
                        Error(); // BMS进入故障, 关闭输出
                    }
                }
                ChargeMachine_Count = 0; // 计数值清零, 代表已经收到了充电机数据
            }
        }
    }
    else
    {
        Error_Handler();
    }
}

/**
 * CAN数据传输, 只发送一组
 * @param	data   待发送的数据
 * 			ID canID号, 详情请看BMSCAN协议
 * 			can1_state 是否用CAN1发送, ENABLE/DISABLE
 * 			can2_state 是否用CAN2发送, ENABLE/DISABLE
 */
void Can_Send_Msg(u8 *data, u16 ID, u8 can1_state, u8 can2_state) // 依据使能来决定通过哪个CAN进行发送
{
    uint32_t txmailbox1 = CAN_TX_MAILBOX0;
    uint32_t txmailbox2 = CAN_TX_MAILBOX1;
    u16 time_out = 0;

    CAN_TxHeaderTypeDef TxMessage;
    TxMessage.StdId = ID;
    TxMessage.ExtId = ID;
    TxMessage.RTR = CAN_RTR_DATA;
    TxMessage.DLC = 8; // 定义单个数据帧发送8个字节

    TxMessage.IDE = CAN_ID_STD; // BDU使用标准帧
    if (can1_state == ENABLE)
    {
        while ((HAL_CAN_AddTxMessage(&hcan1, &TxMessage, data, &txmailbox1) != HAL_OK) && (time_out < 1000))
        { // 等待发送完成, 否则会丢包
            time_out++;
        }
    }
    TxMessage.IDE = CAN_ID_STD; // 使用拓展帧发送, 因为CANA的主控只能收拓展帧
    time_out = 0;
    if (can2_state == ENABLE)
    {
        while ((HAL_CAN_AddTxMessage(&hcan2, &TxMessage, data, &txmailbox2) != HAL_OK) && (time_out < 1000))
        {
            time_out++;
        }
    }
}

/**
 * CAN数据传输, 发送一个包
 * @param	data   待发送的数据
 * 			ID canID号, 详情请看BMSCAN协议
 * 			num 要发送多少组
 * 			can1_state 是否用CAN1发送, ENABLE/DISABLE
 * 			can2_state 是否用CAN2发送, ENABLE/DISABLE
 */
void Can_Send_Package(u8 *data, u16 ID, u8 num, u8 can1_state, u8 can2_state)
{
    for (u8 count = 0; count < num; count++)
    {
        Can_Send_Msg(data, ID, can1_state, can2_state);
        data += 8; //
        ID++;
    }
}

/**
 * 网上抄下来的, 还挺牛的, 可能不一定用
 * CAN数据传输
 * @param  buf    待发送的数据
 * @param  len    数据长度
 * @param  number CAN编号，=0：CAN1，=1：CAN2
 * @return        0：成功  other：失败
 */
uint8_t CAN_Transmit(const void *buf, uint32_t len, uint8_t number)
{
    uint32_t txmailbox = 0;
    uint32_t offset = 0;
    CAN_TxHeaderTypeDef hdr;

    hdr.IDE = CAN_ID_EXT;   // ID类型：扩展帧
    hdr.RTR = CAN_RTR_DATA; // 帧类型：数据帧
                            //    hdr.StdId = 0;															// 标准帧ID,最大11位，也就是0x7FF
                            //    hdr.ExtId = number == 0 ? CAN1_BASE_ID : CAN2_BASE_ID;					// 扩展帧ID,最大29位，也就是0x1FFFFFFF
    hdr.TransmitGlobalTime = DISABLE;
    while (len != 0)
    {
        hdr.DLC = len > 8 ? 8 : len; // 数据长度
        if (HAL_CAN_AddTxMessage(number == 0 ? &hcan1 : &hcan2, &hdr, ((uint8_t *)buf) + offset, &txmailbox) != HAL_OK)
            return 1;
        offset += hdr.DLC;
        len -= hdr.DLC;
    }
    return 0;
}
/**
 * CAN发送数据, 具体为什么要设置成这样请看BMSCAN手册
 * @param  buf    待发送的数据
 * @param  len    数据长度
 * @param  number CAN编号，=0：CAN1，=1：CAN2
 * @return        0：成功  other：失败
 */
void Can_Send_Data(u8 can1_state, u8 can2_state) // 用在BMS_can
{
    Can_Send_Package((u8 *)&A03.State, CAN_ID_BMS_state, CAN_PL_BMS_state, ENABLE, can2_state); // BMS整体状态
    /*电芯状态*/
    Can_Send_Package((u8 *)&A03.Cell.Vol[0], CAN_ID_CELL_Vol, CAN_PL_CELL_Vol, ENABLE, DISABLE);          // 单体电压
    Can_Send_Package((u8 *)&A03.Cell.Temp[0], CAN_ID_CELL_Tem, CAN_PL_CELL_Tem, ENABLE, DISABLE);         // 单体温度
    Can_Send_Package((u8 *)&A03.Cell.MaxVol, CAN_ID_CELL_MVol, CAN_PL_CELL_MVol, ENABLE, DISABLE);        // 单体极值
    Can_Send_Package((u8 *)&A03.Cell.DCC[0], CAN_ID_CELL_Dcc, CAN_PL_CELL_Dcc, ENABLE, DISABLE);          // 单体均衡状态
    Can_Send_Package((u8 *)&A03.Cell.MaxVol_No, CAN_ID_CELL_MVolNo, CAN_PL_CELL_MVolNo, ENABLE, DISABLE); // 单体极值序号
    Can_Send_Package((u8 *)&A03.Relay, CAN_ID_RELAY_STATE, CAN_PL_RELAY_STATE, ENABLE, DISABLE);          // 继电器状态
    Can_Send_Package((u8 *)&A03.charge, CAN_ID_CHARGE_STATE, CAN_PL_CHARGE_STATE, ENABLE, DISABLE);       // 充电状态
    /*告警*/
    Can_Send_Package((u8 *)&A03.Alarm, CAN_ID_ALARM, CAN_PL_ALARM, ENABLE, DISABLE);
    Can_Send_Package((u8 *)&A03.Alarm.Openwire_Vol[0], CAN_ID_OpenVol, CAN_PL_OpenVol, ENABLE, DISABLE);  // 开路电压
    Can_Send_Package((u8 *)&A03.Alarm.Openwire_Temp[0], CAN_ID_OpenTem, CAN_PL_OpenTem, ENABLE, DISABLE); // 开路温度采集
    /*工装的数据只接受但不发, 阈值等待ID为14E的CAN再发送阈值*/
    //! 发送寄存器 EF250810
    Can_Send_Package((u8 *)&A03.Codes.A_CODES_GPIO[0], 0x170, 12, ENABLE, DISABLE);
    Can_Send_Package((u8 *)&A03.Codes.A_CODES_REF[0], 0x17C, 2, ENABLE, DISABLE);
    Can_Send_Package((u8 *)&A03.Codes.B_CODES_DELTA[0], 0x17E, 36, ENABLE, DISABLE);
}
/**
 * CAN发送充电数据, 具体协议需要看铁城充电机的协议
 */
void Can_Send_Charge(u8 Charge_state) // 用在charge_ing
{
    uint8_t aTxData[8];
    CAN_TxHeaderTypeDef TxMessage;
    u8 i = 0;
    u16 time_out = 0;
    uint32_t txmailbox2 = CAN_TX_MAILBOX2;

    //	TxMessage.StdId = 0x3c3;	//其实应该没有用, 本来就应该以拓展帧发给充电机, 先保留
    TxMessage.ExtId = 0x1806E5F4;
    TxMessage.IDE = CAN_ID_EXT;
    TxMessage.RTR = CAN_RTR_DATA;
    TxMessage.DLC = 8;
    if (Charge_state == ENABLE)
    {
        // 定义好发送的CAN数据
        aTxData[0] = (A03.charge.ChargeVol_tx >> 8);   // 充电电压, 输出电压高字节
        aTxData[1] = (A03.charge.ChargeVol_tx & 0xFF); // 充电电压, 输出电压低字节
        aTxData[4] = 0;                                // ENABLE代表开启充电机
        for (i = 5; i < 8; i++)
        { // 都是保留位, 给没用的数据进行0的初始化,防止数据乱跑
            aTxData[i] = 0;
        }
        if (charge_cur_count < 5)
        {                                            // 第1-5次
            aTxData[2] = (Charge_Cur_State1 >> 8);   // 充电电流, 输出电流高字节
            aTxData[3] = (Charge_Cur_State1 & 0xFF); // 充电电流, 输出电流低字节
            charge_cur_count++;
        }
        else if (charge_cur_count < 10)
        {                                            // 第5-10次
            aTxData[2] = (Charge_Cur_State2 >> 8);   // 充电电流, 输出电流高字节
            aTxData[3] = (Charge_Cur_State2 & 0xFF); // 充电电流, 输出电流低字节
            charge_cur_count++;
        }
        else if (charge_cur_count < 15)
        {                                            // 第10-15次
            aTxData[2] = (Charge_Cur_State3 >> 8);   // 充电电流, 输出电流高字节
            aTxData[3] = (Charge_Cur_State3 & 0xFF); // 充电电流, 输出电流低字节
            charge_cur_count++;
        }
        else
        {                                                  // 第15次以后
            aTxData[2] = (A03.charge.ChargeCur_tx >> 8);   // 充电电流, 输出电流高字节
            aTxData[3] = (A03.charge.ChargeCur_tx & 0xFF); // 充电电流, 输出电流低字节
        }
    }
    else
    {
        // 定义好发送的CAN数据
        aTxData[0] = 0; // 关闭充电机, 电压和电流都给0
        aTxData[1] = 0;
        aTxData[2] = 0;
        aTxData[3] = 0;
        aTxData[4] = 1; // DISABLE代表开启充电机
        for (i = 5; i < 8; i++)
        { // 都是保留位, 给没用的数据进行0的初始化,防止数据乱跑
            aTxData[i] = 0;
        }
    }
    // 发送CAN数据
    while ((HAL_CAN_AddTxMessage(&hcan2, &TxMessage, aTxData, &txmailbox2) != HAL_OK) && (time_out < 1000))
    {
        time_out++;
    }
}
