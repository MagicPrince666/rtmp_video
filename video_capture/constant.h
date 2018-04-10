#ifndef _CONSTANT_H_
#define _CONSTANT_H_


void setPort(int p);

void setBitRate(int b);

void setFps(int fps);

void setSync(int sync);

void setVideoDevice(char* dev);

void setAudioDevice(char* dev);

int getFps();

int getBitRate();

int getPort();

char* getVideoDevice();

char* getAudioDevice();

int getSync();


#endif