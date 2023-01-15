/*
 * Copyright (c) 2023 Charles Dias.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <lvgl.h>
#include <stdio.h>
#include <string.h>

#define SLEEP_TIME_MS 1000

// The devicetree node identifier for the "led0" alias.
#define LED0_NODE DT_ALIAS(led0)

static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

// Register module log name
LOG_MODULE_REGISTER(Main, LOG_LEVEL_DBG);

void main(void)
{
   int ret;
   uint32_t count = 0;
   char count_str[] = {"2023/01/15 00:00"};
   const struct device *display_dev;
   lv_obj_t *author_label;
   lv_obj_t *title_label;
   lv_obj_t *timer_label;

   LOG_DBG("Running application on board: %s\n", CONFIG_BOARD);

   if (!device_is_ready(led0.port))
   {
      LOG_ERR("Device %s is not ready.", led0.port->name);
      return;
   }

   LOG_DBG("Device %s is READY.", led0.port->name);

   ret = gpio_pin_configure_dt(&led0, GPIO_OUTPUT_ACTIVE);
   if (ret < 0)
   {
      LOG_ERR("It was not possible configure the device %s.", led0.port->name);
      return;
   }

   display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
   if (!device_is_ready(display_dev))
   {
      LOG_ERR("Device not ready, aborting test");
      return;
   }

   author_label = lv_label_create(lv_scr_act());
   lv_label_set_text(author_label, "By: Charles Dias");
   lv_obj_align(author_label, LV_ALIGN_TOP_LEFT, 0, 0);

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

   lv_label_set_text(title_label, "BLE Watch!");
   lv_obj_align(title_label, LV_ALIGN_CENTER, 0, 0);

   timer_label = lv_label_create(lv_scr_act());
   lv_obj_align(timer_label, LV_ALIGN_BOTTOM_LEFT, 8, 0);
   
   lv_task_handler();
   display_blanking_off(display_dev);

   while (1)
   {
      if ((count % 100) == 0U)
      {
         gpio_pin_toggle_dt(&led0);

         if(count >= 6000U)
         {
            count = 0;
         }
         snprintf(count_str, sizeof(count_str), "2023/01/15 00:%02d", count / 100U);
         lv_label_set_text(timer_label, count_str);
      }
      lv_task_handler();
      ++count;
      k_sleep(K_MSEC(10));
   }
}
