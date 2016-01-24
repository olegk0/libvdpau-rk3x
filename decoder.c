/*
 * 2015 olegk0 <olegvedi@gmail.com>
 *
 * based on:
 *
 * experimental VDPAU implementation for sunxi SoCs.
 * Copyright (c) 2013 Jens Kuske <jenskuske@gmail.com>
 * 
 * and
 * 
 * gst-plugin-x170-1.0
 * Copyright (c) 2008, Atmel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <string.h>
#include "vdpau_private.h"
#include <semaphore.h>

static pthread_mutex_t in_buf_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t in_buf_cv = PTHREAD_COND_INITIALIZER;
static pthread_t dec_thread;
static sem_t in_buf_sem;

#define IN_FBBUF_CNT_SEM IN_FBBUF_CNT-2

in_mem_fb_t *PopInBuf(decoder_ctx_t *dec)
{
	pthread_mutex_lock(&in_buf_mutex);

	int free_blocks;
	sem_getvalue(&in_buf_sem, &free_blocks);
	VDPAU_DBG(5, "free_blocks:%d",free_blocks);
	while(free_blocks >= IN_FBBUF_CNT_SEM){
		pthread_cond_wait(&in_buf_cv, &in_buf_mutex);
		sem_getvalue(&in_buf_sem, &free_blocks);
	}

	dec->cur_in_fbmem++;
	if(dec->cur_in_fbmem >= IN_FBBUF_CNT)
		dec->cur_in_fbmem = 0;
	sem_post(&in_buf_sem);

	VDPAU_DBG(5, "continue, free_blocks:%d, cur_in_fbmem:%d",free_blocks, dec->cur_in_fbmem);
	pthread_mutex_unlock(&in_buf_mutex);

	return &dec->in_fbmem[dec->cur_in_fbmem];
}

VdpStatus vdpPPsetOutBuf(mem_fb_t *mempg, decoder_ctx_t *dec)
{
    PPResult ppret;
    PPConfig ppconfig;

    if ((ppret = PPGetConfig(dec->pp, &ppconfig)) != PP_OK) {
	VDPAU_ERR("cannot retrieve PP settings (err=%d)", ppret);
	return VDP_STATUS_ERROR;
    }

    ppconfig.ppOutImg.bufferBusAddr = mempg->PhyAddr;
    ppconfig.ppOutImg.bufferChromaBusAddr = mempg->PhyAddr + mempg->UVoffset;
//    memset(dec->ppMem.virtualAddress, 0, dec->ppMem.size);

    if ((ppret = PPSetConfig(dec->pp, &ppconfig)) != PP_OK) {
	VDPAU_ERR("cannot init. PP settings (err=%d)", ppret);
	return VDP_STATUS_ERROR;
    }

    return VDP_STATUS_OK;
}

VdpStatus vdpPPsetConfig(decoder_ctx_t *dec, video_surface_ctx_t *output, uint32_t pixformat, Bool interlaced)
{
    PPConfig ppconfig;
    PPResult ppret;
    uint32_t tmp;
    uint32_t width = (dec->dec_width + 7) & ~7;
    uint32_t height = (dec->dec_height + 7) & ~7;


    if ((ppret = PPGetConfig(dec->pp, &ppconfig)) != PP_OK) {
	VDPAU_ERR("cannot retrieve PP settings (err=%d)", ppret);
	return VDP_STATUS_ERROR;
    }

    ppconfig.ppInImg.width = width;
    ppconfig.ppInImg.height = height;
    ppconfig.ppInImg.pixFormat = pixformat;
    ppconfig.ppInImg.videoRange = 1;
    ppconfig.ppInImg.vc1RangeRedFrm = 0;
    ppconfig.ppInImg.vc1MultiResEnable = 0;
    ppconfig.ppInImg.vc1RangeMapYEnable = 0;
    ppconfig.ppInImg.vc1RangeMapYCoeff = 0;
    ppconfig.ppInImg.vc1RangeMapCEnable = 0;
    ppconfig.ppInImg.vc1RangeMapCCoeff = 0;

    dec->crop_width = ((output->width > width ? width : output->width) + 7) & ~7;
    dec->crop_height= ((output->height > height ? height : output->height) + 7) & ~7;

    if (dec->crop_width != 0 || dec->crop_height != 0) {
	ppconfig.ppInCrop.enable = 1;
	ppconfig.ppInCrop.originX = 0;
	ppconfig.ppInCrop.originY = 0;
	ppconfig.ppInCrop.width = dec->crop_width;
	ppconfig.ppInCrop.height = dec->crop_height;
	width = dec->crop_width;
	height = dec->crop_height;
    }
    else
	ppconfig.ppInCrop.enable = 0;

    ppconfig.ppInRotation.rotation = dec->rotation;
    switch(dec->rotation) {
    case PP_ROTATION_RIGHT_90:
	tmp = width;
	width = height;
	height = tmp;
	break;
    case PP_ROTATION_LEFT_90:
	tmp = width;
	width = height;
	height = tmp;
	break;
    }

	ppconfig.ppOutImg.pixFormat = PP_PIX_FMT_YCBCR_4_2_0_SEMIPLANAR;
/*    switch (dec->output) {
    case HT_OUTPUT_UYVY:
	ppconfig.ppOutImg.pixFormat = PP_PIX_FMT_YCBCR_4_2_0_SEMIPLANAR;
	break;
    case HT_OUTPUT_RGB15:
	ppconfig.ppOutImg.pixFormat = PP_PIX_FMT_RGB16_5_5_5;
	break;
    case HT_OUTPUT_RGB16:
	ppconfig.ppOutImg.pixFormat = PP_PIX_FMT_RGB16_5_6_5;
	break;
    case HT_OUTPUT_RGB32:
	ppconfig.ppOutImg.pixFormat = PP_PIX_FMT_RGB32;
	break;
    }
*/
/*    // Check scaler configuration
    if( (dec->output_width != 0) && (dec->output_height != 0) ) {
	// We can not upscale width and downscale height 
	if( (dec->output_width > *width) && (dec->output_height < *height) ) 
	    dec->output_width = *width;
	
	// We can not downscale width and upscale height
	if( (dec->output_width < *width) && (dec->output_height > *height) ) 
	    dec->output_height = *height;

	// Check width output value (min and max)
	if( dec->output_width < 16 )
	    dec->output_width = 16;
	if( dec->output_width > 1280 ) 
	    dec->output_width = 1280;

	// Check height output value (min and max)
	if( dec->output_height < 16 )
	    dec->output_height = 16;
	if( dec->output_height > 720 )
	    dec->output_height = 720;

	// Check width upscale factor (limited to 3x the input width)
	if( dec->output_width  > (3*(*width)) )
	    dec->output_width = (3*(*width));

	// Check height upscale factor (limited to 3x the input width - 2 pixels)
	if( dec->output_height  > ((3*(*height) - 2)) )
	    dec->output_height = ((3*(*height) - 2));

	// Re-align according to vertical and horizontal scaler steps (resp. 8 and 2)
	dec->output_width  = (dec->output_width/8)*8;
	dec->output_height = (dec->output_height/2)*2;
	
	*width  = dec->output_width;
	*height = dec->output_height;
    } 
*/    
//    ppconfig.ppOutImg.width = width;
    ppconfig.ppOutImg.width = dec->device->queue_target->DSP_pitch;
    ppconfig.ppOutImg.height = height;

    VDPAU_DBG(2, "ppOutImg.width = %d, ppOutImg.height = %d", ppconfig.ppOutImg.width, ppconfig.ppOutImg.height);
    VDPAU_DBG(2, "output_width = %d, output_height = %d", dec->crop_width, dec->crop_height);

    mem_fb_t *mempg = GetMemBlkForPut(dec->device->queue_target);
    ppconfig.ppOutImg.bufferBusAddr = mempg->PhyAddr;
    ppconfig.ppOutImg.bufferChromaBusAddr = mempg->PhyAddr + mempg->UVoffset;

