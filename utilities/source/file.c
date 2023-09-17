/**
 * \file
 * \brief file
 *
 * \internal
 * \par Modification history
 * - 1.00 22-07-21  zjk, first implementation
 * \endinternal
 */

#include "file.h"
#include "utilities.h"
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
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

/*******************************************************************************
  外部函数定义
*******************************************************************************/

/**
 * \brief 判断文件是否为指定类型
 */
bool file_is_type (const char *p_path, int type)
{
  struct stat file_stat = {0};
  bool        is_type   = true;

  if (stat(p_path, &file_stat) < 0)
  {
    is_type = false;
    goto err;
  }
  is_type = (file_stat.st_mode & S_IFMT) == type;

err:
  return is_type;
}

/**
 * \brief 文件读取
 */
int file_read (const char *p_path, void *p_buf, size_t read_size, int oflag)
{
  int fd  = 0;
  int err = 0;

  fd = open(p_path, oflag);
  if (fd <= 0)
  {
    zlog_error(gp_utilities_zlogc, "open %s error: %s", p_path, strerror(errno));
    err = -1;
    goto err;
  }

  err = read(fd, p_buf, read_size);
  if (-1 == err)
  {
    // zlog_error(gp_utilities_zlogc, "read %s error: %s", p_path, strerror(errno));
    err = -1;
    goto err_fd_close;
  }

err_fd_close:
  close(fd);
err:
  return err;
}

/**
 * \brief 文件写入
 */
int file_write (const char *p_path, const void *p_buf, size_t size, int oflag)
{
  int fd  = 0;
  int err = 0;

  fd = open(p_path, oflag);
  if (fd <= 0)
  {
    zlog_error(gp_utilities_zlogc, "open %s error: %s", p_path, strerror(errno));
    err = -1;
    goto err;
  }

  err = write(fd, p_buf, size);
  if (-1 == err)
  {
    // zlog_error(gp_utilities_zlogc, "write %s error: %s", p_path, strerror(errno));
    err = -1;
    goto err_fd_close;
  }

err_fd_close:
  close(fd);
err:
  return err;
}

/**
 * \brief 拷贝文件
 */
int file_copy (const char *p_path_src, const char *p_path_dst)
{
  int         fd_src     = 0;
  int         fd_dst     = 0;
  struct stat file_stat  = {0};
  char        buf[512]   = {0};
  size_t      left_read  = 0;
  size_t      left_write = 0;
  ssize_t     size       = 0;
  int         err        = 0;

  if ((NULL == p_path_src) || ('\0' == *p_path_src) ||
      (NULL == p_path_dst) || ('\0' == *p_path_dst))
  {
    err = -1;
    goto err;
  }

  if (stat(p_path_src, &file_stat) < 0)
  {
    err = -1;
    goto err;
  }

  if ((file_stat.st_mode & S_IFREG) != S_IFREG)
  { //不是一般文件
    err = -1;
    goto err;
  }

  fd_src = open(p_path_src, O_RDONLY);
  if (fd_src < 0)
  {
    zlog_error(gp_utilities_zlogc, "open %s error: %s", p_path_src, strerror(errno));
    err = -1;
    goto err;
  }

  fd_dst = open(p_path_dst, O_WRONLY | O_CREAT,
                file_stat.st_mode & (S_ISUID | S_ISGID | S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO));
  if (fd_dst < 0)
  {
    zlog_error(gp_utilities_zlogc, "open %s error: %s", p_path_dst, strerror(errno));
    err = -1;
    goto err_close_src;
  }

  //清空目的文件
  if (ftruncate(fd_dst, 0) != 0)
  {
    zlog_error(gp_utilities_zlogc, "ftruncate %s error: %s", p_path_dst, strerror(errno));
    err = -1;
    goto err_close_dst;
  }

  left_read = file_stat.st_size;
  while (left_read > 0)
  {
    //本次读取大小
    size = left_read <= sizeof(buf) ? left_read : sizeof(buf);
    left_read -= size;

    //读取数据
    size = read(fd_src, buf, size);
    if (-1 == size)
    { //读取错误
      zlog_error(gp_utilities_zlogc, "read %s error: %s", p_path_src, strerror(errno));
      err = -1;
      goto err_close_dst;
    }
    else if (0 == size)
    { //读取完成
      goto err_close_dst;
    }

    //写入数据
    left_write = size;
    while (left_write > 0)
    {
      size = write(fd_dst, buf, left_write);
      if (size <= 0)
      {
        zlog_error(gp_utilities_zlogc, "write %s error: %s", p_path_dst, strerror(errno));
        err = -1;
        goto err_close_dst;
      }
      left_write -= size;
    }
  }

err_close_dst:
  close(fd_dst);
err_close_src:
  close(fd_src);
err:
  return err;
}

