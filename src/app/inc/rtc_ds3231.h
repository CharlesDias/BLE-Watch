#ifndef APP_RTC_DS3231_H_
#define APP_RTC_DS3231_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

void rtc_ds3231_init(void);
const char* rtc_ds3231_get_last_time(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* APP_RTC_DS3231_H_ */