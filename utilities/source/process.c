/**
 * \file
 * \brief process
 *
 * \internal
 * \par Modification history
 * - 1.00 22-07-06  zjk, first implementation
 * \endinternal
 */

#include "process.h"
#include "systick.h"
#include "utilities.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/*******************************************************************************
  宏定义
*******************************************************************************/

/*******************************************************************************
  本地全局变量声明
*******************************************************************************/

/*******************************************************************************
  本地全局变量定义
*******************************************************************************/

/*******************************************************************************
  内部函数定义
*******************************************************************************/

/**
 * \brief 判断目录名是否全为数字
 */
int __is_pid_folder (const struct dirent *entry)
{
  const char *p;

  for (p = entry->d_name; *p; p++)
  {
    if (!isdigit(*p))
    {
      return 0;
    }
  }

  return 1;
}

/*******************************************************************************
  外部函数定义
*******************************************************************************/

/**
 * \brief 进程退出信息打印
 */
void process_exit_print (const char *p_name, pid_t pid, int stat_loc)
{
  if (WIFEXITED(stat_loc))
  {
    zlog_debug(gp_utilities_zlogc, "process normal exit, name: %s, pid: %d, status: %d", p_name, pid, WEXITSTATUS(stat_loc));
  }
  else if (WIFSIGNALED(stat_loc))
  {
    zlog_debug(gp_utilities_zlogc, "process signal exit, name: %s, pid: %d, signal: %d%s",
               p_name,
               pid,
               WTERMSIG(stat_loc),
#ifdef WCOREDUMP
               WCOREDUMP(stat_loc) ? " (core file generated)" : ""
#endif
               "");
  }
  else if (WIFSTOPPED(stat_loc))
  {
    zlog_debug(gp_utilities_zlogc, "process stopped, name: %s, pid: %d, signal: %d", p_name, pid, WSTOPSIG(stat_loc));
  }
}

/**
 * \brief 进程数量获取
 */
int process_num_get (const char *p_name, pid_t *p_pid_first)
{
  DIR           *p_dir;
  FILE          *p_file;
  struct dirent *p_dirent;
  char           path[PATH_MAX];
  char           cmdline[512];
  size_t         size;
  char          *p_str;
  int            i;
  pid_t          pid = 0;
  int            err = 0;

  if (NULL == p_name)
  {
    err = -1;
    goto err;
  }

  p_dir = opendir("/proc");
  if (NULL == p_dir)
  {
    err = -1;
    zlog_error(gp_utilities_zlogc, "opendir /proc error");
    goto err;
  }

  while ((p_dirent = readdir(p_dir)))
  {
    //判断文件夹名是否为全数字
    if (!__is_pid_folder(p_dirent))
    {
      continue;
    }

    //打开命令文件
    snprintf(path, sizeof(path), "/proc/%s/cmdline", p_dirent->d_name);
    p_file = fopen(path, "r");
    if (NULL == p_file)
    {
      continue;
    }

    //读取命令文件
    size = fread(cmdline, 1, sizeof(cmdline) - 1, p_file);
    if (size > 0)
    {
      cmdline[size] = '\0';

      //将字符串结束符替换为空格
      for (i = 0; i < (size - 1); i++)
      {
        if ('\0' == cmdline[i])
        {
          cmdline[i] = ' ';
        }
      }

      //搜索是否有匹配的名称
      p_str = strstr(cmdline, p_name);
      if (p_str != NULL)
      {
        err++;
        if (0 == pid)
        {
          pid = atoi(p_dirent->d_name);
        }
      }
    }

    fclose(p_file);
  }

  closedir(p_dir);

  if (p_pid_first != NULL)
  {
    *p_pid_first = pid;
  }

err:
  return err;
}

/**
 * \brief 启动进程
 */
pid_t process_start (const char *p_cmd, const char *p_name, int timeout_ms)
{
  pid_t    pid  = 0;
  int      ret  = 0;
  uint32_t tick = systick_get();

  //执行命令
  ret = system(p_cmd) >> 8;
  if (ret != 0)
  {
    pid = -1;
    goto err;
  }

  while (process_num_get(p_name, &pid) <= 0)
  {
    if ((timeout_ms > 0) && ((systick_get() - tick) >= timeout_ms))
    {
      pid = -1;
      goto err;
    }
    usleep(10000);
  }

err:
  return pid;
}

