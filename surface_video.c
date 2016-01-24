/*
 * 2015 olegk0 <olegvedi@gmail.com>
 *
 * based on:
 *
 * experimental VDPAU implementation for sunxi SoCs.
 * Copyright (c) 2013-2014 Jens Kuske <jenskuske@gmail.com>
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
#include "tiled_yuv.h"

VdpStatus vdp_video_surface_create(VdpDevice device,
                                   VdpChromaType chroma_type,
                                   uint32_t width,
                                   uint32_t height,
                                   VdpVideoSurface *surface)
{
	VDPAU_DBG(1, "width:%d height:%d chroma_type:%d",width, height, chroma_type);
    /*VDP_CHROMA_TYPE_420
     * VDP_CHROMA_TYPE_422
     * VDP_CHROMA_TYPE_444
     */
	if (!surface)
		return VDP_STATUS_INVALID_POINTER;

	if (width < 1 || width > RK_WDPAU_WIDTH_MAX || height < 1 || height > RK_WDPAU_HEIGHT_MAX)
		return VDP_STATUS_INVALID_SIZE;

	device_ctx_t *dev = handle_get(device);
	if (!dev)
		return VDP_STATUS_INVALID_HANDLE;

	video_surface_ctx_t *vs = handle_create(sizeof(*vs), surface);
	if (!vs)
		return VDP_STATUS_RESOURCES;

	queue_target_ctx_t *qt = dev->queue_target;

	vs->device = dev;
	vs->width = width;
	vs->height = height;
	vs->chroma_type = chroma_type;

	if(!qt->MemPgSize){
/*	    if(!dev->src_width || !dev->src_height){
		dev->src_width = width;
		dev->src_height = height;
	    }
*/
/*	    if(!width || !height)
		qt->MemPgSize = MEMPG_DEF_SIZE;
	    else
*/
	    qt->MemPgSize = OvlVresByXres(width) * height * 2;//TODO Only for YUV modes

	    VDPAU_DBG(3, "MemPgSize:%d",qt->MemPgSize);

	    if(InitPhyMemPg(qt) || AllocPhyMemPg(qt)){ //alloc second fb
		FreeAllMemPg(qt);
		return VDP_STATUS_RESOURCES;
	    }
while(AllocPhyMemPg(qt) >=0);//alloc all avail buffs
	    SetupOut(qt, RK_FORMAT_YCrCb_NV12_SP, width , height);//For calculate dst pitch
	}

	VDPAU_DBG(2, "ok");
	return VDP_STATUS_OK;
}

VdpStatus vdp_video_surface_destroy(VdpVideoSurface surface)
{
	VDPAU_DBG(1, "");
	video_surface_ctx_t *vs = handle_get(surface);
	if (!vs)
		return VDP_STATUS_INVALID_HANDLE;

	if (vs->decoder_private_free)
		vs->decoder_private_free(vs);

	handle_destroy(surface);

	return VDP_STATUS_OK;
}

VdpStatus vdp_video_surface_get_parameters(VdpVideoSurface surface,
                                           VdpChromaType *chroma_type,
                                           uint32_t *width,
                                           uint32_t *height)
{
	VDPAU_DBG(2, "");
	video_surface_ctx_t *vid = handle_get(surface);
	if (!vid)
		return VDP_STATUS_INVALID_HANDLE;

	if (chroma_type)
		*chroma_type = vid->chroma_type;

	if (width)
		*width = vid->width;

	if (height)
		*height = vid->height;

	return VDP_STATUS_OK;
}

VdpStatus vdp_video_surface_get_bits_y_cb_cr(VdpVideoSurface surface,
                                             VdpYCbCrFormat destination_ycbcr_format,
                                             void *const *destination_data,
                                             uint32_t const *destination_pitches)
{
	VDPAU_DBG(4, "");
	video_surface_ctx_t *vs = handle_get(surface);
	if (!vs)
		return VDP_STATUS_INVALID_HANDLE;

	if (vs->chroma_type != VDP_CHROMA_TYPE_420/* || vs->source_format != INTERNAL_YCBCR_FORMAT*/)
		return VDP_STATUS_INVALID_Y_CB_CR_FORMAT;

	if (destination_pitches[0] < vs->width || destination_pitches[1] < vs->width / 2)
		return VDP_STATUS_ERROR;

	mem_fb_t *mb = vs->device->queue_target->DispFbPtr;
	switch (destination_ycbcr_format)
	{
	case VDP_YCBCR_FORMAT_NV12:
		tiled_to_planar(mb->pMapMemBuf, destination_data[0], destination_pitches[0], vs->width, vs->height);
		tiled_to_planar(mb->pMapMemBuf + mb->UVoffset, destination_data[1], destination_pitches[1], vs->width, vs->height / 2);
		return VDP_STATUS_OK;

	case VDP_YCBCR_FORMAT_YV12:
		if (destination_pitches[2] != destination_pitches[1])
			return VDP_STATUS_ERROR;
		tiled_to_planar(mb->pMapMemBuf, destination_data[0], destination_pitches[0], vs->width, vs->height);
		tiled_deinterleave_to_planar(mb->pMapMemBuf + mb->UVoffset, destination_data[2], destination_data[1], destination_pitches[1], vs->width, vs->height / 2);
		return VDP_STATUS_OK;
	}

	return VDP_STATUS_ERROR;
}

