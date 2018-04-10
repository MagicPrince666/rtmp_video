#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <string.h>
#include <pthread.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include "constant.h"
#include "cbuf.h"
#include "v4l2uvc.h"
#include "h264_xu_ctrls.h"

#include "common.h"

struct buffer {
	void *         start;
	size_t         length;
};

static char            dev_name[16];
static int              fd              = -1;
struct buffer *         buffers         = NULL;
static unsigned int     n_buffers       = 0;

struct vdIn *vd;

struct tm *tdate;
time_t curdate;

struct H264Format *gH264fmt = NULL;
int Dbg_Param = 0x1f;



int errnoexit(const char *s)
{
	printf("%s error %d, %s", s, errno, strerror (errno));
	return -1;
}


int xioctl(int fd, int request, void *arg)
{
	int r;

	do r = ioctl (fd, request, arg);
	while (-1 == r && EINTR == errno);

	return r;
};

// FILE* f_h264 = NULL;
int open_device()
{

	printf("-------------- open_device-------------------\n");

	struct stat st;

	strcpy(dev_name, getVideoDevice());
	printf("audioDevice: %s\n", dev_name);
	if (-1 == stat (dev_name, &st))
	{
		printf("Cannot identify '%s': %d, %s\n", dev_name, errno, strerror (errno));
		return -1;
	}

	if (!S_ISCHR (st.st_mode))
	{
		printf("%s is no device", dev_name);
		return -1;
	}
	vd = (struct vdIn *) calloc(1, sizeof(struct vdIn));
	perror(dev_name);
	vd->fd = open(dev_name, O_RDWR);
	printf("vd->fd 11 : %d", vd->fd);

	if (0 >= vd->fd)
	{
		printf("Cannot open '%s': %d, %s", dev_name, errno, strerror (errno));
		return -1;
	}
	return 0;
};

