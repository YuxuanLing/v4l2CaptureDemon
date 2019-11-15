#include "libfg2.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <asm/types.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <libv4l2.h>
#include <assert.h>


#define DEFAULT_SUPPORTED_SIZE  (8)
static fg_format defaults_support_formats[DEFAULT_SUPPORTED_SIZE] = 
										 {
                                            {.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
                                             .fmt.pix.width = 640, 
                                             .fmt.pix.height = 480, 
                                             .fmt.pix.pixelformat = FG_FORMAT_YUV420,
                                             .fmt.pix.field = V4L2_FIELD_INTERLACED
                                            },

                                            {.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
                                             .fmt.pix.width = 864, 
                                             .fmt.pix.height = 480, 
                                             .fmt.pix.pixelformat = FG_FORMAT_YUV420,
                                             .fmt.pix.field = V4L2_FIELD_INTERLACED
                                            },

                                            {.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
                                             .fmt.pix.width = 640, 
                                             .fmt.pix.height = 480, 
                                             .fmt.pix.pixelformat = FG_FORMAT_RGB24,
                                             .fmt.pix.field = V4L2_FIELD_INTERLACED
                                            }, 

											{.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
											 .fmt.pix.width = 864, 
											 .fmt.pix.height = 480, 
											 .fmt.pix.pixelformat = FG_FORMAT_RGB24,
											 .fmt.pix.field = V4L2_FIELD_INTERLACED
											} 
                                         };

//--------------------------------------------------------------------------

void fg_debug(const char *fmt, ...)
{
    va_list argp;
    va_start(argp, fmt);
#ifdef DEBUG
    vfprintf(stdout, fmt, argp);
    printf("\n");
#else
    vsyslog(LOG_MAKEPRI(LOG_USER, LOG_DEBUG), fmt, argp);
#endif
    va_end(argp);
}

void fg_debug_error(const char *fmt, ...)
{
    va_list argp;
    va_start(argp, fmt);
#ifdef DEBUG
    vfprintf(stderr, fmt, argp);
    fprintf(stderr, ": %s\n", strerror(errno));
#else
    vsyslog(LOG_MAKEPRI(LOG_USER, LOG_ERR), fmt, argp);
#endif
    va_end(argp);
}

//--------------------------------------------------------------------------

static int count_tuners(int fd)
{

    int i, num_tuners = 0;
    struct v4l2_tuner tun;

    FG_CLEAR(tun);

    for (i=0; i < FG_MAX_TUNERS; i++)
    {
        tun.index = i;
        if (v4l2_ioctl(fd, VIDIOC_G_TUNER, &tun) == -1)
            break;
        else
            num_tuners++;
    }

    return num_tuners;
}

/*
static int count_controls(int fd)
{
    int num_controls = 0;
    struct v4l2_queryctrl queryctrl;

    for (queryctrl.id = V4L2_CID_BASE;
            queryctrl.id < V4L2_CID_LASTP1;
            queryctrl.id++)
    {
        if ( v4l2_ioctl(fd, VIDIOC_QUERYCTRL, &queryctrl) == 0 )
        {
            if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED)
                continue;

            num_controls++;
        }
    }

    return num_controls;
}
*/




