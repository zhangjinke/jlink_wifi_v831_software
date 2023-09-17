/**
 * \file
 * \brief 配置信息
 *
 * \internal
 * \par Modification history
 * - 1.00 22-06-21  zjk, first implementation
 * \endinternal
 */

#ifndef __CFG_H
#define __CFG_H

#include <stddef.h>

/**
 * \brief 整形配置信息获取
 */
int cfg_int_get (const char *p_group, const char *p_key, int *p_data, int default_value);

/**
 * \brief 整形配置信息保存
 */
int cfg_int_set (const char *p_group, const char *p_key, int data);

/**
 * \brief 字符串配置信息获取
 */
int cfg_str_get (const char *p_group,
                 const char *p_key,
                 char       *p_str,
                 size_t      size,
                 const char *p_default_string);

/**
 * \brief 字符串配置信息保存
 */
int cfg_str_set (const char *p_group,
                 const char *p_key,
                 const char *p_str);

/**
 * \brief 配置信息初始化
 */
int cfg_init (void);

/**
 * \brief 配置信息解初始化
 */
int cfg_deinit (void);

#endif //__CFG_H

/* end of file */
