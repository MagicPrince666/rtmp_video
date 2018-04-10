CPP = g++

TARGET	= rtmp_push 

FFMPEGSRC = ../ffmpeg
#FAACSRC = ../faac-1.28

DIR		= ./video_capture ./H264_camera ./g711  ./rtmp_pusher ./encodeAac
INC		= -I. -I./H264_camera -I./g711 -I./rtmp_pusher -I./video_capture -I./encodeAac -I./include -I./librtmp -I$(FFMPEGSRC)


FFMPEGDIR = -L$(FFMPEGSRC)/libavcodec -L$(FFMPEGSRC)/libavdevice -L$(FFMPEGSRC)/libavfilter -L$(FFMPEGSRC)/libavformat -L$(FFMPEGSRC)/libavutil \
-L$(FFMPEGSRC)/libswresample -L$(FFMPEGSRC)/libswscale

CFLAGS	= -g -Wall -O3
LDFLAGS = $(FFMPEGDIR) -L./lib -L./librtmp -lavdevice -lswscale -lavformat -lavcodec -lavutil -lswresample -lasound -lfaac -lrtmp -lpthread -ldl -lm -lz

OBJPATH	= ./objs

FILES	= $(foreach dir,$(DIR),$(wildcard $(dir)/*.cpp))

OBJS	= $(patsubst %.cpp,%.o,$(FILES))

all:$(OBJS) $(TARGET)

$(OBJS):%.o:%.cpp
	$(CPP) $(CFLAGS) $(INC) -c -o $(OBJPATH)/$(notdir $@) $< 

$(TARGET):$(OBJPATH)
	$(CPP) -o $@ $(OBJPATH)/*.o $(LDFLAGS)

clean:
	-rm $(OBJPATH)/*.o
	-rm $(TARGET)