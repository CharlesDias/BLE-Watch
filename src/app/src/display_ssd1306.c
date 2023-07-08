#include "display_ssd1306.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/display.h>
#include <lvgl.h>
#include <string.h>

// Register module log name
LOG_MODULE_REGISTER(DISPLAY, LOG_LEVEL_DBG);

static uint8_t message_buffer[DISPLAY_MSG_BUFFER_SIZE] = "By: Charles Dias";
static char date_str[] = {"2023/01/20 FRI"};
static char time_str[] = {"00:00:00"};
static const struct device *display_dev;
static lv_obj_t *msg_label;
static lv_obj_t *title_label;
static lv_obj_t *date_label;
static lv_obj_t *time_label;


void display_ssd1306_init(void)
{
   display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
   if (!device_is_ready(display_dev))
   {
      LOG_ERR("Device not ready, aborting test");
      return;
   }

   if (IS_ENABLED(CONFIG_LV_Z_POINTER_KSCAN))
   {
      lv_obj_t *hello_world_button;

      hello_world_button = lv_btn_create(lv_scr_act());
      lv_obj_align(hello_world_button, LV_ALIGN_CENTER, 0, 0);
      title_label = lv_label_create(hello_world_button);
   }
   else
   {
      title_label = lv_label_create(lv_scr_act());
   }

   lv_label_set_text(title_label, "BLE Watch");
   lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 0);

   date_label = lv_label_create(lv_scr_act());
   lv_label_set_text(date_label, date_str);
   lv_obj_align(date_label, LV_ALIGN_TOP_LEFT, 12, 15);
   
   time_label = lv_label_create(lv_scr_act());
   lv_label_set_text(time_label, time_str);
   lv_obj_align(time_label, LV_ALIGN_TOP_LEFT, 32, 30);

   msg_label = lv_label_create(lv_scr_act());
   lv_label_set_text(msg_label, message_buffer);
   lv_obj_align(msg_label, LV_ALIGN_TOP_LEFT, 0, 47);

   lv_task_handler();
   display_blanking_off(display_dev);
}

void display_ssd1306_run_handler(void)
{
   lv_task_handler();
}

const char* display_ssd1306_get_msg_string(void)
{
   return message_buffer;
}

void display_ssd1306_set_msg_string(const char* msg, uint16_t size)
{
   uint16_t size_str = size >= DISPLAY_MSG_BUFFER_SIZE ? DISPLAY_MSG_BUFFER_SIZE - 1 : size; 

   memcpy(message_buffer, msg, size_str);
   message_buffer[size_str] = '\0';
}

// Time format message YYYY-MM-DD HH:MM:SS DOW DOY
void display_ssd1306_update_date_time(const char *date_time_string)
{
   char* temp_string = date_time_string;
   char const* date_substring = strtok_r(temp_string, " ", &temp_string);
   char const* time_substring = strtok_r(temp_string, " ", &temp_string);
   char const* dow_substring = strtok_r(temp_string, " ", &temp_string);

   if(date_substring == NULL || time_substring == NULL || dow_substring == NULL)
   {
      LOG_ERR("Date time string invalid: %s", date_time_string);
      return;
   }

   char date_dow_string[DISPLAY_MSG_BUFFER_SIZE] = {0};
   size_t free_space = sizeof(date_dow_string) - 1;

   memset(date_dow_string, 0x00, sizeof(date_dow_string));
   strncpy(date_dow_string, date_substring, free_space);

   free_space = free_space - strlen(date_dow_string);
   strncat(date_dow_string, " ", free_space);

   free_space = free_space - strlen(date_dow_string);
   strncat(date_dow_string, dow_substring, free_space);

   lv_label_set_text(date_label, date_dow_string);

   lv_label_set_text(time_label, time_substring);

   lv_label_set_text(msg_label, message_buffer);
}