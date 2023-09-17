/**
 * \file
 * \brief wifi_ctl
 *
 * \internal
 * \par Modification history
 * - 1.00 22-06-24  zjk, first implementation
 * \endinternal
 */

#include "wifi_ctl.h"
#include "cfg.h"
#include "file.h"
#include "jlink_ctl.h"
#include "main.h"
#include "process.h"
#include "str.h"
#include "utilities.h"
#include "wpa_ctrl.h"
#include "zlog.h"
#include <errno.h>
#include <fcntl.h>
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

#define __RFKILL_PATH  "/sys/class/rfkill/rfkill0/state" //rfkill 路径

/*******************************************************************************
  本地全局变量声明
*******************************************************************************/

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

static char           __g_wpa_ctrl_path[PATH_MAX]     = {0};               //wpa_ctrl 路径
static char           __g_hostapd_ctrl_path[PATH_MAX] = {0};               //hostapd_ctrl 路径
static char           __g_if_name[33]                 = {0};               //网卡名称
static enum wifi_mode __g_wifi_mode                   = WIFI_MODE_DISABLE; //WiFi 模式
static char           __g_sta_ssid[33]                = {0};               //WiFi-STA 名称
static char           __g_sta_password[65]            = {0};               //WiFi-STA 密码
static int            __g_sta_addr_mode               = 0;                 //WiFi-STA 地址模式，0=DHCP，1=静态 IP
static struct in_addr __g_sta_ip                      = {0};               //WiFi-STA IP 地址
static struct in_addr __g_sta_mask                    = {0};               //WiFi-STA 子网掩码
static struct in_addr __g_sta_gateway                 = {0};               //WiFi-STA 默认网关
static struct in_addr __g_sta_dns0                    = {0};               //WiFi-STA 首选 DNS 服务器
static struct in_addr __g_sta_dns1                    = {0};               //WiFi-STA 备用 DNS 服务器
static char           __g_ap_ssid[33]                 = {0};               //WiFi-AP 名称
static char           __g_ap_password[65]             = {0};               //WiFi-AP 密码
static volatile bool  __g_cfg_update                  = false;             //配置更新标记

static int            __g_sta_state    = -1;  //STA 连接状态，0=连接，-1=断开
static int8_t         __g_sta_avg_rssi = 0;   //STA 平均信号强度
static struct in_addr __g_sta_ip_addr  = {0}; //STA IP 地址

/*******************************************************************************
  内部函数定义
*******************************************************************************/

/**
 * \brief 配置读取
 */
