#include "main.h"          
#include "lwip/tcp.h"        
#include "lwip/pbuf.h"        
#include "Com_protocol.h"    
#include "app_com.h"      
#include "lwip.h"
#include <string.h>
#include <stdarg.h> 
#include "app_lwip.h"
#include "app_can.h"

#define lwip_protocol (Get_Lwip_LP_Instance())
#define can1_protocol (Get_Can1_LP_Instance())

volatile Vision_Target_t current_vision = {0};

static void Handle_Coordinate(const uint8_t *payload, uint8_t len) {
    LP_Reader_t r;
    LP_Reader.init(&r, payload, len);

    current_vision.is_found  = LP_Reader.read_u8(&r);
    current_vision.target_x  = LP_Reader.read_u16(&r);
    current_vision.target_y  = LP_Reader.read_u16(&r);
    current_vision.timestamp = HAL_GetTick(); 
		printf("坐标数据%d %d\n",current_vision.target_x,current_vision.target_y);
}

void App_Init(void) {
    LP_Init(lwip_protocol, Get_Lwip_Net_Vtable());
		LP_Init(can1_protocol,Get_Can1_Net_Vtable());

		//lwip
    LP_RegisterHandler(lwip_protocol, CMD_COORDINATE, COORD_PAYLOAD_LEN, Handle_Coordinate);
		
		//can1
		LP_RegisterHandler(can1_protocol, CMD_COORDINATE, COORD_PAYLOAD_LEN, Handle_Coordinate);
	
	
		Get_Lwip_Interface()->start();
}

static Com_Interface_t com = {
    .init = App_Init,
};

const Com_Interface_t* Get_Com_Interface(){
	return &com;
}
