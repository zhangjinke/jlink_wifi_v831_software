/**
 * \file
 * \brief file
 *
 * \internal
 * \par Modification history
 * - 1.00 22-07-21  zjk, first implementation
 * \endinternal
 */

#ifndef __FILE_H
#define __FILE_H

#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>

/**
 * \brief 判断文件是否为指定类型
 *
 * \param[in] p_path 文件路径
 * \param[in] type   类型，如 S_IFDIR
 *
 * \return 是否为指定类型
 */
bool file_is_type (const char *p_path, int type);

/**
 * \brief 文件读取
 *
 * \param[in]  p_path    文件路径
 * \param[out] p_buf     指向存储读取到的数据的缓冲区的指针
 * \param[in]  read_size 需要读取的大小，必须小于等于缓冲区大小
 * \param[in]  oflag     文件打开标志位
 *
 * \return -1 表示读取失败，否则表示读取到的大小
 */
int file_read (const char *p_path, void *p_buf, size_t read_size, int oflag);

/**
 * \brief 文件写入
 *
 * \param[in]  p_path 文件路径
 * \param[out] p_buf  指向存储待写入数据的缓冲区的指针
 * \param[in]  size   需要写入的大小，必须小于等于缓冲区大小
 * \param[in]  oflag  文件打开标志位
 *
 * \return -1 表示写入失败，否则表示写入的大小
 */
int file_write (const char *p_path, const void *p_buf, size_t size, int oflag);

/**
 * \brief 拷贝文件
 *
 * \param[in] p_path_src 源文件路径
 * \param[in] p_path_dst 目的文件路径
 *
 * \retval  0 成功
 * \retval -1 失败
 */
int file_copy (const char *p_path_src, const char *p_path_dst);

/**
 * \brief 删除文件
 *
 * \param[in] p_path 文件路径
 *
 * \retval  0 成功
 * \retval -1 参数错误
 * \retval -2 文件不存在
 * \retval -3 失败
 */
int file_remove (const char *p_path);

/**
 * \brief 创建文件夹
 *
 * \param[in] p_dir 待创建文件夹路径
 * \param[in] mode  模式，如 S_IRUSR
 *
 * \retval  0 成功
 * \retval -1 参数错误
 * \retval -2 失败
 */
int file_mkdirs (const char *p_dir, mode_t mode);

/**
 * \brief 文件大小获取
 *
 * \param[in] p_path 文件路径
 *
 * \return -1 表示获取失败，否则为文件大小
 */
int file_size_get (const char *p_path);

/**
 * \brief 文件大小字符串获取
 *
 * \param[out] p_str     指向存储结果字符串的缓冲区的指针
 * \param[in]  str_size  存储结果字符串的缓冲区的大小
 * \param[in]  file_size 文件大小
 *
 * \retval  0 成功
 * \retval -1 失败
 */
int file_size_str_get (char *p_str, size_t str_size, uint64_t file_size);

/**
 * \brief 文件后缀获取
 * \param[in] p_path    指向存储文件路径的缓冲区的指针
 * \param[in] path_size 存储文件路径的缓冲区的大小
 *
 * \return 失败返回 NULL，否则返回指向后缀的指针
 */
char *file_suffix_get (char *p_path, size_t path_size);

#endif //__FILE_H

/* end of file */
