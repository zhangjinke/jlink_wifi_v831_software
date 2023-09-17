/**
 * \file
 * \brief 系统滴答
 *
 * \internal
 * \par Modification history
 * - 1.00 21-06-03  zjk, first implementation
 * \endinternal
 */

#ifndef __SYSTICK_H
#define __SYSTICK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * \brief 获取系统滴嗒
 */
uint32_t systick_get (void);

#ifdef __cplusplus
}
#endif

#endif //__SYSTICK_H

/* end of file */
