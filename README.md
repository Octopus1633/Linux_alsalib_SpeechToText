# Linux_alsalib_SpeechToText

---
# 前言
`本篇分享：`

Linux应用编程之音频编程，使用户可以录制一段音频并进行识别(语音转文字)

`环境介绍：`

系统：Ubuntu 22.04
声卡：电脑自带

---

# Linux语音识别

实现目标 ：用户可以录制一段音频并进行识别(语音转文字)
知识点 : `C语言`、`文件IO`、`信号`、`多线程`、`alsa-lib 库`、`libcurl库`、`API调用`

## alsa-lib简介：

`alsa-lib`是一套 Linux 应用层的 C 语言函数库，为音频应用程序开发提供了一套统一、标准的接口，应用程序只需调用这一套 API 即可完成对底层声卡设备的操控，譬如播放与录音。
用户空间的`alsa-lib`对应用程序提供了统一的API 接口，这样可以隐藏驱动层的实现细节，简化了应用 程序的实现难度、无需应用程序开发人员直接去读写音频设备节点。所以，主要就是学习`alsa-lib`库函数的使用、如何基于`alsa-lib`库函数开发音频应用程序。 
`alsa-lib`官方说明文档：https://www.alsa-project.org/alsa-doc/alsa-lib/

## 安装alsa-lib库：

在ubuntu系统上安装alsa-lib库方法：

```
sudo apt-get install libasound2-dev
```
## 安装libcurl库：

在ubuntu系统上安装alsa-lib库方法：

```
sudo apt-get install libcurl4-openssl-dev
```

## API调用

该程序使用的是百度语音识别API

