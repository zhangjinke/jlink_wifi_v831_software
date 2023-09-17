/**
 * \file
 * \brief jlink_ctl
 *
 * \internal
 * \par Modification history
 * - 1.00 23-03-20  zjk, first implementation
 * \endinternal
 */

#include "jlink_ctl.h"
#include "cfg.h"
#include "gpio.h"
#include "main.h"
#include "process.h"
#include "str.h"
#include "systick.h"
#include "utilities.h"
#include "zlog.h"
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
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

//状态
enum state
{
  STATE_IDLE = 0, //空闲态
  STATE_RUN,      //运行态
  STATE_WAIT,     //等待态
};

/*******************************************************************************
  本地全局变量定义
*******************************************************************************/

//zlog 类别
static zlog_category_t *__gp_zlogc = NULL;

//线程号
static pthread_t __g_thread = 0;

//线程是否需要继续执行
static volatile bool __g_thread_run = true;

//互斥量
static pthread_mutex_t __g_mutex;

//是否初始化
static bool __g_is_init = false;

static volatile bool __g_cfg_update                   = false; //配置更新标记
static char          __g_remote_server_path[PATH_MAX] = {0};   //JLinkRemoteServer 路径
static int           __g_usb_switch_gpio_num          = 0;     //USB 切换 GPIO 号

static volatile bool __g_is_run = 0; //是否运行 J-Link 进程
static volatile int  __g_sn     = 0; //J-Link S/N，0=与 J-Link 连接失败

/*******************************************************************************
  内部函数定义
*******************************************************************************/

/**
 * \brief 配置读取
 */
static int __cfg_read (void)
{
  int err = 0;

  err = cfg_str_get("jlink", "remote_server_path", __g_remote_server_path, sizeof(__g_remote_server_path),
                    "/mnt/UDISK/JLinkRemoteServerCLExe");
  if (err != 0)
  {
    cfg_str_set("jlink", "remote_server_path", __g_remote_server_path);
  }

  err = cfg_int_get("jlink", "usb_switch_gpio_num", &__g_usb_switch_gpio_num, 69);
  if (err != 0)
  {
    cfg_int_set("jlink", "usb_switch_gpio_num", __g_usb_switch_gpio_num);
  }

  return 0;
}

/**
 * \brief J-Link 进程关闭
 */
static int32_t __jlink_process_kill (void)
{
  char    *p_path     = NULL;
  char    *p_basename = NULL;
  pid_t    pid        = 0;
  int32_t  err        = 0;

  p_path = strdup(__g_remote_server_path);
  if (NULL == p_path)
  {
    err = -1;
    goto err;
  }

  p_basename = basename(__g_remote_server_path);
  while (process_num_get(p_basename, &pid) > 0)
  {
    zlog_info(__gp_zlogc, "process %s pid %d is run, kill it", p_basename, pid);
    process_kill(p_basename, 2000);
  }
  free(p_path);

err:
  return err;
}

/**
 * \brief 应答处理
 */
static void __reply_process (int fd)
{
  char    reply[512] = {0};
  ssize_t nread      = 0;
  char   *p_str      = NULL;

  //读取应答
  nread = read(fd, reply, sizeof(reply) - 1);
  if (nread <= 0)
  {
    goto err;
  }

  //去除换行符，添加字符串结束符
  while ((nread > 0) && (('\r' == reply[nread - 1]) || ('\n' == reply[nread - 1])))
  {
    nread--;
  }
  reply[nread] = '\0';

  zlog_debug(__gp_zlogc, reply);
  if ((p_str = strstr(reply, "S/N")) != NULL)
  {
    p_str += sizeof("S/N");
    __g_sn = atoi(p_str);
    zlog_info(__gp_zlogc, "sn: %d", __g_sn);
  }

err:
  return;
}

/**
 * \brief 处理
 */
