/**
 * \file
 * \brief 校验和计算
 *
 * \internal
 * \par Modification history
 * - 1.00 19-08-20  zjk, first implementation
 * \endinternal
 */

#ifndef __CHECKSUM_H
#define __CHECKSUM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * \brief 校验和计算（字节）
 *
 * \param[in] p_data 指向需要计算的数据的指针
 * \param[in] length 需要计算的字节数
 *
 * \return 校验和
 */
uint32_t checksum_byte (uint8_t *p_data, uint32_t length);

/**
 * \brief 校验和计算（半字）
 *
 * \param[in] p_data 指向需要计算的数据的指针
 * \param[in] length 需要计算的半字数
 *
 * \return 校验和
 */
uint32_t checksum_halfword (uint16_t *p_data, uint32_t length);

#ifdef __cplusplus
}
#endif

#endif //__CHECKSUM_H

/* end of file */
