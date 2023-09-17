/**
 * \file
 * \brief key
 *
 * \internal
 * \par Modification history
 * - 1.00 22-07-01  zjk, first implementation
 * \endinternal
 */

#include "key.h"
#include "cfg.h"
#include "file.h"
#include "systick.h"
#include "utilities.h"
#include "zlog.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/input.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

/*******************************************************************************
  宏定义
*******************************************************************************/

#define EVENT_NUM  2 //事件源数量

/*******************************************************************************
  本地全局变量声明
*******************************************************************************/

//按键值枚举
enum key_value
{
  KEY_VALUE_UP = 0, //未按下
  KEY_VALUE_DOWN,   //按下
  KEY_VALUE_REPEAT, //持续按下
};

/*******************************************************************************
  本地全局变量定义
*******************************************************************************/

//zlog 类别
static zlog_category_t *__gp_zlogc = NULL;

//互斥量
static pthread_mutex_t __g_mutex;

//是否初始化
static bool __g_is_init = false;

static volatile bool __g_cfg_update                      = false; //配置更新标记
static int           __g_key_code[KEY_USER_MAX]          = {0};   //按键 GPIO 号
static int           __g_key_long_press_ms               = 0;     //长按阈值，单位 ms
static int           __g_event_num                       = 0;     //事件数量
static char          __g_event_path[EVENT_NUM][PATH_MAX] = {0};   //事件路径

static struct key_info __g_key_info[KEY_USER_MAX] = {0}; //按键信息
static int             __g_event_fd[EVENT_NUM]    = {0}; //事件文件描述符

/*******************************************************************************
  内部函数定义
*******************************************************************************/

/**
 * \brief 配置读取
 */
static int __cfg_read (void)
{
  char key[64] = {0};
  char str[64] = {0};
  int  i       = 0;
  int  err     = 0;

  err = cfg_int_get("key", "key_key_code", &__g_key_code[KEY_USER_KEY], KEY_F1);
  if (err != 0)
  {
    cfg_int_set("key", "key_key_code", __g_key_code[KEY_USER_KEY]);
  }

  err = cfg_int_get("key", "power_key_code", &__g_key_code[KEY_USER_POWER], KEY_POWER);
  if (err != 0)
  {
    cfg_int_set("key", "power_key_code", __g_key_code[KEY_USER_POWER]);
  }

  err = cfg_int_get("key", "long_press_ms", &__g_key_long_press_ms, 1000);
  if (err != 0)
  {
    cfg_int_set("key", "long_press_ms", __g_key_long_press_ms);
  }

  err = cfg_int_get("key", "event_num", &__g_event_num, 2);
  if (err != 0)
  {
    cfg_int_set("key", "event_num", __g_event_num);
  }
  if ((__g_event_num < 0) || (__g_event_num > EVENT_NUM))
  {
    __g_event_num = EVENT_NUM;
    cfg_int_set("key", "event_num", __g_event_num);
  }

  for (i = 0; i < __g_event_num; i++)
  {
    snprintf(key, sizeof(key), "event_path%d", i);
    snprintf(str, sizeof(str), "/dev/input/event%d", i);
    err = cfg_str_get("key", key, __g_event_path[i], sizeof(__g_event_path[i]), str);
    if (err != 0)
    {
      cfg_str_set("key", key, __g_event_path[i]);
    }
  }

  return 0;
}

/**
 * \brief 按键名称获取
 */
static char *__key_name_get (enum key key)
{
  char *p_name;

  switch (key)
  {
    case KEY_USER_KEY:   p_name = "key";     break;
    case KEY_USER_POWER: p_name = "power";   break;
    default:             p_name = "unknown"; break;
  }

  return p_name;
}

/**
 * \brief 按键状态机
 */
