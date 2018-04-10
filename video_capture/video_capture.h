# ifndef _VIDEO_CAPTURE_H_
# define _VIDEO_CAPTURE_H_


void start_cap();
void stop_cap();
int getData(unsigned char* buf);
int read_buffer(void* oprg, unsigned char *buf, int buf_size);

# endif /* _VIDEO_CAPTURE_H_ */

