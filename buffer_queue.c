#include "buffer.h"
#include "buffer_list.h"
#include "device.h"

#include <pthread.h>

pthread_mutex_t buffer_lock = PTHREAD_MUTEX_INITIALIZER;

bool buffer_use(buffer_t *buf)
{
  if (!buf) {
    return false;
  }

  pthread_mutex_lock(&buffer_lock);
  if (!buf->enqueued) {
    pthread_mutex_unlock(&buffer_lock);
    return false;
  }

  buf->mmap_reflinks += 1;
  pthread_mutex_unlock(&buffer_lock);
  return true;
}

bool buffer_consumed(buffer_t *buf)
{
  if (!buf) {
    return false;
  }

  pthread_mutex_lock(&buffer_lock);
  if (buf->mmap_reflinks > 0) {
    buf->mmap_reflinks--;
  }

  if (!buf->enqueued && buf->mmap_reflinks == 0) {
    // update used bytes
    if (buf->buf_list->do_mplanes) {
      buf->v4l2_plane.bytesused = buf->used;
    } else {
      buf->v4l2_buffer.bytesused = buf->used;
    }

#if 0
    uint64_t now = get_now_monotonic_u64();
    struct timeval ts = {
      .tv_sec = now / 1000000,
      .tv_usec = now % 1000000,
    };

    buf->v4l2_buffer.timestamp.tv_sec = ts.tv_sec;
    buf->v4l2_buffer.timestamp.tv_usec = ts.tv_usec;
#endif

    E_LOG_DEBUG(buf, "Queuing buffer... used=%zu length=%zu (linked=%s)", buf->used, buf->length, buf->mmap_source ? buf->mmap_source->name : NULL);
    E_XIOCTL(buf, buf->buf_list->device->fd, VIDIOC_QBUF, &buf->v4l2_buffer, "Can't queue buffer.");
    buf->enqueued = true;
  }

  pthread_mutex_unlock(&buffer_lock);
  return true;


error:
  {
    buffer_t *mmap_source = buf->mmap_source;
    buf->mmap_source = NULL;

    pthread_mutex_unlock(&buffer_lock);

    if (mmap_source) {
      buffer_consumed(mmap_source);
    }
  }

  return false;
}

buffer_t *buffer_list_find_slot(buffer_list_t *buf_list)
{
  buffer_t *buf = NULL;

  for (int i = 0; i < buf_list->nbufs; i++) {
    if (!buf_list->bufs[i]->enqueued) {
      buf = buf_list->bufs[i];
      break;
    }
  }

  return buf;
}

int buffer_list_enqueue(buffer_list_t *buf_list, buffer_t *dma_buf)
{
  if (!buf_list->do_mmap && !dma_buf->buf_list->do_mmap) {
    E_LOG_PERROR(buf_list, "Cannot enqueue non-mmap to non-mmap: %s.", dma_buf->name);
  }

  buffer_t *buf = buffer_list_find_slot(buf_list);
  if (!buf) {
    return 0;
  }

  if (buf_list->do_mmap) {
    if (dma_buf->used > buf->length) {
      E_LOG_PERROR(buf_list, "The dma_buf (%s) is too long: %zu vs space=%zu",
        dma_buf->name, dma_buf->used, buf->length);
    }

    E_LOG_DEBUG(buf, "mmap copy: dest=%p, src=%p (%s), size=%zu, space=%zu",
      buf->start, dma_buf->start, dma_buf->name, dma_buf->used, buf->length);

    memcpy(buf->start, dma_buf->start, dma_buf->used);
  } else {
    if (buf_list->do_mplanes) {
      buf->v4l2_plane.m.fd = dma_buf->dma_fd;
    } else {
      buf->v4l2_buffer.m.fd = dma_buf->dma_fd;
    }

    E_LOG_DEBUG(buf, "dmabuf copy: dest=%p, src=%p (%s, dma_fd=%d), size=%zu",
      buf->start, dma_buf->start, dma_buf->name, dma_buf->dma_fd, dma_buf->used);

    buf->mmap_source = dma_buf;
    dma_buf->mmap_reflinks++;
  }

  buf->used = dma_buf->used;
  buffer_consumed(buf);
  return 1;
error:
  return -1;
}

buffer_t *buffer_list_dequeue(buffer_list_t *buf_list)
{
	struct v4l2_buffer v4l2_buf = {0};
	struct v4l2_plane v4l2_plane = {0};

	v4l2_buf.type = buf_list->type;
  v4l2_buf.memory = V4L2_MEMORY_MMAP;

	if (buf_list->do_mplanes) {
		v4l2_buf.length = 1;
		v4l2_buf.m.planes = &v4l2_plane;
	}

	E_XIOCTL(buf_list, buf_list->device->fd, VIDIOC_DQBUF, &v4l2_buf, "Can't grab capture buffer");

  buffer_t *buf = buf_list->bufs[v4l2_buf.index];
	if (buf_list->do_mplanes) {
    buf->used = v4l2_plane.bytesused;
  } else {
    buf->used = v4l2_buf.bytesused;
  }

  buf->enqueued = false;
  buf->mmap_reflinks = 1;

	E_LOG_DEBUG(buf_list, "Grabbed mmap buffer=%u, bytes=%d, used=%d, frame=%d, linked=%s",
    buf->index, buf->length, buf->used, buf_list->frames, buf->mmap_source ? buf->mmap_source->name : NULL);

  if (buf->mmap_source) {
    buf->mmap_source->used = 0;
    buffer_consumed(buf->mmap_source);
    buf->mmap_source = NULL;
  }

  buf_list->frames++;
  return buf;

error:
  return NULL;
}

