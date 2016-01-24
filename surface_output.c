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

VdpStatus vdp_output_surface_create(VdpDevice device,
                                    VdpRGBAFormat rgba_format,
                                    uint32_t width,
                                    uint32_t height,
                                    VdpOutputSurface *surface)
{
	int ret = VDP_STATUS_OK;
	VDPAU_DBG(1, "format:%d:", rgba_format);
	if (!surface)
		return VDP_STATUS_INVALID_POINTER;

	device_ctx_t *dev = handle_get(device);
	if (!dev)
		return VDP_STATUS_INVALID_HANDLE;

	output_surface_ctx_t *out = handle_create(sizeof(*out), surface);
	if (!out)
		return VDP_STATUS_RESOURCES;

	out->contrast = 1.0;
	out->saturation = 1.0;
	out->device = dev;

/*	ret = rgba_create(&out->rgba, dev, width, height, rgba_format);
	if (ret != VDP_STATUS_OK)
	{
		handle_destroy(*surface);
		return ret;
	}
*/
	VDPAU_DBG(2, "ok");
	return VDP_STATUS_OK;
}

VdpStatus vdp_output_surface_destroy(VdpOutputSurface surface)
{
	output_surface_ctx_t *out = handle_get(surface);
	if (!out)
		return VDP_STATUS_INVALID_HANDLE;

//	rgba_destroy(&out->rgba);

	handle_destroy(surface);

	return VDP_STATUS_OK;
}

VdpStatus vdp_output_surface_get_parameters(VdpOutputSurface surface,
                                            VdpRGBAFormat *rgba_format,
                                            uint32_t *width,
                                            uint32_t *height)
{
	output_surface_ctx_t *out = handle_get(surface);
	if (!out)
		return VDP_STATUS_INVALID_HANDLE;

	if (rgba_format)
		*rgba_format = out->rgba.format;

	if (width)
		*width = out->rgba.width;

	if (height)
		*height = out->rgba.height;

	return VDP_STATUS_OK;
}

VdpStatus vdp_output_surface_get_bits_native(VdpOutputSurface surface,
                                             VdpRect const *source_rect,
                                             void *const *destination_data,
                                             uint32_t const *destination_pitches)
{
	output_surface_ctx_t *out = handle_get(surface);
	if (!out)
		return VDP_STATUS_INVALID_HANDLE;



	return VDP_STATUS_ERROR;
}

VdpStatus vdp_output_surface_put_bits_native(VdpOutputSurface surface,
                                             void const *const *source_data,
                                             uint32_t const *source_pitches,
                                             VdpRect const *destination_rect)
{
    VDPAU_DBG(4, "");
	output_surface_ctx_t *out = handle_get(surface);
	if (!out)
		return VDP_STATUS_INVALID_HANDLE;

//	return rgba_put_bits_native(&out->rgba, source_data, source_pitches, destination_rect);
	return VDP_STATUS_OK;
}

VdpStatus vdp_output_surface_put_bits_indexed(VdpOutputSurface surface,
                                              VdpIndexedFormat source_indexed_format,
                                              void const *const *source_data,
                                              uint32_t const *source_pitch,
                                              VdpRect const *destination_rect,
                                              VdpColorTableFormat color_table_format,
                                              void const *color_table)
{
    VDPAU_DBG(4, "format:%d",color_table_format);
    output_surface_ctx_t *out = handle_get(surface);
    if (!out)
	return VDP_STATUS_INVALID_HANDLE;
    if (color_table_format != VDP_COLOR_TABLE_FORMAT_B8G8R8X8)
        return VDP_STATUS_INVALID_COLOR_TABLE_FORMAT; 

    device_ctx_t *dev = out->device;
    queue_target_ctx_t *qt = dev->queue_target;

    if(!dev->osd_enabled)
	return VDP_STATUS_OK;

//VDPAU_DBG( "OSDShowFlags:%X OSDRendrFlags:%X\n", out->OSDShowFlags, out->OSDRendrFlags);
    uint32_t width = destination_rect->x1 - destination_rect->x0;
    uint32_t height = destination_rect->y1 - destination_rect->y0;

    uint32_t tmp = width | (height<<16);//TODO

    if(out->OSDChKey == tmp)
	qt->OSDShowFlags = qt->OSDShowFlags & ~OSDmain;
    out->OSDChKey = tmp;

    if(qt->OSDRendrFlags & OSDmain)
	return VDP_STATUS_OK;

    uint8_t  *I8;
    uint8_t  *A8;

    switch(source_indexed_format){
    case VDP_INDEXED_FORMAT_I8A8:
	I8 = source_data[0];
	A8 = I8 + 1;
	break;
    case VDP_INDEXED_FORMAT_A8I8:
	A8 = source_data[0];
	I8 = A8 + 1;
	break;
    default:
	return VDP_STATUS_INVALID_INDEXED_FORMAT;
    }

    uint32_t doffs = destination_rect->y0 * qt->OSD_pitch + destination_rect->x0 * qt->OSD_bpp;

/*    struct img_pack dst = { qt->OSDdst, qt->OSD_pitch};
    struct index_pack in_img = { I8, A8, color_table, source_pitch[0]};
    iargb_argb( &dst, &in_img, width, height);
*/
    struct img_a8_pack dst = { qt->OSDdst + doffs, qt->OSDColorKey, qt->OSD_pitch};
    struct index_pack in_img = { I8, A8, color_table, source_pitch[0]};
    iargb_argb_arp( &dst, &in_img, width, height);

    return VDP_STATUS_OK;
}

