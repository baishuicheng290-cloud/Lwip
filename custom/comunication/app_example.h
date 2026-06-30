#ifndef APP_EXAMPLE
#define APP_EXAMPLE

#include <stdint.h>

#define CMD_COORDINATE   0x10
#define CMD_QRCODE       0x11
#define CMD_COLOR        0x12

#define COORD_PAYLOAD_LEN    5

typedef enum {
    NET_STATE_IDLE = 0,
    NET_STATE_RECEIVE,
    NET_STATE_SYS_TICK
} Net_State_t;

typedef struct {
    uint8_t  is_found;
    uint16_t target_x;
    uint16_t target_y;
    uint32_t timestamp;
} Vision_Target_t;

typedef struct {
    uint8_t  is_valid;
    char     data[21];
    uint32_t timestamp;
} QRCode_Data_t;

typedef enum {
    CAM_COLOR_NONE    = 0x00,
    CAM_COLOR_RED     = 0x01,
    CAM_COLOR_BLUE    = 0x02,
    CAM_COLOR_GREEN   = 0x03,
    CAM_COLOR_YELLOW  = 0x04,
    CAM_COLOR_BLACK   = 0x05,
    CAM_COLOR_WHITE   = 0x06
} ColorID_t;

typedef struct {
    uint8_t  is_valid;
    ColorID_t color_id;
    uint8_t  confidence;
    uint32_t timestamp;
} Color_Data_t;

typedef struct {
    void (*init)(void);
		void (*send)(uint8_t cmd, const char *format, ...);
		void (*machine)(void);
    void (*irq)(uint8_t received_char);	
} Com_Interface_t;

extern Com_Interface_t com;
extern volatile Vision_Target_t current_vision;
extern volatile QRCode_Data_t   current_qrcode;
extern volatile Color_Data_t    current_color;

#endif