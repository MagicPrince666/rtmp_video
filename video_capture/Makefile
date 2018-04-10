COMPILER := 
CC := $(COMPILER)gcc
GPP := $(COMPILER)g++
AR := $(COMPILER)ar
STRIP := $(COMPILER)strip

INC:= -I../H264_camera -I../ffmpeg
LIBINCLUDES := -L../H264_camera  -L../ffmpeg/libs
LIBS := -lh264uvc -lavformat \
		-lavdevice -lswscale -lavcodec -lavutil -lswresample -lz -ldl

video_capture.o:video_capture.cpp
	$(GPP) -c $< $(INC) 

cbuf.o:cbuf.cpp
	$(GPP) -c $< $(INC) 

CONSTANT.o: CONSTANT.cpp
	$(GPP) -c $<
thread.o: thread.cpp
	$(GPP) -c $<

all: libvideo_capture.a
libvideo_capture.a: video_capture.o CONSTANT.o cbuf.o thread.o
	$(AR) r $@ video_capture.o CONSTANT.o cbuf.o thread.o


clean:
	rm *.o *.a -rf