![在这里插入图片描述](https://img-blog.csdnimg.cn/3dcc556dda2d47c39913c9e6795b6a10.png#pic_center)


注册后领取免费额度及创建中文普通话应用（创建前先领取免费额度（180 天免费额度，可调用约 5 万次左右） ）

![在这里插入图片描述](https://img-blog.csdnimg.cn/cc74fa8953f2493fa03de966775aefcb.png#pic_center)




创建好应用后，可以得到`API key`和`Secret Key`（填写到程序中的相应位置）

![在这里插入图片描述](https://img-blog.csdnimg.cn/dac374b25f0f4d2f8fb3c43899c1a9eb.png#pic_center)



调用API相关说明可在图中所示位置查阅，Demo代码中有多种语言的调用示例可以参考，使用C语言的话也可以直接在本项目程序上面直接更改(**项目源代码在最下方**)：

![在这里插入图片描述](https://img-blog.csdnimg.cn/40f97e3f67dd4a8899a6ee023c33fa7b.png#pic_center)


API相关c文件中 **==需要修改的有asrmain.c和相应的头文件==**：
asrmain.c的fill_config函数中(该函数我已修改，原本无file参数，根据实际情况使用)，需要修改的有：**==音频文件格式，API Key以及Secret Key==**：

```c
RETURN_CODE fill_config(struct asr_config *config,char *file) {
    // 填写网页上申请的appkey 如 g_api_key="g8eBUMSokVB1BHGmgxxxxxx"
    char api_key[] = "填写网页上申请的API key";
    // 填写网页上申请的APP SECRET 如 $secretKey="94dc99566550d87f8fa8ece112xxxxx"
    char secret_key[] = "填写网页上申请的Secret Key";
    // 需要识别的文件
    char *filename = NULL;
    filename = file;

    // 文件后缀仅支持 pcm/wav/amr 格式，极速版额外支持m4a 格式
    char format[] = "pcm";

    char *url = "http://vop.baidu.com/server_api";  // 可改为https

    //  1537 表示识别普通话，使用输入法模型。其它语种参见文档
    int dev_pid = 1537;

    char *scope = "audio_voice_assistant_get"; // # 有此scope表示有asr能力，没有请在网页里勾选，非常旧的应用可能没有
    …………
```

结合音频录制的程序使用的话，还需要删除示例中的`main函数`，`run函数`中的相关初始化以及API调用函数需要根据实际情况重新调整调用位置。本项目总体按照：**==获取token(在程序开始时获取一次即可，根据官网可知获取的token有效期为30天，重新获取token则之前获取的token失效)->调用API->得到返回结果->解析结果->反馈给用户==**。

## 录音

实现音频录制分为以下步骤：**==打开声卡设备->设置硬件参数->读写数据==**

### 相关概念

#### 样本长度（Sample） 

- 样本长度，样本是记录音频数据最基本的单元，样本长度就是采样位数，也称为位深度（Bit Depth、Sample Size、 Sample Width）。是指计算机在采集和播放声音文件时，所使用数字声音信号的二进制位数，或者说每个采样样本所包含的位数（计算机对每个通道采样量化时数字比特位数），通常有 8bit、16bit、24bit 等。

#### 声道数（channel）

- 分为`单声道(Mono)`和`双声道/立体声(Stereo)`。1 表示单声道、2 表示立体声。 

#### 帧（frame） 

- 帧记录了一个声音单元，其长度为样本长度与声道数的乘积，一段音频数据就是由若干帧组成的。把所有声道中的数据加在一起叫做一帧，对于单声道：`一帧 = 样本长度 * 1`；`双声道：一帧 = 样本长度 * 2`。
- 对于本程序中，样本长度为16bit 的单声道来说，一帧的大小等于：16 * 1 / 8 = 2 个字节。

#### 周期（period） 

- 周期是音频设备处理（读、写）数据的单位，换句话说，也就是音频设备读写数据的单位是周期，每一次读或写一个周期的数据，一个周期包含若干个帧；譬如周期的大小为 1024 帧，则表示音频设备进行一次读或写操作的数据量大小为 1024 帧。
- 一个周期其实就是两次硬件中断之间的帧数，音频设备每处理（读或写）完一个周期的数据就会产生一个中断，所以两个中断之间相差一个周期，即每一次读或写一个周期的数据。
- 对于本程序中，`一周期的大小 = 一周期帧数 * 一帧大小` =` 一周期帧数 * (样本长度 * 声道数 / 字节长度)` = 1024 * (16 * 1 / 8) = 2048个字节。

#### 采样率（Sample rate） 

- 也叫采样频率，是指`每秒钟采样次数`，该次数是对于帧而言。


### 打开声卡设备

函数：

```c
函数原型:
int snd_pcm_open(snd_pcm_t **pcmp, const char *name, snd_pcm_stream_t stream, int mode)
    
参数:
pcmp -- 表示一个 PCM 设备,snd_pcm_open 函数会打开参数 name 所指定的设备，实例化 snd_pcm_t 对象，并将对象的指针通过 pcmp 返回出来。
name -- 参数 name 指定 PCM 设备的名字。alsa-lib 库函数中使用逻辑设备名而不是设备文件名,命名方式为"hw:i,j"，i 表示声卡的卡号，j 则表示这块声卡上的设备号
SND_PCM_STREAM_CAPTURE:表示从设备采集音频数据。
stream --SND_PCM_STREAM_PLAYBACK表示播放,SND_PCM_STREAM_CAPTURE则表示采集。
mode -- 最后一个参数 mode 指定了 open 模式，通常情况下将其设置为0，表示默认打开模式，默认情况下使用阻塞方式打开设备；

作用:
打开音频采集卡硬件。
```

代码：

```c
/*打开音频采集卡硬件，并判断硬件是否打开成功，若打开失败则打印出错误提示*/
// SND_PCM_STREAM_PLAYBACK 输出流
// SND_PCM_STREAM_CAPTURE  输入流
if ((err = snd_pcm_open(&capture_handle, "hw:0", SND_PCM_STREAM_CAPTURE, 0)) < 0)
{
    printf("无法打开音频设备: %s (%s)\n","hw:0", snd_strerror(err));
    exit(1);
}
printf("音频接口打开成功.\n");
```

### 设置硬件参数

设置硬件参数再细分的话有：**==初始化硬件参数结构对象，设置访问类型，设置数据编码格式，设置采样频率，设置声道，加载配置好的硬件参数==** 。

#### 初始化硬件参数结构对象

函数：

```c
函数原型:
int snd_pcm_hw_params_malloc(snd_pcm_hw_params_t **hwparams);

参数:
hwparams -- snd_pcm_hw_params_t 对象。

作用:
实例化snd_pcm_hw_params_t对象。

函数原型:
int snd_pcm_hw_params_any(snd_pcm_t *pcm_handle,snd_pcm_hw_params_t *hwparams);

参数:
pcm_handle -- PCM 设备的句柄。
hwparams -- 传入 snd_pcm_hw_params_t 对象的指针。

作用:
调用该函数会使用 PCM 设备当前的配置参数去初始化 snd_pcm_hw_params_t 对象。
```

代码：

```c
/*对象声明*/
snd_pcm_hw_params_t *hw_params;

/*分配硬件参数结构对象，并判断是否分配成功*/
if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0)
{
    printf("无法分配硬件参数结构 (%s)\n", snd_strerror(err));
    exit(1);
}
printf("硬件参数结构已分配成功.\n");

/*按照默认设置对硬件对象进行设置，并判断是否设置成功*/
if ((err = snd_pcm_hw_params_any(capture_handle, hw_params)) < 0)
{
    printf("无法初始化硬件参数结构 (%s)\n", snd_strerror(err));
    exit(1);
}
printf("硬件参数结构初始化成功.\n");

参数:
hw_params:此结构包含有关硬件的信息，用于指定PCM流的配置
```

#### 设置访问类型

函数：

```c
函数原型:
int snd_pcm_hw_params_set_access(snd_pcm_t *pcm,
snd_pcm_hw_params_t * params,
snd_pcm_access_t access 
)

参数:
access -- 指定设备的访问类型，是一个 snd_pcm_access_t 类型常量，这是一个枚举类型。

作用:
调用 snd_pcm_hw_params_set_access 设置访问类型。
```

代码：

```c
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

参数:
SND_PCM_ACCESS_RW_INTERLEAVED:访问类型设置为交错访问模式，通过
snd_pcm_readi/snd_pcm_writei 对 PCM 设备进行读/写操作。
```

#### 设置数据编码格式

函数：

```c
函数原型:
int snd_pcm_hw_params_set_format(snd_pcm_t *pcm,
snd_pcm_hw_params_t *params,
snd_pcm_format_t format
)

参数:
format -- 指定数据格式，该参数是一个 snd_pcm_format_t 类型常量，这是一个枚举类型。

作用:
调用 snd_pcm_hw_params_set_format()函数设置 PCM 设备的数据格式。
```

代码：

```c
/*设置数据编码格式，并判断是否设置成功*/
if ((err = snd_pcm_hw_params_set_format(capture_handle, hw_params, format)) < 0)
{
    printf("无法设置格式 (%s)\n", snd_strerror(err));
    exit(1);
}
printf("PCM数据格式设置成功.\n");

参数:
format:样本长度，样本是记录音频数据最基本的单元，样本长度就是采样位数，也称为位深度。用的最多的格式是SND_PCM_FORMAT_S16_LE，有符号16位、小端模式。
```

#### 设置采样频率

函数：

```c
函数原型:
int snd_pcm_hw_params_set_rate(snd_pcm_t *pcm,
snd_pcm_hw_params_t *params,
unsigned int val,
int dir 
)

参数:
val -- 指定采样率大小，譬如 44100.
dir -- 用于控制方向，若 dir=-1，则实际采样率小于参数 val；dir=0 表示实际采样率等于参数 val；dir=1 表示实际采样率大于参数 val。

作用:
调用 snd_pcm_hw_params_set_rate 设置采样率大小。
```

代码：

```c
/*设置采样频率，并判断是否设置成功*/
if ((err = snd_pcm_hw_params_set_rate_near(capture_handle, hw_params, &rate, 0)) < 0)
{
    printf("无法设置采样率(%s)\n", snd_strerror(err));
    exit(1);
}
printf("采样率设置成功\n");

参数:
rate:采样频率，是指每秒钟采样次数，该次数是针对帧而言，譬如有44100、16000、8000，百度API调用推荐使用16000或8000
```

#### 设置声道

函数：

```c
函数原型:
int snd_pcm_hw_params_set_channels(snd_pcm_t *pcm,
snd_pcm_hw_params_t *params,
unsigned int val
)

参数:
val -- 指定声道数量，val=1 表示单声道，val=2 表示双声道，也就是立体声。

作用:
调用 snd_pcm_hw_params_set_channels()函数设置 PCM 设备的声道数。
```

代码：

```c
/*设置声道，并判断是否设置成功*/
if ((err = snd_pcm_hw_params_set_channels(capture_handle, hw_params, AUDIO_CHANNEL_SET)) < 0)
{
    printf("无法设置声道数(%s)\n", snd_strerror(err));
    exit(1);
}
printf("声道数设置成功.\n");

参数:
AUDIO_CHANNEL_SET:AUDIO_CHANNEL_SET为单声道，值为1。声道分为单声道(Mono)和双声道/立体声(Stereo)。1 表示单声道、2 表示立体声。
```

#### 加载硬件参数

函数：

```c
函数原型:
int snd_pcm_hw_params(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)

作用:
参数设置完成之后，最后调用 snd_pcm_hw_params()加载/安装配置、将配置参数写入硬件使其生效。
```

代码：

```c
/*将配置写入驱动程序中，并判断是否配置成功*/
if ((err = snd_pcm_hw_params(capture_handle, hw_params)) < 0)
{
    printf("无法向驱动程序设置参数(%s)\n", snd_strerror(err));
    exit(1);
}
printf("参数设置成功.\n");
```

### 使声卡设备处于准备状态

函数：

```c
函数原型:
int snd_pcm_prepare(snd_pcm_t *pcm)

作用:
调用 snd_pcm_prepare()函数会使得 PCM 设备处于 SND_PCM_STATE_PREPARED 状态,也就是处于一种准备好的状态。
```

代码：

```c
/*使采集卡处于空闲状态*/
snd_pcm_hw_params_free(hw_params);

/*准备音频接口,并判断是否准备好*/
if ((err = snd_pcm_prepare(capture_handle)) < 0)
{
    printf("无法使用音频接口 (%s)\n", snd_strerror(err));
    exit(1);
}
printf("音频接口已准备好.\n");
```

### 读/写数据

读写数据需要在代码中配置一周期大小的缓冲区：
(读:`声卡设备->缓冲区->文件`，写:`文件->缓冲区->声卡设备`)

函数：

```c
函数原型:
snd_pcm_sframes_t snd_pcm_writei(snd_pcm_t *pcm,
const void *buffer,
snd_pcm_uframes_t size
)

参数:
pcm -- PCM 设备的句柄。
buffer -- 应用程序的缓冲区，调用 snd_pcm_writei()将参数 buffer（应用程序的缓冲区）缓冲区中的数据写入到驱动层的播放环形缓冲区 buffer 中。
size -- 指定写入数据的大小，以帧为单位,通常情况下，每次调用 snd_pcm_writei()写入一个周期数据。

作用:
如果是PCM播放，则调用 snd_pcm_writei()函数向播放缓冲区 buffer中写入音频数据。

函数原型:
snd_pcm_sframes_t snd_pcm_readi(snd_pcm_t *pcm,
void *buffer,
snd_pcm_uframes_t size
)

参数:
buffer -- 应用程序的缓冲区，调用 snd_pcm_readi()函数，将从驱动层的录音环形缓冲区 buffer 中读取数据到参数 buffer 指定的缓冲区中（应用程序的缓冲区）。
size -- 指定读取数据的大小，以帧为单位；通常情况下，每次调用snd_pcm_readi()
读取一个周期数据。

作用:
如果是PCM录音，则调用 snd_pcm_readi()函数从录音缓冲区 buffer 中读取数据。
```

代码：

```c
/*配置一个数据缓冲区用来缓冲数据*/
//snd_pcm_format_width(format) 获取样本格式对应的大小(单位是:bit)
int frame_byte = snd_pcm_format_width(format) * AUDIO_CHANNEL_SET / 8;//一帧大小为2字节
buffer = malloc(buffer_frames * frame_byte); //一周期为2048字节
printf("缓冲区分配成功.\n");
```

录音从声卡设备读数据即可，读写操作代码如下：
(调用成功，返回实际读取/写入的帧数； 调用失败将返回一个负数错误码。 即使调用成功，实际读取/写入的帧数不一定等于参数 size所指定的帧数，仅当发生信号或XRUN时，返回的帧数可能会小于参数第三参数buffer_frames。 )

```c
/*从声卡设备读取一周期音频数据:2048字节*/
ret = snd_pcm_readi(capture_handle, buffer, buffer_frames);
if(0 > ret)
{
    printf("从音频接口读取失败(%s)\n", snd_strerror(ret));
    exit(1);
}

/*向声卡设备写一周期音频数据:2048字节*/
ret = snd_pcm_writei(capture_handle,buffer,buffer_frames);
if(0 > ret)
{
    printf("向音频接口写数据失败(%s)\n",snd_strerror(ret));
    exit(1);
}

参数:
buffer:程序数据缓冲区；
buffer_frames:1024，读/写数据的大小，以帧为单位，通常情况下，每次读/写一个周期数据。
```



## 文件IO

我们需要将录制的音频文件保存到本地，就需要用到文件IO相关知识，打开音频文件以及向音频文件写数据。

### 打开音频文件

函数：

```c
函数原型:
FILE *fopen(const char *filename, const char *mode)

参数:
filename -- 字符串，表示要打开的文件名称。
mode -- 字符串，表示文件的访问模式。

作用:
以指定的方式打开文件。
```

代码：

```c
/*创建一个保存PCM数据的文件*/
if ((pcm_data_file = fopen(argv[1], "wb")) == NULL)
{
    printf("无法创建%s音频文件.\n", argv[1]);
    exit(1);
}
printf("用于录制的音频文件已打开.\n");

参数:
argv[1]:程序执行时传递的参数,例./voice record.cpm，则该参数为"record.cpm"
"wb":只写打开或新建一个二进制文件，只允许写数据。
```

### 向音频文件写数据

函数:

```c
函数原型:
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)

参数:
ptr -- 这是指向要被写入的元素数组的指针。
size -- 这是要被写入的每个元素的大小，以字节为单位。
nmemb -- 这是元素的个数，每个元素的大小为 size 字节。
stream -- 这是指向 FILE 对象的指针，该 FILE 对象指定了一个输出流。

作用:
向指定文件写入指定大小数据。
```

代码:

```c
/*从声卡设备读取一周期音频数据:1024帧 2048字节*/
ret = snd_pcm_readi(capture_handle, buffer, buffer_frames);
if(0 > ret)
{
    printf("从音频接口读取失败(%s)\n", snd_strerror(ret));
    exit(1);
}

/*写数据到文件，写入数据大小：音频的每帧数据大小2个字节*ret(从声卡设备实际读到的帧数)*/
fwrite(buffer, ret, frame_byte, pcm_data_file);
```

## 信号

函数:

```c
函数原型:
int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)

参数:
signum -- 要捕获的信号类型。
act -- 传入参数，新的处理方式。
oldact -- 传出参数，旧的处理方式。

作用:
修改信号处理动作（通常在 Linux 用其来注册一个信号的捕捉函数）。

函数原型:
int sigemptyset(sigset_t *set);

参数:
set -- 需要清空的信号集。

作用:
该函数的作用是将信号集初始化为空。
```

代码:

```c
/*头文件*/
#include <signal.h>

/*注册信号捕获退出接口*/
struct sigaction act;
act.sa_handler = exit_sighandler;//指定信号捕捉后的处理函数名（即注册函数）
act.sa_flags = 0;//通常设置为0，表使用默认属性
sigemptyset(&act.sa_mask);//将屏蔽的信号集合设为空
sigaction(2, &act, NULL); //Ctrl+c→2 SIGINT（终止/中断） 

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
```

## 多线程

函数:

```c
函数原型:
int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg)

thread -- 传出参数，保存系统为我们分配好的线程 ID
attr -- 通常传 NULL，表示使用线程默认属性。若想使用具体属性也可以修改该参数。
start_routine -- 函数指针，指向线程主函数，该函数运行结束，则线程结束。
arg -- 线程主函数执行期间所使用的参数。

作用:
创建一个新线程。

函数原型:
int int truncate(const char *path,off_t length);

参数:
path -- 文件路径名。
length --  截断长度，若文件大小>length大小，额外的数据丢失。若文件大小<length大小，那么，这个文件将会被扩展，扩展的部分将补以null,也就是‘\0’。

作用:
截断或扩展文件。
```

代码:

```c
/*创建子线程读取用户输入*/
pthread_t tid;
ret = pthread_create(&tid, NULL, read_tfn, NULL);
if (ret != 0) perror("pthread_create failed");

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
				/**
				 * 1.使设备恢复进入准备状态
				 * 2.清空音频文件内容
				 * 3.PCM状态标志位置1
				*/
				snd_pcm_prepare(capture_handle); 
				truncate(pcm_file_name,1);
				pcm_flag_now = 1;
			}
			/*用户输入的是字符B*/
            else if(strcmp(buf, "B\n") == 0)
			{
				/**
				 * 1.PCM状态标志位清零
				 * 2.睡眠1s，等待主循环判断，避免出现停止PCM设备后仍继续读取PCM设备
				 * 3.停止PCM设备
				*/
				pcm_flag_now = 0;
				sleep(1);
				snd_pcm_drop(capture_handle);
			}
        }
    }
}
```

## 主循环

主循环内判断声卡设备状态是否改变(用户输入决定)，若当前声卡为运行状态则进行音频采集，若当前声卡为停止状态则调用API进行识别。

```c
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
```


## 实现效果及注意事项

### 实现效果

![在这里插入图片描述](https://img-blog.csdnimg.cn/a1976bf89f4f4c1bbece2c5d31c2b165.png#pic_center)


如图所示A与B之间为音频录制，音频录制完成后会调用百度语言API进行识别，并向用户展示识别的结果。之后用户可自行选择继续识别或退出程序。

### 注意事项

该程序在声卡不进行录音时是将声卡设备给停止工作了的，**==在停止声卡设备前需要加入一小段的延时等待==**，若不添加延时等待，可能会出现子线程使声卡设备停止的同时主线程在读取声卡设备，从而导致下图中出现的错误：
![在这里插入图片描述](https://img-blog.csdnimg.cn/723f7197a06a4aaba3980bc2d3ec8e58.png#pic_center)