static int __cfg_read (void)
{
  char ip_str[16];
  int  err;

  err = cfg_str_get("wifi", "wpa_ctrl_path", __g_wpa_ctrl_path, sizeof(__g_wpa_ctrl_path),
                    "/var/run/wpa_supplicant/wlan0");
  if (err != 0)
  {
    cfg_str_set("wifi", "wpa_ctrl_path", __g_wpa_ctrl_path);
  }

  err = cfg_str_get("wifi", "hostapd_ctrl_path", __g_hostapd_ctrl_path, sizeof(__g_hostapd_ctrl_path),
                    "/var/run/hostapd/wlan0");
  if (err != 0)
  {
    cfg_str_set("wifi", "hostapd_ctrl_path", __g_hostapd_ctrl_path);
  }

  err = cfg_str_get("wifi", "if_name", __g_if_name, sizeof(__g_if_name), "wlan0");
  if (err != 0)
  {
    cfg_str_set("wifi", "if_name", __g_if_name);
  }

  err = cfg_int_get("wifi", "mode", (int *)&__g_wifi_mode, WIFI_MODE_STA);
  if (err != 0)
  {
    cfg_int_set("wifi", "mode", __g_wifi_mode);
  }

  err = cfg_str_get("wifi", "sta_ssid", __g_sta_ssid, sizeof(__g_sta_ssid), "jlink");
  if (err != 0)
  {
    cfg_str_set("wifi", "sta_ssid", __g_sta_ssid);
  }

  err = cfg_str_get("wifi", "sta_password", __g_sta_password, sizeof(__g_sta_password), "");
  if (err != 0)
  {
    cfg_str_set("wifi", "sta_password", __g_sta_password);
  }

  err = cfg_int_get("wifi", "sta_addr_mode", &__g_sta_addr_mode, 0);
  if (err != 0)
  {
    cfg_int_set("wifi", "sta_addr_mode", __g_sta_addr_mode);
  }

  err = cfg_str_get("wifi", "sta_ip", ip_str, sizeof(ip_str), "192.168.1.123");
  if (err != 0)
  {
    cfg_str_set("wifi", "sta_ip", ip_str);
  }
  if (inet_pton(AF_INET, ip_str, &__g_sta_ip) != 1)
  {
    zlog_error(__gp_zlogc, "cfg wifi sta_ip set default, invalid value: %s", ip_str);
    cfg_str_set("wifi", "sta_ip", "192.168.1.123");
    inet_pton(AF_INET, "192.168.1.123", &__g_sta_ip);
  }

  err = cfg_str_get("wifi", "sta_mask", ip_str, sizeof(ip_str), "255.255.255.0");
  if (err != 0)
  {
    cfg_str_set("wifi", "sta_mask", ip_str);
  }
  if (inet_pton(AF_INET, ip_str, &__g_sta_mask) != 1)
  {
    zlog_error(__gp_zlogc, "cfg wifi sta_mask set default, invalid value: %s", ip_str);
    cfg_str_set("wifi", "sta_mask", "255.255.255.0");
    inet_pton(AF_INET, "255.255.255.0", &__g_sta_mask);
  }

  err = cfg_str_get("wifi", "sta_gateway", ip_str, sizeof(ip_str), "192.168.1.1");
  if (err != 0)
  {
    cfg_str_set("wifi", "sta_gateway", ip_str);
  }
  if (inet_pton(AF_INET, ip_str, &__g_sta_gateway) != 1)
  {
    zlog_error(__gp_zlogc, "cfg wifi sta_gateway set default, invalid value: %s", ip_str);
    cfg_str_set("wifi", "sta_gateway", "192.168.1.1");
    inet_pton(AF_INET, "192.168.1.1", &__g_sta_gateway);
  }

  err = cfg_str_get("wifi", "sta_dns0", ip_str, sizeof(ip_str), "192.168.1.1");
  if (err != 0)
  {
    cfg_str_set("wifi", "sta_dns0", ip_str);
  }
  if (inet_pton(AF_INET, ip_str, &__g_sta_dns0) != 1)
  {
    zlog_error(__gp_zlogc, "cfg wifi sta_dns0 set default, invalid value: %s", ip_str);
    cfg_str_set("wifi", "sta_dns0", "192.168.1.1");
    inet_pton(AF_INET, "192.168.1.1", &__g_sta_dns0);
  }

  err = cfg_str_get("wifi", "sta_dns1", ip_str, sizeof(ip_str), "8.8.8.8");
  if (err != 0)
  {
    cfg_str_set("wifi", "sta_dns1", ip_str);
  }
  if (inet_pton(AF_INET, ip_str, &__g_sta_dns1) != 1)
  {
    zlog_error(__gp_zlogc, "cfg wifi sta_dns1 set default, invalid value: %s", ip_str);
    cfg_str_set("wifi", "sta_dns1", "8.8.8.8");
    inet_pton(AF_INET, "8.8.8.8", &__g_sta_dns1);
  }

  err = cfg_str_get("wifi", "ap_ssid", __g_ap_ssid, sizeof(__g_ap_ssid), "J-Link");
  if (err != 0)
  {
    cfg_str_set("wifi", "ap_ssid", __g_ap_ssid);
  }

  err = cfg_str_get("wifi", "ap_password", __g_ap_password, sizeof(__g_ap_password), "jlink wifi");
  if (err != 0)
  {
    cfg_str_set("wifi", "ap_password", __g_ap_password);
  }

  return 0;
}

/**
 * \brief STA 模式初始化
 */
