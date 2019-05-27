#ifndef __FFMPEG_H__
#define __FFMPEG_H__

#ifdef __cplusplus
       extern "C" {
#endif

typedef struct{
	char inUrl[256];
	char outUrl[256];
	char outSize[16];
}FF_TRANS_S;

#define FF_TRANS_RUN 1
#define FF_TRANS_EXIT 2
#define FF_READ_TIMEOUT_ERROR (-30)
#define FF_WRITE_TIMEOUT_ERROR (-40)

#define MP4FILE_DURATION (1800)	//unit:sec

int FF_Fork(char *script);
int FF_Transcode_Live(int id,FF_TRANS_S *pTrans,int *state);
int FF_Transcode_UDP(int id,FF_TRANS_S *pTrans,int *state);
int FF_Transcode_Mp4(int id,char* inUrl, char* outUrl, int duration);


#ifdef __cplusplus
}
#endif 

#endif	 
