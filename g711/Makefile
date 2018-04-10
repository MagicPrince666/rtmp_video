
CROSS_COMPILE := 

GCC := $(CROSS_COMPILE)gcc
GPP := $(CROSS_COMPILE)g++
AR  = $(CROSS_COMPILE)ar
STRIP  = $(CROSS_COMPILE)strip

TARGET := libg711.a
all: $(TARGET) 


INC := -I../video_capture -I../encodeAac/arm/include
SRC := $(wildcard *.cpp)

LIBSDIR := -L../video_capture -L../encodeAac/arm
LIB := -lvideo_capture -lasound

OBJS := $(SRC:.cpp=.o)

%.o : %.cpp
	$(GPP) -c $< $(INC)

#capturepcm.o : capturepcm.c
#	$(GPP) -c $< $(INC)

pcmcapture: capturepcm.o audio_encode.o
	$(GPP) -o $@  $(INC) capturepcm.o audio_encode.o $(LIBSDIR) $(LIB)

$(TARGET) : $(OBJS) 
	$(AR) cr $@ $(OBJS) 



clean:
	rm *.a *.o *.so -rf

