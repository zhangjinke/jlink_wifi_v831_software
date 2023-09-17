/**
 * \file
 * \brief 软件滤波器
 *
 * \internal
 * \par Modification history
 * - 1.00 19-08-27  zjk, first implementation
 * \endinternal
 */

/*******************************************************************************
  头文件包含
*******************************************************************************/

#include "filter.h"
#include "utilities.h"
#include "zlog.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/*******************************************************************************
  内部函数定义
*******************************************************************************/

/**
 * \brief qsort 比较函数
 */
static int __int32_cmp (const void *p_a, const void *p_b)
{
	return *(int32_t *)p_a - *(int32_t *)p_b; //升序
}

/**
 * \brief 滑动滤波器滤波(去掉最大最小值)
 *
 * \param[in] p_buf      指向缓冲区的指针
 * \param[in] total      缓冲区大小
 * \param[in] num        缓冲区内数据数量
 * \param[in] remove_num 需要去掉的最大值个数以及最小值个数
 *
 * \return 滤波器输出值
 */
static int32_t __int32_remove (int32_t *p_buf, int32_t total, int32_t num, int32_t remove_num)
{
  int32_t i;
  int32_t  sum;

  //排序
  qsort(p_buf, num, sizeof(p_buf[0]), __int32_cmp);

  if (num <= remove_num * 2)
  {
    //求缓冲区内所有值的平均值
    sum = 0;
    for (i = 0; i < num; i++)
    {
      sum += p_buf[i];
    }
    sum /= num;
  }
  else
  {
    //去掉指定数量最大最小值后，再取平均值
    sum = 0;
    for (i = remove_num; i < num - remove_num; i++)
    {
      sum += p_buf[i];
    }
    sum /= num - remove_num * 2;
  }

  return sum;
}

/*******************************************************************************
  外部函数定义
*******************************************************************************/

/**
 * \brief 滑动滤波器（int32）滤波
 */
int32_t filter_slide_int32 (struct filter_slide_int32 *p_filter, int32_t data, uint32_t remove_num)
{
  uint32_t num = 0;

  //计算缓冲区中值的和
  p_filter->sum += data - p_filter->p_buf[p_filter->idx];

  //将本次数据存入缓冲区
  p_filter->p_buf[p_filter->idx] = data;
  p_filter->idx++;
  if (p_filter->idx >= p_filter->size)
  {
    p_filter->idx     = 0;
    p_filter->is_full = true;
  }

  //计算平均值
  if (!p_filter->is_full)
  { //缓冲区未填满，计算已有数据
    num = p_filter->idx;
  }
  else
  { //缓冲区已填满，计算所有数据
    num = p_filter->size;
  }

  if ((remove_num > 0) && (p_filter->p_buf_temp != NULL))
  { //去除最大最小值后计算平均值
    memcpy(p_filter->p_buf_temp, p_filter->p_buf, num * sizeof(int32_t));
    p_filter->avg = __int32_remove(p_filter->p_buf_temp, p_filter->size, num, remove_num);
  }
  else
  { //计算平均值
    p_filter->avg = p_filter->sum / num;
  }

  return p_filter->avg;
}

/**
 * \brief 滑动滤波器（int32）是否满
 */
bool filter_slide_int32_is_full (struct filter_slide_int32 *p_filter)
{
  return p_filter->is_full;
}

/**
 * \brief 清空滑动滤波器（int32）
 */
void filter_slide_int32_flush (struct filter_slide_int32 *p_filter)
{
  p_filter->is_full = false;
  p_filter->idx     = 0;
  p_filter->sum     = 0;
}

/**
 * \brief 滑动滤波器（int32）初始化
 */
int32_t filter_slide_int32_init (struct filter_slide_int32 *p_filter, uint32_t size, int32_t *p_buf)
{
  int32_t err = 0;

  p_filter->is_full    = false;
  p_filter->size       = size;
  p_filter->idx        = 0;
  p_filter->sum        = 0;
  p_filter->avg        = 0;
  p_filter->p_buf      = p_buf;
  p_filter->p_buf_temp = malloc(size * sizeof(int32_t));
  if (NULL == p_filter->p_buf_temp)
  {
    zlog_error(gp_utilities_zlogc, "malloc error: %s", strerror(errno));
    err = -1;
  }

  return err;
}

/**
 * \brief 滑动滤波器（int32）解初始化
 */
void filter_slide_int32_deinit (struct filter_slide_int32 *p_filter)
{
  if (p_filter->p_buf_temp != NULL)
  {
    free(p_filter->p_buf_temp);
  }
  memset(p_filter, 0, sizeof(struct filter_slide_int32));
}

/* end of file */
