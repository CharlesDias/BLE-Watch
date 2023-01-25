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

// GATT Device Information Service
#include "device_information_service.h"
#include <zephyr/bluetooth/services/dis.h>
#include <zephyr/settings/settings.h>

// Bluetooth include files
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/services/bas.h>

// RTC
#include "rtc_ds3231.h"

#define SLEEP_TIME_MS 1000

#define DEVICE_NAME        CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN    (sizeof(DEVICE_NAME) - 1)

#define DISPLAY_MSG_BUFFER_SIZE     32

// The devicetree node identifier for the "led0" alias.
#define LED0_NODE DT_ALIAS(led0)

#ifndef DT_NODE_HAS_STATUS(LED0_NODE, okay)
#error "Unsupported board: led0 devicetree not defined"
#endif

static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

// Register module log name
LOG_MODULE_REGISTER(Main, LOG_LEVEL_DBG);

// Bluetooth advertisement
static const struct bt_data ad[] = {
   BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
   BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

// Define custom services and characteristics
// Service: BLE Watch UUID 3C134D60-E275-406D-B6B4-BF0CC712CB7C
static struct bt_uuid_128 ble_watch_service_uuid = 
   BT_UUID_INIT_128( 0x7C, 0xCB, 0x12, 0xC7, 0x0C, 0xBF, 
                     0xB4, 0xB6,
                     0x6D, 0x40,
                     0x75, 0xE2,
                     0x60, 0x4D, 0x13, 0x3C);

// Characteristics: Display message UUID 3C134D61-E275-406D-B6B4-BF0CC712CB7C
static struct bt_uuid_128 display_charac_uuid = 
   BT_UUID_INIT_128( 0x7C, 0xCB, 0x12, 0xC7, 0x0C, 0xBF, 
                     0xB4, 0xB6,
                     0x6D, 0x40,
                     0x75, 0xE2,
                     0x61, 0x4D, 0x13, 0x3C);

static uint8_t display_msg_buffer[DISPLAY_MSG_BUFFER_SIZE] = "By: Charles Dias";

// Display read
ssize_t display_msg_read(struct bt_conn *conn,
                     const struct bt_gatt_attr *attr, void *buf,
                     uint16_t len, uint16_t offset)
{
   uint16_t size_str = (uint16_t)strlen(display_msg_buffer);
   uint16_t size = size_str > DISPLAY_MSG_BUFFER_SIZE ? DISPLAY_MSG_BUFFER_SIZE : size_str;

   LOG_DBG("Read display msg: %s", display_msg_buffer);

   return bt_gatt_attr_read(conn, attr, buf, len, offset, display_msg_buffer, size);
}

// Display write
ssize_t display_msg_write(struct bt_conn *conn,
                     const struct bt_gatt_attr *attr, const void *buf, 
                     uint16_t len, uint16_t offset, uint8_t flags)
{
   uint16_t size = len >= DISPLAY_MSG_BUFFER_SIZE ? DISPLAY_MSG_BUFFER_SIZE - 1 : len; 

   memcpy(display_msg_buffer, buf, size);
   display_msg_buffer[size] = '\0';

   LOG_DBG("Received message size %u: %s", len, display_msg_buffer);

   return len;
}

// Instantiate the Service and its characteristics
BT_GATT_SERVICE_DEFINE(
   ble_watch,

   // BLE Watch service
   BT_GATT_PRIMARY_SERVICE(&ble_watch_service_uuid),

   // Display characteristics
   // Properties: Read, Write
   BT_GATT_CHARACTERISTIC( &display_charac_uuid.uuid,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
                           BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
                           display_msg_read,
                           display_msg_write,
                           display_msg_buffer)
);

// Connected callback function
static void connected(struct bt_conn *conn, uint8_t err)
{
   if(err)
   {
      LOG_ERR("Connection failed (err 0x%02x)\n", err);
   }
   else
   {
      LOG_INF("Connection successful!");
   }
}

// Disconnected callback function
static void disconnected(struct bt_conn *conn, uint8_t reason)
{
   LOG_INF("Disconnected (reason 0x%02x)", reason);
}

// Register for connection callbacks
BT_CONN_CB_DEFINE(conn_callbacks) = {
   .connected = connected,
   .disconnected = disconnected,
};

static void bt_ready(void)
{
   int err = 0;

   // Setting the device information
   if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
      settings_load();
   }

   set_device_information_runtime();

   LOG_DBG("Bluetooth initialized");

   err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), NULL, 0);
   if (err) {
      LOG_ERR("Advertising failed to start (err %d)", err);
      return;
   }

   LOG_INF("Advertising successfully started");
}

// Implements the battery level notification
static void battery_level_notify(void)
{
   uint8_t battery_level = bt_bas_get_battery_level();

   battery_level--;

   if (!battery_level) {
      battery_level = 100U;
   }

   bt_bas_set_battery_level(battery_level);
}

void main(void)
{
   int err;
   uint32_t count = 0;
   char date_str[] = {"2023/01/20 FRI"};
   char time_str[] = {"00:00:00"};
   const struct device *display_dev;
   lv_obj_t *msg_label;
   lv_obj_t *title_label;
   lv_obj_t *date_label;
   lv_obj_t *time_label;

   LOG_DBG("Running application on board: %s\n", CONFIG_BOARD);

   if (!device_is_ready(led0.port))
   {
      LOG_ERR("Device %s is not ready.", led0.port->name);
      return;
   }

   LOG_DBG("Device %s is READY.", led0.port->name);

   err = gpio_pin_configure_dt(&led0, GPIO_OUTPUT_ACTIVE);
   if (err < 0)
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
   lv_label_set_text(msg_label, display_msg_buffer);
   lv_obj_align(msg_label, LV_ALIGN_TOP_LEFT, 0, 47);

   lv_task_handler();
   display_blanking_off(display_dev);

   // Enable Bluetooth
   err = bt_enable(NULL);
   if(err)
   {
      LOG_ERR("Bluetooth init failed (err %d)", err);
      return;
   }

   // Start advertising
   bt_ready();

   rtc_ds3231_init();

   while (1)
   {
      if ((count % 100) == 0U)
      {
         gpio_pin_toggle_dt(&led0);

         const char *rtc_msg_time = rtc_ds3231_get_last_time();
         char sub_string[32] = {0};
         memset(sub_string, 0x00, sizeof(sub_string));
         strncpy(sub_string, rtc_msg_time, 10);
         strncat(sub_string, rtc_msg_time + 19, 4);
         lv_label_set_text(date_label, sub_string);

         memset(sub_string, 0x00, sizeof(sub_string));
         strncpy(sub_string, rtc_msg_time + 11, 8);
         lv_label_set_text(time_label, sub_string);

         lv_label_set_text(msg_label, display_msg_buffer);

         /* Battery level simulation */
         battery_level_notify();
      }
      lv_task_handler();
      ++count;
      k_sleep(K_MSEC(10));
   }
}
