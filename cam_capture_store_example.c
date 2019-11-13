#include <string.h>
#include <stdlib.h>
#include "libfg2.h"
#include "frame.h"


//print out all devices here
static int list_all_camera_devices(fg_grabber*     fg)
{
  GList *l = fg->devices_list;
  int cnt = 0;
  fg_debug("fg_init():	list all video devices :");
  for(l=fg->devices_list->next; l != NULL; l = l->next, cnt++){
	  fg_device_info	*info = l->data;   
	  printf("device %d  %s : friendly name = %s \n\n", cnt, info->device_path, info->friendly_name);
	  
  }
  return cnt;   
}

//list all camera capbilitys here
static int list_all_camera_capbilities(fg_grabber*     fg)
{
  GList *l = fg->camera_caps_list;
  int cnt = 0;
  fg_debug("fg_open():	list all video camera caps :");
  for(l = fg->camera_caps_list->next; l != NULL; l = l->next, cnt++){
	  fg_camera_cap 	*cap = l->data;    
	  printf("pixelfomat = 0x%x  descrption = %12s : width = %4d height = %4d\n", cap->pixel_format, cap->description, cap->pixel_with, cap->pixel_height); 		  
  }
  return cnt;   
}


int main(int argc, char *argv[])
{

    /* get name of video device or default */
    char *device = (argc>1) ? argv[1] : "/dev/video0";

    /* get name of snapshot file or default */
    char *snap_file = (argc>2) ? argv[2] : "snapshot";
    FILE *rawDumpFile = NULL;
    char dumpfilename[256] = {0};
    int frames = 150, i = 0, ret = 0;
    fg_format fmt_set;

    /* open and initialize the fg_grabber */
    /*you can use path and device name to open device*/
    //device = "Logitech Webcam C925e";
    fg_grabber *fg = fg_init();     
    if(!fg) {
      printf("fg_init() failed\n");
      exit(0);
    }
	
    if(fg_open(fg, device) == NULL) {
	  printf("fg_open() failed\n");
      exit(0);
    }
    
	
    FG_CLEAR(fmt_set);
    fmt_set.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt_set.fmt.pix.width = 1280;
    fmt_set.fmt.pix.height = 720;
    fmt_set.fmt.pix.pixelformat = FG_FORMAT_YUV420; //FG_FORMAT_YUYV;
    fmt_set.fmt.pix.field = V4L2_FIELD_INTERLACED;

    //set format must before fg_frame_new
    if(fg_set_fg_format(fg, &fmt_set) < 0) {
      printf("fg_open() failed, using default format\n");      
    }
    //fg_dump_info(fg);
    /* allocate a new fg_frame and fill it with data */
    fg_frame *fr = fg_frame_new(fg);
    if(fr == NULL) {
		printf("fg_frame_new() failed, no fr alloced \n");
        exit(0);
    }
   
    /* open dump file to dump raw data*/   
    strcpy(dumpfilename, snap_file);
    if (fr->format == FG_FORMAT_RGB24)
    {
	  strcat(dumpfilename, ".rgb");
    }else if(fr->format == FG_FORMAT_YUV420 || fr->format == FG_FORMAT_YUYV) {
      strcat(dumpfilename, ".yuv");
    }
    
    if((rawDumpFile = fopen(dumpfilename, "wb")) == NULL) {
		printf("failed to open dump file");
        return 1;
    }
   
	for(i = 0; i < frames; i++) {
		ret = fg_grab(fg, fr);
		if(ret < 0) { 
          printf("fg_grab failed\n");
        }else {
			fg_frame_save(fr, rawDumpFile);
        } 		

    }
	
    /* save the fg_frame as a JPEG */
    //fg_frame_save_Jpeg(fr, snap_file);

    /* free memory for fg_grabber and close device */
    fg_close(fg);

    /* free memory for fg_frame */
    fg_frame_release(fr);


	fclose(rawDumpFile);

    return 0;
}
