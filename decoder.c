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

/*
static gboolean gst_x170_ppsetconfig(Gstx170 *x170, guint pixformat, guint *width, guint *height,
		     guint interlaced)
{
    PPConfig ppconfig;
    PPResult ppret;
    guint tmp;

    if ((ppret = PPGetConfig(x170->pp, &ppconfig)) != PP_OK) {
	GST_DEBUG_OBJECT(x170, "cannot retrieve PP settings (err=%d)",
		 ppret);
	return FALSE;
    }

    *width = (*width + 7) & ~7;
    *height = (*height + 7) & ~7;
    ppconfig.ppInImg.width = *width;
    ppconfig.ppInImg.height = *height;
    ppconfig.ppInImg.pixFormat = pixformat;
    ppconfig.ppInImg.videoRange = 1;
    ppconfig.ppInImg.vc1RangeRedFrm = 0;
    ppconfig.ppInImg.vc1MultiResEnable = 0;
    ppconfig.ppInImg.vc1RangeMapYEnable = 0;
    ppconfig.ppInImg.vc1RangeMapYCoeff = 0;
    ppconfig.ppInImg.vc1RangeMapCEnable = 0;
    ppconfig.ppInImg.vc1RangeMapCCoeff = 0;

    if (x170->crop_width != 0 || x170->crop_height != 0) {
	ppconfig.ppInCrop.enable = 1;
	ppconfig.ppInCrop.originX = x170->crop_x;
	ppconfig.ppInCrop.originY = x170->crop_y;
	ppconfig.ppInCrop.width = x170->crop_width;
	ppconfig.ppInCrop.height = x170->crop_height;
	*width = x170->crop_width;
	*height = x170->crop_height;
    }
    else
	ppconfig.ppInCrop.enable = 0;

    switch (x170->rotation) {
    case HT_ROTATION_0:
	ppconfig.ppInRotation.rotation = PP_ROTATION_NONE;
	break;
    case HT_ROTATION_90:
	ppconfig.ppInRotation.rotation = PP_ROTATION_RIGHT_90;
	tmp = *width;
	*width = *height;
	*height = tmp;
	break;
    case HT_ROTATION_180:
	ppconfig.ppInRotation.rotation = PP_ROTATION_180;
	break;
    case HT_ROTATION_270:
	ppconfig.ppInRotation.rotation = PP_ROTATION_LEFT_90;
	tmp = *width;
	*width = *height;
	*height = tmp;
	break;
    }

    switch (x170->output) {
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

    // Check scaler configuration
    if( (x170->output_width != 0) && (x170->output_height != 0) ) {
	// We can not upscale width and downscale height 
	if( (x170->output_width > *width) && (x170->output_height < *height) ) 
	    x170->output_width = *width;
	
	// We can not downscale width and upscale height
	if( (x170->output_width < *width) && (x170->output_height > *height) ) 
	    x170->output_height = *height;

	// Check width output value (min and max)
	if( x170->output_width < 16 )
	    x170->output_width = 16;
	if( x170->output_width > 1280 ) 
	    x170->output_width = 1280;

	// Check height output value (min and max)
	if( x170->output_height < 16 )
	    x170->output_height = 16;
	if( x170->output_height > 720 )
	    x170->output_height = 720;

	// Check width upscale factor (limited to 3x the input width)
	if( x170->output_width  > (3*(*width)) )
	    x170->output_width = (3*(*width));

	// Check height upscale factor (limited to 3x the input width - 2 pixels)
	if( x170->output_height  > ((3*(*height) - 2)) )
	    x170->output_height = ((3*(*height) - 2));

	// Re-align according to vertical and horizontal scaler steps (resp. 8 and 2)
	x170->output_width  = (x170->output_width/8)*8;
	x170->output_height = (x170->output_height/2)*2;
	
	*width  = x170->output_width;
	*height = x170->output_height;
    } 
    
    ppconfig.ppOutImg.width = *width;
    ppconfig.ppOutImg.height = *height;

    GST_DEBUG_OBJECT(x170, "ppconfig.ppOutImg.width = %d, ppconfig.ppOutImg.height = %d", ppconfig.ppOutImg.width, ppconfig.ppOutImg.height);
    GST_DEBUG_OBJECT(x170, "x170->output_width = %d, x170->output_height = %d", x170->output_width, x170->output_height);

    ppconfig.ppOutImg.bufferBusAddr = x170->outbuf.busAddress;
    ppconfig.ppOutImg.bufferChromaBusAddr = x170->outbuf.busAddress +
			ppconfig.ppOutImg.width *
			ppconfig.ppOutImg.height;
    memset(x170->outbuf.virtualAddress, 0, x170->outbuf_size);

    ppconfig.ppOutRgb.alpha = 255;
    if (interlaced)
	ppconfig.ppOutDeinterlace.enable = 1;

    if ((ppret = PPSetConfig(x170->pp, &ppconfig)) != PP_OK) {
	GST_DEBUG_OBJECT(x170, "cannot init. PP settings (err=%d)",
		 ppret);
	return FALSE;
    }
    return TRUE;
}
*/

