#include "headfile.h"
#include "app_lwip.h"

LP_Instance_t g_lwip_protocol;
struct tcp_pcb *g_tcp_pcb = NULL;

void Ethernet_TCP_Send(const uint8_t *data, uint16_t len) {
    if (g_tcp_pcb != NULL && g_tcp_pcb->state == ESTABLISHED) {
        if (tcp_sndbuf(g_tcp_pcb) >= len) {
            tcp_write(g_tcp_pcb, data, len, TCP_WRITE_FLAG_COPY);
            tcp_output(g_tcp_pcb);
        }
    }
}

static const LP_VTable_t g_net_vtable = {
    .tx_bytes = Ethernet_TCP_Send
};

static void tcp_server_err(void *arg, err_t err) {
    g_tcp_pcb = NULL; 
}

static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (p != NULL) {
        tcp_recved(tpcb, p->tot_len); 
        g_tcp_pcb = tpcb;             

        struct pbuf *q;
        for (q = p; q != NULL; q = q->next) {
            LP_ParseBuffer(&g_lwip_protocol, q->payload, q->len);
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
        tcp_err(newpcb, tcp_server_err); 
        g_tcp_pcb = newpcb;
        tcp_recv(newpcb, tcp_server_recv); 
    }
    return ERR_OK;
}

static void App_Ethernet_Server_Start(void) {
    struct tcp_pcb *pcb = tcp_new();
    if (pcb != NULL) {
        if (tcp_bind(pcb, IP_ADDR_ANY, 8080) == ERR_OK) {
            pcb = tcp_listen(pcb);
            tcp_accept(pcb, tcp_server_accept);
        }
    }
}

static void App_Send_Format(uint8_t cmd, const char *format, ...) {
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
    
    LP_SendPacket(&g_lwip_protocol, cmd, send_buf, w.offset);
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
            MX_LWIP_Process();   
            state = NET_STATE_SYS_TICK;
            break;

        case NET_STATE_SYS_TICK:
            lwip_timer = HAL_GetTick();
            MX_LWIP_Process();
						state = NET_STATE_IDLE;
            break;
    }
}

//回调
void HAL_ETH_RxCpltCallback(ETH_HandleTypeDef *heth){
	 eth_rx_flag=1;
}

static void lwip_init(){
	__HAL_ETH_DMA_ENABLE_IT(&heth, ETH_DMA_IT_NIS | ETH_DMA_IT_R);
}

static Lwip_Interface_t lwip={
	.init=lwip_init,
	.start=App_Ethernet_Server_Start,
	.send=App_Send_Format,
	.machine=App_Net_Engine,
};

const Lwip_Interface_t* Get_Lwip_Interface(){
	return &lwip;
}

const LP_VTable_t* Get_Lwip_Net_Vtable(){
	return &g_net_vtable;
}

LP_Instance_t* Get_Lwip_LP_Instance(){
	return &g_lwip_protocol;
}

struct tcp_pcb* Get_Lwip_Tcp_Pcb(){
	return g_tcp_pcb;
}