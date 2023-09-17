/**
 * \file
 * \brief str
 *
 * \internal
 * \par Modification history
 * - 1.00 22-07-06  zjk, first implementation
 * \endinternal
 */

#include "str.h"
#include "utilities.h"
#include <stdlib.h>
#include <string.h>

/*******************************************************************************
  宏定义
*******************************************************************************/

/*******************************************************************************
  本地全局变量声明
*******************************************************************************/

/*******************************************************************************
  本地全局变量定义
*******************************************************************************/

/*******************************************************************************
  内部函数定义
*******************************************************************************/

/*******************************************************************************
  外部函数定义
*******************************************************************************/

/**
 * \brief 获取指定开始和结尾的字符串
 */
char *str_get (char **pp_dst, char *p_src, const char *p_start, const char *p_end)
{
  char *p_str;
  char *p_str_end;

  if ((NULL == pp_dst) || (NULL == p_src) || (NULL == p_start) || (NULL == p_end))
  {
    return p_src;
  }

  //查找起始字符串
  p_str = strstr(p_src, p_start);
  if (NULL == p_str)
  {
    *pp_dst = NULL;
    return p_src;
  }
  p_str += strlen(p_start);

  //查找结尾字符串
  p_str_end = strstr(p_str, p_end);
  if (NULL == p_str_end)
  {
    *pp_dst = NULL;
    return p_src;
  }
  *p_str_end = '\0';
  p_str_end += strlen(p_end);

  *pp_dst = p_str;
  return p_str_end;
}

/**
 * \brief 字符串 MAC 地址转数组
 */
int str_mac_to_array (uint8_t *p_mac, const char *p_str)
{
  int     i;
  char   *p_end;
  uint8_t mac[6];
  int     err = 0;

  if ((NULL == p_mac) ||
      (NULL == p_str) ||
      (p_str[2] != p_str[5]) ||
      (p_str[2] != p_str[8]) ||
      (p_str[2] != p_str[11]) ||
      (p_str[2] != p_str[14]))
  {
    err = -1;
    goto err;
  }

  for (i = 0; i < 6; i++)
  {
    mac[i] = strtoul(&p_str[i * 3], &p_end, 16);
    if ((p_end - p_str) != (3 * i + 2))
    { //转换错误
      err = -1;
      goto err;
    }
  }
  memcpy(p_mac, mac, sizeof(mac));

err:
  return err;
}

/**
 * \brief MAC 地址数组转字符串
 */
int str_mac_array_to_str (char *p_str, size_t str_size, const uint8_t *p_mac, char separator, bool is_upper)
{
  int err = 0;

  if (str_size < 18)
  {
    err = -1;
    goto err;
  }

  if (is_upper)
  {
    sprintf(p_str, "%02X%c%02X%c%02X%c%02X%c%02X%c%02X",
            p_mac[0], separator,
            p_mac[1], separator,
            p_mac[2], separator,
            p_mac[3], separator,
            p_mac[4], separator,
            p_mac[5]);
  }
  else
  {
    sprintf(p_str, "%02x%c%02x%c%02x%c%02x%c%02x%c%02x",
            p_mac[0], separator,
            p_mac[1], separator,
            p_mac[2], separator,
            p_mac[3], separator,
            p_mac[4], separator,
            p_mac[5]);
  }

err:
  return err;
}

/* end of file */