VdpStatus vdp_output_surface_put_bits_y_cb_cr(VdpOutputSurface surface,
                                              VdpYCbCrFormat source_ycbcr_format,
                                              void const *const *source_data,
                                              uint32_t const *source_pitches,
                                              VdpRect const *destination_rect,
                                              VdpCSCMatrix const *csc_matrix)
{
    VDPAU_DBG(4, "source_ycbcr_format:%d", source_ycbcr_format);
	output_surface_ctx_t *out = handle_get(surface);
	if (!out)
		return VDP_STATUS_INVALID_HANDLE;

	return VDP_STATUS_ERROR;
}

VdpStatus vdp_output_surface_render_output_surface(VdpOutputSurface destination_surface,
                                                   VdpRect const *destination_rect,
                                                   VdpOutputSurface source_surface,
                                                   VdpRect const *source_rect,
                                                   VdpColor const *colors,
                                                   VdpOutputSurfaceRenderBlendState const *blend_state,
                                                   uint32_t flags)
{
    VDPAU_DBG(4, "");
    output_surface_ctx_t *out = handle_get(destination_surface);
    if (!out)
	return VDP_STATUS_INVALID_HANDLE;

    output_surface_ctx_t *in = handle_get(source_surface);

//	return rgba_render_surface(&out->rgba, destination_rect, in ? &in->rgba : NULL, source_rect,
//					colors, blend_state, flags);
	return VDP_STATUS_OK;
}


VdpStatus vdp_output_surface_render_bitmap_surface(VdpOutputSurface destination_surface,
                                                   VdpRect const *destination_rect,
                                                   VdpBitmapSurface source_surface,
                                                   VdpRect const *source_rect,
                                                   VdpColor const *colors,
                                                   VdpOutputSurfaceRenderBlendState const *blend_state,
                                                   uint32_t flags)
{
//    VDPAU_DBG(5, "");
    output_surface_ctx_t *out = handle_get(destination_surface);
    if (!out)
	return VDP_STATUS_INVALID_HANDLE;

    bitmap_surface_ctx_t *in = handle_get(source_surface);
    device_ctx_t *dev = out->device;
    queue_target_ctx_t *qt = dev->queue_target;

    if(!dev->osd_enabled)
	return VDP_STATUS_OK;
//VDPAU_DBG( "OSDShowFlags:%X OSDRendrFlags:%X\n",qt->OSDShowFlags, qt->OSDRendrFlags);

    qt->OSDShowFlags = qt->OSDShowFlags & ~OSDSubTitles;

    if(qt->OSDRendrFlags & OSDSubTitles)
	return VDP_STATUS_OK;

    int width = (source_rect->x1 - source_rect->x0);
    int height = (source_rect->y1 - source_rect->y0);
//    VDPAU_DBG( "src width:%d height:%d",width,height);

    uint32_t Color = (((unsigned int)(colors->blue * 255))&0xff) | //TODO Check color source
	    (((unsigned int)(colors->green * 255)&0xff) << 8) |
	    (((unsigned int)(colors->red * 255)&0xff) << 16);

    uint32_t soffs = (in->rgba.width * source_rect->y0 + (source_rect->x0 << 1) + 3)& ~3;
    uint32_t doffs = (qt->OSD_pitch * destination_rect->y0 + (destination_rect->x0 * qt->OSD_bpp) + 3)& ~3;
/*
XImage *img = XCreateImage (dev->display, NULL, 8, ZPixmap, 0, 
    (uint8_t *)in->rgba.data + soffset, width, height, 0, width);

Pixmap pixmap = XCreatePixmap(dev->display,qt->drawable, img->width, img->height, 8);
XPutImage(dev->display, qt->drawable, qt->gr_context,img, 0, 0,0,0, img->width, img->height);
XCopyArea(dev->display, pixmap, qt->drawable, qt->gr_context, 0, 0, img->width, img->height, destination_rect->x0, destination_rect->y0);

XFreePixmap(dev->display,pixmap);
*/

//    VDPAU_DBG( "dwidth:%d x0:%d y0:%d Yoffs:%d UVoffs:%d", dev->src_width, destination_rect->x0, destination_rect->y0, Yoffs, UVoffs);

/*    struct img_pack dst = { (uint8_t *)(qt->OSDdst) + doffs, qt->OSD_pitch};
    struct img_a8_pack in_img = { (uint8_t *)(in->rgba.data) + soffs, color, in->rgba.width};
    a8_argb( &dst, &in_img, width , height );
*/
    struct img_a8_pack dst = { (uint8_t *)(qt->OSDdst) + doffs, qt->OSDColorKey, qt->OSD_pitch};
    struct img_a8_pack in_img = { (uint8_t *)(in->rgba.data) + soffs, Color, in->rgba.width};
    a8_argb_arp( &dst, &in_img, width , height );


    return VDP_STATUS_OK;
}

