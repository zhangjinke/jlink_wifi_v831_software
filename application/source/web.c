/**
 * \file
 * \brief web
 *
 * \internal
 * \par Modification history
 * - 1.00 23-04-04  zjk, first implementation
 * \endinternal
 */

#include "web.h"
#include "cfg.h"
#include "config.h"
#include "file.h"
#include "jlink_ctl.h"
#include "main.h"
#include "process.h"
#include "str.h"
#include "systick.h"
#include "utilities.h"
#include "wifi_ctl.h"
#include "zlog.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <unistd.h>

/*******************************************************************************
  宏定义
*******************************************************************************/

/*******************************************************************************
  本地全局变量声明
*******************************************************************************/

//WEB 状态枚举
enum web_state
{
  WEB_STATE_NO_INIT = 0, //未初始化态
  WEB_STATE_WAIT,        //等待态
  WEB_STATE_IDLE,        //空闲态
};

//HTTP 状态枚举
enum http_state
{
  HTTP_STATE_WAIT_METHOD = 0, //等待接收请求方法态
  HTTP_STATE_RECV_HEAD,       //接收 HTTP 头态
  HTTP_STATE_RECV_BODY,       //接收 HTTP 数据体态
};

//HTTP 客户端
struct http_client
{
  int                cfd;            //client 文件描述符
  struct sockaddr_in caddr;          //client 地址
  bool               close_req;      //连接关闭请求
  uint8_t            recv_buf[4096]; //接收缓冲区
  size_t             recv_num;       //接收缓冲区有效数据数量
};

//HTTP 请求结构体
struct http_req
{
  enum http_state http_state;     //HTTP 状态
  int             major_version;  //主版本号
  int             minor_version;  //次版本号
  char            method[32];     //请求方法
  char            path[256];      //请求路径
  bool            keepalive;      //是否保持连接
  int             content_length; //内容长度
  char            content[4096];  //内容
  int             content_num;    //内容有效数据数量
};

//HTTP 响应结构体
struct http_resp
{
  int         major_version;    //主版本号
  int         minor_version;    //次版本号
  int         status_code;      //状态码
  const char *p_status_message; //状态消息
  const char *p_location;       //重定位路径
  const char *p_content_type;   //内容类型
  bool        keepalive;        //是否保持连接
  int         content_length;   //内容长度
};

