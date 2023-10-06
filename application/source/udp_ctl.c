/**
 * \file
 * \brief udp_ctl
 *
 * \internal
 * \par Modification history
 * - 1.00 23-01-02  zjk, first implementation
 * \endinternal
 */

#include "udp_ctl.h"
#include "c2000.h"
#include "cfg.h"
#include "checksum.h"
#include "main.h"
#include "str.h"
#include "systick.h"
#include "utilities.h"
#include "zlog.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h> //strerror()
#include <sys/epoll.h>
#include <sys/prctl.h> //prctl()
#include <sys/socket.h>
#include <termios.h>
#include <unistd.h>

/*******************************************************************************
  宏定义
*******************************************************************************/

/*******************************************************************************
  本地全局变量声明
*******************************************************************************/

//UDP 状态枚举
enum udp_state
{
  UDP_STATE_NO_INIT = 0, //未初始化态
  UDP_STATE_WAIT,        //等待态
  UDP_STATE_IDLE,        //空闲态
};

//UDP 结构体
struct udp
{
  int sock; //socket 文件描述符
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

//C2000 信息
static struct c2000_info __g_c2000_info = {0};

//UDP
static struct udp __g_udp = {0};

/*******************************************************************************
  内部函数定义
*******************************************************************************/

/**
 * \brief UDP 初始化
 */
static int __udp_init (struct udp *p_udp, const char *p_host, uint16_t port)
{
  struct sockaddr_in server_addr = {0};
  int                err         = 0;

  if ((p_udp->sock = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0)) == -1)
  {
    err = -1;
    zlog_error(__gp_zlogc, "create socket error: %s", strerror(errno));
    goto err;
  }
  fcntl(p_udp->sock, F_SETFL, fcntl(p_udp->sock, F_GETFL) | O_NONBLOCK);

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family      = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr(p_host);
  server_addr.sin_port        = htons(port);
  if (bind(p_udp->sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
  {
    err = -1;
    zlog_error(__gp_zlogc, "bind socket error: %s", strerror(errno));
    goto err_socket_close;
  }

  goto err;

err_socket_close:
  close(p_udp->sock);
err:
  return err;
}

/**
 * \brief 接收处理
 */
static void __recv_process (struct udp *p_udp, uint8_t *p_buf, size_t len)
{
  char               if_name[33] = {0};
  int                on          = 0;
  uint8_t           *p_src       = p_buf;
  uint8_t            reply[512]  = {0};
  size_t             reply_size  = 0;
  struct sockaddr_in dst_addr    = {0};
  int                ret         = 0;

  if ((len > 2) && (0xfa == p_src[0]) && (0x01 == p_src[1]))
  { //c2000 命令
    cfg_str_get("wifi", "if_name", if_name, sizeof(if_name), "wlan0");
    if_mac_get(if_name, &__g_c2000_info.mac[0]);
    if_ip_get(if_name, &__g_c2000_info.local_ip[0]);

    reply_size = c2000_recv_process(reply, p_buf, len, &__g_c2000_info);
    if (reply_size > 0)
    {
      //使能广播发送
      on = 1;
      if (setsockopt(p_udp->sock, SOL_SOCKET, SO_BROADCAST, (const char*)&on, sizeof(on)) != 0)
      {
        zlog_error(__gp_zlogc, "udp_broadcast enable error: %s", strerror(errno));
      }

      //设置目的地址为广播地址
      dst_addr.sin_family = AF_INET;
      dst_addr.sin_port = htons(21677);
      dst_addr.sin_addr.s_addr = inet_addr("255.255.255.255");

      //发送
      ret = sendto(p_udp->sock, reply, reply_size, 0, (struct sockaddr *)&dst_addr, sizeof(dst_addr));
      if (ret < 0)
      {
        zlog_error(__gp_zlogc, "c2000 send len %zu error: %s", reply_size, strerror(errno));
      }
      else
      {
        zlog_info(__gp_zlogc, "c2000 send len %zu", reply_size);
      }
    }
  }
}

/**
 * \brief udp 处理
 */
static void __udp_process (int epoll_fd, struct epoll_event *p_ev)
{
  uint8_t               buf[4096]    = {0};
  ssize_t               nread        = 0;
  struct epoll_event    ev           = {0};
  uint32_t              systick      = systick_get();
  static enum udp_state s_state      = UDP_STATE_NO_INIT;
  static enum udp_state s_state_next = UDP_STATE_NO_INIT;
  static uint32_t       s_wait_ms    = 0;
  static uint32_t       s_tick       = 0;

  switch (s_state)
  {
    case UDP_STATE_NO_INIT:
    {
      memset(&__g_udp, 0, sizeof(__g_udp));
      if (__udp_init(&__g_udp, "0.0.0.0", 21678) != 0)
      {
        s_tick = systick;
        s_wait_ms = 5000;
        s_state_next = UDP_STATE_NO_INIT;
        s_state = UDP_STATE_WAIT;
        break;
      }

      // 添加 socket 到 epoll
      ev.events = EPOLLIN;
      ev.data.fd = __g_udp.sock;
      if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, __g_udp.sock, &ev) == -1)
      {
        zlog_error(__gp_zlogc, "epoll_ctl add error: %s", strerror(errno));
        close(__g_udp.sock);
        s_tick = systick;
        s_wait_ms = 5000;
        s_state_next = UDP_STATE_NO_INIT;
        s_state = UDP_STATE_WAIT;
        break;
      }

      s_state = UDP_STATE_IDLE;
      break;
    }
    break;

    case UDP_STATE_WAIT:
    {
      if ((systick - s_tick) >= s_wait_ms)
      {
        s_state = s_state_next;
        break;
      }
    }
    break;

    case UDP_STATE_IDLE:
    {
      if (NULL == p_ev)
      {
        break;
      }

      if (p_ev->data.fd == __g_udp.sock)
      {
        nread = read(__g_udp.sock, buf, sizeof(buf));
        if (nread <= 0)
        {
          zlog_info(__gp_zlogc, "read %d error: %s", nread, strerror(errno));
        }
        else
        {
          __recv_process(&__g_udp, buf, nread);
        }
      }
      else
      {
        zlog_error(__gp_zlogc, "epoll fd %d not found", p_ev->data.fd);
      }
    }
    break;

    default:
    {
      s_state = UDP_STATE_NO_INIT;
    }
    break;
  }
}

