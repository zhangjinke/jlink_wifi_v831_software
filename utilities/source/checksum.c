/**
 * \file
 * \brief 校验和计算
 *
 * \internal
 * \par Modification history
 * - 1.00 19-08-20  zjk, first implementation
 * \endinternal
 */

/*******************************************************************************
  头文件包含
*******************************************************************************/

#include "checksum.h"

/*******************************************************************************
  外部函数定义
*******************************************************************************/

/**
 * \brief 校验和计算（字节）
 */
uint32_t checksum_byte (uint8_t *p_data, uint32_t length)
{
    uint32_t checksum = 0;

    while ((length--) != 0) {
        checksum += *p_data++;
    }

    return checksum;
}

/**
 * \brief 校验和计算（半字）
 */
uint32_t checksum_halfword (uint16_t *p_data, uint32_t length)
{
    uint32_t checksum = 0;

    while ((length--) != 0) {
        checksum += *p_data++;
    }

    return checksum;
}

/* end of file */