VdpStatus vdp_video_surface_put_bits_y_cb_cr(VdpVideoSurface surface,
                                             VdpYCbCrFormat source_ycbcr_format,
                                             void const *const *source_data,
                                             uint32_t const *source_pitches)
{
	VDPAU_DBG(4, "format:%d", source_ycbcr_format);
	int i;
	const uint8_t *src;
	uint8_t *dst;
	video_surface_ctx_t *vs = handle_get(surface);

	if (!vs)
		return VDP_STATUS_INVALID_HANDLE;

	device_ctx_t *dev = vs->device;
	queue_target_ctx_t *qt = dev->queue_target;

	vs->source_format = source_ycbcr_format;

	mem_fb_t *CurMemBuf =  GetMemBlkForPut(qt);
/*
#ifdef DEBUG
	    int tms = (get_time() - dev->tmr)/1000000;
	    VDPAU_DBG(5, "time:%d mS", tms);
#endif
*/
	VDPAU_DBG(5, "spitch:%d dpitch:%d", source_pitches[0], qt->DSP_pitch);
	switch (source_ycbcr_format)
	{
	case VDP_YCBCR_FORMAT_YUYV:
	    OvlCopyPackedToFb(CurMemBuf->pMemBuf, source_data[0], qt->DSP_pitch, source_pitches[0], vs->width, vs->height, False);
	    break;
	case VDP_YCBCR_FORMAT_UYVY:
	    OvlCopyPackedToFb(CurMemBuf->pMemBuf, source_data[0], qt->DSP_pitch, source_pitches[0], vs->width, vs->height, True);
	    break;
	case VDP_YCBCR_FORMAT_NV12:
	    OvlCopyNV12SemiPlanarToFb(CurMemBuf->pMemBuf, source_data[0], source_data[1], qt->DSP_pitch, source_pitches[0], vs->width, vs->height);
	    break;
	case VDP_YCBCR_FORMAT_YV12:
	    OvlCopyPlanarToFb(CurMemBuf->pMemBuf, source_data[0], source_data[2], source_data[1], qt->DSP_pitch, source_pitches[0], source_pitches[1], vs->width, vs->height);
	    break;
	case VDP_YCBCR_FORMAT_Y8U8V8A8:
	case VDP_YCBCR_FORMAT_V8U8Y8A8:
	default:
	    return VDP_STATUS_ERROR;
	}
/*
#ifdef DEBUG
	    tms = (get_time() - dev->tmr)/1000000;
	    VDPAU_DBG(5, "time:%d mS", tms);
#endif
*/
	return VDP_STATUS_OK;
}

VdpStatus vdp_video_surface_query_capabilities(VdpDevice device,
                                               VdpChromaType surface_chroma_type,
                                               VdpBool *is_supported,
                                               uint32_t *max_width,
                                               uint32_t *max_height)
{
	VDPAU_DBG(2, "");
	if (!is_supported || !max_width || !max_height)
		return VDP_STATUS_INVALID_POINTER;

	device_ctx_t *dev = handle_get(device);
	if (!dev)
		return VDP_STATUS_INVALID_HANDLE;

	*is_supported = surface_chroma_type == VDP_CHROMA_TYPE_420;
	*max_width = RK_WDPAU_WIDTH_MAX;
	*max_height = RK_WDPAU_HEIGHT_MAX;

	return VDP_STATUS_OK;
}

VdpStatus vdp_video_surface_query_get_put_bits_y_cb_cr_capabilities(VdpDevice device,
                                                                    VdpChromaType surface_chroma_type,
                                                                    VdpYCbCrFormat bits_ycbcr_format,
                                                                    VdpBool *is_supported)
{
	VDPAU_DBG(2, "");
	if (!is_supported)
		return VDP_STATUS_INVALID_POINTER;

	device_ctx_t *dev = handle_get(device);
	if (!dev)
		return VDP_STATUS_INVALID_HANDLE;

	if (surface_chroma_type == VDP_CHROMA_TYPE_420)
		*is_supported = (bits_ycbcr_format == VDP_YCBCR_FORMAT_NV12) ||
				(bits_ycbcr_format == VDP_YCBCR_FORMAT_YV12);
	else
		*is_supported = VDP_FALSE;

	return VDP_STATUS_OK;
}
