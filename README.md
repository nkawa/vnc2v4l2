# vnc2v4l2
Capture VNC to send v4l2loopback

## Usage

VNC image -> MJPEG -> Video4Linux

So, you can record VNC or send VNC image.

Usage: vnc2v4l2 [OPTION...]
vnc2v4l2  -- a program to receive vnc and send JPEG to v4l2 loopback device

  -d, --video-dev[=video_device]   v4l2 video device
  -q, --quality[=quality num]   JPEG Quality
  -v, --vnc[=vnc_address]    VNC server address:port
  -?, --help                 Give this help list
      --usage                Give a short usage message
  -V, --version              Print program version



## Requirements
libjpeg / libjpeg-turbo
libvncclient


## Compile

make


## Japanese
VNC をキャプチャして、video4linux の loopback デバイスに
映像を飛ばす仕組み。

WebRTC client と併用すれば、WebRTC 上に VNC を送信可能

