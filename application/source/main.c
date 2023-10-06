/**
 * \file
 * \brief 主程序
 *
 * \internal
 * \par Modification history
 * - 1.00 22-05-05  zjk, first implementation
 * \endinternal
 */

#include "main.h"
#include "cfg.h"
#include "config.h"
#include "crc.h"
#include "file.h"
#include "jlink_ctl.h"
#include "key.h"
#include "led.h"
#include "udp_ctl.h"
#include "libconfig.h"
#include "process.h"
#include "rngbuf.h"
#include "str.h"
#include "systick.h"
#include "utilities.h"
#include "web.h"
#include "wifi_ctl.h"
#include "zlog.h"
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/*******************************************************************************
  宏定义
*******************************************************************************/

/*******************************************************************************
  本地全局变量声明
*******************************************************************************/

//状态枚举
enum main_state
{
  MAIN_STATE_NO_INIT = 0, //未初始化态
  MAIN_STATE_USB,         //USB J-Link 态
  MAIN_STATE_WIFI_STA,    //WiFi STA 态
  MAIN_STATE_WIFI_AP,     //WiFi AP 态
  MAIN_STATE_WAIT,        //等待切换态
  MAIN_STATE_MAX,
};

/*******************************************************************************
  本地全局变量定义
*******************************************************************************/

//zlog 类别
static zlog_category_t *__gp_zlogc = NULL;

//线程是否需要继续执行
static volatile bool __g_thread_run = true;

//互斥量
static pthread_mutex_t __g_mutex = {0};

//初始化屏障
static pthread_barrier_t __g_init_barrier = {0};

static volatile bool __g_cfg_update = false; //配置更新标记
static int           __g_state_last = 0;     //最近一次的状态

//状态
static enum main_state __g_state = MAIN_STATE_NO_INIT;

//STA 模式下，最近一次的 IP 地址
static struct in_addr __g_ip_addr = {0};

/*******************************************************************************
  内部函数定义
*******************************************************************************/

/**
 * \brief 切换工作目录为程序所在目录
 */
static int __work_dir_change (void)
{
  char     path[PATH_MAX];
  char    *p_dir = NULL;
  ssize_t  size;

  size = readlink("/proc/self/exe", path, sizeof(path));
  if ((size <= 0) || (size >= sizeof(path)))
  {
    return -1;
  }
  path[size] = '\0';
  p_dir = dirname(path);
  if (chdir(p_dir) != 0)
  {
    fprintf(stderr, "chdir %s error: %s\n", p_dir, strerror(errno));
  }

  return 0;
}

/**
 * \brief 信号回调函数
 */
static void __singnl_callback (int exit_status)
{
  if (__g_thread_run)
  {
    zlog_info(__gp_zlogc, "Terminating...");
    __g_thread_run = false;
  }
}

/**
 * \brief zlog 初始化
 */
static int __zlog_init (void)
{
  int ret;

  ret = zlog_init("../etc/zlog.conf");
  if (ret)
  {
    printf("zlog init error\n");
    return -1;
  }

  __gp_zlogc = zlog_get_category("main");
  if (NULL == __gp_zlogc)
  {
    printf("zlog get category main failed\n");
    zlog_fini();
    return -2;
  }

  zlog_info(__gp_zlogc, "zlog init ok");

  return 0;
}

/**
 * \brief 配置读取
 */
static int __cfg_read (void)
{
  int err = 0;

  err = cfg_int_get("main", "state_last", &__g_state_last, 0);
  if (err != 0)
  {
    cfg_int_set("main", "state_last", __g_state_last);
  }

  return 0;
}

/**
 * \brief 电池信息打印
 */