static int __sta_init (const char *p_ctl_path, const char *p_ssid, const char *p_password, bool is_hide)
{
  int              i;
  char             cmd[1024];
  int              cmd_len;
  char             reply[1024];
  char            *p_str;
  size_t           len;
  size_t           len_reply;
  struct wpa_ctrl *p_wpa_ctrl;
  int              err = 0;

  if ((NULL == p_ctl_path) || (NULL == p_ssid))
  {
    zlog_error(__gp_zlogc, "param error");
    err = -1;
    goto err;
  }

  p_wpa_ctrl = wpa_ctrl_open(p_ctl_path);
  if (NULL == p_wpa_ctrl)
  {
    zlog_error(__gp_zlogc, "wpa_ctrl_open %s error", p_ctl_path);
    err = -1;
    goto err;
  }

  //查看网络列表
  len = sizeof(reply) - 1;
  cmd_len = snprintf(cmd, sizeof(cmd), "LIST_NETWORKS");
  wpa_ctrl_request(p_wpa_ctrl, cmd, cmd_len, reply, &len, NULL);
  len = ((len >= 2) && ('\r' == reply[len - 2]) && ('\n' == reply[len - 2])) ? len - 2 : len;
  reply[len] = '\0';
  zlog_debug(__gp_zlogc, "%s reply: %s", cmd, reply);
  p_str = strstr(reply, "\n");
  if (p_str != NULL)
  {
    len_reply = len;
    p_str++;
    while ((p_str - reply) < len_reply)
    { //删除所有网络配置
      if ((*p_str < '0') || (*p_str > '9'))
      {
        break;
      }
      i = atoi(p_str);

      //删除指定 id 的网络配置
      len = sizeof(reply) - 1;
      cmd_len = snprintf(cmd, sizeof(cmd), "REMOVE_NETWORK %d", i);
      wpa_ctrl_request(p_wpa_ctrl, cmd, cmd_len, reply, &len, NULL);
      len = ((len >= 2) && ('\r' == reply[len - 2]) && ('\n' == reply[len - 2])) ? len - 2 : len;
      reply[len] = '\0';
      zlog_debug(__gp_zlogc, "%s reply: %s", cmd, reply);

      //查找下一行
      p_str = strstr(p_str, "\n");
      if (NULL == p_str)
      {
        break;
      }
      p_str++;
    }
  }

  //添加网络
  len = sizeof(reply) - 1;
  cmd_len = snprintf(cmd, sizeof(cmd), "ADD_NETWORK");
  wpa_ctrl_request(p_wpa_ctrl, cmd, cmd_len, reply, &len, NULL);
  len = ((len >= 2) && ('\r' == reply[len - 2]) && ('\n' == reply[len - 2])) ? len - 2 : len;
  reply[len] = '\0';
  zlog_debug(__gp_zlogc, "%s reply: %s", cmd, reply);
  if ((reply[0] >= '0') && (reply[0] <= '9'))
  {
    i = atoi(reply);

    //配置 SSID
    len = sizeof(reply) - 1;
    cmd_len = snprintf(cmd, sizeof(cmd), "SET_NETWORK %d ssid \"%s\"", i, p_ssid);
    wpa_ctrl_request(p_wpa_ctrl, cmd, cmd_len, reply, &len, NULL);
    len = ((len >= 2) && ('\r' == reply[len - 2]) && ('\n' == reply[len - 2])) ? len - 2 : len;
    reply[len] = '\0';
    zlog_debug(__gp_zlogc, "%s reply: %s", cmd, reply);

    if ((p_password != NULL) && (strlen(p_password) > 0))
    {
      //配置密码
      len = sizeof(reply) - 1;
      cmd_len = snprintf(cmd, sizeof(cmd), "SET_NETWORK %d psk \"%s\"", i, p_password);
      wpa_ctrl_request(p_wpa_ctrl, cmd, cmd_len, reply, &len, NULL);
      len = ((len >= 2) && ('\r' == reply[len - 2]) && ('\n' == reply[len - 2])) ? len - 2 : len;
      reply[len] = '\0';
      zlog_debug(__gp_zlogc, "%s reply: %s", cmd, reply);
    }
    else
    {
      len = sizeof(reply) - 1;
      cmd_len = snprintf(cmd, sizeof(cmd), "SET_NETWORK %d key_mgmt NONE", i);
      wpa_ctrl_request(p_wpa_ctrl, cmd, cmd_len, reply, &len, NULL);
      len = ((len >= 2) && ('\r' == reply[len - 2]) && ('\n' == reply[len - 2])) ? len - 2 : len;
      reply[len] = '\0';
      zlog_debug(__gp_zlogc, "%s reply: %s", cmd, reply);
    }

    if (is_hide)
    { //隐藏网络
      len = sizeof(reply) - 1;
      cmd_len = snprintf(cmd, sizeof(cmd), "SET_NETWORK %d scan_ssid 1", i);
      wpa_ctrl_request(p_wpa_ctrl, cmd, cmd_len, reply, &len, NULL);
      len = ((len >= 2) && ('\r' == reply[len - 2]) && ('\n' == reply[len - 2])) ? len - 2 : len;
      reply[len] = '\0';
      zlog_debug(__gp_zlogc, "%s reply: %s", cmd, reply);
    }

    len = sizeof(reply) - 1;
    cmd_len = snprintf(cmd, sizeof(cmd), "ENABLE_NETWORK %d", i);
    wpa_ctrl_request(p_wpa_ctrl, cmd, cmd_len, reply, &len, NULL);
    len = ((len >= 2) && ('\r' == reply[len - 2]) && ('\n' == reply[len - 2])) ? len - 2 : len;
    reply[len] = '\0';
    zlog_debug(__gp_zlogc, "%s reply: %s", cmd, reply);

    len = sizeof(reply) - 1;
    cmd_len = snprintf(cmd, sizeof(cmd), "RECONNECT");
    wpa_ctrl_request(p_wpa_ctrl, cmd, cmd_len, reply, &len, NULL);
    len = ((len >= 2) && ('\r' == reply[len - 2]) && ('\n' == reply[len - 2])) ? len - 2 : len;
    reply[len] = '\0';
    zlog_debug(__gp_zlogc, "%s reply: %s", cmd, reply);
  }

  wpa_ctrl_close(p_wpa_ctrl);

err:
  return err;
}