//    memset(dec->ppMem.virtualAddress, 0, dec->ppMem.size);

    ppconfig.ppOutRgb.alpha = 255;
    if (interlaced)
	ppconfig.ppOutDeinterlace.enable = 1;

    if ((ppret = PPSetConfig(dec->pp, &ppconfig)) != PP_OK) {
	VDPAU_ERR("cannot init. PP settings (err=%d)", ppret);
	return VDP_STATUS_ERROR;
    }
    return VDP_STATUS_OK;
}


VdpStatus vdp_decoder_create(VdpDevice device,
                             VdpDecoderProfile profile,
                             uint32_t width,
                             uint32_t height,
                             uint32_t max_references,
                             VdpDecoder *decoder)
{
    VDPAU_DBG(1, "width:%d height:%d",width, height);

    int pptype, dec_freq=270;

	device_ctx_t *dev = handle_get(device);
	if (!dev)
		return VDP_STATUS_INVALID_HANDLE;

	if (max_references > 16)
		return VDP_STATUS_ERROR;

	decoder_ctx_t *dec = handle_create(sizeof(*dec), decoder);
	if (!dec)
		goto err_ctx;

	dec->in_fbmem = calloc(IN_FBBUF_CNT, sizeof(in_mem_fb_t));
	if(!dec->in_fbmem)
		goto err_data;

	dec->streamMem.virtualAddress = NULL;
	dec->streamMem.busAddress = 0;
	dec->streamMem.size = 0;

	dec->device = dev;
	dec->profile = profile;
	dec->dec_width = width;
	dec->dec_height = height;

	VdpStatus ret;

	switch (profile)
	{
//	case VDP_DECODER_PROFILE_MPEG1:
	case VDP_DECODER_PROFILE_MPEG2_SIMPLE:
	case VDP_DECODER_PROFILE_MPEG2_MAIN:
		ret = new_decoder_mpeg2(dec);
		pptype = PP_PIPELINED_DEC_TYPE_MPEG2;
		dec_freq = 300;
		break;

	case VDP_DECODER_PROFILE_H264_BASELINE:
	case VDP_DECODER_PROFILE_H264_MAIN:
	case VDP_DECODER_PROFILE_H264_HIGH:
//	case VDP_DECODER_PROFILE_H264_CONSTRAINED_BASELINE:
//	case VDP_DECODER_PROFILE_H264_CONSTRAINED_HIGH:
		ret = new_decoder_h264(dec);
		pptype = PP_PIPELINED_DEC_TYPE_H264;
		dec_freq = 300;
		break;

	case VDP_DECODER_PROFILE_MPEG4_PART2_SP:
	case VDP_DECODER_PROFILE_MPEG4_PART2_ASP:
	case VDP_DECODER_PROFILE_DIVX4_QMOBILE:
	case VDP_DECODER_PROFILE_DIVX4_MOBILE:
	case VDP_DECODER_PROFILE_DIVX4_HOME_THEATER:
	case VDP_DECODER_PROFILE_DIVX4_HD_1080P:
	case VDP_DECODER_PROFILE_DIVX5_QMOBILE:
	case VDP_DECODER_PROFILE_DIVX5_MOBILE:
	case VDP_DECODER_PROFILE_DIVX5_HOME_THEATER:
	case VDP_DECODER_PROFILE_DIVX5_HD_1080P:
		ret = new_decoder_mpeg4(dec);
		pptype = PP_PIPELINED_DEC_TYPE_MPEG4;
		dec_freq = 300;
		break;

	default:
		ret = VDP_STATUS_INVALID_DECODER_PROFILE;
		break;
	}

	if (ret != VDP_STATUS_OK)
		goto err_data;

	dec->streamMem.size = DEC_BUF_IN_SIZE;
	if(DWLMallocLinear(dec->DWLinstance, dec->streamMem.size, &dec->streamMem) != DWL_OK){
	    VDPAU_ERR("alloc failed");
	    goto err_decoder;
	}

	PPResult ppret;
	if ((ppret = PPInit(&dec->pp)) != PP_OK) {
	    dec->pp = NULL;
	    VDPAU_ERR("Error PPInit:%d", ppret);
	}else{
	    if((ppret = PPDecCombinedModeEnable(dec->pp, dec->pDecInst, pptype)) != PP_OK) {
		VDPAU_ERR("Error PPDecCombinedModeEnable:%d", ppret);
		PPRelease(dec->pp);
		dec->pp = NULL;
	    }
	}

//	dec->crop_width = width;
//	dec->crop_height = height;

	dec->rotation = PP_ROTATION_NONE;

	dec->last_in_fbmem = 1;
	dec->cur_in_fbmem = 0;
	dec->in_fbmem[dec->last_in_fbmem].offset = 0;
	dec->in_fbmem[dec->last_in_fbmem].data_size = 0;
	sem_init(&in_buf_sem, 0, IN_FBBUF_CNT_SEM);

	DWLSetHWFreq(dec_freq);

// start decoder thread

	dec->drop_cnt = 0;
	dec->th_stat = 0;
	pthread_create(&dec_thread, NULL, dec->decode, dec);

	VDPAU_DBG(2, "ok");
	return VDP_STATUS_OK;

err_decoder:
	dec->private_free(dec);
err_data:
	if(dec->in_fbmem)
		free(dec->in_fbmem);
	handle_destroy(*decoder);
err_ctx:
	return VDP_STATUS_RESOURCES;
}

