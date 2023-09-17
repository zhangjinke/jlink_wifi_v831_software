/**
 * \file
 * \brief 配置信息
 *
 * \internal
 * \par Modification history
 * - 1.00 22-06-21  zjk, first implementation
 * \endinternal
 */

#include "cfg.h"
#include "config.h"
#include "file.h"
#include "libconfig.h"
#include "zlog.h"
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*******************************************************************************
  宏定义
*******************************************************************************/

/*******************************************************************************
  本地全局变量定义
*******************************************************************************/

//zlog 类别
static zlog_category_t *__gp_zlogc = NULL;

//互斥量
static pthread_mutex_t __g_mutex;

//是否初始化
static bool __g_is_init = false;

//有效配置文件号
static int __g_cfg_cur_num = -1;

//配置文件路径
static char *__gp_cfg_path = NULL;

/*******************************************************************************
  内部函数定义
*******************************************************************************/

/**
 * \brief 写入计数
 */
static void __write_cnt (void)
{
  int num = 0;
  int err = 0;

  err = cfg_int_get("cfg", "write_cnt", &num, 0);
  if (err != 0)
  {
    zlog_error(__gp_zlogc, "write_cnt get error, set to 0");
    cfg_int_set("cfg", "write_cnt", num);
  }
  else
  {
    num++;
    if (num <= 0)
    {
      num = 1;
    }
    cfg_int_set("cfg", "write_cnt", num);
  }
  sync();

  if (0 == __g_cfg_cur_num)
  {
    file_copy(CFG_PATH0, CFG_PATH1);
  }
  else
  {
    file_copy(CFG_PATH1, CFG_PATH0);
  }
  sync();
}

/**
 * \brief 配置信息初始化
 */
