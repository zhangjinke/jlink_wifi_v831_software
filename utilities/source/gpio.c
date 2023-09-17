/**
 * \file
 * \brief GPIO
 *
 * \internal
 * \par Modification history
 * - 1.00 22-07-25  zjk, first implementation
 * \endinternal
 */

#include "gpio.h"
#include "file.h"
#include "utilities.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
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
 * \brief GPIO 导出
 */
int gpio_export (int gpio_num)
{
  int  fd;
  char path[64];
  char gpio[8];
  int  err = 0;

  snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d", gpio_num);
  if (file_is_type(path, S_IFDIR))
  { //引脚已导出
    goto err;
  }

  strncpy(path, "/sys/class/gpio/export", sizeof(path));
  fd = open(path, O_WRONLY);
  if (fd <= 0)
  {
    zlog_error(gp_utilities_zlogc, "open %s error: %s", path, strerror(errno));
    err = -1;
    goto err;
  }

  snprintf(gpio, sizeof(gpio), "%d", gpio_num);
  if (write(fd, gpio, strlen(gpio)) != strlen(gpio))
  {
    zlog_error(gp_utilities_zlogc, "write %s %d error: %s", path, gpio_num, strerror(errno));
    err = -1;
    goto err_fd_close;
  }

err_fd_close:
  close(fd);
err:
  return err;
}

/**
 * \brief GPIO 方向设置
 */
int gpio_direction_set (int gpio_num, int direction)
{
  int   fd;
  char  path[64];
  char *p_dir;
  int   err = 0;

  switch (direction)
  {
    case 0:  p_dir = "low";  break;
    case 1:  p_dir = "high"; break;
    default: p_dir = "in";   break;
  }

  snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", gpio_num);
  fd = open(path, O_WRONLY);
  if (fd <= 0)
  {
    zlog_error(gp_utilities_zlogc, "open %s error: %s", path, strerror(errno));
    err = -1;
    goto err;
  }

  if (write(fd, p_dir, strlen(p_dir)) != strlen(p_dir))
  {
    zlog_error(gp_utilities_zlogc, "write %s error: %s", path, strerror(errno));
    err = -1;
    goto err_fd_close;
  }

err_fd_close:
  close(fd);
err:
  return err;
}

/**
 * \brief GPIO 电平获取
 */
int gpio_value_get (int gpio_num)
{
  int  fd;
  char path[64];
  char buf[8];
  int  err = 0;

  snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", gpio_num);
  fd = open(path, O_RDONLY);
  if (fd <= 0)
  {
    zlog_error(gp_utilities_zlogc, "open %s error: %s", path, strerror(errno));
    err = -1;
    goto err;
  }

  if (read(fd, buf, sizeof(buf)) < 1)
  {
    zlog_error(gp_utilities_zlogc, "read %s error: %s", path, strerror(errno));
    err = -1;
    goto err_fd_close;
  }
  err = atoi(buf);
  if (err < 0)
  {
    err = -err;
  }

err_fd_close:
  close(fd);
err:
  return err;
}

/**
 * \brief GPIO 电平设置
 */
int gpio_value_set (int gpio_num, bool value)
{
  int  fd;
  char path[64];
  int  err = 0;

  snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", gpio_num);
  fd = open(path, O_WRONLY);
  if (fd <= 0)
  {
    zlog_error(gp_utilities_zlogc, "open %s error: %s", path, strerror(errno));
    err = -1;
    goto err;
  }

  if (write(fd, value ? "1" : "0", 1) != 1)
  {
    zlog_error(gp_utilities_zlogc, "write %s error: %s", path, strerror(errno));
    err = -1;
    goto err_fd_close;
  }

err_fd_close:
  close(fd);
err:
  return err;
}

/* end of file */
