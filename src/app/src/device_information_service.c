#include "device_information_service.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

// Register module log name
LOG_MODULE_REGISTER(DIS, LOG_LEVEL_DBG);

static int settings_runtime_load(void);

// Setting the device information
int set_device_information_runtime(void)
{
   if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
      settings_load();
   }

   LOG_DBG("Settings device information runtime");
   return settings_runtime_load();
}

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