/**
 * \file
 * \brief 信息矿灯主程序
 *
 * \internal
 * \par Modification history
 * - 1.00 22-07-10  zjk, first implementation
 * \endinternal
 */

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <arpa/inet.h>

/**
 * \brief main STA 模式下，最近一次的 IP 地址获取
 */
struct in_addr main_sta_last_ip_get (void);

/**
 * \brief main 配置更新
 */
int main_cfg_update (void);

/**
 * \brief 工作状态获取
 */
enum main_state main_state_get (void);

/**
 * \brief 等待初始化完成
 */
void main_wait_init (void);

#ifdef __cplusplus
}
#endif

#endif //__MAIN_H

/* end of file */
