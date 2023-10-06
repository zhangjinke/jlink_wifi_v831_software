/**
 * \file
 * \brief 通用工具
 *
 * \internal
 * \par Modification history
 * - 1.00 20-04-14  zjk, first implementation
 * - 1.01 21-09-22  zjk, 添加 ARRAY_SIZE、MEMBER_SIZE、OFFSETOF
 * - 1.02 22-02-18  zjk, 添加位操作支持
 * - 1.03 23-10-05  zjk, 增加 if_ip_get()
 * \endinternal
 */

#ifndef __UTILITIES_H
#define __UTILITIES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "zlog.h"
#include <linux/if_link.h>
#include <stdbool.h>
#include <stdint.h>

//2 字节数组转换为半字，小端模式
#define ARRAY_TO_U16_L(data, idx)   ((data)[(idx)] | (((uint16_t)((data)[(idx) + 1])) << 8))

//4 字节数组转换为字，小端模式
#define ARRAY_TO_U32_L(data, idx)   ((data)[(idx)] |                           \
                                     (((uint32_t)((data)[(idx) + 1])) << 8)  | \
                                     (((uint32_t)((data)[(idx) + 2])) << 16) | \
                                     (((uint32_t)((data)[(idx) + 3])) << 24))

//2 字节数组转换为半字，大端模式
#define ARRAY_TO_U16_B(data, idx)   ((data)[(idx) + 1] | (((uint16_t)((data)[(idx)])) << 8))

//4 字节数组转换为字，大端模式
#define ARRAY_TO_U32_B(data, idx)   ((data)[(idx + 3)] |                       \
                                     (((uint32_t)((data)[(idx) + 2])) << 8)  | \
                                     (((uint32_t)((data)[(idx) + 1])) << 16) | \
                                     (((uint32_t)((data)[(idx)])) << 24))

//获取数组成员数量
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(array)           (sizeof(array) / sizeof(array[0]))
#endif

//获取结构体成员大小
#ifndef MEMBER_SIZE
#define MEMBER_SIZE(type, member)   sizeof(((type *)0)->member)
#endif

//获取结构体成员的偏移
#ifndef OFFSETOF
#define OFFSETOF(type, member)      ((size_t)(&(((type *)0)->member)))
#endif

//bit移位
#ifndef BIT
#define BIT(bit)                    (1u << (bit))
#endif

//值移位
#ifndef SBF
#define SBF(value, field)           ((value) << (field))
#endif

//bit置位
#ifndef BIT_SET
#define BIT_SET(data, bit)          ((data) |= BIT(bit))
#endif

//bit清零
#ifndef BIT_CLR
#define BIT_CLR(data, bit)          ((data) &= ~BIT(bit))
#endif

//bit置位, 根据 mask 指定的位
#ifndef BIT_SET_MASK
#define BIT_SET_MASK(data, mask)    ((data) |= (mask))
#endif

//bit清零, 根据 mask 指定的位
#ifndef BIT_CLR_MASK
#define BIT_CLR_MASK(data, mask)    ((data) &= ~(mask))
#endif

//bit翻转
#ifndef BIT_TOGGLE
#define BIT_TOGGLE(data, bit)       ((data) ^= BIT(bit))
#endif

//bit修改
#ifndef BIT_MODIFY
#define BIT_MODIFY(data, bit, value) \
          ((value) ? BIT_SET(data, bit) : BIT_CLR(data, bit))
#endif

//测试bit是否置位
#ifndef BIT_ISSET
#define BIT_ISSET(data, bit)        ((data) & BIT(bit))
#endif

//获取bit值
#ifndef BIT_GET
#define BIT_GET(data, bit)          (BIT_ISSET(data, bit) ? 1 : 0)
#endif

//获取 n bits 掩码值
#ifndef BITS_MASK
#define BITS_MASK(n)                (~((~0u) << (n)))
#endif

