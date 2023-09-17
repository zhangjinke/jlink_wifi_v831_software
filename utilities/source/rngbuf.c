/*******************************************************************************
*                                 AMetal
*                       ----------------------------
*                       innovating embedded platform
*
* Copyright (c) 2001-2018 Guangzhou ZHIYUAN Electronics Co., Ltd.
* All rights reserved.
*
* Contact information:
* web site:    http://www.zlg.cn/
*******************************************************************************/

/**
 * \file
 * \brief 通用环形缓冲区
 *
 * \internal
 * \par modification history
 * - 1.00 15-12-09  tee, add implementation from AWorks
 * \endinternal
 */

#include "rngbuf.h"
#include "utilities.h"
#include <errno.h>
#include <string.h>

/******************************************************************************/
int rngbuf_init (struct rngbuf *p_rb, char *p_buf, size_t size)
{
    if (size == 0 || p_buf == NULL) {
        return -EINVAL;
    }

    p_rb->in   = 0;
    p_rb->out  = 0;
    p_rb->buf  = p_buf;
    p_rb->size = size;

    return 0;
}

/******************************************************************************/
int rngbuf_putchar (rngbuf_t rb, const char data)
{
    int in = rb->in;

    if (in != rb->out - 1) {
        if (in == rb->size - 1) {
            if (0 != rb->out) {
                rb->buf[in] = data;
                rb->in      = 0;
                return 1;
            }
        } else {
            rb->buf[in] = data;
            rb->in++;
            return 1;
        }
    }

    return 0;
}

/******************************************************************************/
int rngbuf_getchar (rngbuf_t rb, char *p_data)
{
    int out = rb->out;

    if (out != rb->in) {
        *p_data = rb->buf[out];
        rb->out = ++out >= rb->size ? 0 : out;
        return 1;
    }

    return 0;
}

/******************************************************************************/
size_t rngbuf_put (rngbuf_t rb, const char *p_buf, size_t nbytes)
{
    int out = rb->out;
    int in;
    int bytes_put;
    int bytes_tmp;

    if (out > rb->in) {
        /* out is ahead of in.  We can fill up to two bytes before out */

        bytes_put = MIN(nbytes, out - rb->in - 1);
        memcpy(&rb->buf[rb->in], p_buf, bytes_put);
        rb->in += bytes_put;

    } else if (out == 0) {
        /*
         * out is at the beginning of the buffer.  We can fill till
         * the next-to-last element
         */

        bytes_put = MIN(nbytes, rb->size - rb->in - 1);
        memcpy(&rb->buf[rb->in], p_buf, bytes_put);
        rb->in += bytes_put;

    } else {
        /*
         * out has wrapped around, and its not 0, so we can fill
         * at least to the size of the ring buffer.  Do so, then see if
         * we need to wrap and put more at the beginning of the buffer.
         */

        bytes_put = MIN(nbytes, rb->size - rb->in);
        memcpy(&rb->buf[rb->in], p_buf, bytes_put);
        in = rb->in + bytes_put;

        if (in == rb->size) {
            /* We need to wrap, and perhaps put some more chars */

            bytes_tmp = MIN(nbytes - bytes_put, out - 1);
            memcpy(rb->buf, p_buf + bytes_put, bytes_tmp);
            rb->in = bytes_tmp;
            bytes_put += bytes_tmp;
        } else {
            rb->in = in;
        }
    }

    return (bytes_put);
}

/******************************************************************************/
size_t rngbuf_get (rngbuf_t rb, char *p_buf, size_t nbytes)
{
    int in = rb->in;
    int out;
    int bytes_got;
    int bytes_tmp;

    if (in >= rb->out) {
        /* in has not wrapped around */

        bytes_got = MIN(nbytes, in - rb->out);
        memcpy(p_buf, &rb->buf[rb->out], bytes_got);
        rb->out += bytes_got;

    } else {
        /*
         * in has wrapped around.  Grab chars up to the size of the
         * buffer, then wrap around if we need to.
         */

        bytes_got = MIN(nbytes, rb->size - rb->out);
        memcpy(p_buf, &rb->buf[rb->out], bytes_got);
        out = rb->out + bytes_got;

        /*
         * If out is equal to size, we've read the entire buffer,
         * and need to wrap now.  If bytes_got < nbytes, copy some more chars
         * in now.
         */

        if (out == rb->size) {
            bytes_tmp = MIN(nbytes - bytes_got, in);
            memcpy(p_buf + bytes_got, rb->buf, bytes_tmp);
            rb->out = bytes_tmp;
            bytes_got += bytes_tmp;
        } else {
            rb->out = out;
        }
    }

    return (bytes_got);
}

/******************************************************************************/
void rngbuf_flush (rngbuf_t rb)
{
    rb->in  = 0;
    rb->out = 0;
}

/******************************************************************************/
bool rngbuf_isempty (rngbuf_t rb)
{
    return (bool)(rb->in == rb->out);
}

/******************************************************************************/
bool rngbuf_isfull (rngbuf_t rb)
{
    int n = rb->in - rb->out + 1;

    return (bool)((n == 0) || (n == rb->size));
}

/******************************************************************************/
size_t rngbuf_freebytes (rngbuf_t rb)
{
    int n = rb->out - rb->in - 1;

    if (n < 0) {
        n += rb->size;
    }

    return (n);
}

/******************************************************************************/
size_t rngbuf_nbytes (rngbuf_t rb)
{
    int n = rb->in - rb->out;

    if (n < 0) {
        n += rb->size;
    }

    return (n);
}

/******************************************************************************/
void rngbuf_put_ahead (rngbuf_t rb, char byte, size_t offset)
{
    int n = rb->in + offset;

    if (n >= rb->size) {
        n -= rb->size;
    }

    *(rb->buf + n) = byte;
}

/******************************************************************************/
void rngbuf_move_ahead (rngbuf_t rb, size_t n)
{
    n += rb->in;

    if (n >= rb->size) {
        n -= rb->size;
    }

    rb->in = n;
}

/* end of file */
