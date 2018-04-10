#include "RtmpSmartPusher.h"
#include <arpa/inet.h>
#include <sys/time.h>
#include "G711AudioStreamSource.h"
#include "constant.h"
#include "recorder.h"
#include <time.h>
#include "video_capture.h"

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

#define RTMP_HEAD_SIZE   (sizeof(RTMPPacket)+RTMP_MAX_HEADER_SIZE)



int time_diff(struct timeval& end, struct timeval& start)
{
	return (end.tv_sec-start.tv_sec)*1000000+(end.tv_usec-start.tv_usec);
}

bool bavsync = true;
RtmpSmartPusher::RtmpSmartPusher():m_rtmp(NULL),m_audiotime(0),m_videotime(0),m_audioover(0),m_videoover(0)
{
	start_now=0;
	nDropPframe = 1;
	bavsync = getSync();
	memset(&m_264metadata,0,sizeof(h264metadata));

}

RtmpSmartPusher::~RtmpSmartPusher()
{
}

bool RtmpSmartPusher::Init(char* rtmpurl)
{
	pthread_mutex_init(&m_lck,NULL);
	pthread_mutex_init(&m_queuelck,NULL);
	pthread_cond_init(&cv,NULL);
	connectRtmpServer(rtmpurl);
}

//跑RTMP初始协议，握手，协商等
bool RtmpSmartPusher::connectRtmpServer(char* rtmpurl)
{
		m_rtmp = RTMP_Alloc();
		if(!m_rtmp)
		{
				cout<<"Alloc rtmp error"<<endl;
				return false;
		}
		RTMP_Init(m_rtmp);
		if(RTMP_SetupURL(m_rtmp,rtmpurl) == FALSE)
		{
				cout<<"set rtmp url error"<<endl;
				return false;
		}
		RTMP_EnableWrite(m_rtmp);
		if(RTMP_Connect(m_rtmp,NULL) == FALSE)
		{
				cout<<"connect rtmp server error"<<endl;
				return false;
		}
		if(RTMP_ConnectStream(m_rtmp,0) == FALSE)
		{
				cout<<"Connect stream with RTMP error"<<endl;
				return false;
		}
		av_register_all();
		return true;
}

bool RtmpSmartPusher::startPush()
{

	//设置为RR
	pthread_attr_t attr1;
	pthread_attr_t attr2;
	pthread_attr_t attr3;
    struct sched_param sched;
    int rs;

    rs = pthread_attr_init( &attr1 );
    rs = pthread_attr_init( &attr2 );
    rs = pthread_attr_init( &attr3 );
    assert( rs == 0 );

	sched.__sched_priority = 30;
	rs = pthread_attr_setschedpolicy( &attr1, SCHED_RR );
    assert( rs == 0 );
	pthread_attr_setschedparam(&attr1,&sched);
 	pthread_attr_setinheritsched(&attr1,PTHREAD_EXPLICIT_SCHED);
	int ret = 0;
	ret = pthread_create(&m_audioThread,&attr1,do_audio_push_thread,this);
	if(ret != 0)
		return false;


	sched.__sched_priority = 30;
	rs = pthread_attr_setschedpolicy( &attr2, SCHED_RR );
    assert( rs == 0 );
	pthread_attr_setschedparam(&attr2,&sched);
 	pthread_attr_setinheritsched(&attr2,PTHREAD_EXPLICIT_SCHED);
	ret = pthread_create(&m_videoThread,&attr2,do_video_push_thread,this);
	if(ret != 0)
		return false;


	sched.__sched_priority = 30;
	rs = pthread_attr_setschedpolicy( &attr3, SCHED_RR );
    assert( rs == 0 );
	pthread_attr_setschedparam(&attr3,&sched);
 	pthread_attr_setinheritsched(&attr3,PTHREAD_EXPLICIT_SCHED);
	ret = pthread_create(&m_pushThread,&attr3,do_queue_push_thread,this);
	if(ret != 0)
		return false;
	cout << " RtmpSmartPusher::startPush start threads "  <<endl;
		

	int audioover = 0;
	int videoover = 0;

	void *pvideoover = &videoover;
	void *paudioover = &audioover;

	pthread_join(m_audioThread,&(paudioover));
	pthread_join(m_videoThread,&(pvideoover));
	pthread_join(m_pushThread,NULL);
	pthread_attr_destroy(&attr1);
	pthread_attr_destroy(&attr2);
	pthread_attr_destroy(&attr3);
}

