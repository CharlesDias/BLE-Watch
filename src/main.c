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
#include <zephyr/drivers/counter.h>
#include <zephyr/drivers/rtc/maxim_ds3231.h>

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

// Setting the device information
static int settings_runtime_load(void)
{
#if defined(CONFIG_BT_DIS_SETTINGS)
   settings_runtime_set("bt/dis/model",
            CONFIG_BT_DIS_MODEL,
            sizeof(CONFIG_BT_DIS_MODEL));
   settings_runtime_set("bt/dis/manuf",
            CONFIG_BT_DIS_MANUF,
            sizeof(CONFIG_BT_DIS_MANUF));
#if defined(CONFIG_BT_DIS_SERIAL_NUMBER)
   settings_runtime_set("bt/dis/serial",
            CONFIG_BT_DIS_SERIAL_NUMBER_STR,
            sizeof(CONFIG_BT_DIS_SERIAL_NUMBER_STR));
#endif
#if defined(CONFIG_BT_DIS_SW_REV)
   settings_runtime_set("bt/dis/sw",
            CONFIG_BT_DIS_SW_REV_STR,
            sizeof(CONFIG_BT_DIS_SW_REV_STR));
#endif
#if defined(CONFIG_BT_DIS_FW_REV)
   settings_runtime_set("bt/dis/fw",
            CONFIG_BT_DIS_FW_REV_STR,
            sizeof(CONFIG_BT_DIS_FW_REV_STR));
#endif
#if defined(CONFIG_BT_DIS_HW_REV)
   settings_runtime_set("bt/dis/hw",
            CONFIG_BT_DIS_HW_REV_STR,
            sizeof(CONFIG_BT_DIS_HW_REV_STR));
#endif
#endif
   return 0;
}

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

// Instatiate the Service and its characteristics
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

   settings_runtime_load();

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

// ###################################################################################
// RTC
// FIXME
// Use queue to share this information
const char *rtc_msg_time;

/* Format times as: YYYY-MM-DD HH:MM:SS DOW DOY */
static const char *format_time(time_t time,
               long nsec)
{
   static char buf[64];
   char *bp = buf;
   char *const bpe = bp + sizeof(buf);
   struct tm tv;
   struct tm *tp = gmtime_r(&time, &tv);

   bp += strftime(bp, bpe - bp, "%Y-%m-%d %H:%M:%S", tp);
   if (nsec >= 0) {
      bp += snprintf(bp, bpe - bp, ".%09lu", nsec);
   }
   bp += strftime(bp, bpe - bp, " %a %j", tp);
   return buf;
}

static void sec_counter_callback(const struct device *dev,
            uint8_t id,
            uint32_t ticks,
            void *ud)
{
   printk("Counter callback at %u ms, id %d, ticks %u, ud %p\n",
         k_uptime_get_32(), id, ticks, ud);
}

static void sec_alarm_handler(const struct device *dev,
               uint8_t id,
               uint32_t syncclock,
               void *ud)
{
   uint32_t now = maxim_ds3231_read_syncclock(dev);
   struct counter_alarm_cfg alarm = {
      .callback = sec_counter_callback,
      .ticks = 10,
      .user_data = ud,
   };

   printk("setting channel alarm\n");
   int rc = counter_set_channel_alarm(dev, id, &alarm);

   printk("Sec signaled at %u ms, param %p, delay %u; set %d\n",
         k_uptime_get_32(), ud, now - syncclock, rc);
}


/** Calculate the normalized result of a - b.
 *
 * For both inputs and outputs tv_nsec must be in the range [0,
 * NSEC_PER_SEC).  tv_sec may be negative, zero, or positive.
 */
void timespec_subtract(struct timespec *amb,
            const struct timespec *a,
            const struct timespec *b)
{
   if (a->tv_nsec >= b->tv_nsec) {
      amb->tv_nsec = a->tv_nsec - b->tv_nsec;
      amb->tv_sec = a->tv_sec - b->tv_sec;
   } else {
      amb->tv_nsec = NSEC_PER_SEC + a->tv_nsec - b->tv_nsec;
      amb->tv_sec = a->tv_sec - b->tv_sec - 1;
   }
}

/** Calculate the normalized result of a + b.
 *
 * For both inputs and outputs tv_nsec must be in the range [0,
 * NSEC_PER_SEC).  tv_sec may be negative, zero, or positive.
 */
void timespec_add(struct timespec *apb,
      const struct timespec *a,
      const struct timespec *b)
{
   apb->tv_nsec = a->tv_nsec + b->tv_nsec;
   apb->tv_sec = a->tv_sec + b->tv_sec;
   if (apb->tv_nsec >= NSEC_PER_SEC) {
      apb->tv_sec += 1;
      apb->tv_nsec -= NSEC_PER_SEC;
   }
}

