#ifndef APP_LWIP
#define APP_LWIP
#include "headfile.h"

typedef struct {
		void (*init)(void);
    void (*start)(void);
		void (*send)(uint8_t cmd, const char *format, ...);
		void (*machine)(void);
} Lwip_Interface_t;

typedef enum {
    NET_STATE_IDLE = 0,
    NET_STATE_RECEIVE,
    NET_STATE_SYS_TICK
} Net_State_t;

const Lwip_Interface_t* Get_Lwip_Interface();
const LP_VTable_t* Get_Lwip_Net_Vtable();
LP_Instance_t* Get_Lwip_LP_Instance();
struct tcp_pcb* Get_Lwip_Tcp_Pcb();

#endif
