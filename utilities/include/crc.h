/**
 * \file
 * \brief CRC 计算
 *
 * \internal
 * \par Modification history
 * - 1.00 19-08-19  zjk, first implementation
 * \endinternal
 */

#ifndef __CRC_H
#define __CRC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define CRC32_MPEG2_INITIAL  0xffffffff //CRC32/MPEG-2 初始值

/**
 * \brief CRC16/MODBUS 计算
 *
 * \param[in] p_data 指向需要计算的数据的指针
 * \param[in] length 需要计算的字节数
 *
 * \return CRC 校验值
 */
uint16_t crc16_modbus (uint8_t *p_data, uint32_t length);

/**
 * \brief CRC16/MODBUS 计算（半字）
 *
 * \param[in] p_data 指向需要计算的数据的指针
 * \param[in] length 需要计算的字节数
 *
 * \return CRC 校验值
 */
uint16_t crc16_modbus_halfword (uint16_t *p_data, uint32_t length);

/**
 * \brief CRC16/MODBUS 查表计算
 *
 * \param[in] p_data 指向需要计算的数据的指针
 * \param[in] length 需要计算的字节数
 *
 * \return CRC 校验值
 */
uint16_t crc16_modbus_fast (uint8_t *p_data, uint32_t length);

/**
 * \brief CRC16/MODBUS 查表计算（半字）
 *
 * \param[in] p_data 指向需要计算的数据的指针
 * \param[in] length 需要计算的字节数
 *
 * \return CRC 校验值
 */
uint16_t crc16_modbus_fast_halfword (uint16_t *p_data, uint32_t length);

/**
 * \brief CRC32/MPEG-2 计算
 *
 * \param[in] initial 初始值
 * \param[in] p_data  指向需要计算的数据的指针
 * \param[in] length  需要计算的字节数
 *
 * \return CRC 校验值
 */
uint32_t crc32_mpeg2 (uint32_t initial, void *p_data, uint32_t length);

/**
 * \brief CRC32/MPEG-2 查表计算
 *
 * \param[in] initial 初始值
 * \param[in] p_data  指向需要计算的数据的指针
 * \param[in] length  需要计算的字节数
 *
 * \return CRC 校验值
 */
uint32_t crc32_mpeg2_fast (uint32_t initial, void *p_data, uint32_t length);

#ifdef __cplusplus
}
#endif

#endif //__CRC_H

/* end of file */