static void min_alarm_handler(const struct device *dev,
               uint8_t id,
               uint32_t syncclock,
               void *ud)
{
   uint32_t time = 0;
   struct maxim_ds3231_syncpoint sp = { 0 };

   (void)counter_get_value(dev, &time);

   uint32_t uptime = k_uptime_get_32();
   uint8_t us = uptime % 1000U;

   uptime /= 1000U;
   uint8_t se = uptime % 60U;

   uptime /= 60U;
   uint8_t mn = uptime % 60U;

   uptime /= 60U;
   uint8_t hr = uptime;

   (void)maxim_ds3231_get_syncpoint(dev, &sp);

   uint32_t offset_syncclock = syncclock - sp.syncclock;
   uint32_t offset_s = time - (uint32_t)sp.rtc.tv_sec;
   uint32_t syncclock_Hz = maxim_ds3231_syncclock_frequency(dev);
   struct timespec adj;

   adj.tv_sec = offset_syncclock / syncclock_Hz;
   adj.tv_nsec = (offset_syncclock % syncclock_Hz)
      * (uint64_t)NSEC_PER_SEC / syncclock_Hz;

   int32_t err_ppm = (int32_t)(offset_syncclock
            - offset_s * syncclock_Hz)
         * (int64_t)1000000
         / (int32_t)syncclock_Hz / (int32_t)offset_s;
   struct timespec *ts = &sp.rtc;

   ts->tv_sec += adj.tv_sec;
   ts->tv_nsec += adj.tv_nsec;
   if (ts->tv_nsec >= NSEC_PER_SEC) {
      ts->tv_sec += 1;
      ts->tv_nsec -= NSEC_PER_SEC;
   }

	rtc_msg_time = format_time(time, -1);
   printk("%s: adj %d.%09lu, uptime %u:%02u:%02u.%03u, clk err %d ppm\n",
         rtc_msg_time,
         (uint32_t)(ts->tv_sec - time), ts->tv_nsec,
         hr, mn, se, us, err_ppm);
}

struct maxim_ds3231_alarm sec_alarm;
struct maxim_ds3231_alarm min_alarm;

static void show_counter(const struct device *ds3231)
{
   uint32_t now = 0;

   printk("\nCounter at %p\n", ds3231);
   printk("\tMax top value: %u (%08x)\n",
         counter_get_max_top_value(ds3231),
         counter_get_max_top_value(ds3231));
   printk("\t%u channels\n", counter_get_num_of_channels(ds3231));
   printk("\t%u Hz\n", counter_get_frequency(ds3231));

   printk("Top counter value: %u (%08x)\n",
         counter_get_top_value(ds3231),
         counter_get_top_value(ds3231));

   (void)counter_get_value(ds3231, &now);

   printk("Now %u: %s\n", now, format_time(now, -1));
}

/* Take the currently stored RTC time and round it up to the next
* hour.  Program the RTC as though this time had occurred at the
* moment the application booted.
*
* Subsequent reads of the RTC time adjusted based on a syncpoint
* should match the uptime relative to the programmed hour.
*/
static void set_aligned_clock(const struct device *ds3231)
{
   if (!IS_ENABLED(CONFIG_APP_SET_ALIGNED_CLOCK)) {
      return;
   }

   uint32_t syncclock_Hz = maxim_ds3231_syncclock_frequency(ds3231);
   uint32_t syncclock = maxim_ds3231_read_syncclock(ds3231);
   uint32_t now = 0;
   int rc = counter_get_value(ds3231, &now);
   uint32_t align_hour = now + 3600 - (now % 3600);

   struct maxim_ds3231_syncpoint sp = {
      .rtc = {
         .tv_sec = align_hour,
         .tv_nsec = (uint64_t)NSEC_PER_SEC * syncclock / syncclock_Hz,
      },
      .syncclock = syncclock,
   };

   struct k_poll_signal ss;
   struct sys_notify notify;
   struct k_poll_event sevt = K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL,
                        K_POLL_MODE_NOTIFY_ONLY,
                        &ss);

   k_poll_signal_init(&ss);
   sys_notify_init_signal(&notify, &ss);

   uint32_t t0 = k_uptime_get_32();

   rc = maxim_ds3231_set(ds3231, &sp, &notify);

   printk("\nSet %s at %u ms past: %d\n", format_time(sp.rtc.tv_sec, sp.rtc.tv_nsec),
         syncclock, rc);

   /* Wait for the set to complete */
   rc = k_poll(&sevt, 1, K_FOREVER);

   uint32_t t1 = k_uptime_get_32();

   /* Delay so log messages from sync can complete */
   k_sleep(K_MSEC(100));
   printk("Synchronize final: %d %d in %u ms\n", rc, ss.result, t1 - t0);

   rc = maxim_ds3231_get_syncpoint(ds3231, &sp);
   printk("wrote sync %d: %u %u at %u\n", rc,
         (uint32_t)sp.rtc.tv_sec, (uint32_t)sp.rtc.tv_nsec,
         sp.syncclock);
}

