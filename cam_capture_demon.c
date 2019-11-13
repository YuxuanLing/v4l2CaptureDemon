#include <stdlib.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include "libfg2.h"


//--------------------------------------------------------------------------


typedef struct buffer {
	void *start;
	size_t length;
    int   stride;
} pix_buffer;


short table_yuv_rgb[256][256][3];
int yuv_rgb_exists = 0;

void init_buffer(pix_buffer *b, size_t b_len, int stride)
{
	b->length = b_len;
	b->start = calloc(b_len,sizeof(uint8_t));
}

void kill_buffer(pix_buffer *b)
{
	free(b->start);
}

static void gen_table_yuv_rgb(void)
{
	if(yuv_rgb_exists==1) return;
	fprintf(stderr,"Generating color table.\n");
	uint16_t y,u,v;
	for(u=0; u<256; u++)
	for(v=0; v<256; v++)
	{
		table_yuv_rgb[u][v][0] = (short)(1.14*(v-128));
		table_yuv_rgb[u][v][1] = (short)(-0.395*(u-128)-0.581*(v-128));
		table_yuv_rgb[u][v][2] = (short)(2.032*(u-128));
	}
	yuv_rgb_exists = 1;
}
static inline uint8_t short_to_uint8(short s)
{
	if(s>=255) return 255;
	else if(s <= 0) return 0;
	else return (uint8_t)s;
}

void yuy2_to_rgb (pix_buffer *buf_in, pix_buffer *buf_out)
{
	/* Byte ordering:
	0  1  2  3   |  4  5  6  7    
	Y0 U0 Y1 V0  |  Y2 U2 Y3 V2

	rgb pixels from sequence:
        (Y0,U0,V0); (Y1,U0,V0); (Y2,U2,V2), ..
	
	pixel	bytes
	0	(0,1,3)
	1	(2,1,3)
	2	(4,5,7)
	3	(6,5,7)
	...
	for the k-th pixel:
	k		(2*k, 4*floor(k/2)+1, 4*floor(k/2)+3)
	for the r-th pair:
	r		(r,   r+1, r+3)
			(r+2, r+1, r+3)
	 R = Y + 1.140V
	 G = Y - 0.395U - 0.581V
	 B = Y + 2.032U
	*/
	gen_table_yuv_rgb();
	uint8_t *b1_temp;
	uint8_t *b2_temp;
	uint8_t u,v,y0,y1;
	short r0,g0,b0;
	size_t i;

	for(i = 0; i < buf_in->length/2; i+=2)
	{
		b1_temp = (uint8_t *)(buf_in->start + i*2);
		b2_temp = (uint8_t *)(buf_out->start + i*3);

		v = b1_temp[1];
		u = b1_temp[3];
		y0 = b1_temp[0];
		y1 = b1_temp[2];
		
		r0 = table_yuv_rgb[u][v][0];
		g0 = table_yuv_rgb[u][v][1];
		b0 = table_yuv_rgb[u][v][2];

		//pixel 1
		b2_temp[0] = short_to_uint8(r0+y0);	//r
		b2_temp[1] = short_to_uint8(g0+y0);	//g
		b2_temp[2] = short_to_uint8(b0+y0);	//b

		//pixel 2
		b2_temp[3] = short_to_uint8(r0+y1);	//r
		b2_temp[4] = short_to_uint8(g0+y1);	//g
		b2_temp[5] = short_to_uint8(b0+y1);	//b
	}
}




typedef struct
{
    fg_grabber*     fg;
    fg_frame*       frame;
    SDL_Window      *window;
    SDL_Surface     *image;
    SDL_Renderer    *render;
    SDL_Texture     *texture;
    unsigned        frames;
    struct timeval  timestamp;
   
    pix_buffer   yuy2_buf;
    pix_buffer   rgb_buf;
    pix_buffer   yuv420_buf;
    

} camview_app;



/* Call this instead of exit(), so we can clean up SDL: atexit() is evil. */
static void quit(int rc)
{
    SDL_Quit();
    exit(rc);
}


static int cleanup( camview_app* app )
{
    fg_close( app->fg );
    fg_frame_release( app->frame );

    SDL_Quit();

    return 0;
}


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



//--------------------------------------------------------------------------

