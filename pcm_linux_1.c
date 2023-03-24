/***************************************************************
文件名 : pcm_linux_1.c
作者 : Octopus
参考 : https://cloud.tencent.com/developer/article/1932857
描述 : 进行音频采集，本地保存PCM音频文件，识别音频文件
参数 : 声道数：1；采样位数：16bit、LE格式；采样频率：16000Hz
编译 : $ gcc pcm_linux_1.c -o api -lasound -lcurl 
运行示例 : $ ./api record.pcm
***************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>

/*音频相关头文件*/
#include <alsa/asoundlib.h>
/*API调用相关头文件*/
#include <curl/curl.h>
#include "token.h" 
#include "asrmain.h"

/************************************
 PCM相关宏定义
 ************************************/
#define AUDIO_CHANNEL_SET 1				  // 1单声道   2立体声
#define AUDIO_RATE_SET 16000			  // 音频采样率,常用的采样频率: 44100Hz 、16000HZ、8000HZ、48000HZ、22050HZ

/************************************
 全局变量定义
 ************************************/
snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE; // 指定音频的格式,其他常用格式：SND_PCM_FORMAT_U24_LE、SND_PCM_FORMAT_U32_LE
snd_pcm_t *capture_handle = NULL; 	// 一个指向PCM设备的句柄
snd_pcm_hw_params_t *hw_params; 	// 此结构包含有关硬件的信息，可用于指定PCM流的配置
FILE *pcm_data_file = NULL;			// PCM音频文件指针
int buffer_frames = 1024;			// 周期大小（单位: 帧）
char *buffer;						// 指向应用程序缓冲区的指针
int pcm_flag_now = 0;				// PCM设备当前状态
int pcm_flag_old = 0;				// PCM设备前一次状态
int start_flag = 0;					// 程序运行标志位

/************************************
 函数声明
 ************************************/
void *read_tfn(void *arg);		// 用户输入检测线程
void snd_pcm_init(void);		// PCM设备初始化

void exit_sighandler(int sig)
{
	/*释放数据缓冲区*/
	free(buffer);

	/*关闭音频采集卡硬件*/
	snd_pcm_close(capture_handle);

	/*关闭文件流*/
	fclose(pcm_data_file);

	/*正常退出程序*/
	printf("程序已终止!\n");
	exit(0);
}

int main(int argc, char *argv[])
{
	int i;
	int ret;
	struct sigaction act;

	/*判断是否传入参数*/
	if(argc!=2)
	{
		printf("Usage: ./可执行程序 保存音频文件名\n");
		return 0;
	}

	/*API相关初始化*/
	curl_global_init(CURL_GLOBAL_ALL);
	struct asr_config config;
    char token[MAX_TOKEN_SIZE];
	RETURN_CODE res = fill_config(&config,argv[1]);
	if(res == RETURN_OK) {
        /*获取token*/
        res = speech_get_token(config.api_key, config.secret_key, config.scope, token);
    }
	if (res != RETURN_OK) {
        fprintf(stderr, "ERROR: %s, %d", g_demo_error_msg, res);
    }

	/*注册信号捕获退出接口*/
	act.sa_handler = exit_sighandler;//指定信号捕捉后的处理函数名（即注册函数）
	act.sa_flags = 0;//通常设置为0，表使用默认属性
	sigemptyset(&act.sa_mask);//将屏蔽的信号集合设为空
	sigaction(2, &act, NULL); //Ctrl+c→2 SIGINT（终止/中断） 

	/*初始化PCM Capture设备*/
	snd_pcm_init();

	/*创建一个保存PCM数据的文件*/
	if ((pcm_data_file = fopen(argv[1], "wb")) == NULL)
	{
		printf("无法创建%s音频文件.\n", argv[1]);
		exit(1);
	}
	printf("用于录制的音频文件已打开.\n");

	/*配置一个数据缓冲区用来缓冲数据*/
	//snd_pcm_format_width(format) 获取样本格式对应的大小(单位是:bit)
	int frame_byte = snd_pcm_format_width(format) * AUDIO_CHANNEL_SET / 8;//一帧大小为2字节
	buffer = malloc(buffer_frames * frame_byte); //一周期为2048字节
	printf("缓冲区分配成功.\n");

	/*创建子线程读取用户输入*/
	pthread_t tid;
	ret = pthread_create(&tid, NULL, read_tfn, NULL);
	if (ret != 0)
		perror("pthread_create failed");

	/*开始采集音频pcm数据*/
	printf("请输入字母A点击回车采集音频数据!(CTRL+C退出)\n");
	while (1)
	{
		/*判断PCM状态是否更新*/
		if(pcm_flag_now != pcm_flag_old)
		{
			/*视当前状态为旧状态*/
			pcm_flag_old = pcm_flag_now;

			/*若PCM为准备状态*/
			if(pcm_flag_now == 1)
				printf("开始采集音频数据...(输入字母B点击回车结束)\n");
			/*若PCM为停止状态*/
			else
			{
				printf("采集结束!\n");

				/*调用API进行识别*/
				run_asr(&config, token);

				printf("请输入字母A点击回车采集音频数据!(CTRL+C退出)\n");
			}
		}

		/*若PCM为准备状态*/
		if(pcm_flag_now == 1)
		{
			/*从声卡设备读取一周期音频数据:1024帧 2048字节*/
			ret = snd_pcm_readi(capture_handle, buffer, buffer_frames);
			if(0 > ret)
			{
				printf("从音频接口读取失败(%s)\n", snd_strerror(ret));
				exit(1);
			}
			
			/*写数据到文件，写入数据大小：音频的每帧数据大小2个字节*ret（从声卡设备实际读到的帧数）*/
			fwrite(buffer, ret, frame_byte, pcm_data_file);
		}
	}
	return 0;
}

