#define _XOPEN_SOURCE 700
#include "video.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

bool
video_open (VideoDevice *dev)
{
    dev->fd = open (dev->device, O_RDWR | O_CLOEXEC);
    if (dev->fd < 0) {
        fprintf (stderr, "video: open(%s): %s\n", dev->device, strerror (errno));
        return false;
    }

    struct v4l2_format fmt = {
        .type    = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .fmt.pix = {
            .width       = (uint32_t)dev->width,
            .height      = (uint32_t)dev->height,
            .pixelformat = V4L2_PIX_FMT_NV12,
            .field       = V4L2_FIELD_NONE,
        },
    };
    if (ioctl (dev->fd, VIDIOC_S_FMT, &fmt) < 0) {
        fprintf (stderr, "video: VIDIOC_S_FMT: %s\n", strerror (errno));
        goto err_close;
    }
    dev->width  = (int)fmt.fmt.pix.width;
    dev->height = (int)fmt.fmt.pix.height;

    struct v4l2_streamparm parm = {
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .parm.capture.timeperframe = {
            .numerator   = 1,
            .denominator = (uint32_t)dev->fps,
        },
    };
    if (ioctl (dev->fd, VIDIOC_S_PARM, &parm) < 0)
        fprintf (stderr, "video: VIDIOC_S_PARM: %s (non-fatal)\n", strerror (errno));

    struct v4l2_requestbuffers req = {
        .count  = VIDEO_BUF_COUNT,
        .type   = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .memory = V4L2_MEMORY_MMAP,
    };
    if (ioctl (dev->fd, VIDIOC_REQBUFS, &req) < 0) {
        fprintf (stderr, "video: VIDIOC_REQBUFS: %s\n", strerror (errno));
        goto err_close;
    }
    if (req.count < VIDEO_BUF_COUNT) {
        fprintf (stderr, "video: VIDIOC_REQBUFS: driver alocou apenas %u buffers (pedido %d)\n",
                 req.count, VIDEO_BUF_COUNT);
        goto err_close;
    }

    for (uint32_t i = 0; i < VIDEO_BUF_COUNT; i++) {
        struct v4l2_buffer buf = {
            .index  = i,
            .type   = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .memory = V4L2_MEMORY_MMAP,
        };
        if (ioctl (dev->fd, VIDIOC_QUERYBUF, &buf) < 0) {
            fprintf (stderr, "video: VIDIOC_QUERYBUF[%u]: %s\n", i, strerror (errno));
            goto err_unmap;
        }
        dev->bufs[i].length = buf.length;
        dev->bufs[i].start  = mmap (NULL, buf.length,
            PROT_READ | PROT_WRITE, MAP_SHARED, dev->fd, buf.m.offset);
        if (dev->bufs[i].start == MAP_FAILED) {
            fprintf (stderr, "video: mmap[%u]: %s\n", i, strerror (errno));
            dev->bufs[i].start = NULL;
            goto err_unmap;
        }
        if (ioctl (dev->fd, VIDIOC_QBUF, &buf) < 0) {
            fprintf (stderr, "video: VIDIOC_QBUF[%u]: %s\n", i, strerror (errno));
            goto err_unmap;
        }
    }

    const enum v4l2_buf_type btype = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl (dev->fd, VIDIOC_STREAMON, &btype) < 0) {
        fprintf (stderr, "video: VIDIOC_STREAMON: %s\n", strerror (errno));
        goto err_unmap;
    }

    /* Notifica caller que stream está ativo — momento certo para aplicar controles */
    if (dev->on_stream_ready)
        dev->on_stream_ready (dev->on_stream_ready_data);

    printf ("video : %s  %dx%d NV12 @ %dfps\n",
            dev->device, dev->width, dev->height, dev->fps);
    return true;

err_unmap:
    for (int i = 0; i < VIDEO_BUF_COUNT; i++)
        if (dev->bufs[i].start && dev->bufs[i].start != MAP_FAILED)
            munmap (dev->bufs[i].start, dev->bufs[i].length);
err_close:
    close (dev->fd);
    dev->fd = -1;
    return false;
}

void
video_set_on_stream_ready (VideoDevice *dev, VideoOnStreamReady cb, void *userdata)
{
    dev->on_stream_ready      = cb;
    dev->on_stream_ready_data = userdata;
}

void
video_close (VideoDevice *dev)
{
    if (dev->fd < 0) return;
    const enum v4l2_buf_type btype = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl (dev->fd, VIDIOC_STREAMOFF, &btype);
    for (int i = 0; i < VIDEO_BUF_COUNT; i++)
        if (dev->bufs[i].start && dev->bufs[i].start != MAP_FAILED)
            munmap (dev->bufs[i].start, dev->bufs[i].length);
    close (dev->fd);
    dev->fd = -1;
}

bool
video_dequeue (VideoDevice *dev, VideoFrame *frame, bool *fatal)
{
    *fatal = false;

    /* Bloqueia até um frame ficar disponível (timeout 100ms para checar g_quit) */
    struct pollfd pfd = { .fd = dev->fd, .events = POLLIN };
    int ret = poll (&pfd, 1, 100);
    if (ret < 0) {
        if (errno == EINTR) return false;
        fprintf (stderr, "video: poll: %s\n", strerror (errno));
        *fatal = true;
        return false;
    }
    if (ret == 0) return false;   /* timeout — sem frame disponível */

    struct v4l2_buffer vbuf = {
        .type   = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .memory = V4L2_MEMORY_MMAP,
    };

    if (ioctl (dev->fd, VIDIOC_DQBUF, &vbuf) < 0) {
        if (errno == EAGAIN) return false;
        fprintf (stderr, "video: VIDIOC_DQBUF: %s\n", strerror (errno));
        *fatal = true;
        return false;
    }

    frame->index    = vbuf.index;
    frame->y_plane  = (const uint8_t *)dev->bufs[vbuf.index].start;
    frame->uv_plane = frame->y_plane + (size_t)dev->width * dev->height;
    return true;
}

void
video_enqueue (VideoDevice *dev, uint32_t index)
{
    struct v4l2_buffer vbuf = {
        .index  = index,
        .type   = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .memory = V4L2_MEMORY_MMAP,
    };
    if (ioctl (dev->fd, VIDIOC_QBUF, &vbuf) < 0)
        fprintf (stderr, "video: VIDIOC_QBUF[%u]: %s\n", index, strerror (errno));
}
