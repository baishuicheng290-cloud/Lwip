#include "Com_protocol.h"
#include <string.h>

void LP_Init(LP_Instance_t *instance, const LP_VTable_t *vtable) {
    if (!instance || !vtable) return;
    
    instance->vtable = vtable;
    instance->route_count = 0;
    instance->state = LP_STATE_SOF;
    instance->data_cnt = 0;
    instance->checksum = 0;
    memset(instance->routes, 0, sizeof(instance->routes));
}

uint8_t LP_RegisterHandler(LP_Instance_t *instance, uint8_t cmd, uint8_t expected_len, LP_MsgHandler_t handler) {
    if (!instance || !handler) return 0;
    if (instance->route_count >= LP_MAX_ROUTES) return 0; // 路由表已满

    instance->routes[instance->route_count].cmd = cmd;
    instance->routes[instance->route_count].expected_len = expected_len;
    instance->routes[instance->route_count].handler = handler;
    instance->route_count++;
    return 1;
}

// 内部函数：解析成功后进行自适应路由分发
static void LP_Dispatch(LP_Instance_t *instance, const LP_Packet_t *packet) {
    for (uint8_t i = 0; i < instance->route_count; i++) {
        if (instance->routes[i].cmd == packet->cmd) {
            // 长度自适应校验：若定义了固定长度，执行安全检查
            if (instance->routes[i].expected_len != 0 && 
                instance->routes[i].expected_len != packet->len) {
                return; // 长度不匹配，丢弃以防内存越界
            }
            // 执行具体的业务回调
            if (instance->routes[i].handler) {
                instance->routes[i].handler(packet->payload, packet->len);
            }
            return;
        }
    }
}

void LP_ParseChar(LP_Instance_t *instance, uint8_t byte) {
    if (!instance) return;

    switch (instance->state) {
        case LP_STATE_SOF:
            if (byte == LP_SOF) {
                instance->checksum = byte;
                instance->state = LP_STATE_LEN;
            }
            break;

        case LP_STATE_LEN:
            if (byte <= LP_MAX_PAYLOAD_LEN) {
                instance->rx_packet.len = byte;
                instance->checksum += byte;
                instance->data_cnt = 0;
                instance->state = (instance->rx_packet.len > 0) ? LP_STATE_DATA : LP_STATE_CMD;
            } else {
                instance->state = LP_STATE_SOF; // 长度异常，重置
            }
            break;

        case LP_STATE_DATA:
            instance->rx_packet.payload[instance->data_cnt++] = byte;
            instance->checksum += byte;
            if (instance->data_cnt >= instance->rx_packet.len) {
                instance->state = LP_STATE_CMD;
            }
            break;

        case LP_STATE_CMD:
            instance->rx_packet.cmd = byte;
            instance->checksum += byte;
            instance->state = LP_STATE_CHECK;
            break;

        case LP_STATE_CHECK:
            if (byte == instance->checksum) {
                // 校验成功，分发包
                LP_Dispatch(instance, &(instance->rx_packet));
            }
            instance->state = LP_STATE_SOF; // 重置状态机，准备接收下一帧
            break;

        default:
            instance->state = LP_STATE_SOF;
            break;
    }
}

void LP_ParseBuffer(LP_Instance_t *instance, const uint8_t *buf, uint16_t len) {
    if (!instance || !buf) return;
    for (uint16_t i = 0; i < len; i++) {
        LP_ParseChar(instance, buf[i]);
    }
}