//获取位段值
#ifndef BITS_GET
#define BITS_GET(data, start, len)  (((data) >> (start)) & BITS_MASK(len))
#endif

//设置位段值
#ifndef BITS_SET
#define BITS_SET(data, start, len, value)                          \
          ((data) = (((data) & ~SBF(AM_BITS_MASK(len), (start))) | \
          SBF((value) & (BITS_MASK(len)), (start))))
#endif

//修改位段值
#ifndef BITS_MODIFY
#define BITS_MODIFY(data, mask_clr, mask_set)  (((data) & (~(mask_clr))) | (mask_set))
#endif

//获取 2 个数中的较大的数值
#ifndef MAX
#define MAX(x, y)  (((x) < (y)) ? (y) : (x))
#endif

//获取 2 个数中的较小的数值
#ifndef MIN
#define MIN(x, y)  (((x) < (y)) ? (x) : (y))
#endif

//zlog 类别
extern zlog_category_t *gp_utilities_zlogc;

/**
 * \brief IP 地址合法检测
 *
 * \param[in] p_ip 指向存储 IP 地址的缓冲区的指针，缓冲区大小必须大于等于 4
 *
 * \retval  true 合法
 * \retval false 非法
 */
bool ip_check (const uint8_t *p_ip);

/**
 * \brief 子网掩码合法检测
 *
 * \param[in] p_mask 指向存储子网掩码的缓冲区的指针，缓冲区大小必须大于等于 4
 *
 * \retval  true 合法
 * \retval false 非法
 */
bool mask_check (const uint8_t *p_mask);

/**
 * \brief IP 地址是否在同一网段检测
 *
 * \param[in] p_ip0  指向存储 IP 地址 0 的缓冲区的指针，缓冲区大小必须大于等于 4
 * \param[in] p_ip1  指向存储 IP 地址 1 的缓冲区的指针，缓冲区大小必须大于等于 4
 * \param[in] p_mask 指向存储子网掩码的缓冲区的指针，缓冲区大小必须大于等于 4
 *
 * \retval  true 合法
 * \retval false 非法
 */
bool network_segment_check (const uint8_t *p_ip0, const uint8_t *p_ip1, const uint8_t *p_mask);

/**
 * \brief 网络连接参数获取
 *
 * \param[in]  p_if_name 网卡名称
 * \param[out] p_stats   指向存储获取到的网络连接参数的缓冲区的指针
 *
 * \retval  0 成功
 * \retval -1 失败
 */
int if_link_stats_get (const char *p_if_name, struct rtnl_link_stats *p_stats);

/**
 * \brief 网卡 MAC 地址获取
 *
 * \param[in]  p_if_name 网卡名称
 * \param[out] p_mac     指向存储获取到的网卡 MAC 地址的缓冲区的指针，长度必须大于等于 6
 *
 * \retval  0 成功
 * \retval -1 失败
 */
int if_mac_get (const char *p_if_name, void *p_mac);

/**
 * \brief 网卡 IP 地址获取
 *
 * \param[in]  p_if_name 网卡名称
 * \param[out] p_ip      指向存储获取到的网卡 IP 地址的缓冲区的指针，长度必须大于等于 4
 *
 * \retval  0 成功
 * \retval -1 失败
 */
int if_ip_get (const char *p_if_name, void *p_ip);

/**
 * \brief epoll_timer 初始化
 *
 * \param[out] p_epoll_fd      指向存储 epoll_fd 的缓冲区的指针
 * \param[out] p_timer_fd      指向存储 epoll_fd 的缓冲区的指针
 * \param[in]  timer_period_ms 定时器超时周期，单位 ms
 *
 * \retval  0 成功
 * \retval -1 失败
 */
int epoll_timer_init (int *p_epoll_fd, int *p_timer_fd, int timer_period_ms);

/**
 * \brief utilities 初始化
 */
int utilities_init (void);

#ifdef __cplusplus
}
#endif

#endif //__UTILITIES_H

/* end of file */
