/*
 * =====================================================================================
 *
 *       Filename:  alsa.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年10月31日 17时44分56秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>

/**alsa play test
*ALSA用户空间编译,ALSA驱动的声卡在用户空间,不宜直接使用
*文件接口中,而应使用alsa-lib
*打开---->设置参数--->读写音频数据 ALSA全部使用alsa-lib中的API
*交叉编译
*export LD_LIBRARY_PATH=$PWD:$LD_LIBRARY_PATH
*arm-linux-gcc -o alsa_play alsa_play_test.c -L. -lasound
*需要交叉编译后的libasound.so库的支持
*
*/
#include <stdio.h>
#include <stdlib.h>
#include "alsa/asoundlib.h"

typedef struct {
	u_int   data_chunk; /*  'data' */
	u_int   data_length;    /*  samplecount (lenth of rest of block?) */
} scdata_t;

typedef struct {
	u_int       main_chunk; /* 'RIFF' */
	u_int       length;     /* Length of rest of file */
	u_int       chunk_type; /* 'WAVE' */
	u_int       sub_chunk;  /* 'fmt ' */
	u_int       sc_len;     /* length of sub_chunk */
	u_short     format;     /* should be 1 for PCM-code */
	u_short     modus;      /* 1 Mono, 2 Stereo */
	u_int       sample_fq;  /* frequence of sample */
	u_int       byte_p_sec;
	u_short     byte_p_spl; /* samplesize; 1 or 2 bytes */
	u_short     bit_p_spl;  /* 8, 12 or 16 bit */
	/*  
	* FIXME:
	* Apparently, two different formats exist.
	* One with a sub chunk length of 16 and another of length 18.
	* For the one with 18, there are two bytes here.  Don't know
	* what they mean.  For the other type (i.e. length 16) this
	* does not exist.
	*
	* To handle the above issue, some jugglery is done after we
	* read the header
	*      -Varada (Wed Apr 25 14:53:02 PDT 2007)
	*/
	u_char      pad[2];
	scdata_t    sc; 
} __attribute__((packed)) wavhead_t;

static void device_list(void)
{
	snd_ctl_t *handle;
	int card, err, dev, idx;
	snd_ctl_card_info_t *info;
	snd_pcm_info_t *pcminfo;
	snd_ctl_card_info_alloca(&info);
	snd_pcm_info_alloca(&pcminfo);
	snd_pcm_stream_t stream = SND_PCM_STREAM_PLAYBACK;

	card = -1;
	if (snd_card_next(&card) < 0 || card < 0) {
		error("no soundcards found...");
		return;
	}
	printf("**** List of %s Hardware Devices ****\n", snd_pcm_stream_name(stream));
	while (card >= 0) {
		char name[32] = {0};
		sprintf(name, "hw:%d", card);
		printf("hw:%d\n", card);
		if ((err = snd_ctl_open(&handle, name, 0)) < 0) {
			error("control open (%i): %s", card, snd_strerror(err));
			goto next_card;
		}
		if ((err = snd_ctl_card_info(handle, info)) < 0) {
			error("control hardware info (%i): %s", card, snd_strerror(err));
			snd_ctl_close(handle);
			goto next_card;
		}
		dev = -1;
		while (1) {
			unsigned int count;
			if (snd_ctl_pcm_next_device(handle, &dev)<0)
				error("snd_ctl_pcm_next_device");
			if (dev < 0)
				break;
			snd_pcm_info_set_device(pcminfo, dev);
			snd_pcm_info_set_subdevice(pcminfo, 0);
			snd_pcm_info_set_stream(pcminfo, stream);
			if ((err = snd_ctl_pcm_info(handle, pcminfo)) < 0) {
				if (err != -ENOENT)
					error("control digital audio info (%i): %s", card, snd_strerror(err));
				continue;
			}
			printf("card %d: %s [%s], device %d: %s [%s]\n",
				card, snd_ctl_card_info_get_id(info), snd_ctl_card_info_get_name(info),
				dev, snd_pcm_info_get_id(pcminfo), snd_pcm_info_get_name(pcminfo));
			count = snd_pcm_info_get_subdevices_count(pcminfo);
			printf("  Subdevices: %i/%i\n",
				snd_pcm_info_get_subdevices_avail(pcminfo), count);
			for (idx = 0; idx < (int)count; idx++) {
				snd_pcm_info_set_subdevice(pcminfo, idx);
				if ((err = snd_ctl_pcm_info(handle, pcminfo)) < 0) {
					error("control digital audio playback info (%i): %s", card, snd_strerror(err));
				} else {
					printf("  Subdevice #%i: %s\n",
						idx, snd_pcm_info_get_subdevice_name(pcminfo));
				}
			}
		}
		snd_ctl_close(handle);
	next_card:
		if (snd_card_next(&card) < 0) {
			error("snd_card_next");
			break;
		}
	}
}

