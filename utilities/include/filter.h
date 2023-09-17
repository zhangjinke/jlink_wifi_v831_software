/**
 * \file
 * \brief 软件滤波器
 *
 * \internal
 * \par Modification history
 * - 1.00 19-08-27  zjk, first implementation
 * \endinternal
 */

#ifndef __FILTER_H
#define __FILTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

//滑动滤波器（int32）结构
struct filter_slide_int32
{
  bool     is_full;    //缓冲区是否填充满
  int32_t  size;       //p_buf 指向的缓冲区的大小
  int32_t  idx;        //索引
  int32_t  sum;        //缓冲区中值的和
  int32_t  avg;        //平均值输出
  int32_t *p_buf;      //指向缓冲区的指针
  int32_t *p_buf_temp; //指向缓冲区的指针（去掉最大最小值用）
};

/**
 * \brief 滑动滤波器（int32）滤波
 *
 * \param[in] p_filter   指向滑动滤波器结构的指针
 * \param[in] data       本次输入滤波器的数据
 * \param[in] remove_num 需要去掉的最大值个数以及最小值个数
 *
 * \return 滤波器输出值
 */
int32_t filter_slide_int32 (struct filter_slide_int32 *p_filter, int32_t data, uint32_t remove_num);

/**
 * \brief 滑动滤波器（int32）是否满
 *
 * \param[in] p_filter 指向滑动滤波器结构的指针
 *
 * \retval true  满
 * \retval false 未满
 */
bool filter_slide_int32_is_full (struct filter_slide_int32 *p_filter);

/**
 * \brief 清空滑动滤波器（int32）
 *
 * \param[in] p_filter 指向滑动滤波器结构的指针
 */
void filter_slide_int32_flush (struct filter_slide_int32 *p_filter);

/**
 * \brief 滑动滤波器（int32）初始化
 *
 * \param[in] p_filter 指向滑动滤波器结构的指针
 * \param[in] size     缓冲区大小
 * \param[in] p_buf    指向缓冲区的指针
 *
 * \retval  0 成功
 * \retval -1 失败
 */
int32_t filter_slide_int32_init (struct filter_slide_int32 *p_filter, uint32_t size, int32_t *p_buf);

/**
 * \brief 滑动滤波器（int32）解初始化
 *
 * \param[in] p_filter 指向滑动滤波器结构的指针
 */
void filter_slide_int32_deinit (struct filter_slide_int32 *p_filter);

#ifdef __cplusplus
}
#endif

#endif /* __FILTER_H */

/* end of file */