/**
 * \brief 删除文件
 */
int file_remove (const char *p_path)
{
  int err = 0;

  if ((NULL == p_path) || ('\0' == *p_path))
  {
    err = -1;
    goto err;
  }

  if (access(p_path, F_OK) != 0)
  {
    err = -2;
    goto err;
  }

  if (remove(p_path) != 0)
  {
    zlog_error(gp_utilities_zlogc, "remove %s error: %s", p_path, strerror(errno));
    err = -3;
    goto err;
  }

err:
  return err;
}

/**
 * \brief 创建文件夹
 */
int file_mkdirs (const char *p_dir, mode_t mode)
{
  int  i;
  int  len;
  char str[PATH_MAX];
  int  err = 0;

  if ((NULL == p_dir) || ('\0' == *p_dir))
  {
    err = -1;
    goto err;
  }

  strncpy(str, p_dir, sizeof(str));
  len = strlen(str);
  for (i = 0; i < len; i++)
  {
    if ((str[i] == '/') && (i > 0))
    {
      str[i] = '\0';
      if (access(str, F_OK) != 0)
      {
        if (mkdir(str, mode) != 0)
        {
          zlog_error(gp_utilities_zlogc, "mkdir %s error: %s", str, strerror(errno));
          err = -2;
          goto err;
        }
      }
      str[i] = '/';
    }
  }

  if ((len > 0) && (access(str, F_OK) != 0))
  {
    if (mkdir(str, mode) != 0)
    {
      zlog_error(gp_utilities_zlogc, "mkdir %s error: %s", str, strerror(errno));
      err = -2;
      goto err;
    }
  }

err:
  return err;
}

/**
 * \brief 文件大小获取
 */
int file_size_get (const char *p_path)
{
  struct stat file_stat = {0};

  if (stat(p_path, &file_stat) != 0)
  {
    return -1;
  }

  return file_stat.st_size;
}

/**
 * \brief 文件大小字符串获取
 */
int file_size_str_get (char *p_str, size_t str_size, uint64_t file_size)
{
  int                i         = 0;
  double             size      = file_size;
  const static char *sp_unit[] = {"B", "KB", "MB", "GB", "TB"};
  int                err       = 0;

  if (NULL == p_str)
  {
    err = -1;
    goto err;
  }

  for (i = 0; i < ARRAY_SIZE(sp_unit) - 1; i++)
  {
    if (file_size < 1024)
    {
      break;
    }
    size = file_size;
    file_size /= 1024;
    size /= 1024.0;
  }
  snprintf(p_str, str_size, "%.1lf%s", size, sp_unit[i]);

err:
  return err;
}

/**
 * \brief 文件后缀获取
 */
char *file_suffix_get (char *p_path, size_t path_size)
{
  char *p_suffix = NULL;
  int   i        = 0;

  if ((NULL == p_path) || (0 == path_size))
  {
    goto err;
  }

  for (i = path_size - 1; i > 0; i--)
  {
    if ('.' == p_path[i])
    {
      p_suffix = &p_path[i + 1];
      break;
    }
  }

err:
  return p_suffix;
}

/* end of file */