void* RtmpSmartPusher::do_queue_push_thread(void* arg)
{
	RtmpSmartPusher* rtmppusher = (RtmpSmartPusher*)arg;
	rtmppusher->popQueueAndSendPacket();
}

void* RtmpSmartPusher::do_video_push_thread(void* arg)
{
	cout << " getVideoDevice()  "<<getVideoDevice() << "  strcmp(getVideoDevice(),\"none\") :"<< strcmp(getVideoDevice(),"none")  <<endl;
	
	if(!strcmp(getVideoDevice(),"none"))
	{
		cout << " DON'T USE VIDEO "  <<endl;
		return NULL;
	}
	
	RtmpSmartPusher* rtmppusher = (RtmpSmartPusher*)arg;
	rtmppusher->StartPusherH264();
	rtmppusher->m_videoover = 1;
	return &(rtmppusher->m_videoover);
}

void* RtmpSmartPusher::do_audio_push_thread(void* arg)
{
	if(!strcmp(getAudioDevice(),"none"))
	{
		cout << " DON'T USE Audio "  <<endl;
		return NULL;
	}

	RtmpSmartPusher* rtmppusher = (RtmpSmartPusher*)arg;
	rtmppusher->StartPusherAAC();
	rtmppusher->m_audioover = 1;
	return &(rtmppusher->m_audioover);
}

bool RtmpSmartPusher::b_need_clear_queue = true;
bool RtmpSmartPusher::popQueueAndSendPacket()
{
	// cout <<"popQueueAndSendPacket 	1 " <<endl;
	while(m_audioover == 0 || m_videoover == 0)
	{

		if(m_packetQueue.empty())
		{
			usleep(1000);
			continue;
		}

		if(b_need_clear_queue)
		{
			while(!m_packetQueue.empty())
			{
				RTMPPacket* packet = m_packetQueue.front();
				m_packetQueue.pop_front();
				if(packet)
				{
					
					free(packet);
				}
			}
			m_packetQueue.clear();
			b_need_clear_queue = false;
			continue;
		}

		//TODO...是否加锁？
		RTMPPacket* packet = m_packetQueue.front();
		m_packetQueue.pop_front();

		if(packet)
		{
			if (RTMP_IsConnected(m_rtmp))
			{
				// cout<<"start send packet"<<endl;
				int nRet = RTMP_SendPacket(m_rtmp,packet,FALSE); 
				/*TRUE为放进发送队列,FALSE是不放进发送队列,直接发送*/ //FALSE TRUE一样
			}
		}

		// cout <<"popQueueAndSendPacket 	2 " <<endl;

		// if(packet->m_chunk)
		// {
		// 	cout <<"m_chunk not null" <<endl;
		// 	free(packet->m_chunk);
		// 	packet->m_chunk=NULL;
		// }
			
		free(packet);

		// cout <<"popQueueAndSendPacket 	6 " <<endl;
		
	}
	cout<<"RTMP video and audio data send over"<<endl;
}


