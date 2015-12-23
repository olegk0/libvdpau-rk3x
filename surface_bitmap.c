/*
 * 2015 olegk0 <olegvedi@gmail.com>
 *
 * based on:
 *
 * experimental VDPAU implementation for sunxi SoCs.
 * Copyright (c) 2013 Jens Kuske <jenskuske@gmail.com>
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

#include "vdpau_private.h"
#include "rgb_asm.h"
#include <string.h>

VdpStatus vdp_bitmap_surface_create(VdpDevice device,
                                    VdpRGBAFormat rgba_format,
                                    uint32_t width,
                                    uint32_t height,
                                    VdpBool frequently_accessed,
                                    VdpBitmapSurface *surface)
{
	VDPAU_DBG(1," rgba_format:%d width:%d height:%d", rgba_format, width, height);

	if (!surface)
		return VDP_STATUS_INVALID_POINTER;

	device_ctx_t *dev = handle_get(device);
	if (!dev)
		return VDP_STATUS_INVALID_HANDLE;

	int ssize;
	switch(rgba_format){
/*	case VDP_RGBA_FORMAT_B8G8R8A8:
	case VDP_RGBA_FORMAT_R8G8B8A8:
	    ssize *= 4;
	    break;
*/
	case VDP_RGBA_FORMAT_A8:
	    width *= 2; //for align implement
	    ssize = width * height;
	    break;
	default:
	    return VDP_STATUS_ERROR;
	}

	bitmap_surface_ctx_t *out = handle_create(sizeof(*out), surface);
	if (!out)
		return VDP_STATUS_RESOURCES;

	out->frequently_accessed = frequently_accessed;
	out->device = dev;
	out->rgba.width = width;
	out->rgba.height = height;
	out->rgba.format = rgba_format;
	out->rgba.size = ssize;

	out->rgba.data = calloc(1, ssize);
//	ret = rgba_create(&out->rgba, dev, width, height, rgba_format);
	if (!out->rgba.data)
	{
		handle_destroy(*surface);
		return VDP_STATUS_RESOURCES;
	}

	VDPAU_DBG(2,"ok");
	return VDP_STATUS_OK;
}

VdpStatus vdp_bitmap_surface_destroy(VdpBitmapSurface surface)
{
	bitmap_surface_ctx_t *out = handle_get(surface);
	if (!out)
		return VDP_STATUS_INVALID_HANDLE;

	if (out->rgba.data)
	    free(out->rgba.data);

	handle_destroy(surface);

	return VDP_STATUS_OK;
}

VdpStatus vdp_bitmap_surface_get_parameters(VdpBitmapSurface surface,
                                            VdpRGBAFormat *rgba_format,
                                            uint32_t *width,
                                            uint32_t *height,
                                            VdpBool *frequently_accessed)
{
    VDPAU_DBG(1, "");
	bitmap_surface_ctx_t *out = handle_get(surface);
	if (!out)
		return VDP_STATUS_INVALID_HANDLE;

	if (rgba_format)
		*rgba_format = out->rgba.format;

	if (width)
		*width = out->rgba.width;

	if (height)
		*height = out->rgba.height;

	if (frequently_accessed)
		*frequently_accessed = out->frequently_accessed;

	return VDP_STATUS_OK;
}

VdpStatus vdp_bitmap_surface_put_bits_native(VdpBitmapSurface surface,
                                             void const *const *source_data,
                                             uint32_t const *source_pitches,
                                             VdpRect const *destination_rect)
{
    int width = destination_rect->x1 - destination_rect->x0;
    int height = destination_rect->y1 - destination_rect->y0;
    int spitch = *source_pitches;
//    VDPAU_DBG( "vdp_bitmap_surface_put_bits_native, pitch:%d width:%d height:%d",spitch, width, height);
//    VDPAU_DBG( "vdp_bitmap_surface_put_bits_native, x0:%d y0:%d",destination_rect->x0, destination_rect->y0);
    bitmap_surface_ctx_t *out = handle_get(surface);
    if (!out)
	return VDP_STATUS_INVALID_HANDLE;

    if(out->rgba.format == VDP_RGBA_FORMAT_A8){
	uint32_t offset = (out->rgba.width * destination_rect->y0 +
	    (destination_rect->x0 << 1) + 3) & ~3;//align by 4

//    VDPAU_DBG( "in:%p out:%p offset:%d rgba.width:%d", source_data, (uint8_t *)out->rgba.data + offset, offset, out->rgba.width);

	struct img_pack dst = { (uint8_t *)(out->rgba.data) + offset, (out->rgba.width)};
	struct img_pack src = {source_data[0], *source_pitches};

	copy_img_1b( &dst, &src, width, height );

    }

    return VDP_STATUS_OK;
}

VdpStatus vdp_bitmap_surface_query_capabilities(VdpDevice device,
                                                VdpRGBAFormat surface_rgba_format,
                                                VdpBool *is_supported,
                                                uint32_t *max_width,
                                                uint32_t *max_height)
{
    VDPAU_DBG(1, " surface_rgba_format:%d",surface_rgba_format);
	if (!is_supported || !max_width || !max_height)
		return VDP_STATUS_INVALID_POINTER;

	device_ctx_t *dev = handle_get(device);
	if (!dev)
		return VDP_STATUS_INVALID_HANDLE;

	*is_supported = (surface_rgba_format == VDP_RGBA_FORMAT_A8);
//(surface_rgba_format == VDP_RGBA_FORMAT_R8G8B8A8 || surface_rgba_format == VDP_RGBA_FORMAT_B8G8R8A8);
//*is_supported = VDP_FALSE;
	*max_width = RK_WDPAU_WIDTH_MAX;
	*max_height = RK_WDPAU_HEIGHT_MAX;

	return VDP_STATUS_OK;
}