/**
 * \brief 关闭进程
 */
int process_kill (const char *p_name, int timeout_ms)
{
  pid_t    pid  = 0;
  int      err  = 0;
  uint32_t tick = systick_get();

  while (process_num_get(p_name, &pid) > 0)
  {
    if ((timeout_ms > 0) && ((systick_get() - tick) >= (timeout_ms / 2)))
    { //超时时间过半，发送 SIGKILL 信号
      kill(pid, SIGKILL);
    }
    else
    { //超时时间未过半，发送 SIGTERM 信号
      kill(pid, SIGTERM);
    }
    if (process_num_get(p_name, NULL) <= 0)
    {
      goto err;
    }

    if ((timeout_ms > 0) && ((systick_get() - tick) >= timeout_ms))
    {
      pid = -1;
      goto err;
    }
    usleep(10000);
  }

err:
  return err;
}

/**
 * \brief 进程创建
 */
pid_t process_exec (int *p_stdin, int *p_stdout, int *p_stderr, const char *p_cmd)
{
  int   pipe_stdin[2]  = {0};
  int   pipe_stdout[2] = {0};
  int   pipe_stderr[2] = {0};
  pid_t pid            = 0;

  if (NULL == p_cmd)
  {
    pid = -1;
    goto err;
  }

  //创建管道 stdin
  if ((p_stdin != NULL) && (pipe(pipe_stdin) != 0))
  {
    zlog_error(gp_utilities_zlogc, "pipe create error: %s", strerror(errno));
    pid = -1;
    goto err;
  }

  //创建管道 stdout
  if ((p_stdout != NULL) && (pipe(pipe_stdout) != 0))
  {
    zlog_error(gp_utilities_zlogc, "pipe create error: %s", strerror(errno));
    pid = -1;
    goto err;
  }

  //创建管道 stderr
  if ((p_stderr != NULL) && (p_stderr != p_stdout) && (pipe(pipe_stderr) != 0))
  {
    zlog_error(gp_utilities_zlogc, "pipe create error: %s", strerror(errno));
    pid = -1;
    goto err;
  }

  //创建子进程
  pid = fork();
  if (pid < 0)
  {
    zlog_error(gp_utilities_zlogc, "fork error: %s", strerror(errno));
    pid = -1;
    goto err;
  }
  else if (0 == pid)
  { //子进程
    if (pipe_stdin[0] != 0)
    {
      close(pipe_stdin[1]);
      dup2(pipe_stdin[0], STDIN_FILENO);
    }
    if (pipe_stdout[0] != 0)
    {
      close(pipe_stdout[0]);
      dup2(pipe_stdout[1], STDOUT_FILENO);
      if (p_stderr == p_stdout)
      {
        dup2(pipe_stdout[1], STDERR_FILENO);
      }
    }
    if (pipe_stderr[0] != 0)
    {
      close(pipe_stderr[0]);
      dup2(pipe_stderr[1], STDERR_FILENO);
    }
    if (execl("/bin/sh", "sh", "-c", p_cmd, NULL) == -1)
    {
      fprintf(stderr, "execl cmd \"%s\" error: %s", p_cmd, strerror(errno));
      if (pipe_stdin[0] != 0)
      {
        close(pipe_stdin[0]);
      }
      if (pipe_stdout[0] != 0)
      {
        close(pipe_stdout[1]);
      }
      if (pipe_stderr[0] != 0)
      {
        close(pipe_stderr[1]);
      }
      exit(-1);
    }
  }

  if (p_stdin != NULL)
  {
    *p_stdin = pipe_stdin[1];
  }
  if (p_stdout != NULL)
  {
    *p_stdout = pipe_stdout[0];
  }
  if ((p_stderr != NULL) && (p_stderr != p_stdout))
  {
    *p_stderr = pipe_stderr[0];
  }
  goto ret;

err:
  if (pipe_stdin[0] != 0)
  {
    close(pipe_stdin[0]);
    close(pipe_stdin[1]);
  }
  if (pipe_stdout[0] != 0)
  {
    close(pipe_stdout[0]);
    close(pipe_stdout[1]);
  }
  if (pipe_stderr[0] != 0)
  {
    close(pipe_stderr[0]);
    close(pipe_stderr[1]);
  }
ret:
  return pid;
}

/* end of file */
