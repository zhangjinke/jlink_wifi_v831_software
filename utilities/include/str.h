/**
 * \file
 * \brief str
 *
 * \internal
 * \par Modification history
 * - 1.00 22-07-24  zjk, first implementation
 * \endinternal
 */

#ifndef __STR_H
#define __STR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * \brief 获取指定开始和结尾的字符串
 *
 * \param[out] pp_dst  指向存储获取到的字符串的指针，获取失败时被赋值为 NULL
 * \param[in]  p_src   源字符串，查找到目标字符串后，结尾字符串的第一个字符会被修改为 '\0'
 * \param[in]  p_start 待查找的开始字符串
 * \param[in]  p_end   待查找的结尾字符串
 *
 * \return 指向结尾字符串的下一个字符的指针
 */
char *str_get (char **pp_dst, char *p_src, const char *p_start, const char *p_end);

/**
 * \brief 字符串 MAC 地址转数组
 *
 * \param[out] p_mac 指向存储 MAC 地址的缓冲区的指针，缓冲区必须大于 6 个字节
 * \param[in]  p_str 指向待转换的字符串
 *
 * \retval  0 成功
 * \retval -1 失败
 */
int str_mac_to_array (uint8_t *p_mac, const char *p_str);

/**
 * \brief MAC 地址数组转字符串
 *
 * \param[out] p_str     指向存储 MAC 字符串的缓冲区的指针
 * \param[in]  str_size  存储 MAC 字符串的缓冲区的大小
 * \param[in]  p_mac     指向待转换的 MAC 地址数组
 * \param[in]  separator 分隔符
 * \param[in]  is_upper  是否大写
 *
 * \retval  0 成功
 * \retval -1 失败
 */
int str_mac_array_to_str (char *p_str, size_t str_size, const uint8_t *p_mac, char separator, bool is_upper);

#endif //__STR_H

/* end of file */