/*before fg_open , need fg_init first*/
fg_grabber *fg_init()
{
  fg_grabber *fg = malloc(sizeof(fg_grabber));
  const char *dev_base[] = { "/dev/video", "/dev/v4l2/video", NULL };
  int base, n, fd;
  GList *devices_list = NULL;


  memset(fg, 0, sizeof(fg_grabber));
  fg->devices_list = g_list_alloc();
  fg->camera_caps_list = g_list_alloc();

  if(fg == NULL || fg->devices_list == NULL || fg->camera_caps_list == NULL)
  {
	  fg_debug_error("fg_init(): ran out of memory allocating frame grabber.");
	  return NULL;

  }
  
  devices_list = fg->devices_list;  
  /*
  while (devices_list) {
    char *device = devices_list->data;
    devices_list = g_list_remove (devices_list, device);
    g_free (device);
  }
  */
  /*
  * detect /dev entries
  */
  for (n = 0; n < 64; n++) {
    for (base = 0; dev_base[base] != NULL; base++) {
      struct stat s;
      char *device = g_strdup_printf ("%s%d",dev_base[base],n);

      /*
       * does the /dev/ entry exist at all?
       */
      if (stat (device, &s) == 0) {
        /*
         * yes: is a device attached?
         */
        if (S_ISCHR (s.st_mode)) {

          if ((fd = open (device, O_RDWR | O_NONBLOCK)) > 0 || errno == EBUSY) {
            if (fd > 0) {
               fg_caps device_cap;
               fg_device_info *device_info = g_new(fg_device_info, 1);
               if (v4l2_ioctl(fd, VIDIOC_QUERYCAP, &(device_cap)) == -1)
               {
                    fg_init( "fg_open(): query capabilities failed" );
               }
               strncpy(device_info->device_path,   device, 32);
               strncpy(device_info->friendly_name, device_cap.card, 32);                
			   g_list_append (devices_list, device_info);
			   fg_debug("fg_init():  video device %s friendly name is %s added to device list !!!", device, device_cap.card);
               close (fd);
            }

            break;
          }
        }
      }
      g_free (device);
    }
  }

#ifdef DEBUG
  //list all devices here
  {
	GList *l = fg->devices_list;
    int cnt = 0;
    fg_debug("fg_init():  list all video devices :");
	for(l=fg->devices_list->next; l != NULL; l = l->next, cnt++){
        fg_device_info    *info = l->data;   
		fg_debug("device %d  %s : friendly name = %s", cnt, info->device_path, info->friendly_name);
        
    }	
  }

#endif

  return fg;
}


/*
*  search and set device , dev could be device's friendly name or path 
*  if dev is null,  will use the first dev in the fg->divices_list and return 1 
*  else will search the dev as path or friendly name , if found return 1 
*  if success ,return 1 
*  failed return 0
*
*/
int search_and_set_camera_device(fg_grabber* fg, const char *dev)
{
	char *dev_search;
    int  cnt = 0 ,found = 0;

    if(fg->devices_list == NULL || fg == NULL)
    {
		fg_debug_error("search_and_set_camera_device():	set camera device failed , no device found or fg_init not called.");
	    return 0;
    }

	
	if(dev == NULL)
    {
       dev_search = strdup(FG_DEFAULT_DEVICE);
    } else {
       dev_search = strdup(dev);
    }

    
    {
	  GList *l = fg->devices_list;

	  for(l=fg->devices_list->next; l != NULL; l = l->next, cnt++) {
        fg_device_info    *info = l->data;
        if( strcmp(dev_search , info->device_path) == 0 || strcmp(dev_search , info->friendly_name) == 0 ) {
           found = 1;
           strncpy(fg->curr_device.device_path ,   info->device_path, 32);
		   strncpy(fg->curr_device.friendly_name , info->friendly_name, 32);
           break;
        }           
      }
      	
    }
    
    free(dev_search);

    if(found) {
    	return 1;
    }else {
        return 0;
    } 

}



