/*
** FAAD2 - Freeware Advanced Audio (AAC) Decoder including SBR decoding
** Copyright (C) 2003-2005 M. Bakker, Nero AG, http://www.nero.com
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**
** Any non-GPL usage of this software or parts of this software is strictly
** forbidden.
**
** The "appropriate copyright message" mentioned in section 2c of the GPLv2
** must read: "Code from FAAD2 is copyright (c) Nero AG, www.nero.com"
**
** Commercial non-GPL licensing of this software is possible.
** For more info contact Nero AG through Mpeg4AAClicense@nero.com.
**
** $Id: main.c,v 1.85 2008/09/22 17:55:09 menno Exp $
**/

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define off_t __int64
#else
#include <time.h>
#endif

#include "libaacdec.h"
#include <android/log.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#define  LOG_TAG    "audio_codec"
#define audio_codec_print(...) __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)

#define min(a,b) ( (a) < (b) ? (a) : (b) )

/* MicroSoft channel definitions */
#define SPEAKER_FRONT_LEFT             0x1
#define SPEAKER_FRONT_RIGHT            0x2
#define SPEAKER_FRONT_CENTER           0x4
#define SPEAKER_LOW_FREQUENCY          0x8
#define SPEAKER_BACK_LEFT              0x10
#define SPEAKER_BACK_RIGHT             0x20
#define SPEAKER_FRONT_LEFT_OF_CENTER   0x40
#define SPEAKER_FRONT_RIGHT_OF_CENTER  0x80
#define SPEAKER_BACK_CENTER            0x100
#define SPEAKER_SIDE_LEFT              0x200
#define SPEAKER_SIDE_RIGHT             0x400
#define SPEAKER_TOP_CENTER             0x800
#define SPEAKER_TOP_FRONT_LEFT         0x1000
#define SPEAKER_TOP_FRONT_CENTER       0x2000
#define SPEAKER_TOP_FRONT_RIGHT        0x4000
#define SPEAKER_TOP_BACK_LEFT          0x8000
#define SPEAKER_TOP_BACK_CENTER        0x10000
#define SPEAKER_TOP_BACK_RIGHT         0x20000
#define SPEAKER_RESERVED               0x80000000

#define DefaultReadSize 768*6 //read count from kernel audio buf one time
#define DefaultOutBufSize 1024*1024
#define MAX_CHANNELS 6 /* make this higher to support files with more channels */


typedef struct {
    long bytes_into_buffer;
    long bytes_consumed;
    long file_offset;
    unsigned char *buffer;
    int at_eof;
    FILE *infile;
} aac_buffer;
typedef struct FaadContext {
	NeAACDecHandle hDecoder;
	aac_buffer b;
	int init_flag;
	//int audio_out = -1;
	//int object_type = LC;  //set defaute type LC
	//int outputFormat = FAAD_FMT_16BIT;
//	int infoOnly;
}FaadContext;

static int quiet = 0;
FaadContext gFaadCxt;
static int header_type = 1;
static int gSampleRate=0;
static int gChannels=0;
static int adts_sample_rates[] = {96000,88200,64000,48000,44100,32000,24000,22050,16000,12000,11025,8000,7350,0,0,0};
/* globals */
char *progName;
static const char *file_ext[] =
{
    NULL,
    ".wav",
    ".aif",
    ".au",
    ".au",
    ".pcm",
    NULL
};

//=================func====================
/*
    param: 
    b:buffer need to fill
    in_buf:src buf
    inbuf_size: src buf size
    inbuf_consumed: inbuf_size-inbuf_consumed =left size
*/
static int fill_buffer(aac_buffer *b, char *in_buf, long *inbuf_size, long *inbuf_consumed)
{
    int nReadSize=0;
    int bread;
    int nDefaultAACBufSize=FAAD_MIN_STREAMSIZE*MAX_CHANNELS;//768*6
    //audio_codec_print("fill_buffer in! inbuf_size=%d, inbuf_consumed=%d, b->bytes_consumed = %d \n", 	*inbuf_size, *inbuf_consumed, b->bytes_consumed);
    if (b->bytes_consumed > 0)
    {
        if (b->bytes_into_buffer)
        {
            memmove((void*)b->buffer, (void*)(b->buffer + b->bytes_consumed),
                b->bytes_into_buffer*sizeof(unsigned char));
        }

        if (!b->at_eof)
        {
              if((inbuf_size-*inbuf_consumed)>=b->bytes_consumed)
              {
        		memcpy((void*)(b->buffer + b->bytes_into_buffer), in_buf + *inbuf_consumed, b->bytes_consumed);
        		*inbuf_size -= b->bytes_consumed;
        		*inbuf_consumed += b->bytes_consumed;
                      b->bytes_into_buffer += b->bytes_consumed;
                      nReadSize=b->bytes_consumed;
                }
                else //no enough data
                    return nReadSize;
        }
        b->bytes_consumed = 0;
        if (b->bytes_into_buffer > 3)
        {
            if (memcmp(b->buffer, "TAG", 3) == 0)
                b->bytes_into_buffer = 0;
        }
        if (b->bytes_into_buffer > 11)
        {
            if (memcmp(b->buffer, "LYRICSBEGIN", 11) == 0)
                b->bytes_into_buffer = 0;
        }
        if (b->bytes_into_buffer > 8)
        {
            if (memcmp(b->buffer, "APETAGEX", 8) == 0)
                b->bytes_into_buffer = 0;
        }
    }
    else if (b->bytes_consumed == -1)//first fill
    {
    	memcpy((void*)(b->buffer), in_buf, nDefaultAACBufSize);
    	*inbuf_size -= nDefaultAACBufSize;
    	//*inbuf_consumed = nDefaultAACBufSize;
    	*inbuf_consumed = 0;
        b->bytes_into_buffer = nDefaultAACBufSize;
    	 b->bytes_consumed = 0;
    	 nReadSize=nDefaultAACBufSize;
    }
    //audio_codec_print("fill_buffer! inbuf_size=%d, inbuf_consumed=%d, b->bytes_consumed = %d \n", *inbuf_size, *inbuf_consumed, b->bytes_consumed);

    return nReadSize;
}
static void advance_buffer(aac_buffer *b, int bytes)
{
    b->file_offset += bytes;
    b->bytes_consumed = bytes;
    b->bytes_into_buffer -= bytes;
	if (b->bytes_into_buffer < 0)
		b->bytes_into_buffer = 0;
}