VdpStatus vdp_decoder_destroy(VdpDecoder decoder)
{
	VDPAU_DBG(2, "");
	decoder_ctx_t *dec = handle_get(decoder);
	if (!dec)
		return VDP_STATUS_INVALID_HANDLE;

	pthread_mutex_lock(&in_buf_mutex);
	dec->th_stat = -1;
//	dec->in_blocks++;
	int sts;
	sem_getvalue(&in_buf_sem, &sts);
	while(sts  >= IN_FBBUF_CNT_SEM){
		sem_trywait(&in_buf_sem);
		sem_getvalue(&in_buf_sem, &sts);
	}

	pthread_cond_signal(&in_buf_cv);
	pthread_mutex_unlock(&in_buf_mutex);

	GetMemPgFake(dec->device->queue_target, 10);

	VDPAU_DBG(3, "Wait for dec thread ended");
	pthread_join(dec_thread, (void**)&sts);
	VDPAU_DBG(3, "thread ended");

	if(dec->DWLinstance && dec->streamMem.virtualAddress)
	    DWLFreeLinear(dec->DWLinstance, &dec->streamMem);

	if (dec->pp) {
	    PPDecCombinedModeDisable(dec->pp, dec->pDecInst);
	    PPRelease(dec->pp);
	}

	if (dec->private_free)
		dec->private_free(dec);

	sem_destroy(&in_buf_sem);

	if(dec->in_fbmem)
		free(dec->in_fbmem);
	handle_destroy(decoder);

	return VDP_STATUS_OK;
}

