/**
 * \file
 * \brief jlink_ctl
 *
 * \internal
 * \par Modification history
 * - 1.00 23-03-20  zjk, first implementation
 * \endinternal
 */

#ifndef __JLINK_CTL_H
#define __JLINK_CTL_H

#include <stdbool.h>

/**
 * \brief jlink_ctl 运行状态获取
 *
 * \retval  true 正在运行
 * \retval false 停止运行
 */
bool jlink_ctl_run_get (void);

/**
 * \brief jlink_ctl 运行状态设置
 *
 * \param[in]  true 运行
 * \param[in] false 停止运行
 *
 * \retval 0 成功
 */
int jlink_ctl_run_set (bool run);

/**
 * \brief jlink_ctl S/N 获取
 *
 * \return 返回 0 表示与 J-Link 连接失败，否则为 J-Link S/N
 */
int jlink_ctl_sn_get (void);

/**
 * \brief jlink_ctl 配置更新
 */
int jlink_ctl_cfg_update (void);

/**
 * \brief jlink_ctl 初始化
 */
int jlink_ctl_init (void);

/**
 * \brief jlink_ctl 解初始化
 */
int jlink_ctl_deinit (void);

#endif //__JLINK_CTL_H

/* end of file */