/**
 * \brief STA 模式解初始化
 */
static int __sta_deinit (const char *p_ctl_path)
{
  char             cmd[1024];
  int              cmd_len;
  char             reply[1024];
  size_t           len;
  struct wpa_ctrl *p_wpa_ctrl;
  int              err = 0;

  if (NULL == p_ctl_path)
  {
    zlog_error(__gp_zlogc, "param error");
    err = -1;
    goto err;
  }

  p_wpa_ctrl = wpa_ctrl_open(p_ctl_path);
  if (NULL == p_wpa_ctrl)
  {
    zlog_error(__gp_zlogc, "wpa_ctrl_open %s error", p_ctl_path);
    err = -1;
    goto err;
  }

  //断开连接
  len = sizeof(reply) - 1;
  cmd_len = snprintf(cmd, sizeof(cmd), "DISCONNECT");
  wpa_ctrl_request(p_wpa_ctrl, cmd, cmd_len, reply, &len, NULL);
  len = ((len >= 2) && ('\r' == reply[len - 2]) && ('\n' == reply[len - 2])) ? len - 2 : len;
  reply[len] = '\0';
  zlog_debug(__gp_zlogc, "%s reply: %s", cmd, reply);

  wpa_ctrl_close(p_wpa_ctrl);

err:
  return err;
}

/**
 * \brief AP 模式初始化
 */
