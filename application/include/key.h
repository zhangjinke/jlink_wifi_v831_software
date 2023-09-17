/**
 * \file
 * \brief key
 *
 * \internal
 * \par Modification history
 * - 1.00 22-07-01  zjk, first implementation
 * \endinternal
 */

#ifndef __KEY_H
#define __KEY_H

#include <stdint.h>

//按键枚举
enum key
{
  KEY_USER_KEY = 0, //按键
  KEY_USER_POWER,   //电源键
  KEY_USER_MAX,
};

//按键状态枚举
enum key_state
{
  KEY_STATE_RELEASE = 0, //按键释放态
  KEY_STATE_PRESS,       //按键按下态
};

//按键事件枚举
enum key_event
{
  KEY_EVENT_NONE = 0,   //无事件
  KEY_EVENT_CLICK,      //按键单击事件
  KEY_EVENT_LONG_PRESS, //按键长按事件
  KEY_EVENT_CLEAN,      //事件已清除
};

//按键信息
struct key_info
{
  enum key       key;          //按键
  enum key_state state;        //按键状态
  enum key_event event;        //按键事件
  uint32_t       tick_release; //按键释放时刻
  uint32_t       tick_press;   //按键按下时刻
  uint32_t       tick_event;   //按键事件时刻
};

/**
 * \brief key 处理
 */
void key_process (void);

/**
 * \brief key 信息获取
 */
int key_info_get (enum key key, struct key_info *p_key_info);

/**
 * \brief key 配置更新
 */
int key_cfg_update (void);

/**
 * \brief key 初始化
 */
int key_init (void);

/**
 * \brief key 解初始化
 */
int key_deinit (void);

#endif //__KEY_H

/* end of file */