bool RtmpSmartPusher::StartPusherAAC()
{
	
	// G711AudioStreamSource g711source(getAudioDevice());
	Recorder recoder(32000);
	
	const int LEN = 4096;

	setAudioSpecificConfig();
	
	struct timeval first_time;
	struct timeval last_time;

	unsigned int len = 0;
	gettimeofday(&first_time,NULL);
	// cout<<"StartPusherAAC 1"<<endl;
	FILE* faudio1 = fopen("out_1.pcm","wb");
	// cout<<"StartPusherAAC 2"<<endl;
	
	FILE* faudio2 = fopen("out_2.pcm","wb");
	// cout<<"StartPusherAAC 3"<<endl;
	
    // float factor = 1000/44100/2/2;
    // float factor = 1024 * 1000 / 44100;
    // float factor = 10240 / 441; //每一帧的时间
    float factor = 1024 / 32; //每一帧的时间
    // float factor = 1000000/8000;
	unsigned char*  buf_ = NULL;
	struct timeval _now;

	while(1)
	{
			
		if(nDropPframe)
		{
			cout<< " DROP AUDIO FRAME "<< endl;
			usleep(100000);
			continue;
		}

		unsigned int len = 0;
		// cout <<"StartPusherAAC 	1 " <<endl;

		uint32_t time_1 = RTMP_GetTime();
		len = recoder.recodeAAC(buf_);
		cout <<" AAC TIME ----------------- "<< RTMP_GetTime() - time_1 <<endl;

		if(len > 0)
		{
			unsigned char* data = buf_;
			unsigned int num = len;
			// fwrite(buf, 1, len, faudio);
			// fwrite(data, 1, num, faudio2);
			
			static int inc = 0;
			// // int timestamp = (inc++)*(LEN*8*1000000/8000);
			float _ts = (inc++)*factor;
			int timestamp = _ts;
		
			// struct timeval _now;
			// gettimeofday(&_now,NULL);
			// int timestamp = time_diff(_now, first_time) / 1000;
			if(bavsync)
			{
				pthread_mutex_lock(&m_lck);
				if(m_audiotime <= m_videotime)
				{
					m_audiotime = timestamp;
					if(m_audiotime > m_videotime)
					{
						pthread_mutex_unlock(&m_lck);
						pthread_cond_signal(&cv);
						// cout<<"unlock audio"<<endl;
						// continue;
					}else
					{
						pthread_mutex_unlock(&m_lck);						
					}

					SendAACPacket(data,num,timestamp);
					
				}
				else
				{
					m_audiotime = timestamp;
					
					if(m_videoover == 0)
					{
						pthread_cond_wait(&cv,&m_lck);
					}
					
					SendAACPacket(data,num,timestamp);
					pthread_mutex_unlock(&m_lck);

					
				}
			}else
			{
			
				if(start_now==0)
				{
					start_now = RTMP_GetTime();
				}
				uint32_t rtmp_time = RTMP_GetTime()-start_now;
				m_audiotime = rtmp_time;
				// cout << "Audio Time:"<< rtmp_time <<endl;

				SendAACPacket(data,num,m_audiotime);
			}

			usleep(15000);
			
		}else
		{
			usleep(10000);
		}
	}
	fclose(faudio1);
	fclose(faudio2);
}