static int __ap_init (const char *p_ctl_path, const char *p_ssid, const char *p_password, bool is_hide)
{
  char             cmd[1024];
  int              cmd_len;
  char             reply[1024];
  size_t           len;
  struct wpa_ctrl *p_wpa_ctrl;
  int              err = 0;

  if ((NULL == p_ctl_path) || (NULL == p_ssid))
  {
    zlog_error(__gp_zlogc, "param error");
    err = -1;
    goto err;
  }

  p_wpa_ctrl = wpa_ctrl_open(p_ctl_path);
  if (NULL == p_wpa_ctrl)
  {
    zlog_error(__gp_zlogc, "wpa_ctrl_open %s error", p_ctl_path);
    err = -1;
    goto err;
  }

  //关闭接口
  len = sizeof(reply) - 1;
  cmd_len = snprintf(cmd, sizeof(cmd), "DISABLE");
  if (wpa_ctrl_request(p_wpa_ctrl, cmd, cmd_len, reply, &len, NULL) == 0)
  {
    len = ((len >= 2) && ('\r' == reply[len - 2]) && ('\n' == reply[len - 2])) ? len - 2 : len;
    reply[len] = '\0';
    zlog_debug(__gp_zlogc, "%s reply: %s", cmd, reply);
  }

  //配置 SSID
  len = sizeof(reply) - 1;
  cmd_len = snprintf(cmd, sizeof(cmd), "SET ssid %s", p_ssid);
  wpa_ctrl_request(p_wpa_ctrl, cmd, cmd_len, reply, &len, NULL);
  len = ((len >= 2) && ('\r' == reply[len - 2]) && ('\n' == reply[len - 2])) ? len - 2 : len;
  reply[len] = '\0';
  zlog_debug(__gp_zlogc, "%s reply: %s", cmd, reply);

  if ((p_password != NULL) && (strlen(p_password) >= 8))
  {
    //配置密码
    len = sizeof(reply) - 1;
    cmd_len = snprintf(cmd, sizeof(cmd), "SET wpa_passphrase %s", p_password);
    wpa_ctrl_request(p_wpa_ctrl, cmd, cmd_len, reply, &len, NULL);
    len = ((len >= 2) && ('\r' == reply[len - 2]) && ('\n' == reply[len - 2])) ? len - 2 : len;
    reply[len] = '\0';
    zlog_debug(__gp_zlogc, "%s reply: %s", cmd, reply);
  }
  else
  {
    //todo 配置为开放 AP
  }

  //是否隐藏网络
  len = sizeof(reply) - 1;
  cmd_len = snprintf(cmd, sizeof(cmd), "SET ignore_broadcast_ssid %d", is_hide);
  wpa_ctrl_request(p_wpa_ctrl, cmd, cmd_len, reply, &len, NULL);
  len = ((len >= 2) && ('\r' == reply[len - 2]) && ('\n' == reply[len - 2])) ? len - 2 : len;
  reply[len] = '\0';
  zlog_debug(__gp_zlogc, "%s reply: %s", cmd, reply);

  //使能 AP
  len = sizeof(reply) - 1;
  cmd_len = snprintf(cmd, sizeof(cmd), "ENABLE");
  wpa_ctrl_request(p_wpa_ctrl, cmd, cmd_len, reply, &len, NULL);
  len = ((len >= 2) && ('\r' == reply[len - 2]) && ('\n' == reply[len - 2])) ? len - 2 : len;
  reply[len] = '\0';
  zlog_debug(__gp_zlogc, "%s reply: %s", cmd, reply);

  wpa_ctrl_close(p_wpa_ctrl);

err:
  return err;
}

/**
 * \brief AP 模式解初始化
 */
static int __ap_deinit (const char *p_ctl_path)
{
  char             cmd[1024];
  int              cmd_len;
  char             reply[1024];
  size_t           len;
  struct wpa_ctrl *p_wpa_ctrl;
  int              err = 0;

  if (NULL == p_ctl_path)
  {
    zlog_error(__gp_zlogc, "param error");
    err = -1;
    goto err;
  }

  p_wpa_ctrl = wpa_ctrl_open(p_ctl_path);
  if (NULL == p_wpa_ctrl)
  {
    zlog_error(__gp_zlogc, "wpa_ctrl_open %s error", p_ctl_path);
    err = -1;
    goto err;
  }

  //关闭接口
  len = sizeof(reply) - 1;
  cmd_len = snprintf(cmd, sizeof(cmd), "DISABLE");
  if (wpa_ctrl_request(p_wpa_ctrl, cmd, cmd_len, reply, &len, NULL) == 0)
  {
    len = ((len >= 2) && ('\r' == reply[len - 2]) && ('\n' == reply[len - 2])) ? len - 2 : len;
    reply[len] = '\0';
    zlog_debug(__gp_zlogc, "%s reply: %s", cmd, reply);
  }

  wpa_ctrl_close(p_wpa_ctrl);

err:
  return err;
}

/**
 * \brief STA 状态获取
 */