VdpStatus vdp_output_surface_query_capabilities(VdpDevice device,
                                                VdpRGBAFormat surface_rgba_format,
                                                VdpBool *is_supported,
                                                uint32_t *max_width,
                                                uint32_t *max_height)
{
    VDPAU_DBG(2, "format:%d", surface_rgba_format);
	if (!is_supported || !max_width || !max_height)
		return VDP_STATUS_INVALID_POINTER;

	device_ctx_t *dev = handle_get(device);
	if (!dev)
		return VDP_STATUS_INVALID_HANDLE;

	*is_supported = VDP_TRUE;
//(surface_rgba_format == VDP_RGBA_FORMAT_R8G8B8A8 || surface_rgba_format == VDP_RGBA_FORMAT_B8G8R8A8);
	*max_width = RK_WDPAU_WIDTH_MAX;
	*max_height = RK_WDPAU_HEIGHT_MAX;

	return VDP_STATUS_OK;
}

VdpStatus vdp_output_surface_query_get_put_bits_native_capabilities(VdpDevice device,
                                                                    VdpRGBAFormat surface_rgba_format,
                                                                    VdpBool *is_supported)
{
    VDPAU_DBG(2, "format:%d", surface_rgba_format);
	if (!is_supported)
		return VDP_STATUS_INVALID_POINTER;

	device_ctx_t *dev = handle_get(device);
	if (!dev)
		return VDP_STATUS_INVALID_HANDLE;

	*is_supported = VDP_FALSE;

	return VDP_STATUS_OK;
}

VdpStatus vdp_output_surface_query_put_bits_indexed_capabilities(VdpDevice device,
                                                                 VdpRGBAFormat surface_rgba_format,
                                                                 VdpIndexedFormat bits_indexed_format,
                                                                 VdpColorTableFormat color_table_format,
                                                                 VdpBool *is_supported)
{
    VDPAU_DBG(2, " format:%d", surface_rgba_format);
	if (!is_supported)
		return VDP_STATUS_INVALID_POINTER;

	device_ctx_t *dev = handle_get(device);
	if (!dev)
		return VDP_STATUS_INVALID_HANDLE;

	*is_supported = VDP_FALSE;
	*is_supported = VDP_TRUE;
	return VDP_STATUS_OK;
}

VdpStatus vdp_output_surface_query_put_bits_y_cb_cr_capabilities(VdpDevice device,
                                                                 VdpRGBAFormat surface_rgba_format,
                                                                 VdpYCbCrFormat bits_ycbcr_format,
                                                                 VdpBool *is_supported)
{
    VDPAU_DBG(2, "format:%d", surface_rgba_format);
	if (!is_supported)
		return VDP_STATUS_INVALID_POINTER;

	device_ctx_t *dev = handle_get(device);
	if (!dev)
		return VDP_STATUS_INVALID_HANDLE;

	*is_supported = VDP_FALSE;
//	*is_supported = VDP_TRUE;

	return VDP_STATUS_OK;
}