static void __process (void)
{
  uint32_t          systick    = systick_get();
  static enum state s_state    = STATE_IDLE;
  static int        s_reply_fd = 0;
  static pid_t      s_pid      = 0;
  static uint32_t   s_tick     = 0;

  switch (s_state)
  {
    case STATE_IDLE:
    { //空闲态，启动 JLinkRemoteServerCLExe 进程
      if (!__g_is_run)
      {
        break;
      }

      //关闭可能已存在的进程
      if (__jlink_process_kill() != 0)
      {
        s_tick = systick;
        s_state = STATE_WAIT;
        break;
      }

      //启动进程
      zlog_debug(__gp_zlogc, "J-Link process run");
      s_pid = process_exec(NULL, &s_reply_fd, &s_reply_fd, __g_remote_server_path);
      if (-1 == s_pid)
      { //启动失败
        s_tick = systick;
        s_state = STATE_WAIT;
        break;
      }

      fcntl(s_reply_fd, F_SETFL, fcntl(s_reply_fd, F_GETFL) | O_NONBLOCK);
      gpio_direction_set(__g_usb_switch_gpio_num, 1); //J-Link 连接到 MPU
      s_state = STATE_RUN;
      break;
    }
    break;

    case STATE_RUN:
    { //运行态，处理 JLinkRemoteServerCLExe 输出
      if (!__g_is_run)
      {
        zlog_debug(__gp_zlogc, "J-Link process stop");
        if (__jlink_process_kill() == 0)
        {
          gpio_direction_set(__g_usb_switch_gpio_num, 0); //J-Link 连接到 USB
          s_state = STATE_IDLE;
          break;
        }
      }
      else
      {
        __reply_process(s_reply_fd);
      }
    }
    break;

    case STATE_WAIT:
    { //等待态
      if ((systick - s_tick) >= 5000)
      {
        s_state = STATE_IDLE;
        break;
      }
    }
    break;

    default:
    {
      s_state = STATE_IDLE;
    }
    break;
  }
}

/**
 * \brief jlink_ctl 线程
 */
static void *__jlink_ctl_thread (void *p_arg)
{
  int                timer_fd;
  int                epoll_fd;
  int                ready;
  uint64_t           temp_u64;
  struct epoll_event ev;
  int                err = 0;

  //设置线程名称
  prctl(PR_SET_NAME, "jlink_ctl");

  //等待初始化完成
  main_wait_init();
  zlog_info(__gp_zlogc, "jlink_ctl_thread start, arg: %d", *(int *)p_arg);

  //epoll_timer 初始化
  if (epoll_timer_init(&epoll_fd, &timer_fd, 10) != 0)
  {
    zlog_fatal(__gp_zlogc, "epoll_timer_init error");
    err = -1;
    goto err;
  }

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

        //处理
        pthread_mutex_lock(&__g_mutex);
        __process();
        pthread_mutex_unlock(&__g_mutex);
      }
    }
  }

  __jlink_process_kill();

  close(timer_fd);
  close(epoll_fd);
err:
  *(int *)p_arg = err;
  return p_arg;
}

/*******************************************************************************
  外部函数定义
*******************************************************************************/

/**
 * \brief jlink_ctl 运行状态获取
 */
bool jlink_ctl_run_get (void)
{
  return __g_is_run;
}

/**
 * \brief jlink_ctl 运行状态设置
 */
int jlink_ctl_run_set (bool run)
{
  __g_is_run = run;
  return 0;
}

/**
 * \brief jlink_ctl S/N 获取
 */
int jlink_ctl_sn_get (void)
{
  return __g_sn;
}

/**
 * \brief jlink_ctl 配置更新
 */
int jlink_ctl_cfg_update (void)
{
  __g_cfg_update = true;
  return 0;
}

/**
 * \brief jlink_ctl 初始化
 */
int jlink_ctl_init (void)
{
  int        err   = 0;
  static int s_arg = 0;

  if (__g_is_init)
  { //已初始化
    return 0;
  }

  __gp_zlogc = zlog_get_category("jlink_ctl");

  //获取配置信息
  __cfg_read();

  //GPIO 初始化
  if (gpio_export(__g_usb_switch_gpio_num) == 0)
  {
    gpio_direction_set(__g_usb_switch_gpio_num, 0); //J-Link 连接到 USB
  }

  if (pthread_mutex_init(&__g_mutex, NULL) != 0)
  {
    zlog_fatal(__gp_zlogc, "mutex init error");
    err = -1;
    goto err;
  }

  err = pthread_create(&__g_thread, NULL, __jlink_ctl_thread, &s_arg);
  if (err != 0)
  {
    zlog_fatal(__gp_zlogc, "jlink_ctl_thread create failed: %s", strerror(err));
    err = -1;
    goto err_mutex_destroy;
  }
  __g_is_init = true;
  goto err;

err_mutex_destroy:
  pthread_mutex_destroy(&__g_mutex);
err:
  return err;
}

/**
 * \brief jlink_ctl 解初始化
 */
int jlink_ctl_deinit (void)
{
  void *p_jlink_ctl_thread_ret = NULL;

  if (!__g_is_init)
  {
    return 0;
  }

  __g_thread_run = false;
  pthread_kill(__g_thread, SIGINT);
  pthread_join(__g_thread, (void **)&p_jlink_ctl_thread_ret);
  zlog_info(__gp_zlogc, "jlink_ctl_thread exit, ret: %d", *(int *)p_jlink_ctl_thread_ret);
  pthread_mutex_destroy(&__g_mutex);
  gpio_direction_set(__g_usb_switch_gpio_num, 0); //J-Link 连接到 USB
  __g_is_init = false;

  return *(int *)p_jlink_ctl_thread_ret;
}

/* end of file */
