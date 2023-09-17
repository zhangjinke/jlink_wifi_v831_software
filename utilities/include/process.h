/**
 * \file
 * \brief process
 *
 * \internal
 * \par Modification history
 * - 1.00 22-07-06  zjk, first implementation
 * \endinternal
 */

#ifndef __PROCESS_H
#define __PROCESS_H

#include <pthread.h>

/**
 * \brief 进程退出信息打印
 *
 * \param[in] p_name   进程名称
 * \param[in] pid      进程号
 * \param[in] stat_loc 进程退出状态
 */
void process_exit_print (const char *p_name, pid_t pid, int stat_loc);

/**
 * \brief 进程数量获取
 *
 * \param[in]  p_name      进程名称
 * \param[out] p_pid_first 查找到的第一个进程 pid
 *
 * \return 小于 0 表示失败，否则为查找到的进程数量
 */
int process_num_get (const char *p_name, pid_t *p_pid_first);

/**
 * \brief 启动进程
 *
 * \param[in] p_cmd      命令
 * \param[in] p_name     进程名称
 * \param[in] timeout_ms 超时时间
 *
 * \retval  0 成功
 * \retval -1 失败
 */
int process_start (const char *p_cmd, const char *p_name, int timeout_ms);

/**
 * \brief 关闭进程
 *
 * \param[in] p_name     进程名称
 * \param[in] timeout_ms 超时时间
 *
 * \retval  0 成功
 * \retval -1 失败
 */
int process_kill (const char *p_name, int timeout_ms);

/**
 * \brief 进程创建
 *
 * \param[out] p_stdin  指向存储 stdin 文件描述符的缓冲区的指针，可传入 NULL
 * \param[out] p_stdout 指向存储 stdout 文件描述符的缓冲区的指针，可传入 NULL
 * \param[out] p_stderr 指向存储 stderr 文件描述符的缓冲区的指针，可传入 NULL
 * \param[in]  p_cmd    待指向的命令
 *
 * \return -1 表示失败，否则为进程 pid
 */
pid_t process_exec (int *p_stdin, int *p_stdout, int *p_stderr, const char *p_cmd);

#endif //__PROCESS_H

/* end of file */