static void faad_fprintf(FILE *stream, const char *fmt, ...)
{
    va_list ap;

    if (!quiet)
    {
        va_start(ap, fmt);

        vfprintf(stream, fmt, ap);

        va_end(ap);
    }
}

#if 0
static int adts_parse(aac_buffer *b, int *bitrate, float *length)
{
    int frames, frame_length;
    int t_framelength = 0;
    int samplerate;
    float frames_per_sec, bytes_per_frame;

    /* Read all frames to ensure correct time and bitrate */
    for (frames = 0; /* */; frames++)
    {
        fill_buffer(b);

        if (b->bytes_into_buffer > 7)
        {
            /* check syncword */
            if (!((b->buffer[0] == 0xFF)&&((b->buffer[1] & 0xF6) == 0xF0))){
				
				audio_codec_print("11111111111111111111 break\n\n");
                break;
			}

            if (frames == 0)
                samplerate = adts_sample_rates[(b->buffer[2]&0x3c)>>2];

            frame_length = ((((unsigned int)b->buffer[3] & 0x3)) << 11)
                | (((unsigned int)b->buffer[4]) << 3) | (b->buffer[5] >> 5);

            t_framelength += frame_length;

            if (frame_length > b->bytes_into_buffer){
				audio_codec_print("2222222222 break\n\n");
                break;
            }
            advance_buffer(b, frame_length);
        } else {
			audio_codec_print("33333333 break\n\n");
            break;
        }
    }

    frames_per_sec = (float)samplerate/1024.0f;
    if (frames != 0)
        bytes_per_frame = (float)t_framelength/(float)(frames*1000);
    else
        bytes_per_frame = 0;
    *bitrate = (int)(8. * bytes_per_frame * frames_per_sec + 0.5);
    if (frames_per_sec != 0)
        *length = (float)frames/frames_per_sec;
    else
        *length = 1;

    return 1;
}
#endif
static long aacChannelConfig2wavexChannelMask(NeAACDecFrameInfo *hInfo)
{
    if (hInfo->channels == 6 && hInfo->num_lfe_channels)
    {
        return SPEAKER_FRONT_LEFT + SPEAKER_FRONT_RIGHT +
            SPEAKER_FRONT_CENTER + SPEAKER_LOW_FREQUENCY +
            SPEAKER_BACK_LEFT + SPEAKER_BACK_RIGHT;
    } else {
        return 0;
    }
}

static char *position2string(int position)
{
    switch (position)
    {
    case FRONT_CHANNEL_CENTER: return "Center front";
    case FRONT_CHANNEL_LEFT:   return "Left front";
    case FRONT_CHANNEL_RIGHT:  return "Right front";
    case SIDE_CHANNEL_LEFT:    return "Left side";
    case SIDE_CHANNEL_RIGHT:   return "Right side";
    case BACK_CHANNEL_LEFT:    return "Left back";
    case BACK_CHANNEL_RIGHT:   return "Right back";
    case BACK_CHANNEL_CENTER:  return "Center back";
    case LFE_CHANNEL:          return "LFE";
    case UNKNOWN_CHANNEL:      return "Unknown";
    default: return "";
    }

    return "";
}