int8_t LP_SendPacket(LP_Instance_t *instance, uint8_t cmd, const uint8_t *payload, uint8_t len) {
    if (!instance || !instance->vtable || !instance->vtable->tx_bytes) return -1;
    if (len > LP_MAX_PAYLOAD_LEN) return -1;
    if (len > 0 && !payload) return -1;

    uint8_t tx_buf[LP_MAX_PAYLOAD_LEN + 4];
    uint16_t tx_idx = 0;
    uint8_t temp_checksum = 0;

    tx_buf[tx_idx++] = LP_SOF;
    temp_checksum += LP_SOF;

    tx_buf[tx_idx++] = len;
    temp_checksum += len;

    for (uint8_t i = 0; i < len; i++) {
        tx_buf[tx_idx++] = payload[i];
        temp_checksum += payload[i];
    }

    tx_buf[tx_idx++] = cmd;
    temp_checksum += cmd;

    tx_buf[tx_idx++] = temp_checksum;

    // 通过虚表引用的硬件接口发送
    instance->vtable->tx_bytes(tx_buf, tx_idx);
    return 0;
}

/* ==================== 新增：通用数据流序列化工具实现 ==================== */

void LP_WriterInit(LP_Writer_t *w, uint8_t *buffer, uint16_t capacity) {
    if (!w || !buffer) return;
    w->buf = buffer;
    w->capacity = capacity;
    w->offset = 0;
}

void LP_ReaderInit(LP_Reader_t *r, const uint8_t *buffer, uint16_t len) {
    if (!r || !buffer) return;
    r->buf = buffer;
    r->len = len;
    r->offset = 0;
}

void LP_WriteU8(LP_Writer_t *w, uint8_t val) {
    if (w->offset + 1 <= w->capacity) {
        w->buf[w->offset++] = val;
    }
}

void LP_WriteU16(LP_Writer_t *w, uint16_t val) {
    if (w->offset + 2 <= w->capacity) {
        w->buf[w->offset++] = (uint8_t)(val & 0xFF);
        w->buf[w->offset++] = (uint8_t)((val >> 8) & 0xFF);
    }
}

void LP_WriteU32(LP_Writer_t *w, uint32_t val) {
    if (w->offset + 4 <= w->capacity) {
        w->buf[w->offset++] = (uint8_t)(val & 0xFF);
        w->buf[w->offset++] = (uint8_t)((val >> 8) & 0xFF);
        w->buf[w->offset++] = (uint8_t)((val >> 16) & 0xFF);
        w->buf[w->offset++] = (uint8_t)((val >> 24) & 0xFF);
    }
}

void LP_WriteFloat(LP_Writer_t *w, float val) {
    uint32_t temp;
    memcpy(&temp, &val, 4);
    LP_WriteU32(w, temp);
}

uint8_t LP_ReadU8(LP_Reader_t *r) {
    return (r->offset + 1 <= r->len) ? r->buf[r->offset++] : 0;
}

uint16_t LP_ReadU16(LP_Reader_t *r) {
    if (r->offset + 2 <= r->len) {
        uint16_t val = r->buf[r->offset++];
        val |= (uint16_t)r->buf[r->offset++] << 8;
        return val;
    }
    return 0;
}

uint32_t LP_ReadU32(LP_Reader_t *r) {
    if (r->offset + 4 <= r->len) {
        uint32_t val = r->buf[r->offset++];
        val |= (uint32_t)r->buf[r->offset++] << 8;
        val |= (uint32_t)r->buf[r->offset++] << 16;
        val |= (uint32_t)r->buf[r->offset++] << 24;
        return val;
    }
    return 0;
}

float LP_ReadFloat(LP_Reader_t *r) {
    uint32_t temp = LP_ReadU32(r);
    float val;
    memcpy(&val, &temp, 4);
    return val;
}

// 用已有函数直接填充全局接口
const LP_Writer_Interface_t LP_Writer = {
    .init = LP_WriterInit,
    .write_u8 = LP_WriteU8,
    .write_u16 = LP_WriteU16,
    .write_u32 = LP_WriteU32,
    .write_float = LP_WriteFloat
};

const LP_Reader_Interface_t LP_Reader = {
    .init = LP_ReaderInit,
    .read_u8 = LP_ReadU8,
    .read_u16 = LP_ReadU16,
    .read_u32 = LP_ReadU32,
    .read_float = LP_ReadFloat
};