/*only after success fg_open ,  camera caps can be got*/
fg_grabber *fg_open(fg_grabber* fg, const char *dev)
{

    int i, j;
    struct stat st;
    struct v4l2_crop crop;

    if (fg == NULL)
    {
        fg_debug_error("fg_open():  frame grabber is null , please init it before using.");
        return NULL;
    }

    fg->inputs = NULL;
    fg->tuners = NULL;
    fg->controls = NULL;

    // Use default device if none specified    
    if(search_and_set_camera_device(fg, dev)) {
        fg_debug(" \n\nfg_open(): success found and set camera device  path = %s  name = %s", fg->curr_device.device_path, fg->curr_device.friendly_name);
    }else {
        fg_debug_error("fg_open(): failed to found and set camera device path = %s  name = %s", fg->curr_device.device_path, fg->curr_device.friendly_name);
        return NULL;
    }


    // Verify the device exists
    if (stat(fg->curr_device.device_path, &st) == -1)
    {
        fg_debug_error("fg_open(): video device path = '%s' name = '%s' does not exist", fg->curr_device.device_path, fg->curr_device.friendly_name);
        goto error_exit;
    }

    // Verify the device is a character device
    if (!S_ISCHR(st.st_mode))
    {
        fg_debug_error("fg_open(): video device path = '%s' name = '%s' is not a character device", fg->curr_device.device_path, fg->curr_device.friendly_name);
        goto error_exit;
    }

    // Open the video device
    fg->fd = v4l2_open(fg->curr_device.device_path, O_RDWR | O_NONBLOCK, 0);
    if ( fg->fd == -1 )
    {
        fg_debug_error( "fg_open(): open video device failed" );
        goto error_exit;
    }

    // Make sure child processes don't inherit video (close on exec)
    fcntl(fg->fd, F_SETFD, FD_CLOEXEC);

    // Get the device capabilities
    if (v4l2_ioctl(fg->fd, VIDIOC_QUERYCAP, &(fg->caps)) == -1)
    {
        fg_debug_error( "fg_open(): query capabilities failed" );
        goto error_exit;
    }

    // Make sure video capture is supported
    if (!(fg->caps.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
        fg_debug_error("fg_open(): video device does not support video capture");
        goto error_exit;
    }

    // Determine the number of inputs
    if ( (fg->num_inputs = fg_get_input_count(fg)) < 1 )
    {
        fg_debug_error("fg_open(): no video inputs were found on video device");
        goto error_exit;
    }

    // Read info for all input sources, usually input should be 1 , and we only support 1 now
    fg->input = 0;
    fg->inputs = malloc(sizeof(struct v4l2_input) * fg->num_inputs);

    if (fg->inputs == NULL)
    {
        fg_debug_error("fg_open(): ran out of memory allocating inputs.");
        goto error_exit;
    }

    for (i=0; i < fg->num_inputs; i++)
    {
        fg->inputs[i].index = i;
        if (v4l2_ioctl(fg->fd, VIDIOC_ENUMINPUT, &(fg->inputs[i])) == -1)
        {
            fg_debug_error("fg_open(): error getting input information");
            goto error_exit;
        }
    }

    if (fg_set_input(fg, fg->input) == -1)
    {
        fg_debug_error("fg_open(): error setting default input");
        goto error_exit;
    }

    // Determine the number of tuners, not must
    fg->tuner = 0;
    if ((fg->num_tuners = count_tuners(fg->fd)) > 0)
    {
        // Read info for all tuners
        fg->tuners = malloc(sizeof(struct v4l2_tuner) * fg->num_tuners);

        if (fg->tuners == NULL)
        {
            fg_debug_error("fg_open(): ran out of memory allocating tuners");
            goto error_exit;
        }

        for (i=0; i < fg->num_tuners; i++)
        {
            fg->tuners[i].index = 0;
            if (v4l2_ioctl(fg->fd, VIDIOC_G_TUNER, &(fg->tuners[i])) == -1)
            {
                fg_debug_error("fg_open(): error getting tuner information");
                goto error_exit;
            }
        }
    }

    // Reset cropping to default (if supported), not must
    FG_CLEAR(fg->cropcap);
    fg->cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (v4l2_ioctl(fg->fd, VIDIOC_CROPCAP, &(fg->cropcap)) == 0)
    {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = fg->cropcap.defrect;
        if (v4l2_ioctl(fg->fd, VIDIOC_S_CROP, &crop) == -1)
        {
            if (errno == EINVAL)
            {
                fg_debug("fg_open(): warning: cropping not supported");
            }
            else
            {
                // used to be fatal but found devices which it doesn't work with
                fg_debug_error("fg_open(): error setting crop window on video device");
            }
        }
    }
    else
    {
        // should be fatal because the documentation says the VIDIOC_CROPCAP
        // is mandatory on capture devices, but several of my devices disagree
        fg_debug_error("fg_open(): warning: error getting cropping info");
    }

    // Set the default video format
    FG_CLEAR(fg->format);
    fg->format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; 
    fg->format.fmt.pix.width = FG_DEFAULT_WIDTH;
    fg->format.fmt.pix.height = FG_DEFAULT_HEIGHT;
    
    // libv4l should intervene here to support this format even if the device does not support it directly , 
    //such as RGB24 and YUV420P, so we can try select some resolution and format to set this, if it not returen error, then ok
    fg->format.fmt.pix.pixelformat = FG_FORMAT_DEFAULT;      //FG_FORMAT_YUV420;
    fg->format.fmt.pix.field = V4L2_FIELD_INTERLACED;      //V4L2_FIELD_INTERLACED;

    if (v4l2_ioctl(fg->fd, VIDIOC_S_FMT, &(fg->format)) == -1)
    {
        fg_debug_error("fg_open(): setting default video format failed");
        goto error_exit;
    }

    // Reset all supported controls to defaults
    if (fg_default_controls(fg) == -1)
    {
        fg_debug_error("fg_open(): failed setting controls to default.");
        goto error_exit;
    }



    //list all avaliable pixel formats and videosize from v4l2 apis
	struct v4l2_fmtdesc fmt;
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	for (i = 0; ; i++) 
	{ 
		int ret = 0;
        fmt.index = i;
		ret = ioctl(fg->fd, VIDIOC_ENUM_FMT, &fmt);
		if (-1 == ret)
		{
			break;	
		}
	
		/* list all video size for current format */	
		struct v4l2_frmsizeenum frmsize;
		frmsize.pixel_format = fmt.pixelformat;
		for (j = 0; ; j++)					
		{
		   fg_camera_cap *cam_cap = g_new(fg_camera_cap, 1);
           frmsize.index = j;
		   ret = ioctl(fg->fd, VIDIOC_ENUM_FRAMESIZES, &frmsize);
		   if (-1 == ret)					
		   {
				break;
		   }
		   cam_cap->pixel_format = fmt.pixelformat;
           cam_cap->pixel_with   = frmsize.discrete.width;
           cam_cap->pixel_height = frmsize.discrete.height;
           strncpy(cam_cap->description, fmt.description, 32);
           g_list_append(fg->camera_caps_list, cam_cap);
           
		   //following is a wrok around to add yuv420 format, since libv4l should intervene here to support this format even if the device does not support it directly
           //so if it supported YUYV , then assume it will also support 420p
           if(fmt.pixelformat == V4L2_PIX_FMT_YUYV) {
			   fg_camera_cap *cam_cap_420p = g_new(fg_camera_cap, 1);
			   cam_cap_420p->pixel_format = V4L2_PIX_FMT_YUV420;
			   cam_cap_420p->pixel_with	 = frmsize.discrete.width;
			   cam_cap_420p->pixel_height = frmsize.discrete.height;
			   strncpy(cam_cap_420p->description, "IYUV 4:2:0", 32);
			   g_list_append(fg->camera_caps_list, cam_cap_420p);
           }
          
			
		}
	
	}


#ifdef DEBUG
	  //list all camera capbilitys here
	  {
		GList *l = fg->camera_caps_list;
		int cnt = 0;
		fg_debug("fg_open():  list all video camera caps :");
		for(l = fg->camera_caps_list->next; l != NULL; l = l->next, cnt++){
			fg_camera_cap	  *cap = l->data;	 
			fg_debug("pixelfomat = 0x%x  descrption = %12s : width = %4d height = %4d", cap->pixel_format, cap->description, cap->pixel_with, cap->pixel_height);			
		}	
	  }
	
#endif

    // will not List available frame rates in order to make thing easier
    /*
    struct v4l2_frmivalenum fi;
    FG_CLEAR(fi);
    fi.index = 0;
    fi.pixel_format = fg->format.fmt.pix.pixelformat;
    fi.width = fg->format.fmt.pix.width;
    fi.height = fg->format.fmt.pix.height;
    if (v4l2_ioctl(fg->fd, VIDIOC_ENUM_FRAMEINTERVALS, &fi) == -1)
    {
        fg_debug_error("fg_open(): error enumerating frame intervals");
        goto error_exit;
    }
    switch(fi.type)
    {
        case V4L2_FRMIVAL_TYPE_DISCRETE:
            fprintf(stderr, "DISCRETE: %d/%d\n", fi.discrete.numerator, fi.discrete.denominator);
            break;
        case V4L2_FRMIVAL_TYPE_STEPWISE:
            fprintf(stderr, "STEPWISE\n");
            break;
        case V4L2_FRMIVAL_TYPE_CONTINUOUS:
            fprintf(stderr, "CONTINUOUS\n");
            break;
        default:
            fprintf(stderr, "ERR\n");
    }
    */

    return fg;

// Free memory allocated in this function and return NULL
error_exit:
    free(fg->inputs);
    free(fg->tuners);
    free(fg->controls);
    free(fg);
    return NULL;

}

//--------------------------------------------------------------------------

void fg_close(fg_grabber *fg)
{
   GList *devices_list = fg->devices_list;
   GList *cam_caps_list = fg->camera_caps_list;
   

   if (v4l2_close(fg->fd) != 0)
        fg_debug_error("fg_close(): warning: failed closing device file");


    // Make sure we free all memory (backwards!)
	while (devices_list) {
	  fg_device_info *device = devices_list->data;
	  devices_list = g_list_remove (devices_list, device);
	  g_free (device);
	}

	while (cam_caps_list) {
	  fg_camera_cap *cap = cam_caps_list->data;
	  cam_caps_list = g_list_remove (cam_caps_list, cap);
	  g_free (cap);
	}


    if(fg->inputs)free(fg->inputs);
    if(fg->tuners)free(fg->tuners);
    if(fg->controls)free(fg->controls);
    if(fg->devices_list)g_list_free(fg->devices_list);
    if(fg->camera_caps_list)g_list_free(fg->camera_caps_list);
    if(fg)free(fg);
}

//--------------------------------------------------------------------------

int fg_set_capture_size(fg_grabber *fg, fg_size size)
{
    fg->format.fmt.pix.width = size.width;
    fg->format.fmt.pix.height = size.height;

    if (v4l2_ioctl(fg->fd, VIDIOC_S_FMT, &(fg->format)) == -1)
    {
        fg_debug_error("fg_set_capture_size(): setting capture size to "
            "'%dx%d failed", size.width, size.height);
        return -1;
    }

    return 0;
}

fg_size fg_get_capture_size(fg_grabber *fg)
{
    fg_size size = { 0, 0 };
    if (v4l2_ioctl(fg->fd, VIDIOC_G_FMT, &(fg->format)) == -1)
    {
        fg_debug_error("fg_get_capture_size(): getting capture size failed");
        return size;

    }
    size.width = fg->format.fmt.pix.width;
    size.height = fg->format.fmt.pix.height;
    return size;
}

//--------------------------------------------------------------------------

int fg_set_capture_window(fg_grabber *fg, fg_rect rect)
{
    struct v4l2_crop crop;

    crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    crop.c.left = rect.left;
    crop.c.top = rect.top;
    crop.c.width = rect.width;
    crop.c.height = rect.height;

    if (v4l2_ioctl(fg->fd, VIDIOC_S_CROP, &crop) == -1)
    {
        if (errno == EINVAL)
        {
            fg_debug_error("fg_set_capture_window(): "
                            "device does not support cropping");
            return -1;
        }
        else
        {
            fg_debug_error("fg_set_capture_window(): "
                            "setting cropping window failed");
            return -1;
        }
    }

    return 0;
}

fg_rect fg_get_capture_window(fg_grabber *fg)
{
    fg_rect rect = { 0, 0, 0, 0 };
    struct v4l2_crop crop;

    crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (v4l2_ioctl(fg->fd, VIDIOC_S_CROP, &crop) == -1)
    {
        if (errno == EINVAL)
        {
            fg_debug_error("fg_get_capture_window(): "
                            "device does not support cropping");
            return rect;
        }
        else
        {
            fg_debug_error("fg_get_capture_window(): "
                            "getting cropping window failed");
            return rect;
        }
    }

    rect.left = crop.c.left;
    rect.top = crop.c.top;
    rect.width = crop.c.width;
    rect.height = crop.c.height;

    return rect;
}

//--------------------------------------------------------------------------

/*
int fg_enable_capture( fg_grabber* fg, int flag )
{
    if ( v4l1_ioctl( fg->fd, VIDIOCCAPTURE, (flag>0) ) < 0 )
    {
        fg_debug_error( "fg_enable_capture(): capture control failed" );
        return -1;
    }

    return 0;
}
*/

//--------------------------------------------------------------------------
int fg_set_format(fg_grabber *fg, int fmt)
{

    fg->format.fmt.pix.pixelformat = fmt;

    if (v4l2_ioctl(fg->fd, VIDIOC_S_FMT, &(fg->format)) == -1)
    {
        fg_debug_error("fg_set_format(): setting video format failed");
        return -1;
    }

    return 0;
}



int fg_set_fg_format(fg_grabber *fg, fg_format *fmt)
{

    //fg->format.fmt.pix.pixelformat = fmt;
    if(fmt == NULL ) return -1;
    memcpy(&(fg->format), fmt, sizeof(fg_format));
    if (v4l2_ioctl(fg->fd, VIDIOC_S_FMT, &(fg->format)) == -1)
    {
        fg_debug_error("fg_set_format(): setting video format failed");
        return -1;
    }

    return 0;
}



int fg_get_format(fg_grabber *fg)
{
    if (v4l2_ioctl(fg->fd, VIDIOC_G_FMT, &(fg->format)) == -1)
    {
        fg_debug_error("fg_get_format(): getting video format failed");
        return -1;
    }
    return fg->format.fmt.pix.pixelformat;
}

//--------------------------------------------------------------------------

int fg_get_input_count(fg_grabber *fg)
{
    int i, num_inputs = 0;
    struct v4l2_input inp;

    for (i=0; i < FG_MAX_INPUTS; i++)
    {
        inp.index = i;
        if (v4l2_ioctl(fg->fd, VIDIOC_ENUMINPUT, &inp) == -1)
            break;
        else
            num_inputs++;
    }

    return num_inputs;
}

int fg_get_input(fg_grabber *fg)
{
    int current_input;

    if (v4l2_ioctl(fg->fd, VIDIOC_G_INPUT, &current_input) == -1)
    {
        fg_debug_error("fg_get_input(): unable to get current input index.");
        return -1;
    }

    return current_input;
}

int fg_set_input(fg_grabber* fg, int index)
{
    struct v4l2_format fmt;

    if (index >= fg->num_inputs)
    {
        fg_debug_error("fg_set_input(): invalid input number");
        return -1;
    }

    FG_CLEAR(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (v4l2_ioctl(fg->fd, VIDIOC_G_FMT, &fmt) == -1)
    {
        fg_debug_error("fg_set_input(): failed saving current format");
        return -1;
    }

    while (v4l2_ioctl(fg->fd, VIDIOC_S_INPUT, &index) == -1)
    {
        switch(errno)
        {
            case EBUSY:
                continue;
            case EINVAL:
            default:
                fg_debug_error( "fg_set_input(): set input failed" );
                return -1;
        }
    }

    // Reset the video format
    fg->format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fg->format.fmt.pix.width = fmt.fmt.pix.width;
    fg->format.fmt.pix.height = fmt.fmt.pix.height;
    fg->format.fmt.pix.pixelformat = fmt.fmt.pix.pixelformat;
    fg->format.fmt.pix.field = fmt.fmt.pix.field;

    if (v4l2_ioctl(fg->fd, VIDIOC_S_FMT, &(fg->format)) == -1)
    {
        fg_debug_error("fg_set_input(): resetting video format failed");
        return -1;
    }

    fg->input = index;

    return 0;
}

char *fg_get_input_name(fg_grabber *fg, int index)
{

    if (index > fg->num_inputs - 1)
    {
        fg_debug_error("fg_get_input_name(): invalid input number" );
        return NULL;
    }

    return (char *)fg->inputs[index].name;
}

int fg_get_input_type( fg_grabber* fg, int index )
{
    if (index > fg->num_inputs - 1)
    {
        fg_debug_error("fg_get_input_type(): invalid input number" );
        return -1;
    }

    return fg->inputs[index].type;
}

//--------------------------------------------------------------------------

/* TODO
int fg_set_source_norm( fg_grabber* fg, int norm )
{
    fg->sources[fg->source].norm = norm;

    if ( v4l1_ioctl( fg->fd, VIDIOCSCHAN, &(fg->sources[fg->source]) ) < 0 )
    {
        fg_debug_error( "fg_set_source_norm(): set channel/norm failed" );
        return -1;
    }

    return 0;
}
*/

//--------------------------------------------------------------------------
// TODO: fg_*_channel() functions not tested at all yet.
int fg_set_channel( fg_grabber* fg, float freq )
{
    int val, scale;
    struct v4l2_frequency frq;

    if ( !(fg->inputs[fg->input].type & V4L2_INPUT_TYPE_TUNER) )
    {
        fg_debug_error("fg_set_channel(): current source is not a tuner");
        return -1;
    }

    // TODO: is this still correct?
    // The LOW flag means freq in 1/16 MHz, not 1/16 kHz
    if ( fg->tuners[fg->tuner].capability & V4L2_TUNER_CAP_LOW )
        scale = 16000;
    else
        scale = 16;
    val = (int)( freq * scale );

    frq.tuner = fg->inputs[fg->input].tuner;
    frq.type = fg->tuners[fg->tuner].type;
    frq.frequency = val;
    FG_CLEAR(frq.reserved);

    if ( v4l2_ioctl( fg->fd, VIDIOC_S_FREQUENCY, &frq ) < 0 )
    {
        fg_debug_error( "fg_set_channel(): failed to tune channel" );
        return -1;
    }

    return 0;
}

float fg_get_channel( fg_grabber* fg )
{
    int scale;
    struct v4l2_frequency freq;

    // TODO: is this correct? (original lib was backwards from set func)
    // The LOW flag means freq in 1/16 MHz, not 1/16 kHz
    if (fg->tuners[fg->tuner].capability & V4L2_TUNER_CAP_LOW)
        scale = 16000;
    else
        scale = 16;

    freq.tuner = fg->tuner;
    FG_CLEAR(freq.reserved);

    if ( v4l2_ioctl( fg->fd, VIDIOC_G_FREQUENCY, &freq ) == -1 )
    {
        fg_debug_error( "fg_get_channel(): failed to query channel" );
        return -1;
    }

    return ( freq.frequency / scale );
}

//--------------------------------------------------------------------------

int fg_grab(fg_grabber *fg, fg_frame *fr)
{

    if (fr == NULL)
    {
        fg_debug_error("fg_grab(): ran out of memory allocating frame");
        return -1;
    }

    if (fg_grab_frame(fg, fr) == -1)
    {
        return -1;
    }

    return 0;
}



//--------------------------------------------------------------------------

int fg_grab_frame(fg_grabber *fg, fg_frame *fr)
{

    for (;;)
    {

        fd_set fds;
        struct timeval tv;
        int r;

        FD_ZERO(&fds);
        FD_SET(fg->fd, &fds);

        tv.tv_sec = FG_READ_TIMEOUT;
        tv.tv_usec = 0;

        r = select(fg->fd + 1, &fds, NULL, NULL, &tv);

        if ( r == -1 )
        {
            if (EINTR == errno)
                continue;

            fg_debug_error("fg_grab_frame(): grabbing frame failed");
            return -1;
        }

        if (0 == r)
        {
            fg_debug_error("fg_grab_frame(): frame grabbing timeout reached");
            return -1;
        }

        if (v4l2_read(fg->fd, fr->data, fr->length) == -1) {

            if (errno == EAGAIN)
                continue;
            else
            {
                fg_debug_error("fg_grab_frame(): error reading from device");
                return -1;
            }
        } else {
            gettimeofday(&(fr->timestamp), NULL);
            return 0;
        }

    }

    return -1;
}

//--------------------------------------------------------------------------
/*
int fg_set_control(fg_grabber *fg, int control_id, int value)
{
    int valid_control = 0;
    struct v4l2_queryctrl queryctrl;

    FG_CLEAR(queryctrl);

    for (queryctrl.id = V4L2_CID_BASE;
            queryctrl.id < V4L2_CID_LASTP1;
            queryctrl.id++)
    {
        if ( v4l2_ioctl(fg->fd, VIDIOC_QUERYCTRL, &queryctrl) == 0 )
        {
            if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED)
                continue;
            if (queryctrl.id == control_id)
            {
                valid_control = 1;
                break;
            }
            continue;
        }
        else
        {
            if (errno == EINVAL)
                continue;
            fg_debug_error("fg_set_control(): error enumerating controls");
            return -1;
        }
    }

    if (!valid_control)
    {
        fg_debug_error("fg_set_control(): control not supported");
        return -1;
    }

    v4l2_set_control(fg->fd, control_id, value);

    return 0;
}

int fg_get_control(fg_grabber *fg, int control_id)
{
    return v4l2_get_control(fg->fd, control_id);
}

int fg_default_controls(fg_grabber *fg)
{
    int i;
    struct v4l2_control ctl;
    struct v4l2_queryctrl ctrl;

    FG_CLEAR(ctrl);

    for (i=V4L2_CID_BASE; i < V4L2_CID_LASTP1; i++)
    {

        ctrl.id = i;
        if (v4l2_ioctl(fg->fd, VIDIOC_QUERYCTRL, &ctrl) == -1)
        {

            if (errno == EINVAL)
                continue;
            else
            {
                fg_debug_error("fg_default_controls(): error enumerating "
                    "controls");
                return -1;
            }

        }

        FG_CLEAR(ctl);

        ctl.id = ctrl.id;
        ctl.value = ctrl.default_value;
        if (v4l2_ioctl(fg->fd, VIDIOC_S_CTRL, &ctl) == -1)
            continue;

    }

    return 0;
}

char *fg_get_control_name(fg_grabber *fg, int control_id)
{
    FG_CLEAR(fg->controls[control_id]);
    fg->controls[control_id].id = control_id;
    if (v4l2_ioctl(fg->fd, VIDIOC_QUERYCTRL,
        &(fg->controls[control_id])) == -1)
    {
        return NULL;
    }
    return (char *)fg->controls[control_id].name;
}

//--------------------------------------------------------------------------

int fg_set_brightness( fg_grabber* fg, int br )
{
    return fg_set_control(fg, FG_CONTROL_BRIGHTNESS, br);
}

//--------------------------------------------------------------------------

int fg_set_hue(fg_grabber *fg, int hue)
{
    return fg_set_control(fg, FG_CONTROL_HUE, hue);
}

//--------------------------------------------------------------------------

int fg_set_saturation(fg_grabber *fg, int sat)
{
    return fg_set_control(fg, FG_CONTROL_SATURATION, sat);
}

//--------------------------------------------------------------------------

int fg_set_colour( fg_grabber* fg, int co )
{
    fg->picture.colour = FG_PERCENT( co );

    if ( v4l1_ioctl( fg->fd, VIDIOCSPICT, &(fg->picture) ) < 0 )
    {
        fg_debug_error( "fg_set_colour(): set attribute failed" );
        return -1;
    }

    return 0;
}

//--------------------------------------------------------------------------

int fg_set_color( fg_grabber* fg, int co )
{
    // This is the proper way to spell it...
    return fg_set_colour( fg, co );
}

//--------------------------------------------------------------------------

int fg_set_contrast( fg_grabber* fg, int ct )
{
    return fg_set_control(fg, FG_CONTROL_CONTRAST, ct);
}

//--------------------------------------------------------------------------

int fg_set_whiteness( fg_grabber* fg, int wh )
{
    fg->picture.whiteness = FG_PERCENT( wh );

    if ( v4l1_ioctl( fg->fd, VIDIOCSPICT, &(fg->picture) ) < 0 )
    {
        fg_debug_error( "fg_set_whiteness(): set attribute failed" );
        return -1;
    }

    return 0;
}
*/
//--------------------------------------------------------------------------

#define FROM_PC(n)      (n*100/65535)
#define TEST_FLAG(v,f)  (((v)&(f))?"Yes":"___")

void fg_dump_info(fg_grabber* fg)
{
    int i;
    //int type = fg->caps.type;

    if ( fg->fd < 1 )
    {
        fprintf( stderr, "fg_dump_info(): device not open/initialised!" );
        return;
    }

    // Dump out the contents of the capabilities structure
    printf("\nFrame Grabber Details\n" );
    printf("=====================\n" );

    printf("  device         = %s\n", fg->curr_device.device_path );
    printf("  fd handle      = 0x%08xd\n", fg->fd );
    printf("  driver         = %s\n", fg->caps.driver);
    printf("  version        = %d\n", fg->caps.version);
    printf("  card           = %s\n", fg->caps.card);
    printf("  bus_info       = %s\n", fg->caps.bus_info);

    printf("\n");

    // Format
    printf("  capture size   = %dx%d\n", fg->format.fmt.pix.width,
            fg->format.fmt.pix.height);
    printf("  data length    = %d\n", fg->format.fmt.pix.sizeimage);

    printf("\n");

    // Inputs
    printf("  num inputs     = %d\n", fg->num_inputs);
    printf("  active input   = %d\n", fg->input);
    for (i=0; i < fg->num_inputs; i++)
    {
        printf("    %02d: %s\n", i, fg->inputs[i].name);
        if (fg->inputs[i].type == V4L2_INPUT_TYPE_TUNER)
        {
            printf("      tuner %02d: %s (signal: %d)\n",
                fg->inputs[i].tuner, fg->tuners[fg->inputs[i].tuner].name,
                fg->tuners[fg->inputs[i].tuner].signal);
        }
    }

}

//==========================================================================

