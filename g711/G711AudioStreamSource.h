
#ifndef _G711_AUDIO_STREAM_SOURCE_HH
#define _G711_AUDIO_STREAM_SOURCE_HH

#include <pthread.h>
#include "cbuf.h"

class G711AudioStreamSource {
public:
  unsigned char bitsPerSample() const { return fBitsPerSample; }
  unsigned char numChannels() const { return fNumChannels; }
  unsigned samplingFrequency() const { return fSamplingFrequency; }
  void doGetNextFrame();
  int GetOneFrame(unsigned char* *buf, unsigned int *size);
  virtual ~G711AudioStreamSource();
  G711AudioStreamSource(const char* dev = NULL);
protected:



private:
  unsigned char fNumChannels;
  unsigned fSamplingFrequency;
  unsigned char fBitsPerSample;
  unsigned fPreferredFrameSize;
  bool fLimitNumBytesToStream;
  unsigned fNumBytesToStream;
  unsigned fLastPlayTime;
  double fPlayTimePerSample; // useconds
  pthread_t m_thread;

public:
  static cbuf_t data; 
  
};

#endif
