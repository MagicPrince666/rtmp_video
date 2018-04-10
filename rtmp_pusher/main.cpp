#include "RtmpSmartPusher.h"
#include "constant.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// #include <memwatch.h>

int main(int argc, char* argv[])
{
	// mwInit(); 
	setenv("MALLOC_TRACE", "output", 1);
	if(argc < 7)
	{
		cout << "TIPS: " << argv[0]<< "  videodev(\"/dev/video1\")"<< " audiodev(\"default\") " <<" bitrate(1024)" << " fps(30) " << "SYNCFLAG(1,0)" 
		<<" rtmpserver(\"rtmp://baoqianli.8686c.com/live/d272ef7c48d742889c7b5ed8b74dde5d?wsRecord=off\") "  <<endl;
		cout << "(SIMPLE VERSION, NO RTMP SERVER )YOU CAN  : " << "./RtmpPusher /dev/video1 default 2048 30 1"  <<endl;
		cout << "(ADVANTAGE VERSION)YOU CAN  : " << "./RtmpPusher /dev/video1 default 2048 30 1 rtmp://baoqianli.8686c.com/live/d272ef7c48d742889c7b5ed8b74dde5d?wsRecord=off"  <<endl;
		return 0;
	}
	
		setBitRate(atoi(argv[3]));
		setFps(atoi(argv[4]));
		setVideoDevice(argv[1]);
		setAudioDevice(argv[2]);
		setSync(atoi(argv[5]));
		// const char *data = "rtmp://127.0.0.1/live/livestream";
//		rtmp://baoqianli.8686c.com/live/d5f742ff87bb4aaab0fa37e21d26ec1f
		//const char *data = "rtmp://baoqianli.8686c.com/live/d272ef7c48d742889c7b5ed8b74dde5d?wsRecord=off";
		char data[200];
		memset(data,0, sizeof(data));	
		memcpy(data,argv[6], strlen(argv[6]));
		
		printf("\n url: %s \n", data);

		cout <<" RTMPSERVER: "<< data << endl;
		RtmpSmartPusher pusher;
		pusher.Init((char*)data);
		pusher.startPush();
		
		return 0;
}
