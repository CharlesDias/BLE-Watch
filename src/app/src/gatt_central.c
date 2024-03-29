#include <stdlib.h>

#include "gatt_central.h"

#include "device_information_service.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/services/bas.h>

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

// Register module log name
LOG_MODULE_REGISTER(Gatt, LOG_LEVEL_DBG);

static struct k_work advertise_work;
static struct k_work battery_level_work;

// Display variable
static uint8_t ble_message_buffer[DISPLAY_MSG_BUFFER_SIZE];

struct k_msgq display_msg_queue;

K_MSGQ_DEFINE(display_msg_queue, sizeof(display_msg_t), 2, 1);

// Bluetooth advertisement
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

// Define custom services and characteristics
// Service: BLE Watch UUID 3C134D60-E275-406D-B6B4-BF0CC712CB7C
static struct bt_uuid_128 ble_watch_service_uuid =
    BT_UUID_INIT_128(0x7C, 0xCB, 0x12, 0xC7, 0x0C, 0xBF,
                     0xB4, 0xB6,
                     0x6D, 0x40,
                     0x75, 0xE2,
                     0x60, 0x4D, 0x13, 0x3C);

// Characteristics: Display message UUID 3C134D61-E275-406D-B6B4-BF0CC712CB7C
static struct bt_uuid_128 display_charac_uuid =
    BT_UUID_INIT_128(0x7C, 0xCB, 0x12, 0xC7, 0x0C, 0xBF,
                     0xB4, 0xB6,
                     0x6D, 0x40,
                     0x75, 0xE2,
                     0x61, 0x4D, 0x13, 0x3C);

// Display read
ssize_t display_msg_read(struct bt_conn *conn,
                         const struct bt_gatt_attr *attr, void *buf,
                         uint16_t len, uint16_t offset)
{
   const char *msg_buffer = display_ssd1306_get_msg_string();
   uint16_t size = (uint16_t)strlen(msg_buffer);

   LOG_DBG("Read display msg: %s", msg_buffer);

   return bt_gatt_attr_read(conn, attr, buf, len, offset, msg_buffer, size);
}

// Display write
ssize_t display_msg_write(struct bt_conn *conn,
                          const struct bt_gatt_attr *attr, const void *buf,
                          uint16_t len, uint16_t offset, uint8_t flags)
{
   display_msg_t display_msg_buffer;
   const uint16_t buffer_size = sizeof(display_msg_buffer.msg_buffer);
   uint16_t size_str = len >= buffer_size ? buffer_size - 1 : len; 

   memcpy(display_msg_buffer.msg_buffer, buf, size_str);
   display_msg_buffer.msg_buffer[size_str] = '\0';

   while (k_msgq_put(&display_msg_queue, &display_msg_buffer, K_NO_WAIT) != 0)
   {
      /* message queue is full: purge old data & try again */
      k_msgq_purge(&display_msg_queue);
   }

   LOG_DBG("Received message size %u: %s", len, display_msg_buffer.msg_buffer);

   return len;
}

// Instantiate the Service and its characteristics
BT_GATT_SERVICE_DEFINE(
    ble_watch,

    // BLE Watch service
    BT_GATT_PRIMARY_SERVICE(&ble_watch_service_uuid),

    // Display characteristics
    // Properties: Read, Write
    BT_GATT_CHARACTERISTIC(&display_charac_uuid.uuid,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
                           BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
                           display_msg_read,
                           display_msg_write,
                           ble_message_buffer));

// Connected callback function
static void connected(struct bt_conn *conn, uint8_t err)
{
   if (err)
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
   k_work_submit(&advertise_work);
}

// Register for connection callbacks
BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

static void advertise(struct k_work *work)
{
   int err;

   bt_le_adv_stop();

   err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), NULL, 0);

   if (err)
   {
      LOG_ERR("Advertising failed to start (rc %d)", err);
      return;
   }

   LOG_INF("Advertising successfully started");
}

// Implements the battery level notification
static void battery_level_notify(struct k_work *work)
{
   uint8_t battery_level = bt_bas_get_battery_level();

   battery_level--;

   if (!battery_level)
   {
      battery_level = 100U;
   }

   bt_bas_set_battery_level(battery_level);
}

int gatt_central_bt_start_advertising(void)
{
   int err = 0;

   k_work_init(&advertise_work, advertise);
   k_work_init(&battery_level_work, battery_level_notify);

   // Enable Bluetooth
   err = bt_enable(NULL);
   if (err)
   {
      LOG_ERR("Bluetooth init failed (err %d)", err);
      return EXIT_FAILURE;
   }

   // Setting the device information
   set_device_information_runtime();

   LOG_DBG("Bluetooth initialized");

   k_work_submit(&advertise_work);
   LOG_INF("Work queue advertise successfully started");

   return 0;
}

void gatt_server_battery_level_notify(void)
{
   k_work_submit(&battery_level_work);
}