void rtc_ds3231_init(void)
{
   const struct device *const ds3231 = DEVICE_DT_GET_ONE(maxim_ds3231);

   if (!device_is_ready(ds3231)) {
      LOG_ERR("%s: device not ready.", ds3231->name);
      return;
   }

   uint32_t syncclock_Hz = maxim_ds3231_syncclock_frequency(ds3231);

   LOG_DBG("DS3231 on %s syncclock %u Hz\n", CONFIG_BOARD, syncclock_Hz);

   int rc = maxim_ds3231_stat_update(ds3231, 0, MAXIM_DS3231_REG_STAT_OSF);

   if (rc >= 0) {
      LOG_DBG("DS3231 has%s experienced an oscillator fault\n",
            (rc & MAXIM_DS3231_REG_STAT_OSF) ? "" : " not");
   } else {
      LOG_DBG("DS3231 stat fetch failed: %d\n", rc);
      return;
   }

   /* Show the DS3231 counter properties */
   show_counter(ds3231);

   /* Show the DS3231 ctrl and ctrl_stat register values */
   printk("\nDS3231 ctrl %02x ; ctrl_stat %02x\n",
         maxim_ds3231_ctrl_update(ds3231, 0, 0),
         maxim_ds3231_stat_update(ds3231, 0, 0));

   /* Test maxim_ds3231_set, if enabled */
   set_aligned_clock(ds3231);

   struct k_poll_signal ss;
   struct sys_notify notify;
   struct maxim_ds3231_syncpoint sp = { 0 };
   struct k_poll_event sevt = K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL,
                        K_POLL_MODE_NOTIFY_ONLY,
                        &ss);

   k_poll_signal_init(&ss);
   sys_notify_init_signal(&notify, &ss);

   uint32_t t0 = k_uptime_get_32();

   rc = maxim_ds3231_synchronize(ds3231, &notify);
   printk("\nSynchronize init: %d\n", rc);

   rc = k_poll(&sevt, 1, K_FOREVER);

   uint32_t t1 = k_uptime_get_32();

   k_sleep(K_MSEC(100));   /* wait for log messages */

   printk("Synchronize complete in %u ms: %d %d\n", t1 - t0, rc, ss.result);

   rc = maxim_ds3231_get_syncpoint(ds3231, &sp);
   printk("\nread sync %d: %u %u at %u\n", rc,
         (uint32_t)sp.rtc.tv_sec, (uint32_t)sp.rtc.tv_nsec,
         sp.syncclock);

   rc = maxim_ds3231_get_alarm(ds3231, 0, &sec_alarm);
   printk("\nAlarm 1 flags 0x%02X at %u: %d\n", sec_alarm.flags,
         (uint32_t)sec_alarm.time, rc);
   rc = maxim_ds3231_get_alarm(ds3231, 1, &min_alarm);
   printk("Alarm 2 flags 0x%02X at %u: %d\n", min_alarm.flags,
         (uint32_t)min_alarm.time, rc);

   /* One-shot auto-disable callback in 5 s.  The handler will
   * then use the base device counter API to schedule a second
   * alarm 10 s later.
   */
   sec_alarm.time = sp.rtc.tv_sec + 5;
   sec_alarm.flags = MAXIM_DS3231_ALARM_FLAGS_DOW
         | MAXIM_DS3231_ALARM_FLAGS_IGNDA
         | MAXIM_DS3231_ALARM_FLAGS_IGNHR
         | MAXIM_DS3231_ALARM_FLAGS_IGNMN
         | MAXIM_DS3231_ALARM_FLAGS_IGNSE;
   sec_alarm.handler = min_alarm_handler;
   // sec_alarm.user_data = &sec_alarm;

   printk("Min Sec base time: %s\n", format_time(sec_alarm.time, -1));

   /* Repeating callback at rollover to a new minute. */
   min_alarm.time = sec_alarm.time;
   min_alarm.flags = 0
         | MAXIM_DS3231_ALARM_FLAGS_IGNDA
         | MAXIM_DS3231_ALARM_FLAGS_IGNHR
         | MAXIM_DS3231_ALARM_FLAGS_IGNMN
         | MAXIM_DS3231_ALARM_FLAGS_IGNSE;
   min_alarm.handler = min_alarm_handler;

   rc = maxim_ds3231_set_alarm(ds3231, 0, &sec_alarm);
   printk("Set sec alarm 0x%02X at %u ~ %s: %d\n", sec_alarm.flags,
         (uint32_t)sec_alarm.time, format_time(sec_alarm.time, -1), rc);

   rc = maxim_ds3231_set_alarm(ds3231, 1, &min_alarm);
   printk("Set min alarm flags 0x%02X at %u ~ %s: %d\n", min_alarm.flags,
         (uint32_t)min_alarm.time, format_time(min_alarm.time, -1), rc);

   printk("%u ms in: get alarms: %d %d\n", k_uptime_get_32(),
         maxim_ds3231_get_alarm(ds3231, 0, &sec_alarm),
         maxim_ds3231_get_alarm(ds3231, 1, &min_alarm));
   if (rc >= 0) {
      printk("Sec alarm flags 0x%02X at %u ~ %s\n", sec_alarm.flags,
            (uint32_t)sec_alarm.time, format_time(sec_alarm.time, -1));

      printk("Min alarm flags 0x%02X at %u ~ %s\n", min_alarm.flags,
            (uint32_t)min_alarm.time, format_time(min_alarm.time, -1));
   }
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