int main(int argc, char *argv[])
{
	int i;
	int ret;
	int buf[128];
	unsigned int val;
    int dir=0;
	char *buffer;
	int size;
	wavhead_t hdr;
	snd_pcm_uframes_t frames;
    snd_pcm_uframes_t periodsize;
	snd_pcm_t *playback_handle;//PCM设备句柄pcm.h
	snd_pcm_hw_params_t *hw_params;//硬件信息和PCM流配置
	u_char      *tmp;

	//打印所有alsa设备列表
	//device_list();

	if (argc != 2) {
		printf("error: alsa_play_test [music name]\n");
		exit(1);
	}
	printf("play song %s by wolf\n", argv[1]);
	FILE *fp = fopen(argv[1], "rb");
    if(fp == NULL)
    return 0;

	fread((void *)&hdr, 1, sizeof(wavhead_t), fp);
#if __BYTE_ORDER == __BIG_ENDIAN
	hdr.length  = bswap_32(hdr.length);
	hdr.sc_len  = bswap_32(hdr.sc_len);
	hdr.format  = bswap_16(hdr.format);
	hdr.modus   = bswap_16(hdr.modus);
	hdr.sample_fq   = bswap_32(hdr.sample_fq);
	hdr.byte_p_sec  = bswap_32(hdr.byte_p_sec);
	hdr.byte_p_spl  = bswap_16(hdr.byte_p_spl);
	hdr.bit_p_spl   = bswap_16(hdr.bit_p_spl);
#endif
	printf("RIFF ,main_chunk:%s\n", (char *)&hdr.main_chunk);
	printf("Length of rest of file ,length:%u\n", hdr.length);
	printf("WAVE, chunk_type:%s\n", (char *)&hdr.chunk_type);
	printf("fmt, sub_chunk:%s\n", (char *)&hdr.sub_chunk);
	printf("length of sub_chunk, sc_len:%u\n", hdr.sc_len);
	printf("should be 1 for PCM-code, format:%u\n", hdr.format);
	printf("1 Mono 2 Stereo, modus:%u\n", hdr.modus);
	printf("frequence of sample, sample_fq:%u\n", hdr.sample_fq);
	printf("byte_p_sec:%u\n", hdr.byte_p_sec);
	printf("samplesize 1 or 2 bytes, byte_p_spl:%u\n", hdr.byte_p_spl);
	printf("8 or 12 or 16 bit, bit_p_spl:%u\n", hdr.bit_p_spl);
	printf("pad:%02x %02x\n", hdr.pad[0], hdr.pad[1]);
	if (hdr.sc_len == 16) {
		tmp = &hdr.pad[0];
		fseek(fp, 44, SEEK_SET);
	} else if (hdr.sc_len == 18) {
		tmp = &hdr.pad[2];
		fseek(fp, 46, SEEK_SET);
	} else {
		return EINVAL;
	}
	
	//1. 打开PCM,最后一个参数为0意味着标准配置
	//snd_pcm_open参数SND_PCM_STREAM_CAPTURE与snd_pcm_readi相对应,SND_PCM_STREAM_PLAYBACK与snd_pcm_writei相对应
	ret = snd_pcm_open(&playback_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
	if (ret < 0) {
		perror("snd_pcm_open");
		exit(1);
	}
	
	//2. 分配snd_pcm_hw_params_t结构体
	ret = snd_pcm_hw_params_malloc(&hw_params);
	if (ret < 0) {
		perror("snd_pcm_hw_params_malloc");
		exit(1);
	}
	//3. 初始化hw_params
	ret = snd_pcm_hw_params_any(playback_handle, hw_params);
	if (ret < 0) {
		perror("snd_pcm_hw_params_any");
		exit(1);
	}
	//4. 初始化访问权限
	ret = snd_pcm_hw_params_set_access(playback_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (ret < 0) {
		perror("snd_pcm_hw_params_set_access");
		exit(1);
	}
	//5. 初始化采样格式SND_PCM_FORMAT_U8,8位
	snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
	if (8 == hdr.bit_p_spl)
	{
		format = SND_PCM_FORMAT_S8;
	}
	else if (16 == hdr.bit_p_spl)
	{
		format = SND_PCM_FORMAT_S16_LE;
	}
	else if (24 == hdr.bit_p_spl)
	{
		format = SND_PCM_FORMAT_S24_LE;
	}
	else if (32 == hdr.bit_p_spl)
	{
		format = SND_PCM_FORMAT_S32_LE;
	}
	ret = snd_pcm_hw_params_set_format(playback_handle, hw_params, format);
	if (ret < 0) {
		perror("snd_pcm_hw_params_set_format");
		exit(1);
	}

	//6. 设置采样率,如果硬件不支持我们设置的采样率,将使用最接近的
	//val = 44100,有些录音采样频率固定为8KHz
	val = hdr.sample_fq;
	//val = 96000;//设置频率高,播放声音也特别快
	//val = 22050;//设置频率低,播放声音也特别慢,跟电池没电似的
	ret = snd_pcm_hw_params_set_rate_near(playback_handle, hw_params, &val, &dir);
	if (ret < 0) {
		perror("snd_pcm_hw_params_set_rate_near");
		exit(1);
	}
	//7. 设置通道数量
	int modus = hdr.modus;
	ret = snd_pcm_hw_params_set_channels(playback_handle, hw_params, modus);
	if (ret < 0) {
		perror("snd_pcm_hw_params_set_channels");
		exit(1);
	}

//	/* Set period size to 32 frames. */
//    frames = 16*2;//每个frame占的字节数
//    periodsize = 64; //多少个frames;//alsa中将缓冲区分成多个period区,每个period存放若干个frame
//    ret = snd_pcm_hw_params_set_buffer_size_near(playback_handle, hw_params, &frames);
//	printf("set buffer:%lu\n", frames);
//    if (ret < 0) 
//    {
//         printf("Unable to set buffer size %li : %s\n", frames * 2, snd_strerror(ret));
//         
//    }
//
//    ret = snd_pcm_hw_params_set_period_size_near(playback_handle, hw_params, &periodsize, 0);
//	printf("set period:%lu \n", periodsize);
//    if (ret < 0) 
//    {
//        printf("Unable to set period size %li : %s\n", periodsize,  snd_strerror(ret));
//    }
								  
	/*  Set period size to 32 frames. */
	frames = 32;
	periodsize = frames * 2;
	ret = snd_pcm_hw_params_set_buffer_size_near(playback_handle, hw_params, &periodsize);
	printf("set buffer size %li \n", periodsize);
	if (ret < 0)
	{
		printf("Unable to set buffer size %li : %s\n", frames * 2, snd_strerror(ret));
	}
	periodsize /= 2;
	ret = snd_pcm_hw_params_set_period_size_near(playback_handle, hw_params, &periodsize, 0);
	printf("set period size %li \n", periodsize);
	if (ret < 0)
	{
		printf("Unable to set period size %li : %s\n", periodsize,  snd_strerror(ret));
	}
	
	
	//8. 设置hw_params
	ret = snd_pcm_hw_params(playback_handle, hw_params);
	if (ret < 0) {
		perror("snd_pcm_hw_params");
		exit(1);
	}
	
	 /* Use a buffer large enough to hold one period */
//    snd_pcm_hw_params_get_period_size(hw_params, &periodsize, &dir);
//	printf("get period:%lu\n", periodsize);
//                                
//    size = periodsize * ((hdr.bit_p_spl * hdr.modus) / 8); /* 2 bytes/sample, 2 channels */
//    buffer = (char *) malloc(size);
//    fprintf(stderr,
//            "size = %d\n",
//            size);
 /* Use a buffer large enough to hold one period */
	snd_pcm_hw_params_get_period_size(hw_params, &frames, &dir);
	size = frames * 2; /* 2 bytes/sample, 2 channels */
	buffer = (char *) malloc(size);
	printf("size = %d\n", size);

    while (1) 
    {
        ret = fread(buffer, 1, size, fp);
        if(ret == 0) 
        {
              fprintf(stderr, "end of file on input\n");
              break;
        } 
        else if (ret != size) 
        {
        }
		//9. 写音频数据到PCM设备
        while(ret = snd_pcm_writei(playback_handle, buffer, ret/((hdr.bit_p_spl * hdr.modus)/8) ) < 0)
        {
            usleep(2000);
			if (ret == -EPIPE)
            {
                  /* EPIPE means underrun */
                  fprintf(stderr, "underrun occurred\n");
				  //完成硬件参数设置,使设备准备好
                  snd_pcm_prepare(playback_handle);
            } 
            else
            {
                  fprintf(stderr, "error from writei: %s\n", snd_strerror(ret));
				  break;
            }  
        }
        
    }		
	//10. 关闭PCM设备句柄
	snd_pcm_close(playback_handle);
	
	return 0;
}