VdpStatus vdp_decoder_get_parameters(VdpDecoder decoder,
                                     VdpDecoderProfile *profile,
                                     uint32_t *width,
                                     uint32_t *height)
{
	decoder_ctx_t *dec = handle_get(decoder);
	if (!dec)
		return VDP_STATUS_INVALID_HANDLE;

	if (profile)
		*profile = dec->profile;

	if (width)
		*width = dec->dec_width;

	if (height)
		*height = dec->dec_height;

	return VDP_STATUS_OK;
}

VdpStatus vdp_decoder_render(VdpDecoder decoder,
                             VdpVideoSurface target,
                             VdpPictureInfo const *picture_info,
                             uint32_t bitstream_buffer_count,
                             VdpBitstreamBuffer const *bitstream_buffers)
{
	decoder_ctx_t *dec = handle_get(decoder);
	if (!dec)
		return VDP_STATUS_INVALID_HANDLE;

	video_surface_ctx_t *vid = handle_get(target);
	if (!vid)
		return VDP_STATUS_INVALID_HANDLE;

	int i, n, pos = 0, ret=VDP_STATUS_OK;

	dec->pic_info = picture_info;
	dec->vs = vid;

	for (i = 0; i < bitstream_buffer_count; i++)//calc size
		pos += bitstream_buffers[i].bitstream_bytes;

	if(dec->device->queue_target->flush_fl)
		GetMemPgFake(dec->device->queue_target, 10);

	pthread_mutex_lock(&in_buf_mutex);

	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_nsec += 5000;
	VDPAU_DBG(4, "sem_timedwait");
	if(sem_timedwait(&in_buf_sem, &ts))
		return VDP_STATUS_RESOURCES;

/*	while(sem_timedwait(&in_buf_sem, &ts) == ETIMEDOUT){
		GetMemPgFake(dec->device->queue_target);
		clock_gettime(CLOCK_REALTIME, &ts);
		ts.tv_nsec += 1000;
		VDPAU_DBG(4, "sem_timedwait:Timeout");
	}
	*/
//
//	sem_wait(&in_buf_sem);

	if(dec->in_fbmem[dec->last_in_fbmem].offset + pos > (DEC_BUF_IN_SIZE-(DEC_BUF_IN_SIZE/20))){//5% safe
		dec->in_fbmem[dec->last_in_fbmem].offset = 0;
	}

	dec->in_fbmem[dec->last_in_fbmem].data_size = pos;

	uint32_t offset = dec->in_fbmem[dec->last_in_fbmem].offset;
	pos = 0;
	for (i = 0; i < bitstream_buffer_count; i++)
	{
		memcpy((u8 *)(dec->streamMem.virtualAddress) + offset + pos, bitstream_buffers[i].bitstream, bitstream_buffers[i].bitstream_bytes);
		pos += bitstream_buffers[i].bitstream_bytes;
	}

	dec->last_in_fbmem++;
	if(dec->last_in_fbmem >= IN_FBBUF_CNT){
		dec->last_in_fbmem = 0;
	}

	dec->in_fbmem[dec->last_in_fbmem].offset = offset + pos;

	int free_blocks;
	sem_getvalue(&in_buf_sem, &free_blocks);

	if(free_blocks < IN_FBBUF_CNT_SEM - 1){
		if(dec->drop_cnt < 2){
			if(dec->drop_cnt == 1){
				GetMemPgFake(dec->device->queue_target, 1);
			}
			else{
//				dec->drop_cnt = (free_blocks*4/3 ) - 26;
				dec->drop_cnt = free_blocks - 20;
				if(dec->drop_cnt < 0)
					dec->drop_cnt = 0;
				dec->drop_cnt += 2;
			}
		}
		dec->drop_cnt--;
	}else
		dec->drop_cnt = 0;

	if(free_blocks == IN_FBBUF_CNT_SEM - 1) //was 0
		pthread_cond_signal(&in_buf_cv);

	VDPAU_DBG(5, "InLen:%d free_blocks:%d, drop_cnt:%d  ", pos, free_blocks, dec->drop_cnt);
	pthread_mutex_unlock(&in_buf_mutex);

	ret = dec->th_stat;
	dec->th_stat = VDP_STATUS_OK;

//	usleep(5000);
	dec->device->queue_target->flush_fl = 1;
    return ret; 
}

