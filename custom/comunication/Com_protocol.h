#ifndef LIGHT_PROTOCOL_H
#define LIGHT_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

/* ==================== 1. 协议核心配置 ==================== */
#define LP_SOF                 0x5A  // 帧头
#define LP_MAX_PAYLOAD_LEN     64    // 最大负载长度（可根据芯片RAM调整）
#define LP_MAX_ROUTES          8     // 每个协议实例支持的最大消息注册数

/* ==================== 2. 数据结构定义 ==================== */

// 协议数据包结构体
typedef struct {
    uint8_t cmd;
    uint8_t len;
    uint8_t payload[LP_MAX_PAYLOAD_LEN];
} LP_Packet_t;

// 虚函数表定义 (用于绑定硬件接口)
typedef struct {
    void (*tx_bytes)(const uint8_t *data, uint16_t len); // 底层硬件发送函数
} LP_VTable_t;

// 业务消息路由项定义 (用于自适应分发)
typedef void (*LP_MsgHandler_t)(const uint8_t *payload, uint8_t len);

typedef struct {
    uint8_t cmd;
    uint8_t expected_len; // 期望长度 (0 表示变长)
    LP_MsgHandler_t handler;
} LP_Route_t;

// 协议实例控制块 (每个实例一个，实现多路复用)
typedef struct {
    const LP_VTable_t *vtable;       // 指向虚函数表的指针 (vptr)
    LP_Route_t routes[LP_MAX_ROUTES]; // 消息分发路由表
    uint8_t route_count;             // 已注册的消息数量
    
    // 私有解析状态机变量
    enum {
        LP_STATE_SOF = 0,
        LP_STATE_LEN,
        LP_STATE_DATA,
        LP_STATE_CMD,
        LP_STATE_CHECK
    } state;
    
    uint8_t data_cnt;
    uint8_t checksum;
    LP_Packet_t rx_packet;
} LP_Instance_t;

/* ==================== 3. 核心 API 声明 ==================== */

/**
 * @brief 初始化协议实例
 */
void LP_Init(LP_Instance_t *instance, const LP_VTable_t *vtable);

/**
 * @brief 注册自适应消息处理器
 * @return 1: 注册成功, 0: 注册表已满
 */
uint8_t LP_RegisterHandler(LP_Instance_t *instance, uint8_t cmd, uint8_t expected_len, LP_MsgHandler_t handler);

/**
 * @brief 协议逐字节解析器 (在串口中断中调用)
 */
void LP_ParseChar(LP_Instance_t *instance, uint8_t byte);

/**
 * @brief 协议多字节解析器 (在DMA或循环缓存读取中调用)
 */
void LP_ParseBuffer(LP_Instance_t *instance, const uint8_t *buf, uint16_t len);

/**
 * @brief 打包并发送数据
 * @return 0: 成功, -1: 参数错误
 */
int8_t LP_SendPacket(LP_Instance_t *instance, uint8_t cmd, const uint8_t *payload, uint8_t len);


/* ==================== 新增：通用数据流序列化工具 (无业务假设) ==================== */

// 打包流
typedef struct {
    uint8_t *buf;      // 缓冲区指针
    uint16_t capacity; // 缓冲区最大容量
    uint16_t offset;   // 当前写入偏移量
} LP_Writer_t;

// 解包流
typedef struct {
    const uint8_t *buf; // 接收缓冲区指针
    uint16_t len;       // 缓冲区数据长度
    uint16_t offset;    // 当前读取偏移量
} LP_Reader_t;

// 2. 将 init 和所有读写函数打包进“全局接口表”
typedef struct {
    void (*init)(LP_Writer_t *w, uint8_t *buffer, uint16_t capacity);
    void (*write_u8)(LP_Writer_t *w, uint8_t val);
    void (*write_u16)(LP_Writer_t *w, uint16_t val);
    void (*write_u32)(LP_Writer_t *w, uint32_t val);
    void (*write_float)(LP_Writer_t *w, float val);
} LP_Writer_Interface_t;

typedef struct {
    void (*init)(LP_Reader_t *r, const uint8_t *buffer, uint16_t len);
    uint8_t  (*read_u8)(LP_Reader_t *r);
    uint16_t (*read_u16)(LP_Reader_t *r);
    uint32_t (*read_u32)(LP_Reader_t *r);
    float    (*read_float)(LP_Reader_t *r);
} LP_Reader_Interface_t;

// 3. 声明两个全局唯一的接口对象 (充当命名空间)
extern const LP_Writer_Interface_t LP_Writer;
extern const LP_Reader_Interface_t LP_Reader;

#endif // LIGHT_PROTOCOL_H