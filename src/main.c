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

#define SLEEP_TIME_MS 1000

// The devicetree node identifier for the "led0" alias.
#define LED0_NODE DT_ALIAS(led0)

#ifndef DT_NODE_HAS_STATUS(LED0_NODE, okay)
#error "Unsupported board: led0 devicetree not defined"
#endif

static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

// Register module log name
LOG_MODULE_REGISTER(Main, LOG_LEVEL_DBG);

int main(void)
{
   int err;
   uint32_t count = 0;

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

   rtc_ds3231_init();

   display_ssd1306_init();

   // Start advertising
   gatt_central_bt_start_advertising();

   while (1)
   {
      // FIXME
      // Improve this code to using tasks
      if ((count % 100) == 0U)
      {
         gpio_pin_toggle_dt(&led0);

         const char *rtc_msg_time = rtc_ds3231_get_last_time();
         display_ssd1306_update_date_time(rtc_msg_time);

         // Battery level simulation
         gatt_server_battery_level_notify();
      }

      display_ssd1306_run_handler();

      ++count;
      k_sleep(K_MSEC(10));
   }

   return EXIT_SUCCESS;
}