static void print_channel_info(NeAACDecFrameInfo *frameInfo)
{
    /* print some channel info */
    int i;
    long channelMask = aacChannelConfig2wavexChannelMask(frameInfo);

    faad_fprintf(stderr, "  ---------------------\n");
    if (frameInfo->num_lfe_channels > 0)
    {
        faad_fprintf(stderr, " | Config: %2d.%d Ch     |", frameInfo->channels-frameInfo->num_lfe_channels, frameInfo->num_lfe_channels);
    } else {
        faad_fprintf(stderr, " | Config: %2d Ch       |", frameInfo->channels);
    }
    if (channelMask)
        faad_fprintf(stderr, " WARNING: channels are reordered according to\n");
    else
        faad_fprintf(stderr, "\n");
    faad_fprintf(stderr, "  ---------------------");
    if (channelMask)
        faad_fprintf(stderr, "  MS defaults defined in WAVE_FORMAT_EXTENSIBLE\n");
    else
        faad_fprintf(stderr, "\n");
    faad_fprintf(stderr, " | Ch |    Position    |\n");
    faad_fprintf(stderr, "  ---------------------\n");
    for (i = 0; i < frameInfo->channels; i++)
    {
        faad_fprintf(stderr, " | %.2d | %-14s |\n", i, position2string((int)frameInfo->channel_position[i]));
    }
    faad_fprintf(stderr, "  ---------------------\n");
    faad_fprintf(stderr, "\n");
}

static int FindAdtsSRIndex(int sr)
{
    int i;

    for (i = 0; i < 16; i++)
    {
        if (sr == adts_sample_rates[i])
            return i;
    }
    return 16 - 1;
}

static unsigned char *MakeAdtsHeader(int *dataSize, NeAACDecFrameInfo *hInfo, int old_format)
{
    unsigned char *data;
    int profile = (hInfo->object_type - 1) & 0x3;
    int sr_index = ((hInfo->sbr == SBR_UPSAMPLED) || (hInfo->sbr == NO_SBR_UPSAMPLED)) ?
        FindAdtsSRIndex(hInfo->samplerate / 2) : FindAdtsSRIndex(hInfo->samplerate);
    int skip = (old_format) ? 8 : 7;
    int framesize = skip + hInfo->bytesconsumed;

    if (hInfo->header_type == ADTS)
        framesize -= skip;

    *dataSize = 7;

    data = malloc(*dataSize * sizeof(unsigned char));
    memset(data, 0, *dataSize * sizeof(unsigned char));

    data[0] += 0xFF; /* 8b: syncword */

    data[1] += 0xF0; /* 4b: syncword */
    /* 1b: mpeg id = 0 */
    /* 2b: layer = 0 */
    data[1] += 1; /* 1b: protection absent */

    data[2] += ((profile << 6) & 0xC0); /* 2b: profile */
    data[2] += ((sr_index << 2) & 0x3C); /* 4b: sampling_frequency_index */
    /* 1b: private = 0 */
    data[2] += ((hInfo->channels >> 2) & 0x1); /* 1b: channel_configuration */

    data[3] += ((hInfo->channels << 6) & 0xC0); /* 2b: channel_configuration */
    /* 1b: original */
    /* 1b: home */
    /* 1b: copyright_id */
    /* 1b: copyright_id_start */
    data[3] += ((framesize >> 11) & 0x3); /* 2b: aac_frame_length */

    data[4] += ((framesize >> 3) & 0xFF); /* 8b: aac_frame_length */

    data[5] += ((framesize << 5) & 0xE0); /* 3b: aac_frame_length */
    data[5] += ((0x7FF >> 6) & 0x1F); /* 5b: adts_buffer_fullness */

    data[6] += ((0x7FF << 2) & 0x3F); /* 6b: adts_buffer_fullness */
    /* 2b: num_raw_data_blocks */

    return data;
}