static int __sta_status_get (void)
{
  char             cmd[1024];
  int              cmd_len;
  char             reply[1024];
  char            *p_str;
  char            *p_state;
  char            *p_ip;
  size_t           len;
  struct wpa_ctrl *p_wpa_ctrl;
  int              sta_state   = -1;
  struct in_addr   sta_ip_addr = {.s_addr = htonl(INADDR_NONE)};
  int              err = 0;

  p_wpa_ctrl = wpa_ctrl_open(__g_wpa_ctrl_path);
  if (NULL == p_wpa_ctrl)
  {
    zlog_error(__gp_zlogc, "wpa_ctrl_open %s error", __g_wpa_ctrl_path);
    err = -1;
    goto err;
  }

  //获取状态
  len = sizeof(reply) - 1;
  cmd_len = snprintf(cmd, sizeof(cmd), "STATUS");
  wpa_ctrl_request(p_wpa_ctrl, cmd, cmd_len, reply, &len, NULL);
  len = ((len >= 2) && ('\r' == reply[len - 2]) && ('\n' == reply[len - 2])) ? len - 2 : len;
  reply[len] = '\0';
  // zlog_debug(__gp_zlogc, "%s reply: %s", cmd, reply);
  p_str = reply;
  p_str = str_get(&p_state, p_str, "wpa_state=", "\n");
  if (p_state != NULL)
  {
    if (strcmp(p_state, "COMPLETED") == 0)
    { //连接成功
      sta_state = 0;
      p_str = str_get(&p_ip, p_str, "ip_address=", "\n");
      if ((NULL == p_ip) ||
          (inet_pton(AF_INET, p_ip, &sta_ip_addr) != 1))
      {
        sta_ip_addr.s_addr = htonl(INADDR_NONE);
      }
    }
    else
    { //连接断开
      sta_state = -1;
      sta_ip_addr.s_addr = htonl(INADDR_NONE);
    }
  }

  __g_sta_state = sta_state;
  __g_sta_ip_addr = sta_ip_addr;

  wpa_ctrl_close(p_wpa_ctrl);

err:
  return err;
}

/**
 * \brief STA 信号获取
 */
static int __sta_signal_get (void)
{
  char             cmd[1024];
  int              cmd_len;
  char             reply[1024];
  char            *p_str;
  char            *p_avg_rssi;
  size_t           len;
  struct wpa_ctrl *p_wpa_ctrl;
  int              avg_rssi = 0;
  int              err = 0;

  p_wpa_ctrl = wpa_ctrl_open(__g_wpa_ctrl_path);
  if (NULL == p_wpa_ctrl)
  {
    zlog_error(__gp_zlogc, "wpa_ctrl_open %s error", __g_wpa_ctrl_path);
    err = -1;
    goto err;
  }

  //获取状态
  len = sizeof(reply) - 1;
  cmd_len = snprintf(cmd, sizeof(cmd), "SIGNAL_POLL");
  wpa_ctrl_request(p_wpa_ctrl, cmd, cmd_len, reply, &len, NULL);
  len = ((len >= 2) && ('\r' == reply[len - 2]) && ('\n' == reply[len - 2])) ? len - 2 : len;
  reply[len] = '\0';
  // zlog_debug(__gp_zlogc, "%s reply: %s", cmd, reply);
  p_str = reply;
  p_str = str_get(&p_avg_rssi, p_str, "AVG_RSSI=", "\n");
  if (p_avg_rssi != NULL)
  {
    avg_rssi = atoi(p_avg_rssi);
    if (avg_rssi > 128)
    {
      avg_rssi = 128;
    }
    else if (avg_rssi < -127)
    {
      avg_rssi = -127;
    }
  }

  __g_sta_avg_rssi = avg_rssi;

  wpa_ctrl_close(p_wpa_ctrl);

err:
  return err;
}

/**
 * \brief wifi 处理
 */
static int __wifi_process (void)
{
  int err = 0;

  if (WIFI_MODE_STA == __g_wifi_mode)
  { //STA 模式
    err = __sta_status_get();
    if (err != 0)
    {
      err = -1;
      goto err;
    }

    err = __sta_signal_get();
    if (err != 0)
    {
      err = -1;
      goto err;
    }
    //todo 和服务器通讯失败时，扫描 WiFi，尝试重新连接信号好的 AP
  }
  else if (WIFI_MODE_AP == __g_wifi_mode)
  { //AP 模式
    __g_sta_state = -1;
    __g_sta_ip_addr.s_addr = htonl(INADDR_NONE);
    __g_sta_avg_rssi = 0;
  }

err:
  return err;
}

/**
 * \brief wifi_ctl 线程
 */
