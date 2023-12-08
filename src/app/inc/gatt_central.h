#ifndef APP_GATT_CENTRAL_H_
#define APP_GATT_CENTRAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <string.h>

// OLED Display SSD1306
#include "display_ssd1306.h"

typedef struct display_msg
{
   char msg_buffer[DISPLAY_MSG_BUFFER_SIZE];
} display_msg_t;

extern struct k_msgq display_msg_queue;

int gatt_central_bt_start_advertising(void);
void gatt_server_battery_level_notify(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* APP_GATT_CENTRAL_H_ */