/*
static void usage(void)
{
    faad_fprintf(stdout, "\nUsage:\n");
    faad_fprintf(stdout, "%s [options] infile.aac\n", progName);
    faad_fprintf(stdout, "Options:\n");
    faad_fprintf(stdout, " -h    Shows this help screen.\n");
    faad_fprintf(stdout, " -i    Shows info about the input file.\n");
    faad_fprintf(stdout, " -a X  Write MPEG-4 AAC ADTS output file.\n");
    faad_fprintf(stdout, " -t    Assume old ADTS format.\n");
    faad_fprintf(stdout, " -o X  Set output filename.\n");
    faad_fprintf(stdout, " -f X  Set output format. Valid values for X are:\n");
    faad_fprintf(stdout, "        1:  Microsoft WAV format (default).\n");
    faad_fprintf(stdout, "        2:  RAW PCM data.\n");
    faad_fprintf(stdout, " -b X  Set output sample format. Valid values for X are:\n");
    faad_fprintf(stdout, "        1:  16 bit PCM data (default).\n");
    faad_fprintf(stdout, "        2:  24 bit PCM data.\n");
    faad_fprintf(stdout, "        3:  32 bit PCM data.\n");
    faad_fprintf(stdout, "        4:  32 bit floating point data.\n");
    faad_fprintf(stdout, "        5:  64 bit floating point data.\n");
    faad_fprintf(stdout, " -s X  Force the samplerate to X (for RAW files).\n");
    faad_fprintf(stdout, " -l X  Set object type. Supported object types:\n");
    faad_fprintf(stdout, "        1:  Main object type.\n");
    faad_fprintf(stdout, "        2:  LC (Low Complexity) object type.\n");
    faad_fprintf(stdout, "        4:  LTP (Long Term Prediction) object type.\n");
    faad_fprintf(stdout, "        23: LD (Low Delay) object type.\n");
    faad_fprintf(stdout, " -d    Down matrix 5.1 to 2 channels\n");
    faad_fprintf(stdout, " -w    Write output to stdio instead of a file.\n");
    faad_fprintf(stdout, " -g    Disable gapless decoding.\n");
    faad_fprintf(stdout, " -q    Quiet - suppresses status messages.\n");
    faad_fprintf(stdout, "Example:\n");
    faad_fprintf(stdout, "       %s infile.aac\n", progName);
    faad_fprintf(stdout, "       %s infile.mp4\n", progName);
    faad_fprintf(stdout, "       %s -o outfile.wav infile.aac\n", progName);
    faad_fprintf(stdout, "       %s -w infile.aac > outfile.wav\n", progName);
    faad_fprintf(stdout, "       %s -a outfile.aac infile.aac\n", progName);
    return;
}
*/
int AACFindSyncWord(unsigned char *buf, int nBytes)
{
	int i;
	for (i = 0; i < nBytes - 1; i++) {
		if ( ((buf[i+0] & 0xff) == 0xff) && ((buf[i+1] & 0xf6) == 0xf0) )
			return i;
	}
	return nBytes;
}
//read data form kernel audio buffer
//basic init
int audio_dec_init(audio_decoder_operations_t *adec_ops)
{
    memset(&gFaadCxt,0,sizeof(FaadContext));
    memset(&(gFaadCxt.b), 0, sizeof(aac_buffer));
    gFaadCxt.b.buffer=(unsigned char*)malloc(FAAD_MIN_STREAMSIZE*MAX_CHANNELS);
    if (!(gFaadCxt.b.buffer))
    {
            audio_codec_print("===Init Err: buffer malloc failed! \n");
            return -1;
    }
    memset((gFaadCxt.b).buffer, 0, FAAD_MIN_STREAMSIZE*MAX_CHANNELS);
    (gFaadCxt.b).bytes_consumed = -1; //init
    adec_ops->nInBufSize=DefaultReadSize;
    adec_ops->nOutBufSize=DefaultOutBufSize;
    gFaadCxt.init_flag = 0;
    header_type=1;//default adts
    return 0;
}
//real init aac relative
#define SKIP_BYTES 16*4
int audio_decoder_init(audio_decoder_operations_t *adec_ops, char *outbuf, int *outlen, char *inbuf, int inlen, long *inbuf_consumed)
{
    unsigned long samplerate;
    unsigned char channels;
    int bread;
    int tagsize;
    int pos = 0;
    NeAACDecConfigurationPtr config;

    char *in_buf;
    long inbuf_size;
    //long inbuf_consumed;
    in_buf = inbuf;
    inbuf_size = inlen;
    int nReadLen=0;
    fill_buffer(&(gFaadCxt.b), in_buf, &inbuf_size, inbuf_consumed);
    if (gFaadCxt.b.bytes_into_buffer != FAAD_MIN_STREAMSIZE*MAX_CHANNELS)
    {
        audio_codec_print("==init err : not read enough data");
        return -1;
    }

    if ((gFaadCxt.b.buffer[0] == 'A')&&(gFaadCxt.b.buffer[1] == 'D')&&(gFaadCxt.b.buffer[2] == 'I')&&(gFaadCxt.b.buffer[3] == 'F')){
    	header_type = 0;
    }

    tagsize = 0;
    if (!memcmp(gFaadCxt.b.buffer, "ID3", 3))
    {
    	tagsize = (gFaadCxt.b.buffer[6] << 21) | (gFaadCxt.b.buffer[7] << 14) |
    		(gFaadCxt.b.buffer[8] << 7) | (gFaadCxt.b.buffer[9] <<0);
    	tagsize += 10;
    	advance_buffer(&(gFaadCxt.b), tagsize);
    	fill_buffer(&(gFaadCxt.b), in_buf, &inbuf_size, inbuf_consumed);
    }

retry:
    gFaadCxt.hDecoder = NeAACDecOpen();
    config = NeAACDecGetCurrentConfiguration(gFaadCxt.hDecoder);
    config->defObjectType = LC;
    config->outputFormat = FAAD_FMT_16BIT;
    config->downMatrix = 0x01;
    config->useOldADTSFormat = 0;
    //config->dontUpSampleImplicitSBR = 1;
    NeAACDecSetConfiguration(gFaadCxt.hDecoder, config);
    if ((bread = NeAACDecInit(gFaadCxt.hDecoder, gFaadCxt.b.buffer,gFaadCxt.b.bytes_into_buffer, &samplerate, &channels)) < 0)
    {
        /* If some error initializing occured, skip the file */
        advance_buffer(&(gFaadCxt.b), SKIP_BYTES);
        nReadLen=fill_buffer(&(gFaadCxt.b), in_buf, &inbuf_size, inbuf_consumed);
        //NeAACDecClose(gFaadCxt.hDecoder);
        audio_codec_print("==ERR:Init failed");
        if(nReadLen)//have enough data
            goto retry;
        return -1;
    }
    advance_buffer(&(gFaadCxt.b), bread);
    fill_buffer(&(gFaadCxt.b), in_buf, &inbuf_size, inbuf_consumed);
    NeAACDecStruct* hDecoder = (NeAACDecStruct*)(gFaadCxt.hDecoder);
    gFaadCxt.init_flag=1;
    //audio_codec_print("====Init OK consumed:%ld byte used :%d real used:%d  sample:%d channel:%d  ",*inbuf_consumed,bread,gFaadCxt.b.bytes_consumed,samplerate,channels);
    audio_codec_print("====Init OK adif_present :%d adts_present:%d latm_present:%d  ",hDecoder->adif_header_present,hDecoder->adts_header_present,hDecoder->latm_header_present);
    return 0;
}


