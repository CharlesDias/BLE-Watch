#include "rtc_ds3231.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/counter.h>
#include <zephyr/drivers/rtc/maxim_ds3231.h>

// Register module log name
LOG_MODULE_REGISTER(RTC, LOG_LEVEL_DBG);


static struct maxim_ds3231_alarm sec_alarm;
static struct maxim_ds3231_alarm min_alarm;

// FIXME
// Use queue to share this information
static const char *rtc_msg_time;


static const char *format_time(time_t time, long nsec);
static void min_alarm_handler(const struct device *dev, uint8_t id, uint32_t syncclock, void *ud);
static void show_counter(const struct device *ds3231);
static void set_aligned_clock(const struct device *ds3231);


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

const char* rtc_ds3231_get_last_time(void)
{
   return rtc_msg_time;
}

/* Format times as: YYYY-MM-DD HH:MM:SS DOW DOY */
static const char *format_time(time_t time,
               long nsec)
{
   static char buf[RTC_MSG_BUFFER_SIZE];
   char *bp = buf;
   char const *const bpe = bp + sizeof(buf);
   struct tm tv;
   struct tm const *tp = gmtime_r(&time, &tv);

   bp += strftime(bp, bpe - bp, "%Y-%m-%d %H:%M:%S", tp);
   if (nsec >= 0) {
      bp += snprintf(bp, bpe - bp, ".%09lu", nsec);
   }
   // This is a false-positive. The bp value is a pointer to buf variable.
   bp += strftime(bp, bpe - bp, " %a %j", tp);
   return buf;
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
   uint16_t us = uptime % 1000U;

   uptime /= 1000U;
   uint8_t se = uptime % 60U;

   uptime /= 60U;
   uint8_t mn = uptime % 60U;

   uptime /= 60U;
   uint8_t hr = (uint8_t)uptime;

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