VdpStatus vdp_decoder_query_capabilities(VdpDevice device,
                                         VdpDecoderProfile profile,
                                         VdpBool *is_supported,
                                         uint32_t *max_level,
                                         uint32_t *max_macroblocks,
                                         uint32_t *max_width,
                                         uint32_t *max_height)
{
	if (!is_supported || !max_level || !max_macroblocks || !max_width || !max_height)
		return VDP_STATUS_INVALID_POINTER;

	device_ctx_t *dev = handle_get(device);
	if (!dev)
		return VDP_STATUS_INVALID_HANDLE;

//	*max_width = 3840;
	*max_width = RK_WDPAU_WIDTH_MAX;
//	*max_height = 2160;
	*max_height = RK_WDPAU_HEIGHT_MAX;
	*max_macroblocks = (*max_width * *max_height) / (16 * 16);
	VDPAU_DBG(3, "profile:%d", profile);
	switch (profile)
	{
/*	case VDP_DECODER_PROFILE_MPEG1:
		*max_level = VDP_DECODER_LEVEL_MPEG1_NA;
		*is_supported = VDP_TRUE;
		break;
*/	case VDP_DECODER_PROFILE_MPEG2_SIMPLE:
	case VDP_DECODER_PROFILE_MPEG2_MAIN:
		*max_level = VDP_DECODER_LEVEL_MPEG2_HL;
		*is_supported = VDP_TRUE;
		break;

	case VDP_DECODER_PROFILE_H264_BASELINE:
	case VDP_DECODER_PROFILE_H264_MAIN:
	case VDP_DECODER_PROFILE_H264_HIGH:
//	case VDP_DECODER_PROFILE_H264_CONSTRAINED_BASELINE:
//	case VDP_DECODER_PROFILE_H264_CONSTRAINED_HIGH:
		*max_level = VDP_DECODER_LEVEL_H264_5_1;
		*is_supported = VDP_TRUE;
		break;
	case VDP_DECODER_PROFILE_MPEG4_PART2_SP:
	case VDP_DECODER_PROFILE_MPEG4_PART2_ASP:
		*max_level = VDP_DECODER_LEVEL_MPEG4_PART2_ASP_L5;
		*is_supported = VDP_TRUE;
		break;
	case VDP_DECODER_PROFILE_DIVX4_QMOBILE:
	case VDP_DECODER_PROFILE_DIVX4_MOBILE:
	case VDP_DECODER_PROFILE_DIVX4_HOME_THEATER:
	case VDP_DECODER_PROFILE_DIVX4_HD_1080P:
	case VDP_DECODER_PROFILE_DIVX5_QMOBILE:
	case VDP_DECODER_PROFILE_DIVX5_MOBILE:
	case VDP_DECODER_PROFILE_DIVX5_HOME_THEATER:
	case VDP_DECODER_PROFILE_DIVX5_HD_1080P:
		*max_level = VDP_DECODER_LEVEL_DIVX_NA;
		*is_supported = VDP_TRUE;
		break;
	default:
		*is_supported = VDP_FALSE;
		break;
	}

	return VDP_STATUS_OK;
}