int audio_dec_decode(audio_decoder_operations_t *adec_ops, char *outbuf, int *outlen, char *inbuf, int inlen)
{
    int tagsize;
    unsigned long samplerate;
    unsigned char channels;
    void *sample_buffer;
	sample_buffer = NULL;
    
    unsigned char *adtsData;
    int adtsDataSize;
    NeAACDecFrameInfo frameInfo;
    NeAACDecConfigurationPtr config;

    char percents[200];
    int percent, old_percent = -1;
    int bread;
    int bitrate = 0;
    float length = 0;
    int outmaxlen = 0;

    char *in_buf;
    long inbuf_size;
    long inbuf_consumed;
    in_buf = inbuf;
    inbuf_size = inlen;
    if (inbuf_consumed > 0)
    	inbuf_consumed = 0;
    outmaxlen = (*outlen);
    (*outlen) = 0;
    //memset(&g_frameInfo,0,sizeof(NeAACDecFrameInfo));
    (gFaadCxt.b).bytes_into_buffer=0;//reset param
    (gFaadCxt.b).bytes_consumed = -1;
//#if 0
	if(inbuf_size < FAAD_MIN_STREAMSIZE*MAX_CHANNELS){
			audio_codec_print("===data not enough===\n");
			return 0;
	}
//#endif
    if (!gFaadCxt.init_flag)
    {
        int ret = audio_decoder_init(adec_ops,outbuf,outlen,inbuf,inlen,&inbuf_consumed);
        if(ret==-1)
        {
            audio_codec_print(" ====Init failed,Need More data");
            return 0;
        }
		
		return inbuf_consumed;
    }
	else{
   			 fill_buffer(&(gFaadCxt.b), in_buf, &inbuf_size, &inbuf_consumed);
			 
		if ((gFaadCxt.b).bytes_into_buffer != 0){
        	sample_buffer = NeAACDecDecode(gFaadCxt.hDecoder, &frameInfo, (gFaadCxt.b).buffer, (gFaadCxt.b).bytes_into_buffer);
        	gSampleRate=frameInfo.samplerate;
        	gChannels=frameInfo.channels;
        	advance_buffer(&(gFaadCxt.b), frameInfo.bytesconsumed);
        	if ((frameInfo.error == 0) && (frameInfo.samples > 0) )
        	{
				memcpy(outbuf+(*outlen), sample_buffer, 2*frameInfo.samples);
				*outlen+=2*frameInfo.samples;
			}
			if (frameInfo.error > 0)//failed seek to the head
			{
			   audio_codec_print( "Error: %s\n", NeAACDecGetErrorMessage(frameInfo.error));
				
			}// while (sample_buffer != NULL);
			audio_codec_print("===frameInfo.bytesconsumed=%d\n",frameInfo.bytesconsumed);
			return frameInfo.bytesconsumed;
		}else
			return 0;

	}
	
}
#if 0
int audio_dec_decode2(audio_decoder_operations_t *adec_ops, char *outbuf, int *outlen, char *inbuf, int inlen)
{
    int tagsize;
    unsigned long samplerate;
    unsigned char channels;
    void *sample_buffer;
    //NeAACDecFrameInfo frameInfo;

    //audio_file *aufile;

    //FILE *adtsFile;
    unsigned char *adtsData;
    int adtsDataSize;

    NeAACDecConfigurationPtr config;

    char percents[200];
    int percent, old_percent = -1;
    int bread;
    int header_type = 0;
    int bitrate = 0;
    float length = 0;
    int outmaxlen = 0;

    char *in_buf;
    long inbuf_size;
    long inbuf_consumed;
    in_buf = inbuf;
    inbuf_size = inlen;
    if (inbuf_consumed > 0)
    	inbuf_consumed = 0;
    outmaxlen = (*outlen);
    (*outlen) = 0;

    if (gFaadCxt.init_flag)
    	goto START_DECODER;

    fill_buffer(&(gFaadCxt.b), in_buf, &inbuf_size, &inbuf_consumed);
#if 0
        int nSeekNum = AACFindSyncWord(gFaadCxt.b.buffer, gFaadCxt.b.bytes_into_buffer);
	advance_buffer(&gFaadCxt.b, nSeekNum);
	fill_buffer(&(gFaadCxt.b), in_buf, &inbuf_size, &inbuf_consumed);
#endif
    tagsize = 0;
#if 0	
    if (!memcmp(b.buffer, "ID3", 3))
    {
        /* high bit is not used */
        tagsize = (b.buffer[6] << 21) | (b.buffer[7] << 14) |
            (b.buffer[8] <<  7) | (b.buffer[9] <<  0);

        tagsize += 10;
		
		faad_fprintf(stderr, " tagsize = %d\n", tagsize);
        advance_buffer(&b, tagsize);
        fill_buffer(&b);
    }
#endif	
//    audio_codec_print("44444444444444444444444\n\n");

    gFaadCxt.hDecoder = NeAACDecOpen();

    /* Set the default object type and samplerate */
    /* This is useful for RAW AAC files */
    config = NeAACDecGetCurrentConfiguration(gFaadCxt.hDecoder);
#if 0 //def_srate == 0
    if (def_srate)
        config->defSampleRate = def_srate;
#endif
    config->defObjectType = LC;
    config->outputFormat = FAAD_FMT_16BIT;
    config->downMatrix = 0; //downMatrix;
    config->useOldADTSFormat = 0;//old_format;
    //config->dontUpSampleImplicitSBR = 1;
    NeAACDecSetConfiguration(gFaadCxt.hDecoder, config);

    /* get AAC infos for printing */
    header_type = 0;
    if (((gFaadCxt.b).buffer[0] == 0xFF) && (((gFaadCxt.b).buffer[1] & 0xF6) == 0xF0))
    {
    
		faad_fprintf(stderr, "((b.buffer[0] == 0xFF) && ((b.buffer[1] & 0xF6) == 0xF0)), tagsize = %d\n", tagsize);
               //adts_parse(&b, &bitrate, &length);
		
		//refill_buffer(&b);
		
		//faad_fprintf(stderr, "refill buffer\n");
#if 0
		int filelen = ftell(b.infile);
		faad_fprintf(stderr, "filelen = %d\n", filelen);

        fseek(b.infile, tagsize, SEEK_SET);

        bread = fread(b.buffer, 1, FAAD_MIN_STREAMSIZE*MAX_CHANNELS, b.infile);
        if (bread != FAAD_MIN_STREAMSIZE*MAX_CHANNELS)
            b.at_eof = 1;
        else
            b.at_eof = 0;
        b.bytes_into_buffer = bread;
        b.bytes_consumed = 0;
        b.file_offset = tagsize;
#endif
		faad_fprintf(stderr, "b.bytes_into_buffer = %d, b.file_offset = %d\n", (gFaadCxt.b).bytes_into_buffer, (gFaadCxt.b).file_offset);
		faad_fprintf(stderr, "bitrate = %d, length = %d\n", bitrate, length);
		bitrate = 73;
		length = 409872;

        header_type = 1;
    } else if (memcmp((gFaadCxt.b).buffer, "ADIF", 4) == 0) {
        int skip_size = ((gFaadCxt.b).buffer[4] & 0x80) ? 9 : 0;
		
		faad_fprintf(stderr, "adif \n");
        bitrate = ((unsigned int)((gFaadCxt.b).buffer[4 + skip_size] & 0x0F)<<19) |
            ((unsigned int)(gFaadCxt.b).buffer[5 + skip_size]<<11) |
            ((unsigned int)(gFaadCxt.b).buffer[6 + skip_size]<<3) |
            ((unsigned int)(gFaadCxt.b).buffer[7 + skip_size] & 0xE0);

#if FILELEN	
        length = (float)fileread;
        if (length != 0)
        {
            length = ((float)length*8.f)/((float)bitrate) + 0.5f;
        }
#endif
        bitrate = (int)((float)bitrate/1000.0f + 0.5f);

        header_type = 2;
    }

//    audio_codec_print("555555555555555555555555555\n\n");

    fill_buffer(&(gFaadCxt.b), in_buf, &inbuf_size, &inbuf_consumed);
    if ((bread = NeAACDecInit(gFaadCxt.hDecoder, (gFaadCxt.b).buffer,
        (gFaadCxt.b).bytes_into_buffer, &samplerate, &channels)) < 0)
    {
    
    audio_codec_print("bread = %d\n\n", bread);
#if 0    
        /* If some error initializing occured, skip the file */
        faad_fprintf(stderr, "Error initializing decoder library.\n");
        if (b.buffer)
            free(b.buffer);
        NeAACDecClose(hDecoder);
        fclose(b.infile);
        return 1;
#endif
    }
    advance_buffer(&(gFaadCxt.b), bread);
    fill_buffer(&(gFaadCxt.b), in_buf, &inbuf_size, &inbuf_consumed);

    /* print AAC file info */
#if FILELEN	
	
    switch (header_type)
    {
    case 0:
        faad_fprintf(stderr, "RAW\n\n");
        break;
    case 1:
        faad_fprintf(stderr, "ADTS, %.3f sec, %d kbps, %d Hz\n\n",
            length, bitrate, samplerate);
        break;
    case 2:
        faad_fprintf(stderr, "ADIF, %.3f sec, %d kbps, %d Hz\n\n",
            length, bitrate, samplerate);
        break;
    }
#endif
#if 0
    if (gFaadCxt.infoOnly)
    {
        NeAACDecClose(gFaadCxt.hDecoder);
        fclose(gFaadCxt.b.infile);
        if (gFaadCxt.b.buffer)
            free(gFaadCxt.b.buffer);
        return 0;
    }
#endif
 //   audio_codec_print("66666666666666666666666666666\n\n");
	gFaadCxt.init_flag = 1;
START_DECODER:
//    do
    {
            /* fill buffer */
		if ((inbuf_size < FAAD_MIN_STREAMSIZE*MAX_CHANNELS)||(outmaxlen < (*outlen + 4096)))
			return inbuf_consumed;
        fill_buffer(&(gFaadCxt.b), in_buf, &inbuf_size, &inbuf_consumed);

        if ((gFaadCxt.b).bytes_into_buffer == 0)
            sample_buffer = NULL; /* to make sure it stops now */
		
        sample_buffer = NeAACDecDecode(gFaadCxt.hDecoder, &g_frameInfo, (gFaadCxt.b).buffer, (gFaadCxt.b).bytes_into_buffer);

//		audio_codec_print("77777777777777777777, frameInfo.bytesconsumed = %d, b.bytes_into_buffer = %d\n\n", frameInfo.bytesconsumed, (gFaadCxt.b).bytes_into_buffer);


        /* update buffer indices */
        advance_buffer(&(gFaadCxt.b), g_frameInfo.bytesconsumed);
        fill_buffer(&(gFaadCxt.b), in_buf, &inbuf_size, &inbuf_consumed);
        if (g_frameInfo.error > 0)
        {
            faad_fprintf(stderr, "Error: %s\n", NeAACDecGetErrorMessage(g_frameInfo.error));
        }


#if FILELEN	

        percent = min((int)((gFaadCxt.b).file_offset*100)/fileread, 100);
        if (percent > old_percent)
        {
            old_percent = percent;
            faad_fprintf(stderr, "%s\r", percents);
#ifdef _WIN32
            SetConsoleTitle(percents);
#endif
        }
#endif
        if ((g_frameInfo.error == 0) && (g_frameInfo.samples > 0) )
        {
//			audio_codec_print("888888888888888888888, frameInfo.samples = %d\n\n", frameInfo.samples);

//			if (audio_out >= 0) {
				memcpy(outbuf+(*outlen), sample_buffer, 2*g_frameInfo.samples);
				*outlen+=2*g_frameInfo.samples;
				//write(audio_out, sample_buffer, 2*frameInfo.samples);
//			}
		}
	#if 1
	if (g_frameInfo.error > 0)//failed seek to the head
	{
        	advance_buffer(&gFaadCxt.b, 1);
        	fill_buffer(&(gFaadCxt.b), in_buf, &inbuf_size, &inbuf_consumed);
		//fill_buffer(&b);
		if(0)
		{
			int num;
			num = AACFindSyncWord(gFaadCxt.b.buffer, gFaadCxt.b.bytes_into_buffer);
			advance_buffer(&gFaadCxt.b, num);
			fill_buffer(&(gFaadCxt.b), in_buf, &inbuf_size, &inbuf_consumed);
		}
	}
	#endif

    }// while (sample_buffer != NULL);

#if 0
    NeAACDecClose(hDecoder);

    fclose(b.infile);
	if (audio_out >= 0) {
		close(audio_out);	
	}

    if (b.buffer)
        free(b.buffer);
#endif
    return inbuf_consumed;
}
#endif