bool RtmpSmartPusher::SendAACPacket(unsigned char *data,unsigned int size,unsigned int nTimeStamp)
{
	unsigned char *body = (unsigned char*)malloc(size + 2);
	// unsigned char body[2]={0};
	memset(body,0,2);
	int num = 0;
	//body[num] = 0xAF;
	unsigned char flag = 0;
	flag = (10 << 4) |  // soundformat "10 == AAC" linear PCM ,little endian
	(3 << 2) |      // soundrate   "3  == 44-kHZ"
	(1 << 1) |      // soundsize   "1  == 16bit"
	0;              // soundtype   "1  == Stereo"

	body[num] = flag;
	num++;
	body[num] = 0x01;
	num++;
	memcpy(body+2,data,size);
	RTMPPacket* packet;
	/*分配包内存和初始化,len为包体长度*/
	packet = (RTMPPacket *)malloc(RTMP_HEAD_SIZE+size+2);
	packet->m_chunk=NULL;
	packet->m_body=NULL;
	memset(packet,0,RTMP_HEAD_SIZE);
	packet->m_body = (char *)packet + RTMP_HEAD_SIZE;
	//memcpy(packet->m_body,data,size);
	// cout<<"RTMP_HEAD_SIZE:"<<RTMP_HEAD_SIZE<< " SIZE:"<<size<< endl;

	packet->m_packetType = RTMP_PACKET_TYPE_AUDIO;   
	packet->m_nChannel = 0x04;    //用户消息ID 4， 12356 表示数据流上去就播放
// 	#define STREAM_CHANNEL_METADATA  0x03  
// #define STREAM_CHANNEL_VIDEO     0x04  
// #define STREAM_CHANNEL_AUDIO     0x05  
	packet->m_headerType = RTMP_PACKET_SIZE_LARGE;    
	packet->m_nTimeStamp = nTimeStamp;    
	packet->m_nInfoField2 = m_rtmp->m_stream_id;  
	packet->m_nBodySize = size+2;
	// memcpy(packet->m_body,body,2);
	memcpy(packet->m_body,body,size+2);
	free(body);

	//cout<<"start send AAC packet"<<endl;
	pthread_mutex_lock(&m_queuelck);
	m_packetQueue.push_back(packet);
	pthread_mutex_unlock(&m_queuelck);
}

bool RtmpSmartPusher::setAudioSpecificConfig()
{//http://niulei20012001.blog.163.com/blog/static/7514721120130694144813/
	unsigned char *body = (unsigned char*)malloc(1024);
	memset(body,0,1024);
	int num = 0;
	unsigned char flag = 0;
	flag = (10 << 4) |  // soundformat "10 == AAC"
	(3 << 2) |      // soundrate   "3  == 44-kHZ"
	(1 << 1) |      // soundsize   "1  == 16bit"
	0;              // soundtype   "1  == Stereo"   0:solo
	body[num++] = flag;
	body[num++] = 0x00;
	//00010 0101 0001 000 
	//1288
	// http://blog.csdn.net/wzw88486969/article/details/50541311
	//https://wiki.multimedia.cx/index.php?title=MPEG-4_Audio
	body[num++] = 0x12;
	body[num++] = 0x88; ///0x08
	sendPacket(RTMP_PACKET_TYPE_AUDIO,(unsigned char*)body,num,0,0x04);
	free(body);
}


# ifndef AV_FORMAT_READ_BUFFER
#	define AV_FORMAT_READ_BUFFER 65536
# endif


//视频取流函数
int fill_iobuffer(void *opaque,uint8_t *buf,int buf_size)
{
	return read_buffer(opaque,buf,buf_size);
}

