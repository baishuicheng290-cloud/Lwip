#ifndef APP_COM
#define APP_COM
#include <stdint.h>

#define CMD_COORDINATE   0x10

#define COORD_PAYLOAD_LEN    5

typedef struct {
    uint8_t  is_found;
    uint16_t target_x;
    uint16_t target_y;
    uint32_t timestamp;
} Vision_Target_t;

typedef struct {
    void (*init)(void);
} Com_Interface_t;

const Com_Interface_t* Get_Com_Interface();

#endif