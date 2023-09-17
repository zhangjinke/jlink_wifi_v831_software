/**
 * \file
 * \brief wifi_ctl
 *
 * \internal
 * \par Modification history
 * - 1.00 22-06-15  zjk, first implementation
 * \endinternal
 */

#ifndef __WIFI_CTL_H
#define __WIFI_CTL_H

#include <arpa/inet.h>

//WiFi 模式枚举
enum wifi_mode
{
  WIFI_MODE_DISABLE = 0, //WiFi 关闭模式
  WIFI_MODE_STA,         //WiFi STA 模式
  WIFI_MODE_AP,          //WiFi AP 模式
};

/**
 * \brief wifi_ctl WiFi-STA 地址模式获取
 */
int wifi_ctl_sta_addr_mode_get (void);

/**
 * \brief wifi_ctl WiFi 模式获取
 *
 * \return WiFi 当前模式
 */
enum wifi_mode wifi_ctl_mode_get (void);

/**
 * \brief wifi_ctl STA 模式状态获取
 *
 * \param[out] p_ip_addr  指向存储获取到的 STA 模式 IP 地址的缓冲区的指针，可传入 NULL
 * \param[out] p_avg_rssi 指向存储获取到的 STA 模式 RSSI 的缓冲区的指针，可传入 NULL
 *
 * \retval  0 STA 连接成功
 * \retval -1 STA 连接断开
 */
int wifi_ctl_sta_state_get (struct in_addr *p_ip_addr, int8_t *p_avg_rssi);

/**
 * \brief wifi_ctl 配置更新
 */
int wifi_ctl_cfg_update (void);

/**
 * \brief wifi_ctl 初始化
 */
int wifi_ctl_init (void);

/**
 * \brief wifi_ctl 解初始化
 */
int wifi_ctl_deinit (void);

#endif //__WIFI_CTL_H

/* end of file */
