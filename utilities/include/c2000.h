/**
 * \file
 * \brief C2000
 *
 * \internal
 * \par Modification history
 * - 1.00 23-01-01  zjk, first implementation
 * \endinternal
 */

#ifndef __C2000_H
#define __C2000_H

#include <stdint.h>
#include <stddef.h>

struct c2000_info
{
  uint16_t port;        //C2000 端口
  uint8_t  local_ip[4]; //本机地址
  uint8_t  mask[4];     //子网掩码
  uint8_t  gateway[4];  //网关地址
  uint8_t  mac[6];      //MAC 地址
  uint16_t local_port;  //本机端口
  uint8_t  srv_ip[4];   //服务器地址
  uint16_t srv_port;    //服务器端口
  uint8_t  dev_type;    //设备类型
  uint8_t  is_update;   //设置是否更新
  uint8_t  is_save;     //设置是否保存
};

/**
 * \brief C2000 接收处理
 */
size_t c2000_recv_process (uint8_t *p_dst, const uint8_t *p_src, size_t src_size, struct c2000_info *p_info);

#endif //__C2000_H

/* end of file */
