/**
 * \file
 * \brief GPIO
 *
 * \internal
 * \par Modification history
 * - 1.00 22-07-25  zjk, first implementation
 * \endinternal
 */

#ifndef __GPIO_H
#define __GPIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/**
 * \brief GPIO 导出
 *
 * \param[in] gpio_num 引脚号
 *
 * retval  0 成功
 * retval -1 失败
 */
int gpio_export (int gpio_num);

/**
 * \brief GPIO 方向设置
 *
 * \param[in] gpio_num  引脚号
 * \param[in] direction 方向，0=输出低电平，1=输出高电平，2=输入
 *
 * retval  0 成功
 * retval -1 失败
 */
int gpio_direction_set (int gpio_num, int direction);

/**
 * \brief GPIO 电平获取
 *
 * \param[in] gpio_num 引脚号
 *
 * retval  0 低电平
 * retval  1 高电平
 * retval -1 获取失败
 */
int gpio_value_get (int gpio_num);

/**
 * \brief GPIO 电平设置
 *
 * \param[in] gpio_num 引脚号
 * \param[in] value    输出电平
 *
 * retval  0 成功
 * retval -1 失败
 */
int gpio_value_set (int gpio_num, bool value);

#ifdef __cplusplus
}
#endif

#endif //__GPIO_H

/* end of file */