bool RtmpSmartPusher::StartPusherH264()
{
	
	FILE *h264 = NULL;
	h264 = fopen("m.h264","wb");
	AVFormatContext *pFormatCtx = NULL;
	AVCodecContext *pCodecCtx = NULL;
	AVCodec *pCodec = NULL;
	AVInputFormat *piFmt = NULL;
		// cout <<"StartPusherAAC 	1 " <<endl;

	start_cap();
		// cout <<"StartPusherAAC 	2 " <<endl;
	
	pFormatCtx = avformat_alloc_context();
	if(pFormatCtx == NULL)
	{
			cout<<"alloc format error"<<endl;
			return false;
	}
		// cout <<"StartPusherAAC 	3 " <<endl;

	//采用内存方式提供流
	unsigned char * iobuffer=(unsigned char *)av_malloc(AV_FORMAT_READ_BUFFER);  
	AVIOContext *avio =avio_alloc_context(iobuffer, AV_FORMAT_READ_BUFFER,0,NULL,fill_iobuffer,NULL,NULL);
	if(avio == NULL)
	{
			cout<<"alloc avio context error"<<endl;
			return false;
	}  
		// cout <<"StartPusherAAC 	4 " <<endl;
	pFormatCtx->pb=avio;
	av_probe_input_buffer(avio, &piFmt, "", NULL, 0, 0);
		// cout <<"StartPusherAAC 	5 " <<endl;
	if(avformat_open_input(&pFormatCtx,NULL,piFmt,NULL) != 0)
	{
			cout<<"open video input file error"<<endl;
			return false;
	}
		// cout <<"StartPusherAAC 	6 " <<endl;
	if(avformat_find_stream_info(pFormatCtx,NULL) < 0 )
	{
			cout<<"find stream info error"<<endl;
			return false;
	}
	int videoindex = -1;
		// cout <<"StartPusherAAC 	7 " <<endl;
	for(int i = 0; i < pFormatCtx->nb_streams; i++)
	{
			if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
			{
					videoindex = i;
					break;
			}
	}

		// cout <<"StartPusherAAC 	8 " <<endl;
	if(videoindex == -1)
	{
			cout<<"can't find any video"<<endl;
			return false;
	}
	pCodecCtx = pFormatCtx->streams[videoindex]->codec;
		// cout <<"StartPusherAAC 	9 " <<endl;
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if(pCodec == NULL)
	{
			cout<<"can't find codec "<<endl;
			return false;
	}
	
		// cout <<"StartPusherAAC 	10 " <<endl;
	if(avcodec_open2(pCodecCtx,pCodec,NULL) < 0)
	{
			cout<<"open codec error"<<endl;
			return false;
	}
	FILE *dump = NULL;
	dump = fopen("dump.txt","wb");
	if(!dump)
			return false;
	int num = 0;
		// cout <<"StartPusherAAC 	11 " <<endl;
	num = fwrite(pCodecCtx->extradata,1,pCodecCtx->extradata_size,dump);
	if(num != pCodecCtx->extradata_size)
	{
			cout<<"write error"<<endl;
			return false;
	}
	fclose(dump);
		// cout <<"StartPusherAAC 	12 " <<endl;
	getSPSAndPPS(pCodecCtx->extradata,pCodecCtx->extradata_size);
	cout<<"sps size is "<<m_264metadata.sps_size<<endl<<"pps size is "<<m_264metadata.pps_size<<endl;
	bool iskey = false;
	int lasttime = 0;
	int factor = 1000/getFps();
	int goptime = getFps() >= 20 ? 1000 : 2000;

	if(getFps() >= 25)
	{
		goptime = 1000;
	}else if(getFps() > 15 && getFps() <25)
	{
		goptime = 1500;
	}else if(getFps() == 15)
	{
		goptime = 2000;
	}else  if(getFps() >= 10 && getFps() <15)
	{
		goptime = 3000;
	}else
	{
		goptime = 4000;
	}

	// int64_t last_update=0;
	struct timeval tlast;
	tlast.tv_sec = 0;
	tlast.tv_usec = 0;
	struct timeval tnow;
	tnow.tv_sec = 0;
	tnow.tv_usec = 0;

	struct timeval _now;

	int nDropGOP = 1;

	float interval = 1000/30;

	while(1)
	{
		AVPacket* packet = new AVPacket() ;
		// nDropPframe = 0;
		if(av_read_frame(pFormatCtx,packet) >= 0)
		{
			if(packet->stream_index == videoindex)
			{	
				if((packet->data[4] & 0x07) == 0x07 || (packet->data[4] & 0x08) == 0x08)
				{
					packet->data = packet->data + pCodecCtx->extradata_size;
					packet->size = packet->size - pCodecCtx->extradata_size;
				}
				if(packet->flags & AV_PKT_FLAG_KEY)
				{
					nDropPframe = 0;
					iskey = true;
				}
				else{
					iskey = false;
					if(nDropPframe)
					{
						continue;
					}
				}

				
				static int inc = 0;
				int timestamp = (inc++) * interval;
				
				if(bavsync)
				{
					int timestamp = (inc++)*(1000/getFps());
					
					pthread_mutex_lock(&m_lck);
					if(m_videotime <= m_audiotime)
					{
						m_videotime = timestamp;
						if(m_videotime > m_audiotime)
						{
							pthread_mutex_unlock(&m_lck);
							sendRtmp264Packet(packet->data,packet->size,iskey,timestamp);
							pthread_cond_signal(&cv);
							
							continue;
						}
						pthread_mutex_unlock(&m_lck);
						sendRtmp264Packet(packet->data,packet->size,iskey,timestamp);
						
					}
					else
					{
						cout<<"m_video is > m_audio timestamp:"<<timestamp<<endl;
						pthread_cond_wait(&cv,&m_lck);
						
						m_videotime = timestamp;
					
						pthread_mutex_unlock(&m_lck);
						sendRtmp264Packet(packet->data,packet->size,iskey,timestamp);
						
					}
				}else
				{
					if(start_now==0)
					{
						start_now = RTMP_GetTime();
					}

					uint32_t rtmp_time = RTMP_GetTime() - start_now;
					// cout << "Video Time:"<< rtmp_time <<endl;
					sendRtmp264Packet(packet->data,packet->size,iskey,m_audiotime);
				}
				
			}

			usleep(1000);
		}
		else
		{
			cout<<"read 264 frame end"<<endl;
			usleep(10000);
		}

		av_packet_unref(packet); 
		av_packet_free(&packet); 
		delete packet;
	}
	avcodec_close(pCodecCtx);
	avformat_close_input(&pFormatCtx);
	stop_cap();
	fclose(h264);
	return true;
}



