
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <ctype.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <sys/msg.h>
#include <sys/ipc.h>

#include <time.h>
#include <sys/time.h>
#include <pthread.h> 

#include "libavformat/avformat.h"  
#include "libavformat/avio.h"  
#include "libavcodec/avcodec.h"  
#include "libswscale/swscale.h"  
#include "libavutil/avutil.h"  
#include "libavutil/mathematics.h"  
#include "libswresample/swresample.h"  
#include "libavutil/opt.h"  
#include "libavutil/channel_layout.h"  
#include "libavutil/samplefmt.h"  
#include "libavdevice/avdevice.h" 
#include "libavfilter/avfilter.h"  
#include "libavutil/error.h"  
#include "libavutil/mathematics.h"    
#include "libavutil/time.h"    
#include "inttypes.h"  
#include "stdint.h"  

#include "vos.h"
#include "tools.h"
#include "ffmpeg.h" 

extern int Log_MsgLine(char *LogFileName,char *sLine);
#if 1
#define 	FFPrint(fmt,args...)  do{ \
			char info[3000]; \
			sprintf(info,":" fmt , ## args); \
			Log_MsgLine("ff.log",info); \
			}while(0)
#else
#define 	FFPrint(fmt,args...)  
#endif

int FF_Fork(char *script)
{
	int ret=0;
	int pid=-1;
	int ReTryInterval=1;
	
	if(!fork()){
		pid=getpid();
		
		//printf("=============child: pid %d==========\n",pid); 
		//execve("/home/LiveSvr/bin/push_mp4.sh /home/LiveSvr/bin/44d1.mp4",NULL,envp);
		//execve("/home/LiveSvr/bin/ffmpeg -re -i /home/LiveSvr/bin/44d1.mp4 -vcodec copy -acodec copy "
//        "-f flv -y rtmp://127.0.0.1/live/demo",NULL,NULL);

		ret=osSystem(script);
		while(1){
			sleep(ReTryInterval);
			
			ret=osSystem(script);
			if (ReTryInterval<30)
				ReTryInterval++;
		}
	}

	return pid;
}

/*
测试用的rtsp url:
rtsp://uuc.gzopen.cn:554/media_proxy?media_type=monitoring&camera_id=ff8080815b8699d5015ba1a6e68a0006&stream_type=1&audio=1&video=1

*/
int FF_Transcode_Live(int id,FF_TRANS_S *pTrans,int *state)
{
	AVOutputFormat *ofmt = NULL;
	//Input AVFormatContext and Output AVFormatContext
	AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
	AVPacket pkt;
	char *in_filename, *out_filename;
	int ret=0, i;
	int videoindex=-1;
	int frame_index=0;
	int64_t start_time=0;

	in_filename  = pTrans->inUrl;
	out_filename = pTrans->outUrl;
//	FFPrint("%s(%d,%s,%s)\n",__FUNCTION__,id,in_filename,out_filename);

	av_register_all();
	avformat_network_init();

	AVDictionary* options = NULL;  
	av_dict_set(&options, "rtsp_transport", "tcp", 0);
	//av_dict_set(&options, "s", "640x352", 0);
	//if (strlen(pTrans->outSize)>3)
	//	av_dict_set(&options, "s", pTrans->outSize, 0);

//-vcodec libx264 -acodec libfdk_aac. 参数设置好象不生效！！no effect!!why	
//	av_dict_set(&options, "vcodec", "libx264", 0);
//	av_dict_set(&options, "acodec", "libfdk_aac", 0);

	if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, &options)) < 0) {
		printf( "%s:Could not open input file=%s!\n",__FUNCTION__,in_filename);
		ret=-2;
		goto end;
	}

	if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
		printf( "%s:Failed to retrieve input stream information!\n",__FUNCTION__);
		ret=-3;
		goto end;
	}

	for(i=0; i<ifmt_ctx->nb_streams; i++) 
		if(ifmt_ctx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO){
			videoindex=i;
			break;
		}

	av_dump_format(ifmt_ctx, 0, in_filename, 0);

	avformat_alloc_output_context2(&ofmt_ctx, NULL, "flv", out_filename);

	if (!ofmt_ctx) {
		printf( "%s:Could not create output context\n",__FUNCTION__);
		ret = -4;
		goto end;
	}
	ofmt = ofmt_ctx->oformat;
	for (i = 0; i < ifmt_ctx->nb_streams; i++) {
		//Create output AVStream according to input AVStream
		AVStream *in_stream = ifmt_ctx->streams[i];
		AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
		if (!out_stream) {
			printf( "%s:Failed allocating output stream!\n",__FUNCTION__);
			ret = -5;
			goto end;
		}
		//Copy the settings of AVCodecContext
		ret = avcodec_copy_context(out_stream->codec, in_stream->codec);
		if (ret < 0) {
			printf( "%s:Failed to copy context from input to output stream codec context\n",__FUNCTION__);
			ret=-6;
			goto end;
		}
		out_stream->codec->codec_tag = 0;
		if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
			out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}

	av_dump_format(ofmt_ctx, 0, out_filename, 1);

	if (!(ofmt->flags & AVFMT_NOFILE)) {
		ret = avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
		if (ret < 0) {
			printf( "%s:Could not open output URL '%s'\n",__FUNCTION__,out_filename);
			ret=-7;
			goto end;
		}
	}
	
	//Write file header
	ret = avformat_write_header(ofmt_ctx, NULL);
	if (ret < 0) {
		printf( "%s(%d):Error occurred when opening output URL!\n",__FUNCTION__,id);
		ret=-8;
		goto end;
	}

