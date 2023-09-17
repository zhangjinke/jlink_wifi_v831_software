/**
 * \file
 * \brief LED
 *
 * \internal
 * \par Modification history
 * - 1.00 23-02-01  zjk, first implementation
 * \endinternal
 */

#include "led.h"
#include "cfg.h"
#include "file.h"
#include "gpio.h"
#include "main.h"
#include "systick.h"
#include "zlog.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/timerfd.h>
#include <termios.h>
#include <unistd.h>

/*******************************************************************************
  宏定义
*******************************************************************************/

/*******************************************************************************
  本地全局变量声明
*******************************************************************************/

/*******************************************************************************
  本地全局变量定义
*******************************************************************************/

//zlog 类别
static zlog_category_t *__gp_zlogc = NULL;

//互斥量
static pthread_mutex_t __g_mutex;

//是否初始化
static bool __g_is_init = false;

static char __g_led_gpio_name[LED_MAX][64] = {0}; //LED 名称

/*******************************************************************************
  内部函数定义
*******************************************************************************/

/**
 * \brief 配置读取
 */
static int __cfg_read (void)
{
  int err;

  err = cfg_str_get("led", "state_name", __g_led_gpio_name[LED_STATE], sizeof(__g_led_gpio_name[LED_STATE]), "state");
  if (err != 0)
  {
    cfg_str_set("led", "state_name", __g_led_gpio_name[LED_STATE]);
  }

  err = cfg_str_get("led", "error_name", __g_led_gpio_name[LED_ERROR], sizeof(__g_led_gpio_name[LED_ERROR]), "error");
  if (err != 0)
  {
    cfg_str_set("led", "error_name", __g_led_gpio_name[LED_ERROR]);
  }

  return 0;
}

/*******************************************************************************
  外部函数定义
*******************************************************************************/

/**
 * \brief LED 触发器设置
 */
int led_trigger_set (enum led led, const char *p_trigger)
{
  char   path[128] = {0};
  size_t len       = 0;
  int    err       = 0;

  if ((led < 0) || (led >= LED_MAX))
  {
    zlog_error(__gp_zlogc, "led %d error: unknown", led);
    err = -1;
    goto err;
  }

  snprintf(path, sizeof(path), "/sys/class/leds/%s/trigger", __g_led_gpio_name[led]);
  len = strlen(p_trigger);
  pthread_mutex_lock(&__g_mutex);
  if (file_write(path, p_trigger, len, O_WRONLY) != len)
  {
    zlog_error(__gp_zlogc, "write %s error: %s", path, strerror(errno));
    err = -1;
    goto err_unlock;
  }

err_unlock:
  pthread_mutex_unlock(&__g_mutex);
err:
  return err;
}

/**
 * \brief led 亮度设置
 */
int led_brightness_set (enum led led, uint32_t brightness)
{
  char   path[128] = {0};
  char   str[128]  = {0};
  size_t len       = 0;
  int    err       = 0;

  if ((led < 0) || (led >= LED_MAX))
  {
    zlog_error(__gp_zlogc, "led %d error: unknown", led);
    err = -1;
    goto err;
  }

  snprintf(path, sizeof(path), "/sys/class/leds/%s/brightness", __g_led_gpio_name[led]);
  len = snprintf(str, sizeof(str), "%u", brightness);
  pthread_mutex_lock(&__g_mutex);
  if (file_write(path, str, len, O_WRONLY) != len)
  {
    zlog_error(__gp_zlogc, "write %s error: %s", path, strerror(errno));
    err = -1;
    goto err_unlock;
  }

err_unlock:
  pthread_mutex_unlock(&__g_mutex);
err:
  return err;
}

/**
 * \brief led 定时器设置
 */
int led_timer_set (enum led led, uint32_t delay_on, uint32_t delay_off)
{
  char   path[128] = {0};
  char   str[128]  = {0};
  size_t len       = 0;
  int    err       = 0;

  if ((led < 0) || (led >= LED_MAX))
  {
    zlog_error(__gp_zlogc, "led %d error: unknown", led);
    err = -1;
    goto err;
  }

  snprintf(path, sizeof(path), "/sys/class/leds/%s/delay_on", __g_led_gpio_name[led]);
  len = snprintf(str, sizeof(str), "%u", delay_on);
  pthread_mutex_lock(&__g_mutex);
  if (file_write(path, str, len, O_WRONLY) != len)
  {
    zlog_error(__gp_zlogc, "write %s error: %s", path, strerror(errno));
    err = -1;
    goto err_unlock;
  }

  snprintf(path, sizeof(path), "/sys/class/leds/%s/delay_off", __g_led_gpio_name[led]);
  len = snprintf(str, sizeof(str), "%u", delay_off);
  if (file_write(path, str, len, O_WRONLY) != len)
  {
    zlog_error(__gp_zlogc, "write %s error: %s", path, strerror(errno));
    err = -1;
    goto err_unlock;
  }

err_unlock:
  pthread_mutex_unlock(&__g_mutex);
err:
  return err;
}

/**
 * \brief LED 初始化
 */
int led_init (void)
{
  int i   = 0;
  int err = 0;

  if (__g_is_init)
  { //已初始化
    return 0;
  }

  __gp_zlogc = zlog_get_category("led");

  //获取配置信息
  __cfg_read();

  //熄灭所有 LED
  for (i = 0; i < LED_MAX; i++)
  {
    led_trigger_set((enum led)i, LED_TRIGGER_NONE);
  }

  if (pthread_mutex_init(&__g_mutex, NULL) != 0)
  {
    zlog_fatal(__gp_zlogc, "mutex init error");
    err = -1;
    goto err;
  }
  __g_is_init = true;
  goto err;

  pthread_mutex_destroy(&__g_mutex);
err:
  return err;
}

/**
 * \brief LED 解初始化
 */
int led_deinit (void)
{
  int i;

  if (!__g_is_init)
  {
    return 0;
  }

  //熄灭所有 LED
  for (i = 0; i < LED_MAX; i++)
  {
    led_trigger_set((enum led)i, LED_TRIGGER_NONE);
  }

  pthread_mutex_destroy(&__g_mutex);
  __g_is_init = false;

  return 0;
}

/* end of file */
