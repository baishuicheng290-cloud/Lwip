#include "headfile.h"
#include "app_can.h"

extern CAN_HandleTypeDef hcan1; // 声明 HAL 库生成的全局 CAN 句柄

LP_Instance_t g_can1_protocol;


// 底层硬件发送适配器（自动分帧发送）
void CAN_Hardware_Send(const uint8_t *data, uint16_t len) {
    CAN_TxHeaderTypeDef TxHeader;
    uint32_t TxMailbox;
    
    TxHeader.StdId = 0x103; 
    TxHeader.RTR = CAN_RTR_DATA;
    TxHeader.IDE = CAN_ID_STD;
    TxHeader.TransmitGlobalTime = DISABLE;

    uint16_t sent = 0;
    while (sent < len) {
        uint8_t chunk = (len - sent > 8) ? 8 : (len - sent);
        TxHeader.DLC = chunk;
        
        while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) == 0); // 等待发送邮箱空闲
        HAL_CAN_AddTxMessage(&hcan1, &TxHeader, (uint8_t*)&data[sent], &TxMailbox);
        sent += chunk;
    }
}

static const LP_VTable_t g_can_vtable = {
    .tx_bytes = CAN_Hardware_Send
};

// 中断接收回调，直接解析并喂给本驱动的协议实例
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
    CAN_RxHeaderTypeDef RxHeader;
    uint8_t RxData[8];
    
    if (hcan->Instance == hcan1.Instance) {
        if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, RxData) == HAL_OK) {
            LP_ParseBuffer(&g_can1_protocol, RxData, RxHeader.DLC);
        }
    }
}

// 格式化打包发送
void App_Can_Send_Format(uint8_t cmd, const char *format, ...) {
    uint8_t send_buf[LP_MAX_PAYLOAD_LEN];
    LP_Writer_t w;
    va_list args;

    LP_Writer.init(&w, send_buf, sizeof(send_buf));
    
    va_start(args, format);
    for (const char *p = format; *p != '\0'; p++) {
        switch (*p) {
            case 'b':
                LP_Writer.write_u8(&w, (uint8_t)va_arg(args, int)); 
                break;
            case 'w': 
                LP_Writer.write_u16(&w, (uint16_t)va_arg(args, int));
                break;
            case 'd': 
                LP_Writer.write_u32(&w, va_arg(args, uint32_t));
                break;
            case 'f': 
                LP_Writer.write_float(&w, (float)va_arg(args, double));
                break;
            default:
                break; 
        }
    }
    va_end(args);
    
    LP_SendPacket(&g_can1_protocol, cmd, send_buf, w.offset);
}

void can_init(void) {
    CAN_FilterTypeDef sFilterConfig;

    // 【必须赋值！】配置过滤器接收任意 ID 的数据
    sFilterConfig.FilterBank = 0;                        // 使用过滤器组 0
    sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;    // 屏蔽位模式
    sFilterConfig.FilterScale = CAN_FILTERSCALE_16BIT;   // 32位宽
    sFilterConfig.FilterIdHigh = 0x0000;                 // 接收任意 ID (不进行过滤)
    sFilterConfig.FilterIdLow = 0x0000;
    sFilterConfig.FilterMaskIdHigh = 0x0000;
    sFilterConfig.FilterMaskIdLow = 0x0000;
    sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;   // 接收到的数据放入 FIFO 0
    sFilterConfig.FilterActivation = ENABLE;             // 使能该过滤器
    sFilterConfig.SlaveStartFilterBank = 14;

    // 这时再将配置写入硬件寄存器
    HAL_CAN_ConfigFilter(&hcan1, &sFilterConfig); 
    
    // 启动 CAN
    HAL_CAN_Start(&hcan1);
    
    // 激活 FIFO0 消息挂起中断
    HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING); 
}

static Can_Interface_t can={
	.init=can_init,
	.send=App_Can_Send_Format,
};


/* ==================== 驱动层 Get 接口实现 ==================== */
const Can_Interface_t* Get_Can1_Interface() {
    return &can;
}

const LP_VTable_t* Get_Can1_Net_Vtable(void) {
    return &g_can_vtable;
}

LP_Instance_t* Get_Can1_LP_Instance(void) {
    return &g_can1_protocol;
}