VdpStatus vdp_decoder_create(VdpDevice device,
                             VdpDecoderProfile profile,
                             uint32_t width,
                             uint32_t height,
                             uint32_t max_references,
                             VdpDecoder *decoder)
{
	VDPAU_DBG(1, "vdp_decoder_create, width:%d height:%d",width, height);

    int pptype, dec_freq=270;

	device_ctx_t *dev = handle_get(device);
	if (!dev)
		return VDP_STATUS_INVALID_HANDLE;

	if (max_references > 16)
		return VDP_STATUS_ERROR;

	decoder_ctx_t *dec = handle_create(sizeof(*dec), decoder);
	if (!dec)
		goto err_ctx;

	dec->streamMem.virtualAddress = NULL;
	dec->streamMem.busAddress = 0;
	dec->streamMem.size = 0;

	dec->device = dev;
	dec->profile = profile;
	dec->width = width;
	dec->height = height;

	dev->src_width = width;
	dev->src_height = height;

	VdpStatus ret;

	switch (profile)
	{
//	case VDP_DECODER_PROFILE_MPEG1:
	case VDP_DECODER_PROFILE_MPEG2_SIMPLE:
	case VDP_DECODER_PROFILE_MPEG2_MAIN:
		ret = new_decoder_mpeg2(dec);
		dec_freq = 300;
		break;

	case VDP_DECODER_PROFILE_H264_BASELINE:
	case VDP_DECODER_PROFILE_H264_MAIN:
	case VDP_DECODER_PROFILE_H264_HIGH:
//	case VDP_DECODER_PROFILE_H264_CONSTRAINED_BASELINE:
//	case VDP_DECODER_PROFILE_H264_CONSTRAINED_HIGH:
		ret = new_decoder_h264(dec);
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

	dec->streamMem.size = 4096 * 250; //TODO check 1Mb
	if(DWLMallocLinear(dec->DWLinstance, dec->streamMem.size, &dec->streamMem) != DWL_OK){
	    VDPAU_ERR("vdp_decoder_create: alloc failed");
	    goto err_decoder;
	}
/*
	if ((ppret = PPInit(&dec->pp)) != PP_OK) {

	if ((ppret = PPDecCombinedModeEnable(dec->pp, dec->pDecInst, pptype)) != PP_OK) {


	dec->streamMem.size = 1920*1080*4;
	if(DWLMallocLinear(dec->DWLinstance, dec->streamMem.size, &dec->streamMem) != DWL_OK){

*/
	dec->smBufLen = 0;

	DWLSetHWFreq(dec_freq);
	VDPAU_DBG(2, "vdp_decoder_create:ok");
	return VDP_STATUS_OK;

err_decoder:
	dec->private_free(dec);
err_data:
	handle_destroy(*decoder);
err_ctx:
	return VDP_STATUS_RESOURCES;
}

VdpStatus vdp_decoder_destroy(VdpDecoder decoder)
{
	VDPAU_DBG(2, "vdp_decoder_destroy");
	decoder_ctx_t *dec = handle_get(decoder);
	if (!dec)
		return VDP_STATUS_INVALID_HANDLE;

	if(dec->DWLinstance && dec->streamMem.virtualAddress)
	    DWLFreeLinear(dec->DWLinstance, &dec->streamMem);

	if (dec->pp) {
	    PPDecCombinedModeDisable(dec->pp, dec->pDecInst);
	    PPRelease(dec->pp);
	}

	if (dec->private_free)
		dec->private_free(dec);

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
		*width = dec->width;

	if (height)
		*height = dec->height;

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

	int i, pos = 0, ret=VDP_STATUS_OK;

	for (i = 0; i < bitstream_buffer_count; i++)
	{
		if((pos + dec->smBufLen + bitstream_buffers[i].bitstream_bytes) > dec->streamMem.size)
		    break;
//		    return VDP_STATUS_RESOURCES;
		memcpy((u8 *)(dec->streamMem.virtualAddress) + pos + dec->smBufLen, bitstream_buffers[i].bitstream, bitstream_buffers[i].bitstream_bytes);
		pos += bitstream_buffers[i].bitstream_bytes;
	}

	VDPAU_DBG(4, "vdp_decoder_render: InLen:%d smBufLen:%d\n", pos, dec->smBufLen);
	dec->smBufLen += pos;

//	if ((dec->smBufLen > dec->inbuf_thresh)/* || (x170->codec == HT_VIDEO_VC1)*/) {
	pos = dec->smBufLen;
	ret = dec->decode(dec, picture_info, &pos, vid);
	if(pos){
	    dec->smBufLen -= pos;
	    if(dec->smBufLen)
		memmove(dec->streamMem.virtualAddress, (u8 *)(dec->streamMem.virtualAddress) + pos, dec->smBufLen);
	}
//	}

    VDPAU_DBG(4, "----------vdp_decoder_render: bufLen:%d\n", dec->smBufLen);
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
	VDPAU_DBG(3, "vdp_decoder_query_capabilities:%d", profile);
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