static void __bat_info_print (void)
{
  char            buf[64]      = {0};
  int             bat_capacity = 0;
  float           bat_voltage  = 0;
  uint32_t        systick      = systick_get();
  static uint32_t s_tick       = -(3 * 60 * 1000);

  if ((systick - s_tick) < (3 * 60 * 1000))
  {
    return;
  }
  s_tick = systick;

  if (file_read("/sys/class/power_supply/battery/capacity", buf, sizeof(buf), O_RDONLY) > 0)
  {
    bat_capacity = atoi(buf);
    if (bat_capacity >= 0)
    {
      if (file_read("/sys/class/power_supply/battery/voltage_now", buf, sizeof(buf), O_RDONLY) > 0)
      {
        bat_voltage = atoi(buf) / 1000000.0f;
      }
      zlog_info(__gp_zlogc, "battery capacity: %d%% voltage: %.3fV ", bat_capacity, bat_voltage);
    }
  }
}

/**
 * \brief 处理
 */
static void __process (void)
{
  int                    i                      = 0;
  bool                   mode_is_change         = false;
  struct key_info        key_info[KEY_USER_MAX] = {0};
  uint32_t               systick                = 0;
  int                    sta_state              = 0;
  struct in_addr         ip_addr                = {0};
  static int             s_sta_state            = 1;
  static bool            s_state_init           = false;
  static enum main_state s_state_next           = MAIN_STATE_NO_INIT;
  static uint32_t        s_state_tick           = 0;

  //电池信息打印
  __bat_info_print();

  //获取按键信息
  for (i = 0; i < KEY_USER_MAX; i++)
  {
    key_info_get(i, &key_info[i]);
  }

  if (KEY_EVENT_LONG_PRESS == key_info[KEY_USER_POWER].event)
  { //长按电源键，关机
    zlog_info(__gp_zlogc, "power key long press, poweroff");
    sync();
    system("poweroff -f");
  }
  else if (KEY_EVENT_LONG_PRESS == key_info[KEY_USER_KEY].event)
  {
    zlog_info(__gp_zlogc, "power user long press, mode change");
    mode_is_change = true;
  }

  switch (__g_state)
  {
    case MAIN_STATE_NO_INIT:
    { //未初始化态
      s_state_init = false;
      if ((__g_state_last > MAIN_STATE_NO_INIT) && (__g_state_last < MAIN_STATE_MAX))
      {
        __g_state = __g_state_last;
      }
      else
      {
        __g_state = MAIN_STATE_USB;
      }
      break;
    }
    break;

    case MAIN_STATE_USB:
    { //USB J-Link 态
      if (!s_state_init)
      {
        zlog_info(__gp_zlogc, "usb state");
        s_state_init = true;
        led_trigger_set(LED_STATE, LED_TRIGGER_TIMER);
        led_timer_set(LED_STATE, 0, 1);
        jlink_ctl_run_set(false);
        cfg_int_set("wifi", "mode", WIFI_MODE_DISABLE);
        wifi_ctl_cfg_update();
        cfg_int_set("main", "state_last", __g_state);
      }

      if (mode_is_change)
      {
        s_state_init = false;
        s_state_next = MAIN_STATE_WIFI_STA;
        __g_state = MAIN_STATE_WAIT;
        break;
      }
    }
    break;

    case MAIN_STATE_WIFI_STA:
    { //WiFi STA 态
      if (!s_state_init)
      {
        zlog_info(__gp_zlogc, "sta state");
        s_state_init = true;
        led_trigger_set(LED_STATE, LED_TRIGGER_TIMER);
        led_timer_set(LED_STATE, 50, 2500);
        jlink_ctl_run_set(true);
        cfg_int_set("wifi", "mode", WIFI_MODE_STA);
        wifi_ctl_cfg_update();
        cfg_int_set("main", "state_last", __g_state);
      }

      sta_state = wifi_ctl_sta_state_get(&ip_addr, NULL);
      if (s_sta_state != sta_state)
      {
        s_sta_state = sta_state;
        if (0 == sta_state)
        { //STA 连接成功
          zlog_info(__gp_zlogc, "sta connect success, ip: %s", inet_ntoa(ip_addr));
          led_trigger_set(LED_ERROR, LED_TRIGGER_NONE);
          __g_ip_addr = ip_addr;
        }
        else
        { //STA 连接断开
          zlog_info(__gp_zlogc, "sta disconnect");
          led_trigger_set(LED_ERROR, LED_TRIGGER_TIMER);
          led_timer_set(LED_ERROR, 500, 500);
          __g_ip_addr.s_addr = htonl(INADDR_NONE);
        }
      }
      else if ((__g_ip_addr.s_addr == htonl(INADDR_NONE)) &&
               (ip_addr.s_addr != htonl(INADDR_NONE)) &&
               (0 == sta_state))
      { //IP 地址无效，且获取到 IP 地址
        zlog_info(__gp_zlogc, "sta ip get success: %s", inet_ntoa(ip_addr));
        __g_ip_addr = ip_addr;
      }

      if (mode_is_change)
      {
        s_sta_state = 1;
        led_trigger_set(LED_ERROR, LED_TRIGGER_NONE);

        s_state_init = false;
        s_state_next = MAIN_STATE_WIFI_AP;
        __g_state = MAIN_STATE_WAIT;
        break;
      }
    }
    break;

    case MAIN_STATE_WIFI_AP:
    { //WiFi AP 态
      if (!s_state_init)
      {
        zlog_info(__gp_zlogc, "ap state");
        s_state_init = true;
        led_trigger_set(LED_STATE, LED_TRIGGER_HEARTBEAT);
        jlink_ctl_run_set(true);
        cfg_int_set("wifi", "mode", WIFI_MODE_AP);
        wifi_ctl_cfg_update();
        cfg_int_set("main", "state_last", __g_state);
      }

      if (mode_is_change)
      {
        s_state_init = false;
        s_state_next = MAIN_STATE_USB;
        __g_state = MAIN_STATE_WAIT;
        break;
      }
    }
    break;

    case MAIN_STATE_WAIT:
    {
      systick = systick_get();

      if (!s_state_init)
      {
        zlog_info(__gp_zlogc, "wait state");
        s_state_init = true;
        led_trigger_set(LED_STATE, LED_TRIGGER_TIMER);
        led_timer_set(LED_STATE, 100, 100);
        s_state_tick = systick;
      }

      if ((systick - s_state_tick) >= 500)
      {
        s_state_init = false;
        __g_state = s_state_next;
        break;
      }
    }
    break;

    default:
    {
      s_state_init = false;
      __g_state = MAIN_STATE_USB;
    }
    break;
  }
}