#if USE_H264BSF
	AVBitStreamFilterContext* h264bsfc =  av_bitstream_filter_init("h264_mp4toannexb"); 
#endif
	int ReadFailCount=0;
	int WriteFailCount=0;

	start_time=av_gettime();
	while ((*state) != FF_TRANS_EXIT) {
		AVStream *in_stream, *out_stream;
		//Get an AVPacket
		ret = av_read_frame(ifmt_ctx, &pkt);
		if (ret < 0){
			usleep(500000);	// 500ms
			ReadFailCount++;
			if (ReadFailCount>(2*60*30)){
				ret=FF_READ_TIMEOUT_ERROR;
				break;
			}
			continue;
		}
		ReadFailCount=0;	//clear counter
		
		//FIX：No PTS (Example: Raw H.264)
		//Simple Write PTS
		if(pkt.pts==AV_NOPTS_VALUE){
			//Write PTS
			AVRational time_base1=ifmt_ctx->streams[videoindex]->time_base;
			//Duration between 2 frames (us)
			int64_t calc_duration=(double)AV_TIME_BASE/av_q2d(ifmt_ctx->streams[videoindex]->r_frame_rate);
			//Parameters
			pkt.pts=(double)(frame_index*calc_duration)/(double)(av_q2d(time_base1)*AV_TIME_BASE);
			pkt.dts=pkt.pts;
			pkt.duration=(double)calc_duration/(double)(av_q2d(time_base1)*AV_TIME_BASE);
		}
		//Important:Delay
		if(pkt.stream_index==videoindex){
			AVRational time_base=ifmt_ctx->streams[videoindex]->time_base;
			AVRational time_base_q={1,AV_TIME_BASE};
			int64_t pts_time = av_rescale_q(pkt.dts, time_base, time_base_q);
			int64_t now_time = av_gettime() - start_time;
			if (pts_time > now_time)
				av_usleep(pts_time - now_time);

		}

		in_stream  = ifmt_ctx->streams[pkt.stream_index];
		out_stream = ofmt_ctx->streams[pkt.stream_index];
		/* copy packet */
		//Convert PTS/DTS
		pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (enum AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
		pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (enum AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
		pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
		pkt.pos = -1;
		//Print to Screen
		if(pkt.stream_index==videoindex){
			//printf("Receive %8d video frames from input URL\n",frame_index);
			frame_index++;

#if USE_H264BSF
			av_bitstream_filter_filter(h264bsfc, in_stream->codec, NULL, &pkt.data, &pkt.size, pkt.data, pkt.size, 0);
#endif
		}

//		//ret = av_write_frame(ofmt_ctx, &pkt);
		ret = av_interleaved_write_frame(ofmt_ctx, &pkt);
		av_free_packet(&pkt);

	//	if (ret < 0) {
	//		printf( "%s:Error muxing packet\n",__FUNCTION__);
	//		break;
	//	}
		if (ret < 0){
			//printf( "%s:Error muxing packet!(FMS error!)\n",__FUNCTION__);
			WriteFailCount++;		// waiting 10s
			if (WriteFailCount>(150)){
				ret=FF_WRITE_TIMEOUT_ERROR;
				break;
			}
		}
		else{
			WriteFailCount=0;	//clear counter
		}
		
	}

#if USE_H264BSF
	av_bitstream_filter_close(h264bsfc);  
#endif

	//Write file trailer
	av_write_trailer(ofmt_ctx);
end:
	if (!ofmt_ctx) {
		avformat_close_input(&ifmt_ctx);
		/* close output */
		if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
			avio_close(ofmt_ctx->pb);
		avformat_free_context(ofmt_ctx);
	}
	
//	if (ret < 0 && ret != AVERROR_EOF) {
		//printf( "Exit! %s(%d,%s,%s),ret=%d\n",__FUNCTION__,id,rtspurl,rtmpurl,ret);
	//	return ret;
//	}

	FFPrint( "%s(%d , %s , %s),ret=%d\n",__FUNCTION__,id,in_filename,out_filename,ret);
	return ret;
}


int FF_Transcode_UDP(int id,FF_TRANS_S *pTrans,int *state)
{
	AVOutputFormat *ofmt = NULL;
	AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
	AVPacket pkt;
	char *in_filename, *out_filename;
	int ret=0, i;
	int videoindex=-1;
	int frame_index=0;
	int64_t start_time=0;

	in_filename  = pTrans->inUrl;
	out_filename = pTrans->outUrl;

	av_register_all();
	avformat_network_init();

	AVDictionary* options = NULL;  
//	av_dict_set(&options, "rtsp_transport", "tcp", 0);

//-vcodec libx264 -acodec libfdk_aac. 参数设置好象不生效！！no effect!!why	
//	av_dict_set(&options, "vcodec", "libx264", 0);
//	av_dict_set(&options, "acodec", "libfdk_aac", 0);

	if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, &options)) < 0) {
		printf( "%s:Could not open input file=%s!\n",__FUNCTION__,in_filename);
		ret=-2;
		goto end;
	}

	if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
		printf( "%s:Failed to retrieve input stream information!\n",__FUNCTION__);
		ret=-3;
		goto end;
	}

	for(i=0; i<ifmt_ctx->nb_streams; i++) 
		if(ifmt_ctx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO){
			videoindex=i;
			break;
		}

	av_dump_format(ifmt_ctx, 0, in_filename, 0);

	avformat_alloc_output_context2(&ofmt_ctx, NULL, "flv", out_filename);

	if (!ofmt_ctx) {
		printf( "%s:Could not create output context\n",__FUNCTION__);
		ret = -4;
		goto end;
	}
	ofmt = ofmt_ctx->oformat;
	for (i = 0; i < ifmt_ctx->nb_streams; i++) {
		//Create output AVStream according to input AVStream
		AVStream *in_stream = ifmt_ctx->streams[i];
		AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
		if (!out_stream) {
			printf( "%s:Failed allocating output stream!\n",__FUNCTION__);
			ret = -5;
			goto end;
		}
		//Copy the settings of AVCodecContext
		ret = avcodec_copy_context(out_stream->codec, in_stream->codec);
		if (ret < 0) {
			printf( "%s:Failed to copy context from input to output stream codec context\n",__FUNCTION__);
			ret=-6;
			goto end;
		}
		out_stream->codec->codec_tag = 0;
		if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
			out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}

	av_dump_format(ofmt_ctx, 0, out_filename, 1);

	if (!(ofmt->flags & AVFMT_NOFILE)) {
		ret = avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
		if (ret < 0) {
			printf( "%s:Could not open output URL '%s'\n",__FUNCTION__,out_filename);
			ret=-7;
			goto end;
		}
	}
	
	//Write file header
	ret = avformat_write_header(ofmt_ctx, NULL);
	if (ret < 0) {
		printf( "%s(%d):Error occurred when opening output URL!\n",__FUNCTION__,id);
		ret=-8;
		goto end;
	}

	int ReadFailCount=0;
	int WriteFailCount=0;

	start_time=av_gettime();
	while ((*state) != FF_TRANS_EXIT) {
		AVStream *in_stream, *out_stream;
		//Get an AVPacket
		ret = av_read_frame(ifmt_ctx, &pkt);
		if (ret < 0){
			usleep(500000);	// 500ms
			ReadFailCount++;
			if (ReadFailCount>(2*60*30)){
				ret=FF_READ_TIMEOUT_ERROR;
				break;
			}
			continue;
		}
		ReadFailCount=0;	//clear counter
		
		//FIX：No PTS (Example: Raw H.264)
		//Simple Write PTS
		if(pkt.pts==AV_NOPTS_VALUE){
			//Write PTS
			AVRational time_base1=ifmt_ctx->streams[videoindex]->time_base;
			//Duration between 2 frames (us)
			int64_t calc_duration=(double)AV_TIME_BASE/av_q2d(ifmt_ctx->streams[videoindex]->r_frame_rate);
			//Parameters
			pkt.pts=(double)(frame_index*calc_duration)/(double)(av_q2d(time_base1)*AV_TIME_BASE);
			pkt.dts=pkt.pts;
			pkt.duration=(double)calc_duration/(double)(av_q2d(time_base1)*AV_TIME_BASE);
		}
		//Important:Delay
		if(pkt.stream_index==videoindex){
			AVRational time_base=ifmt_ctx->streams[videoindex]->time_base;
			AVRational time_base_q={1,AV_TIME_BASE};
			int64_t pts_time = av_rescale_q(pkt.dts, time_base, time_base_q);
			int64_t now_time = av_gettime() - start_time;
			if (pts_time > now_time)
				av_usleep(pts_time - now_time);

		}

		in_stream  = ifmt_ctx->streams[pkt.stream_index];
		out_stream = ofmt_ctx->streams[pkt.stream_index];
		/* copy packet */
		//Convert PTS/DTS
		pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (enum AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
		pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (enum AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
		pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
		pkt.pos = -1;
		//Print to Screen
		if(pkt.stream_index==videoindex){
			//printf("Receive %8d video frames from input URL\n",frame_index);
			frame_index++;
		}

//		//ret = av_write_frame(ofmt_ctx, &pkt);
		ret = av_interleaved_write_frame(ofmt_ctx, &pkt);
		av_free_packet(&pkt);

	//	if (ret < 0) {
	//		printf( "%s:Error muxing packet\n",__FUNCTION__);
	//		break;
	//	}
		if (ret < 0){
			//printf( "%s:Error muxing packet!(FMS error!)\n",__FUNCTION__);
			WriteFailCount++;		// waiting 10s
			if (WriteFailCount>(150)){
				ret=FF_WRITE_TIMEOUT_ERROR;
				break;
			}
		}
		else{
			WriteFailCount=0;	//clear counter
		}
		
	}

	//Write file trailer
	av_write_trailer(ofmt_ctx);
end:
	if (!ofmt_ctx) {
		avformat_close_input(&ifmt_ctx);
		/* close output */
		if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
			avio_close(ofmt_ctx->pb);
		avformat_free_context(ofmt_ctx);
	}
	
//	if (ret < 0 && ret != AVERROR_EOF) {
		//printf( "Exit! %s(%d,%s,%s),ret=%d\n",__FUNCTION__,id,rtspurl,rtmpurl,ret);
	//	return ret;
//	}

	FFPrint( "%s(%d , %s , %s),ret=%d\n",__FUNCTION__,id,in_filename,out_filename,ret);
	return ret;
}


int FF_Transcode_Mp4(int id,char* inUrl, char* outUrl, int duration)
{
	AVOutputFormat *ofmt = NULL;
	//Input AVFormatContext and Output AVFormatContext
	AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
	AVPacket pkt;
	char *in_filename, *out_filename;
	int ret=0, i;
	int videoindex=-1;
	int frame_index=0;
	int64_t start_time=0;
	unsigned int cost=0;

	in_filename  = inUrl;
	out_filename = outUrl;
//	FFPrint("%s(%d,%s,%s)\n",__FUNCTION__,id,in_filename,out_filename);

    avcodec_register_all();
//#if CONFIG_AVDEVICE
//    avdevice_register_all();
//#endif
    avfilter_register_all();
	av_register_all();
	avformat_network_init();

	AVDictionary* options = NULL;  
	av_dict_set(&options, "rtsp_transport", "tcp", 0);
//	av_dict_set(&options, "t", "1800", 0);		// -t duration

//-vcodec libx264 -acodec libfdk_aac. 参数设置好象不生效！！no effect!!why	
//	av_dict_set(&options, "vcodec", "libx264", 0);
//	av_dict_set(&options, "acodec", "libfdk_aac", 0);

	if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, &options)) < 0) {
		printf( "%s:Could not open input file=%s!\n",__FUNCTION__,in_filename);
		ret=-2;
		goto end;
	}

	if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
		printf( "%s:Failed to retrieve input stream information!\n",__FUNCTION__);
		ret=-3;
		goto end;
	}

	for(i=0; i<ifmt_ctx->nb_streams; i++) 
		if(ifmt_ctx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO){
			videoindex=i;
			break;
		}

	av_dump_format(ifmt_ctx, 0, in_filename, 0);