/**
 * \brief udp_ctl 线程
 */
static void *__udp_ctl_thread (void *p_arg)
{
  int                timer_fd;
  int                epoll_fd;
  int                ready;
  uint64_t           temp_u64;
  struct epoll_event ev;
  int                err = 0;

  //设置线程名称
  prctl(PR_SET_NAME, "udp_ctl");

  //等待初始化完成
  main_wait_init();
  zlog_info(__gp_zlogc, "udp_ctl_thread start, arg: %d", *(int *)p_arg);

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

        //udp 处理
        pthread_mutex_lock(&__g_mutex);
        __udp_process(epoll_fd, NULL);
        pthread_mutex_unlock(&__g_mutex);
      }
      else
      {
        pthread_mutex_lock(&__g_mutex);
        __udp_process(epoll_fd, &ev);
        pthread_mutex_unlock(&__g_mutex);
      }
    }
  }

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
 * \brief udp_ctl 初始化
 */
int udp_ctl_init (void)
{
  int        err   = 0;
  static int s_arg = 0;

  if (__g_is_init)
  { //已初始化
    return 0;
  }

  __gp_zlogc = zlog_get_category("udp_ctl");

  if (pthread_mutex_init(&__g_mutex, NULL) != 0)
  {
    zlog_fatal(__gp_zlogc, "mutex init error");
    err = -1;
    goto err;
  }

  err = pthread_create(&__g_thread, NULL, __udp_ctl_thread, &s_arg);
  if (err != 0)
  {
    zlog_fatal(__gp_zlogc, "udp_ctl_thread create failed: %s", strerror(err));
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
 * \brief udp_ctl 解初始化
 */
int udp_ctl_deinit (void)
{
  void *p_thread_ret = NULL;

  if (!__g_is_init)
  {
    return 0;
  }

  __g_thread_run = false;
  pthread_kill(__g_thread, SIGINT);
  pthread_join(__g_thread, (void **)&p_thread_ret);
  zlog_info(__gp_zlogc, "udp_ctl_thread exit, ret: %d", *(int *)p_thread_ret);
  pthread_mutex_destroy(&__g_mutex);
  __g_is_init = false;

  return *(int *)p_thread_ret;
}

/* end of file */