static void *__wifi_ctl_thread (void *p_arg)
{
  char               cmd[128];
  char               ip_str[16];
  char               ip1_str[16];
  int                timer_fd;
  int                epoll_fd;
  int                ready;
  uint64_t           temp_u64;
  struct epoll_event ev;
  pid_t              pid;
  int                err = 0;

  //设置线程名称
  prctl(PR_SET_NAME, "wifi_ctl");

  //等待初始化完成
  main_wait_init();
  zlog_info(__gp_zlogc, "wifi_ctl_thread start, arg: %d", *(int *)p_arg);

  //epoll_timer 初始化
  if (epoll_timer_init(&epoll_fd, &timer_fd, 10) != 0)
  {
    zlog_fatal(__gp_zlogc, "epoll_timer_init error");
    err = -1;
    goto err;
  }

  wifi_ctl_cfg_update();

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

          if (WIFI_MODE_STA == __g_wifi_mode)
          {
            __sta_deinit(__g_wpa_ctrl_path);
          }
          else if (WIFI_MODE_AP == __g_wifi_mode)
          {
            __ap_deinit(__g_hostapd_ctrl_path);
          }

          //获取配置信息
          __cfg_read();

          //关闭 wpa_supplicant 进程
          if (process_kill("wpa_supplicant", 10000) != 0)
          {
            zlog_error(__gp_zlogc, "kill wpa_supplicant error, reboot system");
            sync();
            system("reboot -f"); //重启系统
            continue;
          }

          //关闭 udhcpc 进程
          if (process_kill("udhcpc", 10000) != 0)
          {
            zlog_error(__gp_zlogc, "kill udhcpc error, reboot system");
            sync();
            system("reboot -f"); //重启系统
            continue;
          }

          //关闭 hostapd 进程
          if (process_kill("hostapd", 10000) != 0)
          {
            zlog_error(__gp_zlogc, "kill hostapd error, reboot system");
            sync();
            system("reboot -f"); //重启系统
            continue;
          }

          //关闭 dnsmasq 进程
          if (process_kill("dnsmasq", 10000) != 0)
          {
            zlog_error(__gp_zlogc, "kill dnsmasq error, reboot system");
            sync();
            system("reboot -f"); //重启系统
            continue;
          }

          //重启网卡
          system("ip addr flush dev wlan0 && ip link set wlan0 down && ip link set wlan0 up");

          if (WIFI_MODE_DISABLE == __g_wifi_mode)
          { //WiFi 关闭模式
            //WiFi 掉电
            file_write(__RFKILL_PATH, "0", 1, O_WRONLY);
          }
          else if (WIFI_MODE_STA == __g_wifi_mode)
          { //STA 模式
            zlog_info(__gp_zlogc, "STA mode");

            //WiFi 上电
            file_write(__RFKILL_PATH, "1", 1, O_WRONLY);

            pid = process_start("wpa_supplicant -D nl80211 -i wlan0 -c /opt/jlink/etc/wpa_supplicant.conf -B",
                                "wpa_supplicant", 10000);
            if (pid <= 0)
            {
              zlog_error(__gp_zlogc, "start wpa_supplicant error, reboot system");
              sync();
              system("reboot -f"); //重启系统
              continue;
            }
            zlog_debug(__gp_zlogc, "wpa_supplicant start, pid: %d", pid);

            __sta_init(__g_wpa_ctrl_path, __g_sta_ssid, __g_sta_password, false);
            if (__g_sta_addr_mode != 1)
            { //DHCP
              zlog_info(__gp_zlogc, "DHCP mode");
              system("udhcpc -b -i wlan0 -R &");
            }
            else
            { //静态 IP
              //ip、mask 配置
              inet_ntop(AF_INET, &__g_sta_ip, ip_str, sizeof(ip_str));
              inet_ntop(AF_INET, &__g_sta_mask, ip1_str, sizeof(ip1_str));
              snprintf(cmd, sizeof(cmd), "ip addr add %s/%s dev wlan0", ip_str, ip1_str);
              system(cmd);

              //默认网关配置
              inet_ntop(AF_INET, &__g_sta_gateway, ip_str, sizeof(ip_str));
              snprintf(cmd, sizeof(cmd), "ip route add default via %s dev wlan0", ip_str);
              system(cmd);

              //dns 配置
              inet_ntop(AF_INET, &__g_sta_dns0, ip_str, sizeof(ip_str));
              inet_ntop(AF_INET, &__g_sta_dns1, ip1_str, sizeof(ip1_str));
              snprintf(cmd, sizeof(cmd), "echo -e \"nameserver %s\\nnameserver %s\" > /etc/resolv.conf", ip_str, ip1_str);
              system(cmd);
            }
          }
          else if (WIFI_MODE_AP == __g_wifi_mode)
          { //AP 模式
            zlog_info(__gp_zlogc, "AP mode");

            //WiFi 上电
            file_write(__RFKILL_PATH, "1", 1, O_WRONLY);

            pid = process_start("hostapd -i wlan0 -B /opt/jlink/etc/hostapd.conf",
                                "hostapd", 10000);
            if (pid <= 0)
            {
              zlog_error(__gp_zlogc, "start hostapd error, reboot system");
              sync();
              system("reboot -f"); //重启系统
              continue;
            }
            zlog_debug(__gp_zlogc, "hostapd start, pid: %d", pid);

            snprintf(cmd, sizeof(cmd), "%s_%d", __g_ap_ssid, jlink_ctl_sn_get());
            __ap_init(__g_hostapd_ctrl_path, cmd, __g_ap_password, false);

            //ip、mask 配置
            system("ip addr add 192.168.1.1/24 dev wlan0");
            system("dnsmasq -i wlan0 -C /opt/jlink/etc/dnsmasq.conf");
          }
        }

        //wifi 处理
        pthread_mutex_lock(&__g_mutex);
        __wifi_process();
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
 * \brief wifi_ctl WiFi-STA 地址模式获取
 */