int init_device(int width, int height,int format)
{
	
	printf("--------------- init_device------------------\n");

	struct v4l2_capability cap;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_format fmt;
	unsigned int min;

	if (-1 == xioctl (vd->fd, VIDIOC_QUERYCAP, &cap))
	{
		if (EINVAL == errno)
		{
			printf("%s is no V4L2 device", dev_name);
			return -1;
		}
		else
		{
			return errnoexit ("VIDIOC_QUERYCAP");
		}
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
	{
		printf("%s is no video capture device", dev_name);
		return -1;
	}

	if (!(cap.capabilities & V4L2_CAP_STREAMING))
	{
		printf("%s does not support streaming i/o", dev_name);
		return -1;
	}

	CLEAR (cropcap);
	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (0 == xioctl (vd->fd, VIDIOC_CROPCAP, &cropcap))
	{
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect; 

		if (-1 == xioctl (vd->fd, VIDIOC_S_CROP, &crop))
		{
			switch (errno)
			{
				case EINVAL:
					break;
				default:
					break;
			}
		}
	}
	else
	{

	}

	CLEAR (fmt);
	fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width       = width;
	fmt.fmt.pix.height      = height;
	fmt.fmt.pix.pixelformat = format;
	fmt.fmt.pix.field       = V4L2_FIELD_ANY;

	if (-1 == xioctl (vd->fd, VIDIOC_S_FMT, &fmt))
		return errnoexit ("VIDIOC_S_FMT");

	min = fmt.fmt.pix.width * 2;

	if (fmt.fmt.pix.bytesperline < min)
		fmt.fmt.pix.bytesperline = min;
	min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
	if (fmt.fmt.pix.sizeimage < min)
		fmt.fmt.pix.sizeimage = min;

	struct v4l2_streamparm parm;
	memset(&parm, 0, sizeof parm);
	parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ioctl(vd->fd, VIDIOC_G_PARM, &parm);
	parm.parm.capture.timeperframe.numerator = 1;
	parm.parm.capture.timeperframe.denominator = getFps();
	//parm.parm.capture.timeperframe.denominator = 30; 
	ioctl(vd->fd, VIDIOC_S_PARM, &parm);


	int init_mmap(void);
	return init_mmap ();

};

int SUPPORTED_BUFFER_NUMBER = 20;

int init_mmap(void)
{

	printf("---------------init_mmap------------------\n");
		perror("-init_mmap ");

	struct v4l2_requestbuffers req;

	CLEAR (req);
	req.count               = SUPPORTED_BUFFER_NUMBER;
	req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory              = V4L2_MEMORY_MMAP;

	if (-1 == xioctl (vd->fd, VIDIOC_REQBUFS, &req))
	{
		if (EINVAL == errno)
		{
			printf("%s does not support memory mapping", dev_name);
			return -1;
		}
		else
		{
			return errnoexit ("VIDIOC_REQBUFS");
		}
	}

	if (req.count < 2)
	{
		printf("Insufficient buffer memory on %s", dev_name);
		return -1;
 	}

	buffers = (struct buffer*) calloc (req.count, sizeof (*buffers));
	SUPPORTED_BUFFER_NUMBER = req.count;
		printf("SUPPORTED_BUFFER_NUMBER %d\n",SUPPORTED_BUFFER_NUMBER);


	if (!buffers)
	{
		printf("Out of memory");
		return -1;
	}

	for (n_buffers = 0; n_buffers < req.count; ++n_buffers)
	{
		struct v4l2_buffer buf;
		CLEAR (buf);

		buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory      = V4L2_MEMORY_MMAP;
		buf.index       = n_buffers;

		if (-1 == xioctl (vd->fd, VIDIOC_QUERYBUF, &buf))
			return errnoexit ("VIDIOC_QUERYBUF");

		buffers[n_buffers].length = buf.length;
		buffers[n_buffers].start =
		mmap (NULL ,
			buf.length,
			PROT_READ | PROT_WRITE,
			MAP_SHARED,
			vd->fd, buf.m.offset);

		if (MAP_FAILED == buffers[n_buffers].start)
			return errnoexit ("mmap");

	}
	

	return 0;
};

int start_previewing(void)
{

	printf("--------------start_previewing-------------------\n");
	perror("-start_previewing ");

	unsigned int i;
	enum v4l2_buf_type type;

	for (i = 0; i < n_buffers; ++i)
	{
		struct v4l2_buffer buf;
		CLEAR (buf);

		buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory      = V4L2_MEMORY_MMAP;
		buf.index       = i;

		if (-1 == xioctl (vd->fd, VIDIOC_QBUF, &buf))
			return errnoexit ("VIDIOC_QBUF");
	}

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (-1 == xioctl (vd->fd, VIDIOC_STREAMON, &type))
		return errnoexit ("VIDIOC_STREAMON");

	return 0;
};

int capturing = 1;


void cameraInit()
{

	printf("-cameraInit 1\n");
	
	int width = 1280; 
	int height = 720;
		
	int format = V4L2_PIX_FMT_H264;
	printf("-cameraInit 2\n");
	
	int ret;
	ret = open_device();

	printf("-cameraInit 3 ret:%d\n", ret);

	if(ret != -1)
	{
		printf("------open_device--success-- !\n ");
		ret = init_device(width,height,format);
	}
	if(ret != -1)
	{
		printf("------init_device---success------- !\n ");
		ret = start_previewing();
	}

	if(ret != -1)
	{
		printf("---start_previewing------success------- !\n ");
	}

	time(&curdate);  
	tdate = localtime (&curdate);
		  printf("XU_OSD_Set_RTC_ curdate tm_year:%d \n",tdate->tm_year);
	
	XU_OSD_Set_CarcamCtrl(vd->fd, (unsigned char)0, (unsigned char)0, (unsigned char)0);
	if(XU_OSD_Set_RTC(vd->fd, tdate->tm_year + 1900, tdate->tm_mon + 1,tdate->tm_mday, tdate->tm_hour, tdate->tm_min, tdate->tm_sec) <0)
		  printf("XU_OSD_Set_RTC_fd = %d Failed\n",fd);
	if(XU_OSD_Set_Enable(vd->fd, 1, 1) <0)
		  printf(" XU_OSD_Set_Enable_fd = %d Failed\n",fd);


    ret = XU_Init_Ctrl(vd->fd);
    if(ret<0)
    {
	printf("XU_H264_Set_BitRate Failed\n");	
     }else
     {
	double m_BitRate = 0.0;
		
	if(XU_H264_Set_BitRate(vd->fd, getBitRate()) < 0 )
	// if(XU_H264_Set_BitRate(vd->fd, 1024000) < 0 )
	{
		printf("XU_H264_Set_BitRate Failed\n");
	}else
	{
	}

	XU_H264_Get_BitRate(vd->fd, &m_BitRate);
	if(m_BitRate < 0 )
	{
		printf("XU_H264_Get_BitRate Failed\n");
	}
	printf("------------XU_H264_Set_BitRate ok------m_BitRate:%f----\n", m_BitRate);
    }
};

FILE* rec_fp1 = NULL;

struct v4l2_buffer __buf;


void cameraUninit(void)
{
	perror("TSS cameraUninit 110219-1");

	if(!buffers) return;//已经释放，直接返回

	perror("TSS cameraUninit 110219-2");
	  
	for (n_buffers = 0; n_buffers < SUPPORTED_BUFFER_NUMBER; ++n_buffers)
	{
		perror("TSS cameraUninit 110219-3 c ");
		
		if(buffers[n_buffers].start!=NULL)
		{   
			perror("TSS cameraUninit 110219-4");
			
		if(-1==munmap(buffers[n_buffers].start,buffers[n_buffers].length))
		{
			perror("Fail to \"munmap\"\n");
		}
		}else
		{   
			DBGFUNS("__buffers[%d].start=0__\n",n_buffers);
		}   
	}

	// 释放申请的存储空间
	if(buffers)
	{
		perror("TSS cameraUninit 110219-5");
		free(buffers);
		buffers=NULL;
	}


	if(vd)
	{
		perror("TSS  Uinit 6");
		if(vd->fd > 0)
		{
			int r = close(vd->fd);
			printf("Uinit 1-2  close(vd->fd) r:%d\n", r);
			vd->fd = -1;
		}
		printf("3 Uinit 2\n");
		int r1 = close_v4l2(vd);
		printf("Uinit 2-1  close_v4l2(vd); r1:%d \n",r1);
		vd = NULL;
	}
	printf("Uinit END 3\n");
	
};

// Queue<Mediadata*> sWorkDataQueue;
const int QUEUE_LEN_MAX = 4;
const int QUEUE_SIMPLE_UNIT_SIZE = 100000;
const int MAX_BIPBUF_LEN = 8388608;//8M

//视频数据
static cbuf_t data;

int Uinit()
{
	
	perror("TSS  Uinit 1");
	cameraUninit();
	perror("TSS  Uinit 2");

	if(rec_fp1)
	{
		fclose(rec_fp1);
		rec_fp1 = NULL;
	}

	perror("TSS  Uinit 3");
	cbuf_destroy(&data);
	// fclose(f_h264);
	return 0;
}

void Init()
{
	// f_h264 = fopen("my.h264","wb");
   	Uinit();

  	cameraInit();
  	if(rec_fp1 == NULL)
  	{
  		char rec_filename[] = "rtmp_record.264";	
  		rec_fp1 = fopen(rec_filename, "wb");
  	}

	data.CUBFEACHDATALEN = 20000;
	cbuf_init(&data);
}


int s_quit = 1; 

int read_buffer(void* oprg, unsigned char* buf, int wantsize)
{
	if(vd == NULL || vd->fd == NULL)
	{
		printf("read_buffer test vd error \n");
		return 0;
	}
	  
	CLEAR (__buf);
	__buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	__buf.memory = V4L2_MEMORY_MMAP;
	
	int ret = ioctl(vd->fd, VIDIOC_DQBUF, &__buf);
	if (ret < 0) 
	{
		printf("read_buffer Unable to dequeue buffer from mmap ret:%d!\n", ret);
		return 0;
	}
	
	int l = __buf.bytesused;	
	memcpy(buf, buffers[__buf.index].start, __buf.bytesused);
	// fwrite(buffers[__buf.index].start, 1, __buf.bytesused, rec_fp1);
	// cbuf_enqueue(&data,buffers[__buf.index].start, __buf.bytesused);

	ret = ioctl(vd->fd, VIDIOC_QBUF, &__buf);
	if (ret < 0) 
	{
		printf("Unable to requeue buffer from mmap \n");
	}

	return l;
};

int capture_buffer()
{
	if(vd == NULL || vd->fd == NULL)
	{
		printf("read_buffer test vd error \n");
		return 0;
	}
	  
	CLEAR (__buf);
	__buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	__buf.memory = V4L2_MEMORY_MMAP;
	
	int ret = ioctl(vd->fd, VIDIOC_DQBUF, &__buf);
	if (ret < 0) 
	{
		printf("read_buffer Unable to dequeue buffer from mmap ret:%d!\n", ret);
		return 0;
	}
	
	int l = __buf.bytesused;	
	// memcpy(buf, buffers[__buf.index].start, __buf.bytesused);
	// fwrite(buffers[__buf.index].start, 1, __buf.bytesused, rec_fp1);
	cbuf_enqueue(&data,buffers[__buf.index].start, __buf.bytesused);

	ret = ioctl(vd->fd, VIDIOC_QBUF, &__buf);
	if (ret < 0) 
	{
		printf("Unable to requeue buffer from mmap \n");
	}

	return l;
};

int b_run_flag = 1;
void* capture_proc(void* args)
{
    // unsigned char audio_tmp_buffer[4096];
    // unsigned int audio_buffer_vaiable_len = 0;
	// FILE* fout = fopen("g711.pcm", "wb");
    while(b_run_flag)
    {
        // audioGetOneFrame(audio_tmp_buffer, &audio_buffer_vaiable_len);
        int l = capture_buffer();
		// fwrite(audio_tmp_buffer, 1, audio_buffer_vaiable_len, fout);
	    // printf("audio_proc ret:%d\n", l);  
        usleep(10);
    }

	// fclose(fout);
}

int getData(unsigned char* buf)
{
	return cbuf_dequeue(&data, buf);
}

pthread_t m_thread;
void start_cap()
{

	if(!s_quit)
	{
		return;
	}

	s_quit = 0;
	printf("void FetchData::startCap() 3\n");

	Init();
	printf("void FetchData::startCap() 4\n");
	// b_run_flag = 1;
	// int res = pthread_create(&m_thread, NULL, capture_proc,NULL); 
    // if (res != 0)  
    // {  
	// 	printf("void FetchData::startCap() \n");  
	
	// 	perror("Thread creation failed!");  
	// 	exit(EXIT_FAILURE);  
    // }  
	
	
}

void stop_cap()
{
	printf("FetchData stopCap 1\n");  
	Uinit();

	// b_run_flag = 0;
    // int iret = pthread_join(m_thread, NULL);

}