//HTTP 服务器结构体
struct http_server
{
  int                sfd;    //socket 文件描述符
  struct http_client client; //HTTP 客户端
  struct http_req    req;    //HTTP 请求
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
static pthread_mutex_t __g_mutex = {0};

//是否初始化
static bool __g_is_init = false;

static volatile bool __g_cfg_update = false; //配置更新标记

//HTTP 服务器
static struct http_server __g_http_server = {0};

/*******************************************************************************
  内部函数定义
*******************************************************************************/

/**
 * \brief 配置读取
 */
static int __cfg_read (void)
{
  return 0;
}

/**
 * \brief HTTP 服务器初始化
 */
static int __http_server_init (struct http_server *p_http_server, const char *p_host, uint16_t port)
{
  struct sockaddr_in server_addr = {0};
  int                err         = 0;

  if ((p_http_server->sfd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0)) == -1)
  {
    err = -1;
    zlog_error(__gp_zlogc, "create socket error: %s", strerror(errno));
    goto err;
  }
  fcntl(p_http_server->sfd, F_SETFL, fcntl(p_http_server->sfd, F_GETFL) | O_NONBLOCK);

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family      = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr(p_host);
  server_addr.sin_port        = htons(port);
  if (bind(p_http_server->sfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
  {
    err = -1;
    zlog_error(__gp_zlogc, "bind socket error: %s", strerror(errno));
    goto err_socket_close;
  }

  if (listen(p_http_server->sfd, 8) == -1)
  {
    err = -1;
    zlog_error(__gp_zlogc, "listen socket error: %s", strerror(errno));
    goto err_socket_close;
  }

  goto err;

err_socket_close:
  close(p_http_server->sfd);
err:
  return err;
}

/**
 * \brief HTTP 应答
 */
static int __http_write (struct http_server *p_http_server, const void *p_buf, size_t buf_size)
{
  const char *p_char = p_buf;
  ssize_t     nwrite = 0;
  int         left   = 0;
  int         idx    = 0;

  left = buf_size;
  while (left > 0)
  {
    nwrite = write(p_http_server->client.cfd, p_char + idx, left);
    if (nwrite <= 0)
    {
      if (errno != EAGAIN)
      {
        break;
      }
      usleep(1000);
    }
    else
    {
      idx += nwrite;
      left -= nwrite;
    }
  }

  return (0 == left) ? 0 : -1;
}

/**
 * \brief HTTP 响应打包
 */
static int http_resp_package (struct http_resp *p_resp, char *p_buf, int len)
{
  int idx = 0;

  idx += snprintf(p_buf + idx, len - idx, "HTTP/%d.%d %d %s\r\n",
                  p_resp->major_version, p_resp->minor_version, p_resp->status_code, p_resp->p_status_message);

  //应答头
  idx += snprintf(p_buf + idx, len - idx, "Server: jlink/%s\r\n", VERSION);
  idx += snprintf(p_buf + idx, len - idx, "Connection: %s\r\n", p_resp->keepalive ? "keep-alive" : "close");
  if (p_resp->content_length > 0)
  {
    idx += snprintf(p_buf + idx, len - idx, "Content-Length: %d\r\n", p_resp->content_length);
  }
  if ((p_resp->p_content_type != NULL) && (p_resp->p_content_type[0] != '\0'))
  {
    idx += snprintf(p_buf + idx, len - idx, "Content-Type: %s\r\n", p_resp->p_content_type);
  }

  if ((p_resp->status_code >= 300) && (p_resp->status_code < 400) &&
      (p_resp->p_location != NULL) && (p_resp->p_location[0] != '\0'))
  {
    idx += snprintf(p_buf + idx, len - idx, "Location: %s\r\n", p_resp->p_location);
  }

  idx += snprintf(p_buf + idx, len - idx, "\r\n");

  return idx;
}

/**
 * \brief HTTP 应答
 */
static int __http_reply (struct http_server *p_http_server,
                         struct http_resp   *p_resp,
                         int                 status_code,
                         const char         *p_status_message,
                         const char         *p_content_type,
                         const void         *p_content,
                         int                 content_len)
{
  char             buf[4096] = {0};
  int              len       = 0;
  struct http_req *p_req     = &p_http_server->req;
  int              err       = 0;

  p_resp->major_version    = p_req->major_version;
  p_resp->minor_version    = p_req->minor_version;
  p_resp->status_code      = status_code;
  p_resp->p_status_message = p_status_message;
  p_resp->p_content_type   = p_content_type;
  p_resp->keepalive        = p_req->keepalive;
  if (p_content)
  {
    if (content_len <= 0)
    {
      content_len = strlen(p_content);
    }
    p_resp->content_length = content_len;
  }

  len = http_resp_package(p_resp, buf, sizeof(buf));
  err = __http_write(p_http_server, buf, len);
  if ((0 == err) && (p_content != NULL) && (p_resp->content_length > 0))
  {
    err = __http_write(p_http_server, p_content, p_resp->content_length);
  }

  return err;
}

/**
 * \brief 文件发送
 */
static void __file_send (struct http_server *p_http_server, const char *p_path)
{
  int              size           = 0;
  char            *p_buf          = 0;
  char             path[PATH_MAX] = {0};
  char            *p_type         = NULL;
  struct http_resp resp           = {0};

  snprintf(path, sizeof(path), "../resource/www/%s", p_path);
  size = file_size_get(path);
  if (size <= 0)
  {
    goto err;
  }

  p_buf = malloc(size + 1024);
  if (NULL == p_buf)
  {
    goto err;
  }

  size = file_read(path, p_buf, size, O_RDONLY);
  if (size <= 0)
  {
    goto err_free;
  }

  p_type = strrchr(path, '.');
  if (p_type != NULL)
  {
    p_type++;
    if (strcmp(p_type, "html") == 0)
    {
      p_type = "text/html";
    }
    else if (strcmp(p_type, "gif") == 0)
    {
      p_type = "image/gif";
    }
    else if (strcmp(p_type, "log") == 0)
    {
      p_type = "application/octet-stream";
    }
  }

  __http_reply(p_http_server, &resp, 200, "OK", p_type, p_buf, size);

err_free:
  free(p_buf);
err:
  return;
}

/**
 * \brief 登录页面发送
 */
static void __http_login_send (struct http_server *p_http_server, const char *p_info)
{
  int              size            = 0;
  char            *p_buf           = 0;
  const char      *p_path          = NULL;
  struct http_resp resp            = {0};
  char             buf[128]        = {0};
  int              bat_capacity    = 0;
  float            bat_voltage     = 0;
  bool             bat_charge      = false;
  int8_t           wifi_rssi       = 0;
  struct in_addr   ip_addr         = {0};
  int              system_info_len = 0;

  p_path = "../resource/www/login.html";
  size = file_size_get(p_path);
  if (size <= 0)
  {
    goto err;
  }

  p_buf = malloc(size + 1024);
  if (NULL == p_buf)
  {
    goto err;
  }

  size = file_read(p_path, p_buf, size, O_RDONLY);
  if (size <= 0)
  {
    goto err_free;
  }

  if (file_read("/sys/class/power_supply/battery/capacity", buf, sizeof(buf), O_RDONLY) > 0)
  {
    bat_capacity = atoi(buf);
    if (bat_capacity >= 0)
    {
      if (file_read("/sys/class/power_supply/battery/voltage_now", buf, sizeof(buf), O_RDONLY) > 0)
      {
        bat_voltage = atoi(buf) / 1000000.0f;
      }
      if (file_read("/sys/class/power_supply/battery/status", buf, sizeof(buf), O_RDONLY) > 0)
      {
        if (memcmp(buf, "Charging", sizeof("Charging") - 1) == 0)
        {
          bat_charge = true;
        }
      }
      system_info_len = snprintf(buf + system_info_len, sizeof(buf) - system_info_len,
                                 "电量: %d%% 电池电压: %.3fV %s",
                                 bat_capacity, bat_voltage,
                                 bat_charge ? "充电中 " : " ");
    }
  }

  if ((wifi_ctl_mode_get() == WIFI_MODE_STA) && (wifi_ctl_sta_state_get(NULL, &wifi_rssi) == 0))
  {
    system_info_len += snprintf(buf + system_info_len, sizeof(buf) - system_info_len, "WiFi RSSI: %ddBm ", wifi_rssi);
  }
  else if (wifi_ctl_mode_get() == WIFI_MODE_AP)
  {
    ip_addr = main_sta_last_ip_get();
    if (ip_addr.s_addr != htonl(INADDR_NONE))
    {
      system_info_len += snprintf(buf + system_info_len, sizeof(buf) - system_info_len, "Last STA IP: %s ", inet_ntoa(ip_addr));
    }
  }

  system_info_len += snprintf(buf + system_info_len, sizeof(buf) - system_info_len, "J-Link S/N: %d", jlink_ctl_sn_get());

  size += sprintf(p_buf + size, "<script>document.getElementById('system_info').innerHTML='%s';</script>", buf);
  size += sprintf(p_buf + size, "<script>document.getElementById('version').innerHTML='%s %s';</script>", CFG_DEV_NAME, VERSION);
  size += sprintf(p_buf + size, "<script>document.getElementById('info').innerHTML='%s';</script>", (p_info == NULL) ? "" : p_info);
  __http_reply(p_http_server, &resp, 200, "OK", "text/html", p_buf, size);

err_free:
  free(p_buf);
err:
  return;
}

/**
 * \brief 配置页面 1 发送
 */
static void __http_config1_send (struct http_server *p_http_server, const char *p_info)
{
  int              size             = 0;
  char            *p_buf            = 0;
  const char      *p_path           = NULL;
  struct http_resp resp             = {0};
  char             if_name[33]      = {0};
  char             sta_ssid[33]     = {0};
  char             sta_password[65] = {0};
  uint8_t          mac[6]           = {0};

  p_path = "../resource/www/config1.html";
  size = file_size_get(p_path);
  if (size <= 0)
  {
    goto err;
  }

  p_buf = malloc(size + 1024);
  if (NULL == p_buf)
  {
    goto err;
  }

  size = file_read(p_path, p_buf, size, O_RDONLY);
  if (size <= 0)
  {
    goto err_free;
  }

  cfg_str_get("wifi", "if_name", if_name, sizeof(if_name), "wlan0");
  cfg_str_get("wifi", "sta_ssid", sta_ssid, sizeof(sta_ssid), "");
  cfg_str_get("wifi", "sta_password", sta_password, sizeof(sta_password), "");
  if_mac_get(if_name, &mac[0]);
  size += sprintf(p_buf + size,
                  "<script>document.getElementById('macaddr').innerHTML='MAC地址:%02X-%02X-%02X-%02X-%02X-%02X';</script>",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  size += sprintf(p_buf + size, "<script>setform.T0.value='%s';</script>", sta_ssid);
  size += sprintf(p_buf + size, "<script>setform.T1.value='%s';</script>", sta_password);
  size += sprintf(p_buf + size, "<script>document.getElementById('info').innerHTML='%s';</script>", (p_info == NULL) ? "" : p_info);
  __http_reply(p_http_server, &resp, 200, "OK", "text/html", p_buf, size);

err_free:
  free(p_buf);
err:
  return;
}

/**
 * \brief MAC 地址设置页面发送
 */
static void __http_mac_set_send (struct http_server *p_http_server, const char *p_info)
{
  int              size        = 0;
  char            *p_buf       = 0;
  const char      *p_path      = NULL;
  struct http_resp resp        = {0};
  char             if_name[33] = {0};
  uint8_t          mac[6]      = {0};

  p_path = "../resource/www/m.html";
  size = file_size_get(p_path);
  if (size <= 0)
  {
    goto err;
  }

  p_buf = malloc(size + 1024);
  if (NULL == p_buf)
  {
    goto err;
  }

  size = file_read(p_path, p_buf, size, O_RDONLY);
  if (size <= 0)
  {
    goto err_free;
  }

  cfg_str_get("wifi", "if_name", if_name, sizeof(if_name), "wlan0");
  if_mac_get(if_name, &mac[0]);
  size += sprintf(p_buf + size,
                  "<script>document.getElementById('macaddrset').innerHTML='%02X-%02X-%02X-%02X-%02X-%02X';</script>",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  size += sprintf(p_buf + size, "<script>document.getElementById('info').innerHTML='%s';</script>", (p_info == NULL) ? "" : p_info);
  __http_reply(p_http_server, &resp, 200, "OK", "text/html", p_buf, size);

err_free:
  free(p_buf);
err:
  return;
}

/**
 * \brief 请求处理
 */
static void __req_process (struct http_server *p_http_server)
{
  char             cmd[128]        = {0};
  char            *p_cur           = NULL;
  char            *p_str           = NULL;
  const char      *p_info          = NULL;
  static bool      s_password_pass = false;
  static uint32_t  s_password_tick = 0;
  struct http_req *p_req           = &p_http_server->req;
  struct http_resp resp            = {0};
  uint32_t         systick         = systick_get();
  uint8_t          mac[6]          = {0};
  int              err             = 0;

  p_http_server->client.close_req = true;
  memset(&resp, 0, sizeof(resp));

  if (strcmp(p_req->method, "GET") == 0)
  {
    if ((strcmp(p_req->path, "/") == 0))
    { //重定位到登录页面
      resp.p_location = "/login.html";
      __http_reply(p_http_server, &resp, 302, "Found", "text/html", NULL, 0);
    }
    else if (strcmp(p_req->path, "/login.html") == 0)
    { //登录页面
      __http_login_send(p_http_server, NULL);
    }
    else if ((strcmp(p_req->path, "/m") == 0))
    { //重定位到 MAC 地址设置页面
      resp.p_location = "/m.html";
      __http_reply(p_http_server, &resp, 302, "Found", "text/html", NULL, 0);
    }
    else if (strcmp(p_req->path, "/m.html") == 0)
    { //MAC 地址设置页面
      __http_mac_set_send(p_http_server, NULL);
    }
    else if (strcmp(p_req->path, "/logo.gif") == 0)
    { //logo 文件
      __file_send(p_http_server, "logo.gif");
    }
  }
  else if (strcmp(p_req->method, "POST") == 0)
  {
    p_cur = p_req->content;
    if (s_password_pass && ((systick - s_password_tick) >= 120000))
    { //密码超时
      s_password_pass = false;
    }

    if (strcmp(p_req->path, "/config1.html") == 0)
    { //网络模块配置页面
      p_cur = str_get(&p_str, p_cur, "pwd=", "&");
      if ((NULL == p_str) || (strcmp(p_str, "12345678") != 0))
      {
        __http_login_send(p_http_server, "您输入的密码错误!");
      }
      else
      { //密码正确
        s_password_pass = true;
        s_password_tick = systick;
        __http_config1_send(p_http_server, NULL);
      }
    }
    else if (strcmp(p_req->path, "/save1.html") == 0)
    { // 网络配置页面
      if (!s_password_pass)
      { //密码校验未通过
        resp.p_location = "/login.html";
        __http_reply(p_http_server, &resp, 302, "Found", "text/html", NULL, 0);
      }
      else
      { //密码校验通过
        //SSID
        p_cur = str_get(&p_str, p_cur, "T0=", "&");
        if (p_str != NULL)
        {
          zlog_info(__gp_zlogc, "sta_ssid set: %s", p_str);
          cfg_str_set("wifi", "sta_ssid", p_str);
        }

        //密码
        p_cur = str_get(&p_str, p_cur, "T1=", "&");
        if (p_str != NULL)
        {
          zlog_info(__gp_zlogc, "sta_password set: %s", p_str);
          cfg_str_set("wifi", "sta_password", p_str);
        }

        p_info = "保存成功";
        __http_config1_send(p_http_server, p_info);
        wifi_ctl_cfg_update();
      }
    }
    else if (strcmp(p_req->path, "/m.html") == 0)
    { //MAC 地址设置
      p_cur = str_get(&p_str, p_cur, "mac=", "&");
      if (NULL == p_str)
      { //未收到 MAC 地址
        err = -1;
      }
      else
      {
        if (str_mac_to_array(mac, p_str) != 0)
        { //MAC 地址错误
          err = -1;
        }
      }

      if (-1 == err)
      {
        __http_mac_set_send(p_http_server, "您输入的MAC地址错误!");
      }
      else
      {
        snprintf(cmd, sizeof(cmd), "echo \"%02x:%02x:%02x:%02x:%02x:%02x\" >/etc/wifi/xr_wifi.conf",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        system(cmd);
        __http_mac_set_send(p_http_server, "修改MAC地址成功!");
        zlog_info(__gp_zlogc,
                  "web set mac: %02x:%02x:%02x:%02x:%02x:%02x ",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        //重启生效
        sync();
        system("reboot -f");
      }
    }
  }

  return;
}

/**
 * \brief 接收状态机
 */
static void __recv_state_machine (struct http_server *p_http_server)
{
  int                 ret      = 0;
  size_t              size     = 0;
  struct http_client *p_client = &p_http_server->client;
  struct http_req    *p_req    = &p_http_server->req;

  switch (p_req->http_state)
  {
    case HTTP_STATE_WAIT_METHOD:
    {
      ret = sscanf((char *)p_client->recv_buf,
                   "%31s %255s HTTP/%d.%d",
                   p_req->method, p_req->path, &p_req->major_version, &p_req->minor_version);
      if (ret != 4)
      {
        zlog_error(__gp_zlogc, "method error: %s", (char *)p_client->recv_buf);
        p_http_server->client.close_req = true;
        break;
      }

      zlog_debug(__gp_zlogc, "method: %s path: %s", p_req->method, p_req->path);
      p_req->http_state = HTTP_STATE_RECV_HEAD;
      break;
    }
    break;

    case HTTP_STATE_RECV_HEAD:
    {
      if (memcmp(p_client->recv_buf, "Content-Length", sizeof("Content-Length") - 1) == 0)
      {
        p_req->content_length = atoi((char *)p_client->recv_buf + sizeof("Content-Length") - 1 + 1);
      }
      else if (memcmp(p_client->recv_buf, "\r\n", sizeof("\r\n") - 1) == 0)
      {
        if (0 == p_req->content_length)
        {
          //HTTP 请求完成
          __req_process(p_http_server);
          memset(p_req, 0, sizeof(*p_req));
          p_req->http_state = HTTP_STATE_WAIT_METHOD;
          break;
        }
        else
        {
          p_req->http_state = HTTP_STATE_RECV_BODY;
          break;
        }
      }
    }
    break;

    case HTTP_STATE_RECV_BODY:
    {
      size = p_req->content_num + p_client->recv_num;
      if (size >= p_req->content_length)
      {
        size = p_req->content_length - p_req->content_num;
        memcpy(p_req->content + p_req->content_num, p_client->recv_buf, size);
        p_req->content_num = p_req->content_length;

        //HTTP 请求完成
        __req_process(p_http_server);
        memset(p_req, 0, sizeof(*p_req));
        p_req->http_state = HTTP_STATE_WAIT_METHOD;
        break;
      }
      else
      {
        memcpy(p_req->content + p_req->content_num, p_client->recv_buf, p_client->recv_num);
        p_req->content_num += p_client->recv_num;
      }
    }
    break;

    default:
    {
      p_http_server->client.close_req = true;
    }
    break;
  }
}

/**
 * \brief 接收处理
 */
static void __recv_process (struct http_server *p_http_server, uint8_t *p_buf, size_t len)
{
  size_t i   = 0;
  size_t num = 0;

  if (p_http_server->req.http_state != HTTP_STATE_RECV_BODY)
  { //接收 HTTP 头，根据 \r\n 处理
    for (i = 0; i < len; i++)
    {
      p_http_server->client.recv_buf[p_http_server->client.recv_num++] = p_buf[i];
      if ((p_http_server->client.recv_num >= 2) &&
          ('\r' == p_http_server->client.recv_buf[p_http_server->client.recv_num - 2]) &&
          ('\n' == p_http_server->client.recv_buf[p_http_server->client.recv_num - 1]))
      { //完成一行接收
        if (2 == p_http_server->client.recv_num)
        {
          p_http_server->client.recv_buf[p_http_server->client.recv_num] = '\0';
        }
        else
        {
          p_http_server->client.recv_buf[p_http_server->client.recv_num - 2] = '\0';
          p_http_server->client.recv_num -= 2;
        }
        __recv_state_machine(p_http_server);
        p_http_server->client.recv_num = 0;
        if (HTTP_STATE_RECV_BODY == p_http_server->req.http_state)
        {
          i++;
          break;
        }
      }
      else if (p_http_server->client.recv_num >= (sizeof(p_http_server->client.recv_buf) - 1))
      {
        zlog_warn(__gp_zlogc, "recv buf full");
        break;
      }
    }
  }

  while ((HTTP_STATE_RECV_BODY == p_http_server->req.http_state) && (i < len))
  {
    if ((len - i) > (sizeof(p_http_server->client.recv_buf) - p_http_server->client.recv_num))
    {
      num = sizeof(p_http_server->client.recv_buf) - p_http_server->client.recv_num;
    }
    else
    {
      num = len - i;
    }
    memcpy(&p_http_server->client.recv_buf[p_http_server->client.recv_num],
           &p_buf[i],
            num);
    p_http_server->client.recv_num += num;
    i -= num;
    __recv_state_machine(p_http_server);
    p_http_server->client.recv_num = 0;
  }
}

/**
 * \brief web 处理
 */
static void __web_process (int epoll_fd, struct epoll_event *p_ev)
{
  int                   cfd          = 0;
  struct sockaddr_in    caddr        = {0};
  socklen_t             socklen      = 0;
  uint8_t               buf[4096]    = {0};
  ssize_t               nread        = 0;
  struct epoll_event    ev           = {0};
  uint32_t              systick      = systick_get();
  static enum web_state s_state      = WEB_STATE_NO_INIT;
  static enum web_state s_state_next = WEB_STATE_NO_INIT;
  static uint32_t       s_wait_ms    = 0;
  static uint32_t       s_tick       = 0;

  switch (s_state)
  {
    case WEB_STATE_NO_INIT:
    {
      memset(&__g_http_server, 0, sizeof(__g_http_server));
      if (__http_server_init(&__g_http_server, "0.0.0.0", 80) != 0)
      {
        s_tick = systick;
        s_wait_ms = 5000;
        s_state_next = WEB_STATE_NO_INIT;
        s_state = WEB_STATE_WAIT;
        break;
      }

      // 添加 socket 到 epoll
      ev.events = EPOLLIN;
      ev.data.fd = __g_http_server.sfd;
      if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, __g_http_server.sfd, &ev) == -1)
      {
        zlog_error(__gp_zlogc, "epoll_ctl add error: %s", strerror(errno));
        close(__g_http_server.sfd);
        s_tick = systick;
        s_wait_ms = 5000;
        s_state_next = WEB_STATE_NO_INIT;
        s_state = WEB_STATE_WAIT;
        break;
      }

      s_state = WEB_STATE_IDLE;
      break;
    }
    break;

    case WEB_STATE_WAIT:
    {
      if ((systick - s_tick) >= s_wait_ms)
      {
        s_state = s_state_next;
        break;
      }
    }
    break;

    case WEB_STATE_IDLE:
    {
      if (NULL == p_ev)
      {
        break;
      }

      if (p_ev->data.fd == __g_http_server.sfd)
      {
        memset(&caddr, 0, sizeof(caddr));
        socklen = sizeof(caddr);
        cfd = accept(__g_http_server.sfd, (struct sockaddr *)&caddr, &socklen);
        if (-1 == cfd)
        {
          zlog_error(__gp_zlogc, "accept socket error: %s", strerror(errno));
          break;
        }

        if (__g_http_server.client.cfd > 0)
        {
          zlog_error(__gp_zlogc, "accept socket error: busy, current socket %d", __g_http_server.client.cfd);
          close(cfd);
          break;
        }
        memset(&__g_http_server.client, 0, sizeof(__g_http_server.client));
        memset(&__g_http_server.req, 0, sizeof(__g_http_server.req));
        __g_http_server.client.cfd = cfd;
        __g_http_server.client.caddr = caddr;

        // 添加 client 到 epoll
        ev.events = EPOLLIN;
        ev.data.fd = __g_http_server.client.cfd;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, __g_http_server.client.cfd, &ev) == -1)
        {
          zlog_error(__gp_zlogc, "epoll_ctl add error: %s", strerror(errno));
          close(__g_http_server.client.cfd);
          __g_http_server.client.cfd = 0;
          break;
        }

        zlog_info(__gp_zlogc, "accept socket %d addr: %s port: %u", cfd, inet_ntoa(caddr.sin_addr), ntohs(caddr.sin_port));
      }
      else if (p_ev->data.fd == __g_http_server.client.cfd)
      {
        nread = read(__g_http_server.client.cfd, buf, sizeof(buf));
        if (nread <= 0)
        { //连接断开
          if (__g_http_server.client.recv_num > 0)
          {
            __recv_state_machine(&__g_http_server);
          }
          epoll_ctl(epoll_fd, EPOLL_CTL_DEL, __g_http_server.client.cfd, NULL);
          close(__g_http_server.client.cfd);
          __g_http_server.client.cfd = 0;
          zlog_error(__gp_zlogc, "socket remote close");
        }
        else
        {
          __recv_process(&__g_http_server, buf, nread);
          if (__g_http_server.client.close_req)
          {
            zlog_error(__gp_zlogc, "socket local close");
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, __g_http_server.client.cfd, NULL);
            close(__g_http_server.client.cfd);
            __g_http_server.client.cfd = 0;
          }
        }
      }
    }
    break;

    default:
    {
      s_state = WEB_STATE_NO_INIT;
    }
    break;
  }
}

/**
 * \brief web 线程
 */
static void *__web_thread (void *p_arg)
{
  int                timer_fd;
  int                epoll_fd;
  int                ready;
  uint64_t           temp_u64;
  struct epoll_event ev;
  int                err = 0;

  //设置线程名称
  prctl(PR_SET_NAME, "web");

  //等待初始化完成
  main_wait_init();
  zlog_info(__gp_zlogc, "web_thread start, arg: %d", *(int *)p_arg);

  //epoll_timer 初始化
  if (epoll_timer_init(&epoll_fd, &timer_fd, 10) != 0)
  {
    zlog_fatal(__gp_zlogc, "epoll_timer_init error");
    err = -1;
    goto err;
  }

  web_cfg_update();

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

        //web 处理
        pthread_mutex_lock(&__g_mutex);
        __web_process(epoll_fd, NULL);
        pthread_mutex_unlock(&__g_mutex);
      }
      else
      {
        pthread_mutex_lock(&__g_mutex);
        __web_process(epoll_fd, &ev);
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
 * \brief web 配置更新
 */
int web_cfg_update (void)
{
  __g_cfg_update = true;
  return 0;
}

/**
 * \brief web 初始化
 */
int web_init (void)
{
  int        err   = 0;
  static int s_arg = 0;

  if (__g_is_init)
  { //已初始化
    return 0;
  }

  __gp_zlogc = zlog_get_category("web");

  //获取配置信息
  __cfg_read();

  if (pthread_mutex_init(&__g_mutex, NULL) != 0)
  {
    zlog_fatal(__gp_zlogc, "mutex init error");
    err = -1;
    goto err;
  }

  err = pthread_create(&__g_thread, NULL, __web_thread, &s_arg);
  if (err != 0)
  {
    zlog_fatal(__gp_zlogc, "web_thread create failed: %s", strerror(err));
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
 * \brief web 解初始化
 */
int web_deinit (void)
{
  void *p_web_thread_ret = NULL;

  if (!__g_is_init)
  {
    return 0;
  }

  __g_thread_run = false;
  pthread_kill(__g_thread, SIGINT);
  pthread_join(__g_thread, (void **)&p_web_thread_ret);
  zlog_info(__gp_zlogc, "web_thread exit, ret: %d", *(int *)p_web_thread_ret);
  pthread_mutex_destroy(&__g_mutex);
  __g_is_init = false;

  return *(int *)p_web_thread_ret;
}

/* end of file */