int wifi_ctl_sta_addr_mode_get (void)
{
  return __g_sta_addr_mode;
}

/**
 * \brief wifi_ctl WiFi 模式获取
 */
enum wifi_mode wifi_ctl_mode_get (void)
{
  return __g_wifi_mode;
}

/**
 * \brief wifi_ctl STA 模式状态获取
 */
int wifi_ctl_sta_state_get (struct in_addr *p_ip_addr, int8_t *p_avg_rssi)
{
  int state = 0;

  pthread_mutex_lock(&__g_mutex);

  state = __g_sta_state;
  if (p_ip_addr != NULL)
  {
    *p_ip_addr = __g_sta_ip_addr;
  }
  if (p_avg_rssi != NULL)
  {
    *p_avg_rssi = __g_sta_avg_rssi;
  }

  pthread_mutex_unlock(&__g_mutex);

  return state;
}

/**
 * \brief wifi_ctl 配置更新
 */
int wifi_ctl_cfg_update (void)
{
  __g_cfg_update = true;
  return 0;
}

/**
 * \brief wifi_ctl 初始化
 */
int wifi_ctl_init (void)
{
  int        err   = 0;
  static int s_arg = 0;

  if (__g_is_init)
  { //已初始化
    return 0;
  }

  __gp_zlogc = zlog_get_category("wifi_ctl");

  //获取配置信息
  __cfg_read();

  __g_sta_ip_addr.s_addr = htonl(INADDR_NONE);

  if (pthread_mutex_init(&__g_mutex, NULL) != 0)
  {
    zlog_fatal(__gp_zlogc, "mutex init error");
    err = -1;
    goto err;
  }

  err = pthread_create(&__g_thread, NULL, __wifi_ctl_thread, &s_arg);
  if (err != 0)
  {
    zlog_fatal(__gp_zlogc, "wifi_ctl_thread create failed: %s", strerror(err));
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
 * \brief wifi_ctl 解初始化
 */
int wifi_ctl_deinit (void)
{
  void *p_wifi_ctl_thread_ret = NULL;

  if (!__g_is_init)
  {
    return 0;
  }

  __g_thread_run = false;
  pthread_kill(__g_thread, SIGINT);
  pthread_join(__g_thread, (void **)&p_wifi_ctl_thread_ret);
  zlog_info(__gp_zlogc, "wifi_ctl_thread exit, ret: %d", *(int *)p_wifi_ctl_thread_ret);
  pthread_mutex_destroy(&__g_mutex);
  __g_is_init = false;

  return *(int *)p_wifi_ctl_thread_ret;
}

/* end of file */
