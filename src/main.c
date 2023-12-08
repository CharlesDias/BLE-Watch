/*
 * Copyright (c) 2023 Charles Dias.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "display_ssd1306.h"
#include "gatt_central.h"
#include "rtc_ds3231.h"

// Register module log name
LOG_MODULE_REGISTER(Main, LOG_LEVEL_DBG);

#define STACKSIZE 2048
#define PRIORITY 7

typedef struct rtc_msg
{
   uint8_t msg_buffer[RTC_MSG_BUFFER_SIZE];
} rtc_msg_t;

struct k_msgq rtc_msg_queue;

K_MSGQ_DEFINE(rtc_msg_queue, sizeof(rtc_msg_t), 2, 1);

// The devicetree node identifier for the "led0" alias.
#define LED0_NODE DT_ALIAS(led0)

#ifndef DT_NODE_HAS_STATUS(LED0_NODE, okay)
#error "Unsupported board: led0 devicetree not defined"
#endif

static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

void rtc_thread(void)
{
   rtc_msg_t msg_buffer;

   rtc_ds3231_init();

   while (1)
   {
      const char *rtc_msg_time = rtc_ds3231_get_last_time();
      strncpy(msg_buffer.msg_buffer, rtc_msg_time, RTC_MSG_BUFFER_SIZE);

      while (k_msgq_put(&rtc_msg_queue, &msg_buffer, K_NO_WAIT) != 0)
      {
         /* message queue is full: purge old data & try again */
         k_msgq_purge(&rtc_msg_queue);
      }

      k_sleep(K_SECONDS(1));
   }
}

void display_thread(void)
{
   rtc_msg_t rtc_msg_buffer;
   display_msg_t display_msg_buffer;

   display_ssd1306_init();

   while (1)
   {
      // Handle RTC messages
      if (k_msgq_get(&rtc_msg_queue, &rtc_msg_buffer, K_NO_WAIT) == 0)
      {
         display_ssd1306_update_date_time(rtc_msg_buffer.msg_buffer);
      }

      // Handle messages from BLE work queue
      if (k_msgq_get(&display_msg_queue, &display_msg_buffer, K_NO_WAIT) == 0)
      {
         display_ssd1306_set_msg_string(display_msg_buffer.msg_buffer, (uint16_t)strlen(display_msg_buffer.msg_buffer));
      }

      display_ssd1306_run_handler();

      k_sleep(K_MSEC(500));
   }
}

K_THREAD_DEFINE(rtc_thread_id, STACKSIZE, rtc_thread, NULL, NULL, NULL, PRIORITY, 0, 100);
K_THREAD_DEFINE(display_thread_id, STACKSIZE, display_thread, NULL, NULL, NULL, PRIORITY, 0, 200);

int main(void)
{
   int err;

   LOG_INF("Running application on board: %s.", CONFIG_BOARD);
   LOG_INF("Build time: " __DATE__ " " __TIME__);
   k_sleep(K_SECONDS(2));

   if (!device_is_ready(led0.port))
   {
      LOG_ERR("Device %s is not ready.", led0.port->name);
      return EXIT_FAILURE;
   }

   LOG_DBG("Device %s is READY.", led0.port->name);

   err = gpio_pin_configure_dt(&led0, GPIO_OUTPUT_ACTIVE);
   if (err < 0)
   {
      LOG_ERR("It was not possible configure the device %s.", led0.port->name);
      return EXIT_FAILURE;
   }

   // Start advertising
   gatt_central_bt_start_advertising();

   while (1)
   {
      gpio_pin_toggle_dt(&led0);

      // Battery level simulation
      gatt_server_battery_level_notify();

      k_sleep(K_SECONDS(1));
   }

   return EXIT_SUCCESS;
}
