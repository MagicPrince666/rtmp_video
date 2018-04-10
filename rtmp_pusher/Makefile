
CPPFLAGS := -g -O0
CFLAGS := -g

CROSS_COMPILER:=
GCC := $(CROSS_COMPILER)gcc
GPP := $(CROSS_COMPILER)g++
CC := $(CROSS_COMPILER)cc
STRIP := $(CROSS_COMPILER)strip

TARGET := RtmpPusher
all : $(TARGET) 

INCLUDE += -I./ -I../librtmp -I../ffmpeg -I../video_capture -I../camera -I../g711 -I../encodeAac/arm/include -I../encodeAac/ -I../faac-1.28/include
$(info INCLUDES:$(INCLUDE))

LIBS := -lm -lpthread -lavformat -lvideo_capture -lrtmp -lh264uvc -lg711 -lasound\
 	-lavdevice -lswscale -lavcodec -lavutil -lswresample -lz -lx264 -ldl -lrecordeaac -lfaac

LIBSINC := -L../librtmp -L../ffmpeg/libs -L../video_capture -L../camera -L../zlib-1.2.8 -L../g711 -L../encodeAac/arm -L../encodeAac -L../faac-1.28/

$(TARGET) : main.cpp RtmpSmartPusher.cpp memwatch.c
	$(GPP) $(CPPFLAGS)  -DMEMWATCH -DMW_STDIO -o  $@ main.cpp RtmpSmartPusher.cpp  memwatch.c $(INCLUDE) $(LIBSINC) $(LIBS)
	$(STRIP) $(TARGET)

.PHONY :clean

clean:
	rm $(TARGET)

# hello: main.cpp RtmpSmartPusher.cpp
# 	g++ -g -o hello \
# 	main.cpp RtmpSmartPusher.cpp \
# 	-I ../librtmp/ -I ../video_capture/ -I /home/tangss/IPCamera_X86/ffmpeg/ffmpeg \
# 	-L ../librtmp/ -lrtmp \
# 	-L ../ffmpeg/out/lib \
# 	-lm -lpthread -lavformat \
# 	-lavdevice -lswscale -lavcodec -lavutil -lswresample -lz -lx264 -ldl