static void __key_state_machine (enum key key, int32_t value)
{
  uint32_t systick = systick_get();

  pthread_mutex_lock(&__g_mutex);
  switch (__g_key_info[key].state)
  {
    case KEY_STATE_RELEASE:
    { //按键释放态
      if (value != KEY_VALUE_UP)
      { //按键按下，转换到按下态
        __g_key_info[key].event = KEY_EVENT_NONE;
        __g_key_info[key].state = KEY_STATE_PRESS;
        __g_key_info[key].tick_press = systick;
        zlog_debug(__gp_zlogc, "key %s pressed", __key_name_get(key));
        break;
      }
    }
    break;

    case KEY_STATE_PRESS:
    { //按键按下态
      if (value != KEY_VALUE_UP)
      { //按键按下
        if ((KEY_EVENT_NONE == __g_key_info[key].event) &&
            ((systick - __g_key_info[key].tick_press) >= __g_key_long_press_ms))
        { //按下时间达到长按阈值
          __g_key_info[key].event = KEY_EVENT_LONG_PRESS;
          __g_key_info[key].tick_event = systick;
          zlog_debug(__gp_zlogc, "key %s long press", __key_name_get(key));
        }
      }
      else
      { //按键释放，转换到释放态
        __g_key_info[key].state = KEY_STATE_RELEASE;
        __g_key_info[key].tick_release = systick;
        if (KEY_EVENT_NONE == __g_key_info[key].event)
        { //未长按，产生单击事件
          __g_key_info[key].event = KEY_EVENT_CLICK;
          __g_key_info[key].tick_event = systick;
          zlog_debug(__gp_zlogc, "key %s click", __key_name_get(key));
        }
        else if ((KEY_EVENT_LONG_PRESS == __g_key_info[key].event) ||
                 (KEY_EVENT_CLEAN == __g_key_info[key].event))
        { //已长按，不产生事件
          zlog_debug(__gp_zlogc, "key %s release", __key_name_get(key));
        }
        break;
      }
    }
    break;

    default:
    {
      __g_key_info[key].state = KEY_STATE_RELEASE;
      break;
    }
  }
  pthread_mutex_unlock(&__g_mutex);
}

/*******************************************************************************
  外部函数定义
*******************************************************************************/

/**
 * \brief key 处理
 */
void key_process (void)
{
  int                i;
  int                j;
  struct input_event event = {0};

  for (i = 0; i < __g_event_num; i++)
  {
    //读取按键事件
    if (read(__g_event_fd[i], &event, sizeof(event)) != sizeof(event))
    {
      continue;
    }
    if (event.type != EV_KEY)
    {
      continue;
    }

    //按键状态机
    for (j = 0; j < KEY_USER_MAX; j++)
    {
      if (event.code == __g_key_code[j])
      {
        __key_state_machine((enum key)j, event.value);
        break;
      }
    }
  }
}

/**
 * \brief key 信息获取
 */
int key_info_get (enum key key, struct key_info *p_key_info)
{
  if ((key < KEY_USER_KEY) || (key >= KEY_USER_MAX) || (NULL == p_key_info))
  {
    return -1;
  }

  pthread_mutex_lock(&__g_mutex);
  *p_key_info = __g_key_info[key];
  if (__g_key_info[key].event != KEY_EVENT_NONE)
  {
    __g_key_info[key].event = KEY_EVENT_CLEAN; //清除事件
  }
  pthread_mutex_unlock(&__g_mutex);

  return 0;
}

/**
 * \brief key 配置更新
 */
int key_cfg_update (void)
{
  __g_cfg_update = true;
  return 0;
}

/**
 * \brief key 初始化
 */
int key_init (void)
{
  int i   = 0;
  int err = 0;

  if (__g_is_init)
  { //已初始化
    return 0;
  }

  __gp_zlogc = zlog_get_category("key");

  if (pthread_mutex_init(&__g_mutex, NULL) != 0)
  {
    zlog_fatal(__gp_zlogc, "mutex init error");
    err = -1;
    goto err;
  }

  //获取配置信息
  __cfg_read();

  for (i = 0; i < __g_event_num; i++)
  {
    __g_event_fd[i] = open(__g_event_path[i], O_RDONLY | O_NONBLOCK);
    if (__g_event_fd[i] < 0)
    {
      zlog_error(__gp_zlogc, "open %s error: %s", __g_event_path[i], strerror(errno));
    }
  }

  __g_is_init = true;

err:
  return err;
}

/**
 * \brief key 解初始化
 */
int key_deinit (void)
{
  int i = 0;

  if (!__g_is_init)
  {
    return 0;
  }

  for (i = 0; i < __g_event_num; i++)
  {
    if (__g_event_fd[i] >= 0)
    {
      close(__g_event_fd[i]);
    }
  }

  pthread_mutex_destroy(&__g_mutex);
  __g_is_init = false;

  return 0;
}

/* end of file */