void *read_tfn(void *arg)
{
	char buf[1024];
    int ret;

    while(1)
    {
        memset(buf, 0, sizeof(buf));//清空buf数组
        ret = read(STDIN_FILENO, buf, sizeof(buf));//阻塞等待用户输入
		/*判断输入长度是否正确*/
        if(ret == 2)// A\n or B\n
        {
			/*用户输入的是字符A*/
			if(strcmp(buf, "A\n") == 0)
			{
				snd_pcm_prepare(capture_handle); //使设备恢复进入准备状态
				fseek(pcm_data_file, 0,SEEK_SET);//文件读写指针偏移 使文件从头开始写 等于重新录制音频进行识别
				pcm_flag_now = 1;//PCM状态标志位置1
			}
			/*用户输入的是字符B*/
            else if(strcmp(buf, "B\n") == 0)
			{
				pcm_flag_now = 0;//PCM状态标志位清零
				sleep(1);//sleep 1
				snd_pcm_drop(capture_handle);//停止PCM设备
			}
        }
    }
}

void snd_pcm_init(void)
{
	int err;
	unsigned int rate = AUDIO_RATE_SET;
	
	/*打开音频采集卡硬件，并判断硬件是否打开成功，若打开失败则打印出错误提示*/
	// SND_PCM_STREAM_PLAYBACK 输出流
	// SND_PCM_STREAM_CAPTURE  输入流
	if ((err = snd_pcm_open(&capture_handle, "hw:0", SND_PCM_STREAM_CAPTURE, 0)) < 0)
	{
		printf("无法打开音频设备: %s (%s)\n","hw:0", snd_strerror(err));
		exit(1);
	}
	if(!start_flag) printf("音频接口打开成功.\n");

	/*分配硬件参数结构对象，并判断是否分配成功*/
	if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0)
	{
		printf("无法分配硬件参数结构 (%s)\n", snd_strerror(err));
		exit(1);
	}
	if(!start_flag) printf("硬件参数结构已分配成功.\n");

	/*按照默认设置对硬件对象进行设置，并判断是否设置成功*/
	if ((err = snd_pcm_hw_params_any(capture_handle, hw_params)) < 0)
	{
		printf("无法初始化硬件参数结构 (%s)\n", snd_strerror(err));
		exit(1);
	}
	if(!start_flag) printf("硬件参数结构初始化成功.\n");

	/*
		设置数据为交叉模式，并判断是否设置成功
		interleaved/non interleaved:交叉/非交叉模式。
		表示在多声道数据传输的过程中是采样交叉的模式还是非交叉的模式。
		对多声道数据，如果采样交叉模式，使用一块buffer即可，其中各声道的数据交叉传输；
		如果使用非交叉模式，需要为各声道分别分配一个buffer，各声道数据分别传输。
	*/
	if ((err = snd_pcm_hw_params_set_access(capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
	{
		printf("无法设置访问类型(%s)\n", snd_strerror(err));
		exit(1);
	}
	if(!start_flag) printf("访问类型设置成功.\n");

	/*设置数据编码格式，并判断是否设置成功*/
	if ((err = snd_pcm_hw_params_set_format(capture_handle, hw_params, format)) < 0)
	{
		printf("无法设置格式 (%s)\n", snd_strerror(err));
		exit(1);
	}
	if(!start_flag) printf("PCM数据格式设置成功.\n");

	/*设置采样频率，并判断是否设置成功*/
	if ((err = snd_pcm_hw_params_set_rate_near(capture_handle, hw_params, &rate, 0)) < 0)
	{
		printf("无法设置采样率(%s)\n", snd_strerror(err));
		exit(1);
	}
	if(!start_flag) printf("采样率设置成功\n");

	/*设置声道，并判断是否设置成功*/
	if ((err = snd_pcm_hw_params_set_channels(capture_handle, hw_params, AUDIO_CHANNEL_SET)) < 0)
	{
		printf("无法设置声道数(%s)\n", snd_strerror(err));
		exit(1);
	}
	if(!start_flag) printf("声道数设置成功.\n");

	/*将配置写入驱动程序中，并判断是否配置成功*/
	if ((err = snd_pcm_hw_params(capture_handle, hw_params)) < 0)
	{
		printf("无法向驱动程序设置参数(%s)\n", snd_strerror(err));
		exit(1);
	}
	if(!start_flag) printf("参数设置成功.\n");

	/*使采集卡处于空闲状态*/
	snd_pcm_hw_params_free(hw_params);

	/*准备音频接口,并判断是否准备好*/
	if ((err = snd_pcm_prepare(capture_handle)) < 0)
	{
		printf("无法使用音频接口 (%s)\n", snd_strerror(err));
		exit(1);
	}
	if(!start_flag) printf("音频接口已准备好.\n");

	start_flag = 1;
}