//	AVOutputFormat *ofmt1 = NULL;
//	ofmt1.audio_codec=AV_CODEC_ID_AAC;
//	ofmt1.video_codec=AV_CODEC_ID_H264;
//	avformat_alloc_output_context2(&ofmt_ctx, &ofmt1, "mp4", out_filename);

	avformat_alloc_output_context2(&ofmt_ctx, NULL, "mp4", out_filename);

	if (!ofmt_ctx) {
		printf( "%s:Could not create output context\n",__FUNCTION__);
		ret = -4;
		goto end;
	}
	ofmt = ofmt_ctx->oformat;
	for (i = 0; i < ifmt_ctx->nb_streams; i++) {
		//Create output AVStream according to input AVStream
		AVStream *in_stream = ifmt_ctx->streams[i];
		AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
		if (!out_stream) {
			printf( "%s:Failed allocating output stream!\n",__FUNCTION__);
			ret = -5;
			goto end;
		}
		//Copy the settings of AVCodecContext
		ret = avcodec_copy_context(out_stream->codec, in_stream->codec);
		if (ret < 0) {
			printf( "%s:Failed to copy context from input to output stream codec context\n",__FUNCTION__);
			ret=-6;
			goto end;
		}
		out_stream->codec->codec_tag = 0;
		if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
			out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}

	av_dump_format(ofmt_ctx, 0, out_filename, 1);

	if (!(ofmt->flags & AVFMT_NOFILE)) {
		ret = avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
		if (ret < 0) {
			printf( "%s:Could not open output URL '%s'\n",__FUNCTION__,out_filename);
			ret=-7;
			goto end;
		}
	}
	
	//Write file header
	ret = avformat_write_header(ofmt_ctx, NULL);
	if (ret < 0) {
		printf( "%s(%d):Error occurred when opening output URL!\n",__FUNCTION__,id);
		ret=-8;
		goto end;
	}

	int ReadFailCount=0;
	int WriteFailCount=0;

	start_time=av_gettime();
	while (1) {
		AVStream *in_stream, *out_stream;
		//Get an AVPacket
		ret = av_read_frame(ifmt_ctx, &pkt);
		if (ret < 0){
			usleep(500000);	// 500ms
			ReadFailCount++;
			if (ReadFailCount>(2*60*30)){
				ret=FF_READ_TIMEOUT_ERROR;
				break;
			}
			continue;
		}
		ReadFailCount=0;	//clear counter
		
		//FIX：No PTS (Example: Raw H.264)
		//Simple Write PTS
		if(pkt.pts==AV_NOPTS_VALUE){
			//Write PTS
			AVRational time_base1=ifmt_ctx->streams[videoindex]->time_base;
			//Duration between 2 frames (us)
			int64_t calc_duration=(double)AV_TIME_BASE/av_q2d(ifmt_ctx->streams[videoindex]->r_frame_rate);
			//Parameters
			pkt.pts=(double)(frame_index*calc_duration)/(double)(av_q2d(time_base1)*AV_TIME_BASE);
			pkt.dts=pkt.pts;
			pkt.duration=(double)calc_duration/(double)(av_q2d(time_base1)*AV_TIME_BASE);
		}
		//Important:Delay
		if(pkt.stream_index==videoindex){
			AVRational time_base=ifmt_ctx->streams[videoindex]->time_base;
			AVRational time_base_q={1,AV_TIME_BASE};
			int64_t pts_time = av_rescale_q(pkt.dts, time_base, time_base_q);
			int64_t now_time = av_gettime() - start_time;
			if (pts_time > now_time)
				av_usleep(pts_time - now_time);
		}

		in_stream  = ifmt_ctx->streams[pkt.stream_index];
		out_stream = ofmt_ctx->streams[pkt.stream_index];
		/* copy packet */
		//Convert PTS/DTS
		pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (enum AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
		pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (enum AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
		pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
		pkt.pos = -1;
		//Print to Screen
		if(pkt.stream_index==videoindex){
			//printf("Receive %8d video frames from input URL\n",frame_index);
			frame_index++;

		}

//		//ret = av_write_frame(ofmt_ctx, &pkt);
		ret = av_interleaved_write_frame(ofmt_ctx, &pkt);
		av_free_packet(&pkt);

	//	if (ret < 0) {
	//		printf( "%s:Error muxing packet\n",__FUNCTION__);
	//		break;
	//	}
		if (ret < 0){
			//printf( "%s:Error muxing packet!(FMS error!)\n",__FUNCTION__);
			WriteFailCount++;		// waiting 10s
			if (WriteFailCount>(150)){
				ret=FF_WRITE_TIMEOUT_ERROR;
				break;
			}
		}
		else{
			WriteFailCount=0;	//clear counter
		}

		cost=av_gettime() - start_time;
		if (cost >= duration)
			break;
	}

	//Write file trailer
	av_write_trailer(ofmt_ctx);
end:
	if (!ofmt_ctx) {
		avformat_close_input(&ifmt_ctx);
		/* close output */
		if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
			avio_close(ofmt_ctx->pb);
		avformat_free_context(ofmt_ctx);
	}
	
//	if (ret < 0 && ret != AVERROR_EOF) {
		//printf( "Exit! %s(%d,%s,%s),ret=%d\n",__FUNCTION__,id,rtspurl,rtmpurl,ret);
	//	return ret;
//	}

	FFPrint( "%s(%d,%s,%s),ret=%d\n",__FUNCTION__,id,inUrl,outUrl,ret);
	return ret;
}