bool RtmpSmartPusher::sendRtmp264Packet(unsigned char* data, int size , bool iskeyframe,int nTimeStamp)
{
	if(data == NULL && size<11)
	{  
		return false;  
	}
	unsigned char *body =  (unsigned char*)malloc(size+9);  
	memset(body,0,size+9);
	int i = 0;
	if(iskeyframe)
	{
		body[i++] = 0x17;// 1:Iframe  7:AVC   
		body[i++] = 0x01;// AVC NALU   
		body[i++] = 0x00;  
		body[i++] = 0x00;  
		body[i++] = 0x00;  


		// NALU size   
		body[i++] = size>>24 &0xff;  
		body[i++] = size>>16 &0xff;  
		body[i++] = size>>8 &0xff;  
		body[i++] = size&0xff;
		// NALU data   
		memcpy(&body[i],data,size);
		sendRtmpspsppsPacket(m_264metadata.sps,m_264metadata.sps_size,m_264metadata.pps,m_264metadata.pps_size);		
	}
	else
	{
		body[i++] = 0x27;// 2:Pframe  7:AVC   
		body[i++] = 0x01;// AVC NALU   
		body[i++] = 0x00;  
		body[i++] = 0x00;  
		body[i++] = 0x00;  

		// NALU size   
		body[i++] = size>>24 &0xff;  
		body[i++] = size>>16 &0xff;  
		body[i++] = size>>8 &0xff;  
		body[i++] = size&0xff;
		// NALU data   
		memcpy(&body[i],data,size);
	}

	sendPacket(RTMP_PACKET_TYPE_VIDEO,body,i+size,nTimeStamp,0x04);

	free(body);
	return true;
}

bool RtmpSmartPusher::sendPacket(unsigned int nPacketType,unsigned char *data,unsigned int size,unsigned int nTimestamp, unsigned int channel)
{
	RTMPPacket* packet;
	/*分配包内存和初始化,len为包体长度*/
	packet = (RTMPPacket *)malloc(RTMP_HEAD_SIZE+size);
	packet->m_chunk=NULL;
	packet->m_body=NULL;
	
	memset(packet,0,RTMP_HEAD_SIZE);
	packet->m_body = (char *)packet + RTMP_HEAD_SIZE;
	memcpy(packet->m_body,data,size);
	packet->m_nBodySize = size;
	packet->m_hasAbsTimestamp = 0;
	packet->m_packetType = nPacketType; /*此处为类型有两种一种是音频,一种是视频*/
	packet->m_nInfoField2 = m_rtmp->m_stream_id;
	packet->m_nChannel = channel;
	packet->m_headerType = RTMP_PACKET_SIZE_LARGE;
	packet->m_nTimeStamp = nTimestamp;
	
	pthread_mutex_lock(&m_queuelck);
	m_packetQueue.push_back(packet);
	pthread_mutex_unlock(&m_queuelck);

	return true;
}


