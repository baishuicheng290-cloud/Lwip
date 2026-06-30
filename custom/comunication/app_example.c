#include "main.h"             // 引入 STM32 HAL 库 (内含 HAL_GetTick)
#include "lwip/tcp.h"         // 引入 LwIP TCP 头文件
#include "lwip/pbuf.h"        // 引入 LwIP 内存缓冲头文件
#include "Com_protocol.h"     // 引入协议栈头文件
#include "app_example.h"      // 引入本应用头文件
#include "lwip.h"
#include <string.h>
#include <stdarg.h> // 确保引入可变参数头文件

/* ==================== 1. 核心变量定义 ==================== */

volatile Vision_Target_t current_vision = {0}; // 仅保留这一个核心接收结构体

LP_Instance_t g_uart1_protocol;       // 协议栈控制句柄
struct tcp_pcb *g_tcp_pcb = NULL;     // 全局活跃的 TCP 连接指针

/* ==================== 2. 底层以太网发送接口 ==================== */

void Ethernet_TCP_Send(const uint8_t *data, uint16_t len) {
    if (g_tcp_pcb != NULL && g_tcp_pcb->state == ESTABLISHED) {
        if (tcp_sndbuf(g_tcp_pcb) >= len) {
            tcp_write(g_tcp_pcb, data, len, TCP_WRITE_FLAG_COPY);
            tcp_output(g_tcp_pcb);
        }
    }
}

// 虚函数表 (绑定网口发送)
static const LP_VTable_t g_net_vtable = {
    .tx_bytes = Ethernet_TCP_Send
};

/* ==================== 3. 唯一的业务接收回调 (坐标接收) ==================== */

static void Handle_Coordinate(const uint8_t *payload, uint8_t len) {
    LP_Reader_t r;
    LP_Reader.init(&r, payload, len);

    // 顺序读取出包里的：是否找到(1字节)、X坐标(2字节)、Y坐标(2字节)
    current_vision.is_found  = LP_Reader.read_u8(&r);
    current_vision.target_x  = LP_Reader.read_u16(&r);
    current_vision.target_y  = LP_Reader.read_u16(&r);
    current_vision.timestamp = HAL_GetTick(); // 使用 HAL 库自带的毫秒时间戳函数
		printf("坐标数据：%d %d\n",current_vision.target_x,current_vision.target_y);
}

/* ==================== 4. TCP 接收与连接管理 ==================== */

static void tcp_server_err(void *arg, err_t err) {
    g_tcp_pcb = NULL; // 异常断开，清理句柄
}

static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (p != NULL) {
        tcp_recved(tpcb, p->tot_len); // 确认接收
        g_tcp_pcb = tpcb;             // 更新当前连接

        // 核心：网口收到的字节流直接注入协议栈
        struct pbuf *q;
        for (q = p; q != NULL; q = q->next) {
            LP_ParseBuffer(&g_uart1_protocol, q->payload, q->len);
        }

        pbuf_free(p);
    } 
    else if (err == ERR_OK) {
        g_tcp_pcb = NULL;
        tcp_close(tpcb);
    }
    return ERR_OK;
}

static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err) {
    if (err == ERR_OK && newpcb != NULL) {
        tcp_err(newpcb, tcp_server_err); // 注册错误回调
        g_tcp_pcb = newpcb;
        tcp_recv(newpcb, tcp_server_recv); // 注册接收回调
    }
    return ERR_OK;
}

// 开启 TCP 服务器 (监听 8080 端口)
void App_Ethernet_Server_Start(void) {
    struct tcp_pcb *pcb = tcp_new();
    if (pcb != NULL) {
        if (tcp_bind(pcb, IP_ADDR_ANY, 8080) == ERR_OK) {
            pcb = tcp_listen(pcb);
            tcp_accept(pcb, tcp_server_accept);
        }
    }
}

/* ==================== 5. 系统核心初始化 API ==================== */

void App_Init(void) {
    // 1. 初始化协议栈并绑定网卡发送
    LP_Init(&g_uart1_protocol, &g_net_vtable);

    // 2. 仅仅注册这一个坐标指令(CMD_COORDINATE)，长度为 5 字节
    LP_RegisterHandler(&g_uart1_protocol, CMD_COORDINATE, COORD_PAYLOAD_LEN, Handle_Coordinate);
	
		App_Ethernet_Server_Start();
}

void App_Send_Format(uint8_t cmd, const char *format, ...) {
    uint8_t send_buf[LP_MAX_PAYLOAD_LEN]; // 临时打包缓冲区
    LP_Writer_t w;
    va_list args;
    
    // 1. 初始化写入器
    LP_Writer.init(&w, send_buf, sizeof(send_buf));
    
    // 2. 开始解析可变参数
    va_start(args, format);
    
    for (const char *p = format; *p != '\0'; p++) {
        switch (*p) {
            case 'b': // 写入 1 字节 (uint8_t)
                // C语言可变参数规定：char/short 在传递时会被自动提升为 int，故用 int 接收
                LP_Writer.write_u8(&w, (uint8_t)va_arg(args, int)); 
                break;
                
            case 'w': // 写入 2 字节 (uint16_t)
                // short 在传递时会被自动提升为 int，故用 int 接收
                LP_Writer.write_u16(&w, (uint16_t)va_arg(args, int));
                break;
                
            case 'd': // 写入 4 字节 (uint32_t)
                LP_Writer.write_u32(&w, va_arg(args, uint32_t));
                break;
                
            case 'f': // 写入 4 字节浮点数 (float)
                // float 在传递时会被自动提升为 double，故用 double 接收
                LP_Writer.write_float(&w, (float)va_arg(args, double));
                break;
                
            default:
                break; // 忽略未知格式
        }
    }
    va_end(args);
    
    // 3. 调用协议栈发送
    LP_SendPacket(&g_uart1_protocol, cmd, send_buf, w.offset);
}

volatile static uint8_t eth_rx_flag=0;

static void App_Net_Engine(void) {
    static uint32_t lwip_timer = 0;
		static Net_State_t state = NET_STATE_IDLE;
	
    switch (state)
    {
        case NET_STATE_IDLE:
            if (HAL_GetTick() - lwip_timer >= 50)state = NET_STATE_SYS_TICK;
						else if(eth_rx_flag)state=NET_STATE_RECEIVE;
            break;

        case NET_STATE_RECEIVE:
						eth_rx_flag=0;
            MX_LWIP_Process();     // 纯接收搬运包
            state = NET_STATE_SYS_TICK;    // 接收完顺路去维护时钟
            break;

        case NET_STATE_SYS_TICK:
            lwip_timer = HAL_GetTick();
            MX_LWIP_Process();
						state = NET_STATE_IDLE;
            break;
    }
}

// 留空以兼容外层结构体定义
void App_irq(uint8_t received_char) {}
	
void HAL_ETH_RxCpltCallback(ETH_HandleTypeDef *heth){
	 eth_rx_flag=1;
}

Com_Interface_t com = {
    .init = App_Init,
		.send = App_Send_Format,
		.machine= App_Net_Engine,
    .irq  = App_irq,
};