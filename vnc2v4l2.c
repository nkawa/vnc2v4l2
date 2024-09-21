#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <argp.h>

#include <jpeglib.h>

#include <rfb/rfbclient.h>
#include <linux/videodev2.h>


#define VIDEO_DEVICE "/dev/video0"

struct timespec lasttime;
int video_fd ;

// 圧縮後のデータ
unsigned char *compressed;

int V_WIDTH  = 1280;
int V_HEIGHT = 800;
int JPEG_quality = 75;


int openVideo(char *videoDevice){
  struct v4l2_capability cap;
  struct v4l2_format fmt;  

  int fd = open(videoDevice, O_RDWR);

  if (fd == -1) {
    perror("Opening video device");
    exit(1);
  }

  // capability check
  if (-1 == ioctl(fd, VIDIOC_QUERYCAP, &cap)){
    perror("QueryCap");
    exit(1);
  }

  if (!(cap.capabilities &V4L2_CAP_STREAMING)){
    perror("Can't stream!");
    exit(1);
  }

  fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  fmt.fmt.pix.width = V_WIDTH;
  fmt.fmt.pix.height = V_HEIGHT;
  //  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;
  //  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;  
  //fmt.fmt.pix.field = V4L2_FIELD_ANY;
  fmt.fmt.pix.field = V4L2_FIELD_NONE;
  
  if (ioctl(fd, VIDIOC_S_FMT, &fmt) == -1){
    perror("Can't set video format");
    exit(1);
  }

  // malloc destination
  compressed = (unsigned char*) malloc(V_WIDTH*V_HEIGHT*3);
  if(compressed == NULL){
    perror("Can't allocate memory!");
    exit(1);
  }
  
  return fd;
}



long compress_image(unsigned char *rgb_buffer, int width, int height){
  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;

  JSAMPROW row_pointer[1];
  int row_stride;

  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_compress(&cinfo);

  unsigned long outsize=0; // to obtain size
  jpeg_mem_dest(&cinfo, &compressed, &outsize);

  cinfo.image_width = width;
  cinfo.image_height = height;
  cinfo.input_components = 4;
  cinfo.in_color_space = JCS_EXT_RGBX;

  jpeg_set_defaults(&cinfo);
  jpeg_set_quality(&cinfo, JPEG_quality, TRUE);

  jpeg_start_compress(&cinfo, TRUE);

  row_stride = width * 4;

  int compline = 0;
  while (cinfo.next_scanline < cinfo.image_height) {
    row_pointer[0] = &rgb_buffer[cinfo.next_scanline * row_stride];
    jpeg_write_scanlines(&cinfo, row_pointer, 1);
  }

  jpeg_finish_compress(&cinfo);
  jpeg_destroy_compress(&cinfo);
  //  printf("Compreesed %d x %d with %ld bytes\n",width,height,outsize);
  return outsize;
}


void writeVideo(rfbClient *clt){
  long size = compress_image(clt->frameBuffer, clt->width, clt->height);
  
  if(write(video_fd, compressed, size) == -1){
    perror("Can't write video format");
    exit(1);
  }
}

// calc millsec diff of timespec
int  msec_diff(struct timespec *start, struct timespec *end){
  time_t sec;
  long nsec;
  if ((end->tv_nsec - start->tv_nsec) < 0) {
    sec = end->tv_sec - start->tv_sec - 1;
    nsec = 1000000000 + end->tv_nsec - start->tv_nsec;
  } else {
    sec = end->tv_sec - start->tv_sec;
    nsec = end->tv_nsec - start->tv_nsec;
  }
  return (int)(sec*1000+(int)(nsec/10000000));
}


static void HandleRect(rfbClient *clt, int x, int y, int w, int h){
  struct timespec newtime;
  
  // ここで framebuffer の内容を video に書き込む
  // 最後に書き込んだ時間を覚えておくのが良い

  clock_gettime(CLOCK_REALTIME, &newtime);

  // 最後の差が 30msec を越えたら
  int mdif = msec_diff(&lasttime,&newtime);

    //  printf("Get handle rect %d,%d %d-%d  %d\n", x,y,w,h,mdif);    
  if (mdif > 20){
    lasttime = newtime;
    writeVideo(clt);
  }
}


// from here main.
const char *argp_program_version = "vnc2v4l2 1.0";

/* Program documentation. */
static char doc[] =
  "vnc2v4l2  -- a program to receive vnc and send JPEG to v4l2 loopback device";

/* A description of the arguments we accept. */
static char args_doc[] = "";

/* The options we understand. */
static struct argp_option options[] = {
  {"vnc",  'v',  "vnc_address",      OPTION_ARG_OPTIONAL,  "VNC server address:port" },
  {"video-dev",'d', "video_device",      OPTION_ARG_OPTIONAL,  "v4l2 video device" },
  {"quality",   'q', "quality num",      OPTION_ARG_OPTIONAL, "JPEG Quality"},
  { 0 }
};

struct arguments
{
  char *vnc_address;
  int quality;
  char *video_device;
};

/* Parse a single option. */
static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  struct arguments *arguments = state->input;

  switch (key)
    {
    case 'v':
      arguments->vnc_address = arg;
      break;
    case 'q':
      arguments->quality = atoi(arg);
      if (arguments->quality==0){
	printf("Error JPEG quality\n");
	arguments->quality = JPEG_quality;
      }
      break;
    case 'd':
      arguments->video_device = arg;
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

/* Our argp parser. */
static struct argp argp = { options, parse_opt, args_doc, doc };


int main(int argc, char **argv)
{

  rfbClient* client = rfbGetClient(8,3,4);
  struct timespec newtime;
  struct arguments arguments = {
    "127.0.0.1:5900",
    JPEG_quality,
    VIDEO_DEVICE
  };

  argp_parse(&argp, argc, argv, 0,0,&arguments);
  

  clock_gettime(CLOCK_REALTIME, &lasttime);
  
  client->GotFrameBufferUpdate = HandleRect;

  int myargc = 1;
  char *myargv[2];
  myargv[0] = argv[0];
  myargv[1] = arguments.vnc_address;


  if (!rfbInitClient(client,&myargc,myargv))
    return 1;

  printf("VNC Client start! %d x %d \n", client->width, client->height);
  V_WIDTH  = client->width;
  V_HEIGHT = client->height;

  
  JPEG_quality = arguments.quality;
  printf("Set JPEG quality to %d for video deivce %s\n", JPEG_quality, arguments.video_device);

  //initialize video4linux loopback
  video_fd = openVideo(arguments.video_device);

  

  while (1) {
    /* After each idle second, send a message */
    if(WaitForMessage(client,50000)>0){

      HandleRFBServerMessage(client);
    }else{
      clock_gettime(CLOCK_REALTIME, &newtime);
      int mdif = msec_diff(&lasttime,&newtime);
      if (mdif > 20){
	lasttime = newtime;
	writeVideo(client);
      }
    }
  }

  rfbClientCleanup(client);

  return 0;
}