/*******************************************************************************
  外部函数定义
*******************************************************************************/

/**
 * \brief main STA 模式下，最近一次的 IP 地址获取
 */
struct in_addr main_sta_last_ip_get (void)
{
  return __g_ip_addr;
}

/**
 * \brief main 配置更新
 */
int main_cfg_update (void)
{
  __g_cfg_update = true;
  return 0;
}

/**
 * \brief 等待初始化完成
 */
void main_wait_init (void)
{
  pthread_barrier_wait(&__g_init_barrier);
}

/**
 * \brief 主函数
 */
int main (int argc, char *argv[])
{
  int                epoll_fd = 0;
  int                timer_fd = 0;
  int                ready    = 0;
  struct epoll_event ev       = {0};
  uint64_t           temp_u64 = 0;
  int                err      = 0;

  //切换工作目录为程序所在目录
  __work_dir_change();

  //zlog 初始化
  err = __zlog_init();
  if (err != 0)
  {
    err = -1;
    goto err;
  }
  zlog_info(__gp_zlogc, "Build: %s %s", __DATE__, __TIME__);
  zlog_info(__gp_zlogc, "Version: %s", VERSION);

  //utilities 初始化
  utilities_init();

  //注册信号回调函数
  signal(SIGTERM, __singnl_callback);
  signal(SIGINT, __singnl_callback);
  signal(SIGPIPE, SIG_IGN);

  //互斥量初始化
  if (pthread_mutex_init(&__g_mutex, NULL) != 0)
  {
    zlog_fatal(__gp_zlogc, "mutex init error");
    err = -1;
    goto err_zlog_deinit;
  }

  //epoll_timer 初始化
  if (epoll_timer_init(&epoll_fd, &timer_fd, 10) != 0)
  {
    zlog_fatal(__gp_zlogc, "epoll_timer_init error");
    err = -1;
    goto err_mutex_deinit;
  }

  //配置信息初始化
  if (cfg_init() != 0)
  {
    err = -1;
    goto err_epoll_timer_close;
  }

  //获取配置信息
  __cfg_read();

  //LED 初始化
  if (led_init() != 0)
  {
    err = -1;
    goto err_cfg_deinit;
  }

  //按键初始化
  if (key_init() != 0)
  {
    err = -1;
    goto err_led_deinit;
  }

  //初始化屏障初始化，注意线程数量需正确配置
  if (pthread_barrier_init(&__g_init_barrier, NULL, 5) != 0)
  {
    zlog_fatal(__gp_zlogc, "pthread barrier init error");
    err = -1;
    goto err_key_deinit;
  }

  //wifi_ctl 初始化
  if (wifi_ctl_init() != 0)
  {
    err = -1;
    goto err_barrier_deinit;
  }

  //jlink_ctl 初始化
  if (jlink_ctl_init() != 0)
  {
    err = -1;
    goto err_wifi_ctl_deinit;
  }

  //web 初始化
  if (web_init() != 0)
  {
    err = -1;
    goto err_jlink_ctl_deinit;
  }

  //udp ctl_初始化
  if (udp_ctl_init() != 0)
  {
    err = -1;
    goto err_web_deinit;
  }

  //指示灯
  led_trigger_set(LED_STATE, LED_TRIGGER_TIMER);
  led_timer_set(LED_STATE, 0, 1);
  led_trigger_set(LED_ERROR, LED_TRIGGER_NONE);

  main_wait_init();
  while (__g_thread_run)
  {
    ready = epoll_wait(epoll_fd, &ev, 1, -1);
    if (-1 == ready)
    {
      if (EINTR == errno)
      {
        continue;
      }
      err = -1;
      zlog_error(__gp_zlogc, "epoll_wait epoll_fd %d error: %s", epoll_fd, strerror(errno));
      break;
    }
    else if (ready != 1)
    {
      zlog_error(__gp_zlogc, "epoll_wait epoll_fd %d error, return %d", epoll_fd, ready);
    }
    else
    {
      if (ev.data.fd == timer_fd)
      {
        if (read(timer_fd, &temp_u64, sizeof(temp_u64)) != sizeof(temp_u64))
        {
          zlog_error(__gp_zlogc, "read timer_fd %d error: %s", timer_fd, strerror(errno));
          continue;
        }

        if (__g_cfg_update)
        {
          __g_cfg_update = false;

          //获取配置信息
          __cfg_read();
        }

        //主线程处理
        pthread_mutex_lock(&__g_mutex);
        __process();
        pthread_mutex_unlock(&__g_mutex);

        //按键处理
        key_process();
      }
    }
  }

  udp_ctl_deinit();
err_web_deinit:
  web_deinit();
err_jlink_ctl_deinit:
  jlink_ctl_deinit();
err_wifi_ctl_deinit:
  wifi_ctl_deinit();
err_barrier_deinit:
  pthread_barrier_destroy(&__g_init_barrier);
err_key_deinit:
  key_deinit();
err_led_deinit:
  led_deinit();
err_cfg_deinit:
  cfg_deinit();
err_epoll_timer_close:
  close(timer_fd);
  close(epoll_fd);
err_mutex_deinit:
  pthread_mutex_destroy(&__g_mutex);
err_zlog_deinit:
  zlog_fini();
err:
  return err;
}

/* end of file */
