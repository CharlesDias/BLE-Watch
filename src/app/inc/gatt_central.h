#ifndef APP_GATT_CENTRAL_H_
#define APP_GATT_CENTRAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <string.h>

int gatt_central_bt_start_advertising(void);
void gatt_server_battery_level_notify(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* APP_GATT_CENTRAL_H_ */