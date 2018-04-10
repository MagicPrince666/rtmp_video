CROSS_PRE := 
GPP := $(CROSS_PRE)g++
GCC := $(CROSS_PRE)gcc
AR := $(CROSS_PRE)ar
RANLIB := $(CROSS_PRE)ranlib

LIBS := -L../lib -lfaac  -lasound
INC := -I/usr/include/ -I../faac-1.28/include

ALL: alsaCapture


OBJS := main.o recorder.o

SRC := main.cpp \
	recorder.cpp

alsaCapture: $(OBJS)
	$(GPP) -o $@ $(OBJS)  $(LIBS)

main.o: main.cpp
	$(GPP) -c $< $(INC) 

recorder.o: recorder.cpp
	$(GPP) -c $< $(INC) 

AACLIB: librecordeaac.a

librecordeaac.a : recorder.o
	$(AR) cr $@ recorder.o
	$(RANLIB) $@


clean:
	rm *.o alsaCapture
