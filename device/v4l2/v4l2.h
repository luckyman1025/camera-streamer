#pragma once

#include <linux/videodev2.h>
#include <linux/v4l2-subdev.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct buffer_s buffer_t;
typedef struct buffer_list_s buffer_list_t;
typedef struct device_s device_t;
struct pollfd;

typedef struct device_v4l2_s {
  int dev_fd;
  int subdev_fd;
} device_v4l2_t;

int v4l2_device_open(device_t *dev);
void v4l2_device_close(device_t *dev);
int v4l2_device_set_decoder_start(device_t *dev, bool do_on);
int v4l2_device_video_force_key(device_t *dev);
int v4l2_device_set_fps(device_t *dev, int desired_fps);
int v4l2_device_set_option(device_t *dev, const char *key, const char *value);

int v4l2_buffer_open(buffer_t *buf);
void v4l2_buffer_close(buffer_t *buf);
int v4l2_buffer_enqueue(buffer_t *buf, const char *who);
int v4l2_buffer_list_dequeue(buffer_list_t *buf_list, buffer_t **bufp);
int v4l2_buffer_list_refresh_states(buffer_list_t *buf_list);
int v4l2_buffer_list_pollfd(buffer_list_t *buf_list, struct pollfd *pollfd, bool can_dequeue);

int v4l2_buffer_list_set_format(buffer_list_t *buf_list, unsigned width, unsigned height, unsigned format, unsigned bytesperline);
int v4l2_buffer_list_set_buffers(buffer_list_t *buf_list, int nbufs);
int v4l2_buffer_list_set_stream(buffer_list_t *buf_list, bool do_on);

int v4l2_device_open_media_device(device_t *dev);
int v4l2_device_open_v4l2_subdev(device_t *dev, int subdev);
int v4l2_device_set_pad_format(device_t *dev, unsigned width, unsigned height, unsigned format);