static inline double get_time(void)
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    return tv.tv_sec + tv.tv_usec * 1e-6;
}
int audio_dec_release(audio_decoder_operations_t *adec_ops)
{
	if( gFaadCxt.hDecoder)
	    	NeAACDecClose(gFaadCxt.hDecoder);
	if (gFaadCxt.b.buffer)
		free(gFaadCxt.b.buffer);
	return 0;
}

int audio_dec_getinfo(audio_decoder_operations_t *adec_ops, void *pAudioInfo)
{
    ((AudioInfo *)pAudioInfo)->channels = gChannels;
    ((AudioInfo *)pAudioInfo)->samplerate = gSampleRate;
    return 0;
}

#if 0
int audio_dec_getinfo2(audio_decoder_operations_t *adec_ops, void *fileInfo, char *inbuf, int inlen)
{
	int tagsize;
	unsigned long samplerate;
	unsigned char channels;
	void *sample_buffer;
	NeAACDecFrameInfo frameInfo;

	//audio_file *aufile;

	//FILE *adtsFile;
	unsigned char *adtsData;
	int adtsDataSize;

	NeAACDecConfigurationPtr config;

	char percents[200];
	int percent, old_percent = -1;
	int bread;
	int header_type = 0;
	int bitrate = 0;
	float length = 0;
	int outmaxlen = 0;

	char *in_buf;
	long inbuf_size;
	long inbuf_consumed;

//	inbuf_data inbuf_d;

	in_buf = inbuf;
	inbuf_size = inlen;
	if (inbuf_consumed > 0)
		inbuf_consumed = 0;
	
//	if (gFaadCxt.init_flag)
//		goto START_DECODER;
	
	fill_buffer(&(gFaadCxt.b), in_buf, &inbuf_size, &inbuf_consumed);

	tagsize = 0;
	gFaadCxt.hDecoder = NeAACDecOpen();

	/* Set the default object type and samplerate */
	/* This is useful for RAW AAC files */
	config = NeAACDecGetCurrentConfiguration(gFaadCxt.hDecoder);
	config->defObjectType = LC;
	config->outputFormat = FAAD_FMT_16BIT;
	config->downMatrix = 0; //downMatrix;
	config->useOldADTSFormat = 0;//old_format;
	//config->dontUpSampleImplicitSBR = 1;
	NeAACDecSetConfiguration(gFaadCxt.hDecoder, config);

	/* get AAC infos for printing */
	header_type = 0;
	if (((gFaadCxt.b).buffer[0] == 0xFF) && (((gFaadCxt.b).buffer[1] & 0xF6) == 0xF0))
	{
	
		faad_fprintf(stderr, "((b.buffer[0] == 0xFF) && ((b.buffer[1] & 0xF6) == 0xF0)), tagsize = %d\n", tagsize);
		//adts_parse(&b, &bitrate, &length);
		
		//refill_buffer(&b);
		
		//faad_fprintf(stderr, "refill buffer\n");
		faad_fprintf(stderr, "b.bytes_into_buffer = %d, b.file_offset = %d\n", (gFaadCxt.b).bytes_into_buffer, (gFaadCxt.b).file_offset);
		faad_fprintf(stderr, "bitrate = %d, length = %d\n", bitrate, length);
		bitrate = -1;
		length = 409872;

		header_type = 1;
	} else if (memcmp((gFaadCxt.b).buffer, "ADIF", 4) == 0) {
		int skip_size = ((gFaadCxt.b).buffer[4] & 0x80) ? 9 : 0;
		
		faad_fprintf(stderr, "adif \n");
		bitrate = ((unsigned int)((gFaadCxt.b).buffer[4 + skip_size] & 0x0F)<<19) |
			((unsigned int)(gFaadCxt.b).buffer[5 + skip_size]<<11) |
			((unsigned int)(gFaadCxt.b).buffer[6 + skip_size]<<3) |
			((unsigned int)(gFaadCxt.b).buffer[7 + skip_size] & 0xE0);

#if FILELEN	
		length = (float)fileread;
		if (length != 0)
		{
			length = ((float)length*8.f)/((float)bitrate) + 0.5f;
		}
#endif
		bitrate = (int)((float)bitrate/1000.0f + 0.5f);

		header_type = 2;
	}


	fill_buffer(&(gFaadCxt.b), in_buf, &inbuf_size, &inbuf_consumed);
	if ((bread = NeAACDecInit(gFaadCxt.hDecoder, (gFaadCxt.b).buffer,
		(gFaadCxt.b).bytes_into_buffer, &samplerate, &channels)) < 0)
	{
	
		audio_codec_print("bread = %d\n\n", bread);
#if 1    
		/* If some error initializing occured, skip the file */
		faad_fprintf(stderr, "Error initializing decoder library.-----------------------------\n");
		if ((gFaadCxt.b).buffer)
			free((gFaadCxt.b).buffer);
		NeAACDecClose(gFaadCxt.hDecoder);
//		fclose((gFaadCxt.b).infile);
		return -1;
#endif
	}
	advance_buffer(&(gFaadCxt.b), bread);
	fill_buffer(&(gFaadCxt.b), in_buf, &inbuf_size, &inbuf_consumed);

	/* print AAC file info */
#if FILELEN	
	
	switch (header_type)
	{
	case 0:
		faad_fprintf(stderr, "RAW\n\n");
		break;
	case 1:
		faad_fprintf(stderr, "ADTS, %.3f sec, %d kbps, %d Hz\n\n",
			length, bitrate, samplerate);
		break;
	case 2:
		faad_fprintf(stderr, "ADIF, %.3f sec, %d kbps, %d Hz\n\n",
			length, bitrate, samplerate);
		break;
	}
#endif
	((FileInfo *)fileInfo)->channels = channels;
	((FileInfo *)fileInfo)->samplerate = samplerate;
	((FileInfo *)fileInfo)->bitrate = bitrate;
	((FileInfo *)fileInfo)->file_profile = ((NeAACDecStruct *)(gFaadCxt.hDecoder))->object_type;

	audio_codec_print("channels=%d,samplerate=%d,bitrate=%d,file_profile=%d,------------\n\n",
		((FileInfo *)fileInfo)->channels,((FileInfo *)fileInfo)->samplerate,
		((FileInfo *)fileInfo)->bitrate, ((FileInfo *)fileInfo)->file_profile);

	NeAACDecClose(gFaadCxt.hDecoder);
//	fclose(gFaadCxt.b.infile);
	if (gFaadCxt.b.buffer)
		free(gFaadCxt.b.buffer);

//	audio_codec_print("get info return----------------------------------------\n");
	return 0;
}
#endif
audio_decoder_operations_t AudioAacDecoder = {
	"AacDecoder",
	2,
	0,
	0,
	.init=audio_dec_init,
	.decode=audio_dec_decode,
	.release=audio_dec_release,
	.getinfo=audio_dec_getinfo,
	NULL,
	NULL
};