static int __cfg_init (char *p_file)
{
  int      fd;
  int      num;
  config_t cfg;
  int      err = 0;

  if ((access(p_file, F_OK)) == -1)
  { //配置文件不存在
    zlog_info(__gp_zlogc, "cfg %s not exist, created", p_file);
    fd = open(p_file, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
    if (fd < 0)
    {
      zlog_fatal(__gp_zlogc, "cfg %s open error", p_file);
      err = -1;
      goto err;
    }
    close(fd);
  }

  config_init(&cfg);
  if (config_read_file(&cfg, p_file) != CONFIG_TRUE)
  { //配置文件读取失败
    zlog_info(__gp_zlogc, "cfg %s read error, remove it", p_file);
    remove(p_file);
    err = -1;
    goto err_config_destory;
  }

  //写入计数
  __gp_cfg_path = p_file;
  err = cfg_int_get("cfg", "write_cnt", &num, 0);
  if (err != 0)
  {
    cfg_int_set("cfg", "write_cnt", num);
  }
  err = num;

err_config_destory:
  config_destroy(&cfg);
err:
  return err;
}

/*******************************************************************************
  外部函数定义
*******************************************************************************/

/**
 * \brief 整形配置信息获取
 */
int cfg_int_get (const char *p_group, const char *p_key, int *p_data, int default_value)
{
  int               err = 0;
  config_t          cfg;
  config_setting_t *p_set_root;
  config_setting_t *p_set_group;
  config_setting_t *p_set;

  if ((NULL == p_group) || (NULL == p_key) || (NULL == p_data))
  {
    err = -1;
    goto err;
  }

  pthread_mutex_lock(&__g_mutex);
  config_init(&cfg);
  if (config_read_file(&cfg, __gp_cfg_path) != CONFIG_TRUE)
  { //配置文件读取失败
    zlog_info(__gp_zlogc, "cfg %s read error", __gp_cfg_path);
    err = -1;
    goto err_config_destory;
  }

  if (((p_set_root = config_root_setting(&cfg)) != NULL) &&
      ((p_set_group = config_setting_lookup(p_set_root, p_group)) != NULL) &&
      ((p_set = config_setting_lookup(p_set_group, p_key)) != NULL))
  {
    *p_data = config_setting_get_int(p_set);
  }
  else
  {
    err = -1;
    goto err_config_destory;
  }

err_config_destory:
  config_destroy(&cfg);
  pthread_mutex_unlock(&__g_mutex);
err:
  if ((err != 0) && (p_data != NULL))
  {
    *p_data = default_value;
  }
  return err;
}

/**
 * \brief 整形配置信息保存
 */
int cfg_int_set (const char *p_group, const char *p_key, int data)
{
  int               err = 0;
  config_t          cfg;
  config_setting_t *p_set_root;
  config_setting_t *p_set_group;
  config_setting_t *p_set;

  if ((NULL == p_group) || (NULL == p_key))
  {
    err = -1;
    goto err;
  }

  pthread_mutex_lock(&__g_mutex);
  config_init(&cfg);
  if (config_read_file(&cfg, __gp_cfg_path) != CONFIG_TRUE)
  { //配置文件读取失败
    zlog_info(__gp_zlogc, "cfg %s read error", __gp_cfg_path);
    err = -1;
    goto err_config_destory;
  }

  p_set_root = config_root_setting(&cfg);
  if (NULL == p_set_root)
  {
    err = -1;
    goto err_config_destory;
  }

  //查找 group，若不存在，创建
  p_set_group = config_setting_lookup(p_set_root, p_group);
  if (NULL == p_set_group)
  {
    p_set_group = config_setting_add(p_set_root, p_group, CONFIG_TYPE_GROUP);
    if (NULL == p_set_group)
    {
      zlog_error(__gp_zlogc, "cfg group: %s add error", p_group);
      err = -1;
      goto err_config_destory;
    }
  }

  //查找 key，若不存在，创建
  p_set = config_setting_lookup(p_set_group, p_key);
  if (NULL == p_set)
  {
    p_set = config_setting_add(p_set_group, p_key, CONFIG_TYPE_INT);
    if (NULL == p_set)
    {
      zlog_error(__gp_zlogc, "cfg group: %s key: %s add error", p_group, p_key);
      err = -1;
      goto err_config_destory;
    }
  }

  //写入配置项
  if (config_setting_set_int(p_set, data) != CONFIG_TRUE)
  { //写入失败，可能是类型不匹配，删除并重新创建配置项
    if (config_setting_remove(p_set_group, p_key) != CONFIG_TRUE)
    {
      zlog_error(__gp_zlogc, "cfg group: %s key: %s remove error", p_group, p_key);
      err = -1;
      goto err_config_destory;
    }

    p_set = config_setting_add(p_set_group, p_key, CONFIG_TYPE_INT);
    if (NULL == p_set)
    {
      zlog_error(__gp_zlogc, "cfg group: %s key: %s add error", p_group, p_key);
      err = -1;
      goto err_config_destory;
    }

    //写入配置项
    if (config_setting_set_int(p_set, data) != CONFIG_TRUE)
    {
      zlog_error(__gp_zlogc, "cfg group: %s key: %s set error", p_group, p_key);
      err = -1;
      goto err_config_destory;
    }
  }

  //保存配置文件
  if (config_write_file(&cfg, __gp_cfg_path) != CONFIG_TRUE)
  {
    zlog_error(__gp_zlogc, "cfg %s write error", __gp_cfg_path);
    err = -1;
    goto err_config_destory;
  }

err_config_destory:
  config_destroy(&cfg);
  pthread_mutex_unlock(&__g_mutex);
  if ((0 == err) && (strcmp(p_group, "cfg") != 0) && (strcmp(p_key, "write_cnt") != 0))
  {
    __write_cnt(); //写入计数
  }
err:
  return err;
}

/**
 * \brief 字符串配置信息获取
 */
int cfg_str_get (const char *p_group,
                 const char *p_key,
                 char       *p_str,
                 size_t      size,
                 const char *p_default_string)
{
  const char       *p_str_get = NULL;
  int               err       = 0;
  config_t          cfg;
  config_setting_t *p_set_root;
  config_setting_t *p_set_group;
  config_setting_t *p_set;

  if ((NULL == p_group) || (NULL == p_key) || (NULL == p_str) || (NULL == p_default_string))
  {
    err = -1;
    goto err;
  }

  pthread_mutex_lock(&__g_mutex);
  config_init(&cfg);
  if (config_read_file(&cfg, __gp_cfg_path) != CONFIG_TRUE)
  { //配置文件读取失败
    zlog_info(__gp_zlogc, "cfg %s read error", __gp_cfg_path);
    err = -1;
    goto err_config_destory;
  }

  if (((p_set_root = config_root_setting(&cfg)) != NULL) &&
      ((p_set_group = config_setting_lookup(p_set_root, p_group)) != NULL) &&
      ((p_set = config_setting_lookup(p_set_group, p_key)) != NULL))
  {
    p_str_get = config_setting_get_string(p_set);
  }

  if (NULL == p_str_get)
  {
    err = -1;
    goto err_config_destory;
  }

  strncpy(p_str, p_str_get, size);

err_config_destory:
  config_destroy(&cfg);
  pthread_mutex_unlock(&__g_mutex);
err:
  if ((err != 0) && (p_str != NULL) && (p_str != p_default_string))
  {
    strncpy(p_str, p_default_string, size);
  }
  return err;
}

/**
 * \brief 字符串配置信息保存
 */
int cfg_str_set (const char *p_group,
                 const char *p_key,
                 const char *p_str)
{
  int               err = 0;
  config_t          cfg;
  config_setting_t *p_set_root;
  config_setting_t *p_set_group;
  config_setting_t *p_set;

  if ((NULL == p_group) || (NULL == p_key))
  {
    err = -1;
    goto err;
  }

  pthread_mutex_lock(&__g_mutex);
  config_init(&cfg);
  if (config_read_file(&cfg, __gp_cfg_path) != CONFIG_TRUE)
  { //配置文件读取失败
    zlog_info(__gp_zlogc, "cfg %s read error", __gp_cfg_path);
    err = -1;
    goto err_config_destory;
  }

  p_set_root = config_root_setting(&cfg);
  if (NULL == p_set_root)
  {
    err = -1;
    goto err_config_destory;
  }

  //查找 group，若不存在，创建
  p_set_group = config_setting_lookup(p_set_root, p_group);
  if (NULL == p_set_group)
  {
    p_set_group = config_setting_add(p_set_root, p_group, CONFIG_TYPE_GROUP);
    if (NULL == p_set_group)
    {
      zlog_error(__gp_zlogc, "cfg group: %s add error", p_group);
      err = -1;
      goto err_config_destory;
    }
  }

  //查找 key，若不存在，创建
  p_set = config_setting_lookup(p_set_group, p_key);
  if (NULL == p_set)
  {
    p_set = config_setting_add(p_set_group, p_key, CONFIG_TYPE_STRING);
    if (NULL == p_set)
    {
      zlog_error(__gp_zlogc, "cfg group: %s key: %s add error", p_group, p_key);
      err = -1;
      goto err_config_destory;
    }
  }

  //写入配置项
  if (config_setting_set_string(p_set, p_str) != CONFIG_TRUE)
  { //写入失败，可能是类型不匹配，删除并重新创建配置项
    if (config_setting_remove(p_set_group, p_key) != CONFIG_TRUE)
    {
      zlog_error(__gp_zlogc, "cfg group: %s key: %s remove error", p_group, p_key);
      err = -1;
      goto err_config_destory;
    }

    p_set = config_setting_add(p_set_group, p_key, CONFIG_TYPE_STRING);
    if (NULL == p_set)
    {
      zlog_error(__gp_zlogc, "cfg group: %s key: %s add error", p_group, p_key);
      err = -1;
      goto err_config_destory;
    }

    //写入配置项
    if (config_setting_set_string(p_set, p_str) != CONFIG_TRUE)
    {
      zlog_error(__gp_zlogc, "cfg group: %s key: %s set error", p_group, p_key);
      err = -1;
      goto err_config_destory;
    }
  }

  //保存配置文件
  if (config_write_file(&cfg, __gp_cfg_path) != CONFIG_TRUE)
  {
    zlog_error(__gp_zlogc, "cfg %s write error", __gp_cfg_path);
    err = -1;
    goto err_config_destory;
  }

err_config_destory:
  config_destroy(&cfg);
  pthread_mutex_unlock(&__g_mutex);
  if ((0 == err) && (strcmp(p_group, "cfg") != 0) && (strcmp(p_key, "write_cnt") != 0))
  {
    __write_cnt(); //写入计数
  }
err:
  return err;
}

/**
 * \brief 配置信息初始化
 */
int cfg_init (void)
{
  int write_cnt[2] = {0};
  int err          = 0;

  if (__g_is_init)
  { //已初始化
    return 0;
  }

  __gp_zlogc = zlog_get_category("cfg");

  if (pthread_mutex_init(&__g_mutex, NULL) != 0)
  {
    zlog_fatal(__gp_zlogc, "mutex init error");
    err = -1;
    goto err;
  }

  write_cnt[0] = __cfg_init(CFG_PATH0);
  write_cnt[1] = __cfg_init(CFG_PATH1);
  if ((write_cnt[0] >= 0) || (write_cnt[1] >= 0))
  {
    if (write_cnt[0] > write_cnt[1])
    { //配置文件 0 比配置文件 1 新
      zlog_error(__gp_zlogc, "cfg0 is newer than cfg1, copy cfg0 to cfg1");
      __g_cfg_cur_num = 0;
      __gp_cfg_path = CFG_PATH0;
      file_copy(CFG_PATH0, CFG_PATH1);
      sync();
    }
    else if (write_cnt[1] > write_cnt[0])
    { //配置文件 1 比配置文件 0 新
      zlog_error(__gp_zlogc, "cfg1 is newer than cfg0, copy cfg1 to cfg0");
      __g_cfg_cur_num = 1;
      __gp_cfg_path = CFG_PATH1;
      file_copy(CFG_PATH1, CFG_PATH0);
      sync();
    }
    else
    { //两个配置文件版本相同
      __g_cfg_cur_num = 0;
      __gp_cfg_path = CFG_PATH0;
    }
    zlog_info(__gp_zlogc, "current is cfg%d", __g_cfg_cur_num);
  }
  else
  { //两个配置文件均初始化失败
    zlog_fatal(__gp_zlogc, "cfg init error");
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
 * \brief 配置信息解初始化
 */
int cfg_deinit (void)
{
  if (!__g_is_init)
  {
    return 0;
  }

  pthread_mutex_destroy(&__g_mutex);
  __g_is_init = false;
  return 0;
}

/* end of file */