static int init( camview_app* app )
{
    SDL_RendererInfo info;
    unsigned int  sdl_pix_fmt;
    // Open framegrabber device
    app->fg = fg_init();
    //only after fg_init , you can list all devices
    list_all_camera_devices(app->fg);

    if(fg_open(app->fg,  NULL ) == NULL){
		printf("fg_open failed , please check if the device exist\n");
        exit(0);
    }
    //only after fg_open , you can list current device's caps
    list_all_camera_capbilities(app->fg); 
   
    fg_set_format(app->fg, FG_FORMAT_YUV420);
	//fg_set_format(app->fg, FG_FORMAT_RGB24);
    //fg_set_format(app->fg, FG_FORMAT_YUYV);
    


    app->frame = fg_frame_new( app->fg);
    app->frames = 0;

    //fg_dump_info( app->fg );
    
    switch ( app->frame->format )
    {
        case FG_FORMAT_YUV420:
			sdl_pix_fmt = SDL_PIXELFORMAT_IYUV;  //YUV SDL_PIXELFORMAT_YV12 is YVU
            break;
        case FG_FORMAT_RGB24:
            sdl_pix_fmt = SDL_PIXELFORMAT_RGB24;
            break;
        case FG_FORMAT_YUYV:
            sdl_pix_fmt = SDL_PIXELFORMAT_YUY2;
            break;
        default:
            fprintf( stderr, "Unsupported frame format! %u",  app->frame->format );
            return -1;
    }

    // Start SDL
	 if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		 fprintf(stderr, "Couldn't initialize SDL: %s\n", SDL_GetError());
		 quit(2);
	 }

	 /* Create the window and renderer */
	app->window = SDL_CreateWindow("Camera example",
							   SDL_WINDOWPOS_UNDEFINED,
							   SDL_WINDOWPOS_UNDEFINED,
							   app->frame->size.width, app->frame->size.height,
							   SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE);

	 if (!app->window) {
		 fprintf(stderr, "Couldn't set create window: %s\n", SDL_GetError());
		 quit(5);
	 }
 
	 app->render = SDL_CreateRenderer(app->window, -1, 0);
	 
	 if (!app->render) {
		 fprintf(stderr, "Couldn't set create renderer: %s\n", SDL_GetError());
		 quit(6);
	 }
	 SDL_GetRendererInfo(app->render, &info);
	 printf("Using %s rendering width = %d, height = %d\n", info.name, app->frame->size.width, app->frame->size.height);
     
     //not use , just put here 
     init_buffer(&app->yuy2_buf, app->frame->size.width*app->frame->size.height*2, app->frame->size.width*2);
     init_buffer(&app->rgb_buf,  app->frame->size.width*app->frame->size.height*3, app->frame->size.width*3);
     init_buffer(&app->yuv420_buf,  app->frame->size.width*app->frame->size.height*3/2, app->frame->size.width);
     
	 app->texture = SDL_CreateTexture(app->render, sdl_pix_fmt, SDL_TEXTUREACCESS_STREAMING, app->frame->size.width , app->frame->size.height);

	 if (!app->texture) {
		 fprintf(stderr, "Couldn't set create texture: %s\n", SDL_GetError());
		 quit(7);
	 }


    gettimeofday( &app->timestamp, NULL );

    return 0;
}



//--------------------------------------------------------------------------

static int update( camview_app* app )
{
    fg_grab_frame( app->fg, app->frame );
    app->frames++;

    
	SDL_UpdateTexture(app->texture, NULL, app->frame->data, app->frame->size.width);
	//SDL_RenderClear(app->render);
	SDL_RenderCopy(app->render, app->texture, NULL, NULL);
	SDL_RenderPresent(app->render);
    
    if ( app->frames % 10 == 0 )
    {
        struct timeval now;
        gettimeofday( &now, NULL );

        float elapsed = ( now.tv_sec - app->timestamp.tv_sec ) + ( now.tv_usec - app->timestamp.tv_usec )/1e6;

        printf( "%0.2f frames/sec    \n", app->frames/elapsed );
    }

    return 0;
}

//--------------------------------------------------------------------------

static int run( camview_app* app )
{
    SDL_Event event;
    SDL_bool running = SDL_TRUE; 

    while( running )
    {
        while ( SDL_PollEvent(&event) )
        {
            if ( event.type == SDL_QUIT )
            {
                running = SDL_FALSE;
            }

            if ( event.type == SDL_KEYDOWN )
            {
                if ( event.key.keysym.sym == SDLK_ESCAPE )
                {
                    running = SDL_FALSE;
                }
            }
        }

        update( app );
    }

    return 0;
}



int main(int argc, char **argv)
{
    camview_app app;
       
    init(&app );
    
    int rc = run( &app );
    
	SDL_DestroyRenderer(app.render);

    cleanup( &app );

    return 0;
}