bool RtmpSmartPusher::sendRtmpspsppsPacket(unsigned char* sps, int sps_size,unsigned char* pps , int pps_size)
{
	RTMPPacket *packet = NULL;
	packet = (RTMPPacket *)malloc(RTMP_HEAD_SIZE+1024);
	packet->m_chunk=NULL;

	unsigned char * body=NULL;
	memset(packet,0,RTMP_HEAD_SIZE+1024);
	packet->m_body = (char *)packet + RTMP_HEAD_SIZE;
	body = (unsigned char *)packet->m_body;

	int i = 0;
	body[i++] = 0x17;
	body[i++] = 0x00;

	body[i++] = 0x00;
	body[i++] = 0x00;
	body[i++] = 0x00;

	/*AVCDecoderConfigurationRecord*/
	body[i++] = 0x01;
	body[i++] = sps[1];
	body[i++] = sps[2];
	body[i++] = sps[3];
	body[i++] = 0xff;

	/*sps*/
	body[i++]   = 0xe1;
	body[i++] = (sps_size >> 8) & 0xff;
	body[i++] = sps_size & 0xff;
	memcpy(&body[i],sps,sps_size);
	i +=  sps_size;

	/*pps*/
	body[i++]   = 0x01;
	body[i++] = (pps_size >> 8) & 0xff;
	body[i++] = (pps_size) & 0xff;
	memcpy(&body[i],pps,pps_size);
	i +=  pps_size;

	packet->m_packetType = RTMP_PACKET_TYPE_VIDEO;
	packet->m_nBodySize = i;
	packet->m_nChannel = 0x04;
	packet->m_nTimeStamp = 0;
	packet->m_hasAbsTimestamp = 0;
	packet->m_headerType = RTMP_PACKET_SIZE_LARGE;
	packet->m_nInfoField2 = m_rtmp->m_stream_id;
	pthread_mutex_lock(&m_queuelck);
	m_packetQueue.push_back(packet);
	pthread_mutex_unlock(&m_queuelck);
	
	return true;
}

void RtmpSmartPusher::getSPSAndPPS(const unsigned char* sps_pps_data,int size)
{	
	int i = 0;
	for(;i<size-4;i++)
	{
			if(sps_pps_data[i] ==0x00 && sps_pps_data[i+1] == 0x00 && sps_pps_data[i+2] == 0x00 && sps_pps_data[i+3] == 0x01)
			{
					i = i+3+1;
					break;	
			}
	}	
	int sps_start = i;
	int sps_end = 0;
	for(; i<size-4;i++)
	{
		if(sps_pps_data[i] ==0x00 && sps_pps_data[i+1] == 0x00 && sps_pps_data[i+2] == 0x00 && sps_pps_data[i+3] == 0x01)
		{
			sps_end = i-1;
			i = i+3;
			break;	
		}
	}
	int pps_start = i+1;
	int ppssize = size - pps_start;
	int sps_size = sps_end-sps_start + 1;
	unsigned char* sps = new unsigned char[sps_size];
	if(!sps)
		return;
	memset(sps,0,sps_size);
	memcpy(sps,sps_pps_data+sps_start,sps_size);
	m_264metadata.sps = sps;
	m_264metadata.sps_size = sps_size;

	for(int j = 0; j< sps_size;j++)
	{
			printf("%x",sps[j]);
	}
	unsigned char* pps = new unsigned char[ppssize];
	if(!pps)
		return;
	memset(pps,0,ppssize);
	memcpy(pps,sps_pps_data+pps_start,ppssize);
	m_264metadata.pps = pps;
	m_264metadata.pps_size = ppssize;
}