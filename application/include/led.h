/**
 * \file
 * \brief LED
 *
 * \internal
 * \par Modification history
 * - 1.00 23-02-01  zjk, first implementation
 * \endinternal
 */

#ifndef __LED_H
#define __LED_H

#include <stdint.h>

#define LED_TRIGGER_NONE       "none"
#define LED_TRIGGER_TIMER      "timer"
#define LED_TRIGGER_HEARTBEAT  "heartbeat"

//LED 枚举
enum led
{
  LED_STATE = 0, //状态 LED
  LED_ERROR,     //错误 LED
  LED_MAX,
};

/**
 * \brief LED 触发器设置
 *
 * \param[in] led       LED，参见 enum led
 * \param[in] p_trigger LED 触发器，参见 LED_TRIGGER_*，如 LED_TRIGGER_HEARTBEAT
 *
 * \retval  0 成功
 * \retval -1 失败
 */
int led_trigger_set (enum led led, const char *p_trigger);

/**
 * \brief led 亮度设置
 *
 * \param[in] led        LED，参见 enum led
 * \param[in] brightness 点亮时间，0 表示常亮
 *
 * \retval  0 成功
 * \retval -1 失败
 */
int led_brightness_set (enum led led, uint32_t brightness);

/**
 * \brief led 定时器设置
 *
 * \param[in] led       LED，参见 enum led
 * \param[in] delay_on  点亮时间，0 表示常亮，此时 delay_off 禁止为 0
 * \param[in] delay_off 熄灭时间，0 表示常灭，此时 delay_on 禁止为 0
 *
 * \retval  0 成功
 * \retval -1 失败
 */
int led_timer_set (enum led led, uint32_t delay_on, uint32_t delay_off);

/**
 * \brief LED 初始化
 */
int led_init (void);

/**
 * \brief LED 解初始化
 */
int led_deinit (void);

#endif //__LED_H

/* end of file */
