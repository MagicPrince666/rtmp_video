# ifndef _RTMP_COMMON_H_
# define _RTMP_COMMON_H_

# ifndef __DBGFUNS
#   define __DBGFUNS
# endif 

# ifdef __DBGFUNS
#   define DBGFUNS(fmt,args...) printf(fmt,  ##args)
# else
#   define DBGFUNS(fmt,args...)
# endif


# ifndef V4L2_PIX_FMT_H264
#   define V4L2_PIX_FMT_H264 v4l2_fourcc('H','2','6','4') /* H264 */
# endif

# ifndef V4L2_PIX_FMT_MP2T
#   define V4L2_PIX_FMT_MP2T v4l2_fourcc('M','P','2','T') /* MPEG-2 TS */
# endif

# ifndef CLEAR
#   define CLEAR
#   define CLEAR(x) memset (&(x), 0, sizeof (x))
# endif


# endif //_RTMP_COMMON_H_