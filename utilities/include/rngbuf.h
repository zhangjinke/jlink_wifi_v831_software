/**
 * \file
 * \brief 通用环形缓冲区
 *
 * \internal
 * \par Modification history
 * - 1.00 22-07-21  zjk, add implementation
 * \endinternal
 */
#ifndef __RNGBUF_H
#define __RNGBUF_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief 环形缓冲区管理结构
 * \note 不要直接操作本结构的成员
 */
struct rngbuf {
    int     in;     /**< \brief 缓冲区数据写入位置 */
    int     out;    /**< \brief 缓冲区数据读出位置 */
    int     size;   /**< \brief 缓冲区大小 */
    char   *buf;    /**< \brief 缓冲区     */
};

/** \brief 环形缓冲区类型 */
typedef struct rngbuf *rngbuf_t;

/**
 * \brief 初始化环形缓存区
 *
 * \param[in] p_rb    : 要初始化的环形缓冲区
 * \param[in] p_buf   : 环形缓冲区使用的缓冲区空间
 * \param[in] size    : 缓存区大小，系统保留一个字节，
 *                      实际环形缓冲区可用大小为 size - 1
 *
 * \retval  0      : 初始化完成
 * \retval -EINVAL : 初始化失败，参数无效
 *
 * \warning 特别注意，缓冲区空间在使用中保留了一字节空间，环形缓冲区实际可用大小为
 *          \b size - \b 1
 */
int rngbuf_init (struct rngbuf *p_rb, char *p_buf, size_t size);

/**
 * \brief 存放一个字节到环形缓冲区
 *
 * \param[in] rb   : 要操作的环形缓冲
 * \param[in] data : 要存放到缓冲区的数据字节
 *
 * \retval 0 : 数据存放失败，缓冲区满
 * \retval 1 : 数据成功存放
 */
int rngbuf_putchar (rngbuf_t rb, const char data);

/**
 * \brief 从环形缓冲区取出一个字节数据
 *
 * \param[in] rb     : 要操作的环形缓冲
 * \param[in] p_data : 存放数据字节的指针
 *
 * \retval 0 : 数据取出失败，缓冲区空
 * \retval 1 : 数据成功取出
 */
int rngbuf_getchar (rngbuf_t rb, char *p_data);

/**
 * \brief 存放若干字节到环形缓冲区
 *
 * \param[in] rb     : 要操作的环形缓冲
 * \param[in] p_buf  : 要存放到环形缓冲区的数据缓冲
 * \param[in] nbytes : 要存放到环形缓冲区的数据个数
 *
 * \return 成功存放的数据个数
 */
size_t rngbuf_put (rngbuf_t rb, const char *p_buf, size_t nbytes);

/**
 * \brief 从环形缓冲区获取数据
 *
 * \param[in] rb     : 要操作的环形缓冲
 * \param[in] p_buf  : 存放获取数据的缓冲
 * \param[in] nbytes : 要获取的数据个数
 *
 * \return 成功获取的数据个数
 */
size_t rngbuf_get (rngbuf_t rb, char *p_buf, size_t nbytes);

/**
 * \brief 清空环形缓冲区
 *
 * 这个函数清空环形缓冲区，所有之前留在缓冲区的数据会丢失
 *
 * \param[in] rb : 要操作的环形缓冲
 *
 * \return 无
 */
void rngbuf_flush (rngbuf_t rb);

/**
 * \brief 测试环形缓冲区是否为空
 *
 * \param[in] rb : 要测试的环形缓冲区
 *
 * \return 环形缓冲区空返回true, 否则返回false
 */
bool rngbuf_isempty (rngbuf_t rb);

/**
 * \brief 测试环形缓冲区是否添满
 *
 * \param[in] rb : 要测试的环形缓冲区
 *
 * \return 环形缓冲区满返回true, 否则返回false
 */
bool rngbuf_isfull (rngbuf_t rb);

/**
 * \brief 获取环形缓冲区空闲空间大小
 *
 * \param[in] rb : 要判断的环形缓冲区
 *
 * \return 环形缓冲区空闲空间大小
 */
size_t rngbuf_freebytes (rngbuf_t rb);

/**
 * \brief 获取环形缓冲区中已填充的数据字节个数
 *
 * \param[in] rb : 要判断的环形缓冲区
 *
 * \return 环形缓冲区已填充字节个数
 */
size_t rngbuf_nbytes (rngbuf_t rb);

#ifdef __cplusplus
}
#endif

#endif /* __RNGBUF_H */

/* end of file */
