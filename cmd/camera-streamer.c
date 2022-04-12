#include "http/http.h"
#include "opts/opts.h"
#include "opts/log.h"
#include "opts/fourcc.h"
#include "device/camera/camera.h"

#include <signal.h>
#include <unistd.h>

extern unsigned char html_index_html[];
extern unsigned int html_index_html_len;
extern unsigned char html_video_html[];
extern unsigned int html_video_html_len;
extern unsigned char html_jmuxer_min_js[];
extern unsigned int html_jmuxer_min_js_len;

http_method_t http_methods[] = {
  { "GET /snapshot?", http_snapshot },
  { "GET /stream?", http_stream },
  { "GET /?action=snapshot", http_snapshot },
  { "GET /?action=stream", http_stream },
  { "GET /video?", http_content, "text/html", html_video_html, 0, &html_video_html_len },
  { "GET /video.h264?", http_h264_video },
  { "GET /video.mkv?", http_mkv_video },
  { "GET /video.mp4?", http_mp4_video },
  { "GET /jmuxer.min.js?", http_content, "text/javascript", html_jmuxer_min_js, 0, &html_jmuxer_min_js_len },
  { "GET /?", http_content, "text/html", html_index_html, 0, &html_index_html_len },
  { }
};

camera_options_t camera_options = {
  .path = "/dev/video0",
  .width = 1920,
  .height = 1080,
  .format = 0,
  .nbufs = 3,
  .fps = 30,
  .allow_dma = true,
  .high_res_factor = 1.0,
  .low_res_factor = 0.0,
  .h264 = {
    .options =
      "video_bitrate_mode=0" OPTION_VALUE_LIST_SEP
      "video_bitrate=5000000" OPTION_VALUE_LIST_SEP
      "repeat_sequence_header=5000000" OPTION_VALUE_LIST_SEP
      "h264_i_frame_period=30" OPTION_VALUE_LIST_SEP
      "h264_level=11" OPTION_VALUE_LIST_SEP
      "h264_profile=4" OPTION_VALUE_LIST_SEP
      "h264_minimum_qp_value=16" OPTION_VALUE_LIST_SEP
      "h264_maximum_qp_value=32"
  },
  .jpeg = {
    .options = "compression_quality=80"
  },
};

http_server_options_t http_options = {
  .port = 8080,
  .maxcons = 10
};

log_options_t log_options = {
  .debug = false,
  .verbose = false
};

option_value_t camera_formats[] = {
  { "DEFAULT", 0 },
  { "YUYV", V4L2_PIX_FMT_YUYV },
  { "YUV420", V4L2_PIX_FMT_YUV420 },
  { "YUYV", V4L2_PIX_FMT_YUYV },
  { "MJPG", V4L2_PIX_FMT_MJPEG },
  { "MJPEG", V4L2_PIX_FMT_MJPEG },
  { "H264", V4L2_PIX_FMT_H264 },
  { "RG10", V4L2_PIX_FMT_SRGGB10P },
  { "RGB565", V4L2_PIX_FMT_RGB565 },
  { "RGBP", V4L2_PIX_FMT_RGB565 },
  { "RGB24", V4L2_PIX_FMT_RGB24 },
  { "RGB", V4L2_PIX_FMT_RGB24 },
  {}
};

option_value_t camera_type[] = {
  { "v4l2", CAMERA_V4L2 },
  { "libcamera", CAMERA_LIBCAMERA },
  {}
};

option_t all_options[] = {
  DEFINE_OPTION_PTR(camera, path, string),
  DEFINE_OPTION(camera, width, uint),
  DEFINE_OPTION(camera, height, uint),
  DEFINE_OPTION_VALUES(camera, format, camera_formats),
  DEFINE_OPTION(camera, nbufs, uint),
  DEFINE_OPTION_VALUES(camera, type, camera_type),
  DEFINE_OPTION(camera, fps, uint),
  DEFINE_OPTION_DEFAULT(camera, allow_dma, bool, "1"),
  DEFINE_OPTION(camera, high_res_factor, float),
  DEFINE_OPTION(camera, low_res_factor, float),
  DEFINE_OPTION_PTR(camera, options, list),

  DEFINE_OPTION_PTR(camera, isp.options, list),
  DEFINE_OPTION_PTR(camera, jpeg.options, list),
  DEFINE_OPTION_PTR(camera, h264.options, list),

  DEFINE_OPTION(http, port, uint),
  DEFINE_OPTION(http, maxcons, uint),

  DEFINE_OPTION_DEFAULT(log, debug, bool, "1"),
  DEFINE_OPTION_DEFAULT(log, verbose, bool, "1"),
  DEFINE_OPTION_PTR(log, filter, list),

  {}
};

int main(int argc, char *argv[])
{
  int http_fd = -1;
  int ret = -1;
  const char *env;
  camera_t *camera;

  if (parse_opts(all_options, argc, argv) < 0) {
    return -1;
  }

  camera = camera_open(&camera_options);
  if (!camera) {
    goto error;
  }

  http_fd = http_server(&http_options, http_methods);
  if (http_fd < 0) {
    goto error;
  }

  ret = camera_run(camera);

error:
  close(http_fd);
  camera_close(camera);
  return ret;
}
