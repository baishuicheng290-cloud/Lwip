#ifndef APP_CAN
#define APP_CAN
#include "headfile.h"

typedef struct {
    void (*init)(void);
		void (*send)(uint8_t cmd, const char *format, ...);
		void (*machine)(void);
} Can_Interface_t;

// 只暴露驱动层的 Get 接口函数
const Can_Interface_t* Get_Can1_Interface(void);
const LP_VTable_t*     Get_Can1_Net_Vtable(void);
LP_Instance_t*         Get_Can1_LP_Instance(void);

#endif
