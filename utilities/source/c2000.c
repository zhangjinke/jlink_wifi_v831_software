/**
 * \file
 * \brief C2000
 *
 * \internal
 * \par Modification history
 * - 1.00 23-01-01  zjk, first implementation
 * \endinternal
 */

#include "c2000.h"
#include <string.h>
#include <stdbool.h>

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

/**
 * \brief C2000 控件校验算法
 *
 * \note 下发高在前低在后，回发低在前高在后
 */
static uint16_t __crc16_ccitt (const void *p_data, size_t size)
{
  size_t   i;
  uint8_t  temp_u8;
  uint8_t *p_u8 = (uint8_t *)p_data;
  uint16_t crc  = 0;

  for (i = 0; i < size; i++)
  {
    temp_u8 = 0x80;
    while (temp_u8 > 0)
    {
      if ((crc & 0x8000) > 0)
      {
        crc += crc;
        crc ^= 0x1021;
      }
      else
      {
        crc += crc;
      }

      if ((p_u8[i] & temp_u8) > 0)
      {
        crc ^= 0x1021;
      }
      temp_u8 >>= 1;
    }
  }

  return crc;
}

/*******************************************************************************
  外部函数定义
*******************************************************************************/

/**
 * \brief C2000 接收处理
 */
size_t c2000_recv_process (uint8_t *p_dst, const uint8_t *p_src, size_t src_size, struct c2000_info *p_info)
{
  uint16_t             pkg_len;
  uint16_t             cmd;
  uint16_t             crc[2];
  size_t               l_len          = 0;
  const static uint8_t s_reply_head[] = {0xfa, 0x01, 0x34, 0x33, 0x21, 0x56, 0x23, 0xa5, 0x7b,
                                         0x29, 0xc5, 0x5d, 0x3c, 0x32, 0x12, 0xfe, 0x00, 0x00};

  //寻找引导符
  if ((p_src[0] == 0xfa) && (p_src[1] == 0x01))
  {
    pkg_len = p_src[26] << 8 | p_src[27];
    if (pkg_len < 1000)
    {
      crc[0] = __crc16_ccitt(&p_src[0], 28 + pkg_len);
      crc[1] = p_src[28 + pkg_len] << 8 | p_src[29 + pkg_len];
      if (crc[1] == crc[0])
      {
        memcpy(p_dst, s_reply_head, 18);
        memset(&p_dst[18], 0, 480);
        cmd = p_src[18] << 8 | p_src[19];
        if (cmd == 0xff01)
        { //网络模块搜索
          p_dst[18] = 0xff;                        //命令
          p_dst[19] = 0x02;
          p_dst[20] = p_info->dev_type;            //设备类型
          p_dst[21] = 0x00;
          p_dst[22] = 0x00;
          p_dst[23] = 0x00;
          p_dst[24] = 0x00;
          p_dst[25] = 0x00;
          p_dst[26] = 0x00;                        //后续字节长度 高位
          p_dst[27] = 12;                          //后续字节长度 低位 (不包含检验字节)
          memcpy(&p_dst[28], p_info->mac, 6);      //本机 MAC
          memcpy(&p_dst[34], p_info->local_ip, 4); //本机 IP
          p_dst[38] = 0x02;                        //固定
          p_dst[39] = 0x16;                        //固定
          crc[0] = __crc16_ccitt(p_dst, 40);
          p_dst[40] = (uint8_t)crc[0];             //CRC
          p_dst[41] = (uint8_t)(crc[0] >> 8);
          l_len = 42;
        }
        else
        { //不支持的命令
        }
      }
    }
  }

  return l_len;
}

/* end of file */
