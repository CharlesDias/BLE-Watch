#ifndef APP_SSD1306_H_
#define APP_SSD1306_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#define DISPLAY_MSG_BUFFER_SIZE     32

void display_ssd1306_init(void);
void display_ssd1306_run_handler(void);
const char* display_ssd1306_get_msg_string(void);
void display_ssd1306_set_msg_string(const char* msg, uint16_t size);
void display_ssd1306_update_date_time(const char *date_time_string);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* APP_SSD1306_H_ */