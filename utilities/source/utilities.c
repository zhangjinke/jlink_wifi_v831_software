/**
 * \file
 * \brief 通用工具
 *
 * \internal
 * \par Modification history
 * - 1.00 22-02-18  zjk, first implementation
 * \endinternal
 */

#include "utilities.h"
#include <errno.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <net/if.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <unistd.h>

//zlog 类别
zlog_category_t *gp_utilities_zlogc = NULL;

/**
 * \brief IP 地址合法检测
 */
bool ip_check (const uint8_t *p_ip)
{
  uint32_t ip;
  bool     valid = true;

  if (NULL == p_ip)
  {
    valid = false;
    goto err;
  }

  ip = ARRAY_TO_U32_B(p_ip, 0);

  if ((0 == ip) || (0xffffffff == ip))
  {
    valid = false;
    goto err;
  }

err:
  return valid;
}

/**
 * \brief 子网掩码合法检测
 */
bool mask_check (const uint8_t *p_mask)
{
  uint32_t mask;
  bool     valid = true;

  if (NULL == p_mask)
  {
    valid = false;
    goto err;
  }

  mask = ARRAY_TO_U32_B(p_mask, 0);

  if (0 == mask)
  {
    valid = false;
    goto err;
  }

  mask = ~mask + 1;
  if ((mask & (mask - 1)) != 0)
  { //判断是否为 2^n
    valid = false;
    goto err;
  }

err:
  return valid;
}

/**
 * \brief IP 地址是否在同一网段检测
 */
bool network_segment_check (const uint8_t *p_ip0, const uint8_t *p_ip1, const uint8_t *p_mask)
{
  uint32_t ip0   = 0;
  uint32_t ip1   = 0;
  uint32_t mask  = 0;
  bool     valid = true;

  if (!ip_check(p_ip0) || !ip_check(p_ip1) || !mask_check(p_mask))
  {
    valid = false;
    goto err;
  }

  ip0 = ARRAY_TO_U32_L(p_ip0, 0);
  ip1 = ARRAY_TO_U32_L(p_ip1, 0);
  mask = ARRAY_TO_U32_L(p_mask, 0);

  if ((ip0 & mask) != (ip1 & mask))
  {
    valid = false;
    goto err;
  }

err:
  return valid;
}

/**
 * \brief 网络连接参数获取
 */
int if_link_stats_get (const char *p_if_name, struct rtnl_link_stats *p_stats)
{
  struct ifaddrs *ifa_list = NULL;
  struct ifaddrs *ifa      = NULL;
  int             err      = -1;

  if ((NULL == p_if_name) || (NULL == p_stats))
  {
    zlog_error(gp_utilities_zlogc, "param error");
    goto err;
  }

  if (getifaddrs(&ifa_list) != 0)
  {
    zlog_error(gp_utilities_zlogc, "getifaddrs error: %s", strerror(errno));
    goto err;
  }

  for (ifa = ifa_list; ifa != NULL; ifa = ifa->ifa_next)
  {
    if ((NULL == ifa->ifa_addr) ||
        (ifa->ifa_addr->sa_family != AF_PACKET) ||
        (NULL == ifa->ifa_data))
    {
      continue;
    }

    if (strcmp(ifa->ifa_name, p_if_name) == 0)
    {
      memcpy(p_stats, ifa->ifa_data, sizeof(*p_stats));
      err = 0;
      break;
    }
  }

  freeifaddrs(ifa_list);

err:
  return err;
}

/**
 * \brief 网卡 MAC 地址获取
 */
int if_mac_get (const char *p_if_name, void *p_mac)
{
  int          sock = 0;
  struct ifreq ifr  = {0};
  int          err  = 0;

  if ((NULL == p_if_name) || (NULL == p_mac))
  {
    zlog_error(gp_utilities_zlogc, "param error");
    goto err;
  }

  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (-1 == sock)
  {
    zlog_error(gp_utilities_zlogc, "create socket error: %s", strerror(errno));
    err = -1;
    goto err;
  }

  strcpy(ifr.ifr_name, p_if_name);
  if (ioctl(sock, SIOCGIFHWADDR, &ifr) < 0)
  {
    zlog_error(gp_utilities_zlogc, "ioctl SIOCGIFHWADDR %s error: %s", p_if_name, strerror(errno));
    err = -1;
    goto err_close_sock;
  }

  memcpy(p_mac, ifr.ifr_hwaddr.sa_data, 6);

err_close_sock:
  close(sock);
err:
  return err;
}

/**
 * \brief epoll_timer 初始化
 */
int epoll_timer_init (int *p_epoll_fd, int *p_timer_fd, int timer_period_ms)
{
  int                timer_fd   = 0;
  int                epoll_fd   = 0;
  struct epoll_event ev         = {0};
  struct itimerspec  timer_spec = {0};
  int                err        = 0;

  if ((NULL == p_epoll_fd) || (NULL == p_timer_fd) || (timer_period_ms <= 0))
  {
    err = -1;
    goto err;
  }

  //创建 epoll
  epoll_fd = epoll_create(1);
  if (-1 == epoll_fd)
  {
    zlog_error(gp_utilities_zlogc, "epoll_create error: %s", strerror(errno));
    err = -1;
    goto err;
  }

  //创建 timer
  timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
  if (-1 == timer_fd)
  {
    zlog_error(gp_utilities_zlogc, "timerfd_create error: %s", strerror(errno));
    err = -1;
    goto err_epoll_close;
  }

  //添加 timer 到 epoll
  ev.events = EPOLLIN;
  ev.data.fd = timer_fd;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, timer_fd, &ev) == -1)
  {
    zlog_error(gp_utilities_zlogc, "epoll_ctl error: %s", strerror(errno));
    err = -1;
    goto err_timer_close;
  }

  timer_spec.it_interval.tv_sec = timer_period_ms / 1000;
  timer_spec.it_interval.tv_nsec = (timer_period_ms % 1000) * 1000000;
  timer_spec.it_value.tv_sec = 0;
  timer_spec.it_value.tv_nsec = 1; //启动定时器
  if (timerfd_settime(timer_fd, 0, &timer_spec, NULL) != 0)
  {
    zlog_error(gp_utilities_zlogc, "timerfd_settime error: %s", strerror(errno));
    err = -1;
    goto err_timer_close;
  }

  *p_epoll_fd = epoll_fd;
  *p_timer_fd = timer_fd;
  goto err;

err_timer_close:
  close(timer_fd);
err_epoll_close:
  close(epoll_fd);
err:
  return err;
}

/**
 * \brief utilities 初始化
 */
int utilities_init (void)
{
  if (gp_utilities_zlogc != NULL)
  {
    return -1;
  }
  gp_utilities_zlogc = zlog_get_category("utilities");
  return 0;
}

/* end of file */
