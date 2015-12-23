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

#ifndef __VDPAU_PRIVATE_H__
#define __VDPAU_PRIVATE_H__

#define DEBUG

#define MAX_HANDLES 64
#define VBV_SIZE (1 * 1024 * 1024)

#include <stdlib.h>
#include <vdpau/vdpau.h>
#include <vdpau/vdpau_x11.h>
#include <X11/Xlib.h>
#include <rk_layers.h>
#include <inc/dwl.h>
#include <inc/ppapi.h>

#define MEMPG_MAX_CNT 8
#define RK_WDPAU_WIDTH_MAX 1920
#define RK_WDPAU_HEIGHT_MAX 1080
#define MEMPG_DEF_SIZE RK_WDPAU_WIDTH_MAX*RK_WDPAU_HEIGHT_MAX*4

#ifdef DEBUG
extern int Dbg_Level;
#endif

typedef struct queue_target_ctx queue_target_ctx;

typedef struct
{
	Display *display;
	int screen;
	VdpPreemptionCallback *preemption_callback;
	void *preemption_callback_context;
	int osd_enabled;
	uint64_t tmr;
//	uint32_t src_width;
//	uint32_t src_height;
	struct queue_target_ctx *queue_target;
} device_ctx_t;

typedef struct mem_fb mem_fb;

typedef struct mem_fb
{
	OvlMemPgPtr pMemBuf;
	void* pMapMemBuf;
	uint32_t UVoffset;
	uint32_t PhyAddr;
	struct mem_fb *Next;
} mem_fb_t;

enum{
    OSDmain=1,
    OSDSubTitles=2,
};

typedef struct queue_target_ctx
{
	Drawable drawable;
	device_ctx_t *device;
	GC gr_context;
	uint32_t OSDColorKey;
	OvlLayPg VideoLayer;
	OvlLayPg OSDLayer;
	OvlMemPgPtr OSDMemPg;
	void *OSDmmap;
	void *OSDdst;
	uint32_t OSD_pitch;
	uint32_t OSD_bpp;
	uint32_t OSD_DispMode;
	int FbAllCnt;
	int FbFilledCnt;
	uint32_t DSP_pitch;
	mem_fb_t *DispFbPtr;
	mem_fb_t *WorkFbPtr;
	mem_fb_t *PutFbPtr;
	uint32_t MemPgSize;
	uint32_t OSDShowFlags;
	uint32_t OSDRendrFlags;
	int PicBalance;
} queue_target_ctx_t;

typedef struct video_surface_ctx_struct
{
	device_ctx_t *device;
	uint32_t width, height;
	VdpChromaType chroma_type;
	VdpYCbCrFormat source_format;
//	yuv_data_t *yuv;
//	uint32_t luma_size;
	void *decoder_private;
	void (*decoder_private_free)(struct video_surface_ctx_struct *surface);
} video_surface_ctx_t;

typedef struct decoder_ctx_struct
{
	VdpDecoderProfile profile;
	uint32_t dec_width;
	uint32_t dec_height;
	device_ctx_t *device;
	VdpStatus (*decode)(struct decoder_ctx_struct *decoder, VdpPictureInfo const *info, int *len, video_surface_ctx_t *output);
	void *private;
	void (*private_free)(struct decoder_ctx_struct *decoder);
	DWLLinearMem_t streamMem;
	uint32_t smBufLen;
	void *DWLinstance;
	void *pDecInst;
	PPInst pp;
//TODO next lines maybe move to another object
	uint32_t crop_width;
	uint32_t crop_height;
//	uint32_t crop_x;
//	uint32_t crop_y;
	uint32_t rotation;
} decoder_ctx_t;

typedef struct
{
	queue_target_ctx_t *target;
	VdpColor background;
	device_ctx_t *device;
	u32 DispMode;
	int Drw_x;
	int Drw_y;
	int Drw_w;
	int Drw_h;
} queue_ctx_t;

typedef struct
{
	device_ctx_t *device;
	int csc_change;
	float brightness;
	float contrast;
	float saturation;
	float hue;
} mixer_ctx_t;

#define RGBA_FLAG_DIRTY (1 << 0)
#define RGBA_FLAG_NEEDS_FLUSH (1 << 1)
#define RGBA_FLAG_NEEDS_CLEAR (1 << 2)

typedef struct
{
//	device_ctx_t *device;
	VdpRGBAFormat format;
	uint32_t width, height, size;
	void *data;
	VdpRect dirty;
	uint32_t flags;
} rgba_surface_t;

typedef struct
{
	device_ctx_t *device;
	rgba_surface_t rgba;
	video_surface_ctx_t *vs;
	VdpRect video_src_rect, video_dst_rect;
	int csc_change;
	float brightness;
	float contrast;
	float saturation;
	float hue;
	uint32_t OSDChKey;
} output_surface_ctx_t;

typedef struct
{
	device_ctx_t *device;
	rgba_surface_t rgba;
	VdpBool frequently_accessed;
} bitmap_surface_ctx_t;


#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof((a)) / sizeof((a)[0]))
#endif

#define max(a, b) \
	({ __typeof__ (a) _a = (a); \
	   __typeof__ (b) _b = (b); \
	  _a > _b ? _a : _b; })

#define min(a, b) \
	({ __typeof__ (a) _a = (a); \
	   __typeof__ (b) _b = (b); \
	  _a < _b ? _a : _b; })

#define min_nz(a, b) \
        ({ __typeof__ (a) _a = (a); \
           __typeof__ (b) _b = (b); \
           _a < _b ? (_a == 0 ? _b : _a) : (_b == 0 ? _a : _b); })

#define ALIGN(x, a) (((x) + ((typeof(x))(a) - 1)) & ~((typeof(x))(a) - 1))

#define VDPAU_ERR(format, ...) fprintf(stderr, "[VDPAU RK3X](%s):" format "\n",__func__ ,##__VA_ARGS__)

#ifdef DEBUG
#include <stdio.h>

#define VDPAU_DBG(dl, format, ...) {if(dl <= Dbg_Level ) fprintf(stderr, "[VDPAU RK3X](%s):" format "\n", __func__, ##__VA_ARGS__);}
#define VDPAU_DBG_ONCE(format, ...) do { static uint8_t __once; if (!__once) { fprintf(stderr, "[VDPAU RK3X] " format "\n", ##__VA_ARGS__); __once = 1; } } while(0)
#else
#define VDPAU_DBG(dl, dbg_lvl, format, ...)
#define VDPAU_DBG_ONCE(format, ...)
#endif

#define EXPORT __attribute__ ((visibility ("default")))

#define PROP_DEFAULT_SKIP_NON_REFERENCE False
#define PROP_DEFAULT_DISABLE_OUTPUT_REORDERING False
#define PROP_DEFAULT_INTRA_FREEZE_CONCEALMENT False
#define PROP_DEFAULT_USE_DISPLAY_SMOOTHING False
#define PROP_DEFAULT_NUM_FRAME_BUFFS 0
#define PROP_DEFAULT_DPB_FLAGS DEC_DPB_ALLOW_FIELD_ORDERING


#define AllocPhyMemPg(qt) AllocMemPg(qt, False) //new MemPg assigned to PutFbPtr->Next
#define InitPhyMemPg(qt) AllocMemPg(qt, True) //new MemPg assigned to PutFbPtr
int AllocMemPg(queue_target_ctx_t *qt, Bool init);
void FreeAllMemPg(queue_target_ctx_t *qt);
mem_fb_t *GetMemBlkForPut(queue_target_ctx_t *qt);
#define GetMemPgForPut(qt) GetMemBlkForPut(qt)->pMemBuf

int SetupOut(queue_target_ctx_t *qt, OvlLayoutFormatType DstFrmt, uint32_t xres, uint32_t yres);

VdpStatus new_decoder_mpeg2(decoder_ctx_t *decoder);
VdpStatus new_decoder_h264(decoder_ctx_t *decoder);
VdpStatus new_decoder_mpeg4(decoder_ctx_t *decoder);

VdpStatus vdpPPsetConfig(decoder_ctx_t *dec, video_surface_ctx_t *output, uint32_t pixformat, Bool interlaced);
VdpStatus vdpPPsetOutBuf(mem_fb_t *mempg, decoder_ctx_t *dec);
uint64_t get_time(void);

typedef uint32_t VdpHandle;

void *handle_create(size_t size, VdpHandle *handle);
void *handle_get(VdpHandle handle);
void handle_destroy(VdpHandle handle);

EXPORT VdpDeviceCreateX11 vdp_imp_device_create_x11;
VdpDeviceDestroy vdp_device_destroy;
VdpPreemptionCallbackRegister vdp_preemption_callback_register;

VdpGetProcAddress vdp_get_proc_address;

VdpGetErrorString vdp_get_error_string;
VdpGetApiVersion vdp_get_api_version;
VdpGetInformationString vdp_get_information_string;

VdpPresentationQueueTargetCreateX11 vdp_presentation_queue_target_create_x11;

VdpPresentationQueueTargetDestroy vdp_presentation_queue_target_destroy;
VdpPresentationQueueCreate vdp_presentation_queue_create;
VdpPresentationQueueDestroy vdp_presentation_queue_destroy;
VdpPresentationQueueSetBackgroundColor vdp_presentation_queue_set_background_color;
VdpPresentationQueueGetBackgroundColor vdp_presentation_queue_get_background_color;
VdpPresentationQueueGetTime vdp_presentation_queue_get_time;
VdpPresentationQueueDisplay vdp_presentation_queue_display;
VdpPresentationQueueBlockUntilSurfaceIdle vdp_presentation_queue_block_until_surface_idle;
VdpPresentationQueueQuerySurfaceStatus vdp_presentation_queue_query_surface_status;

VdpVideoSurfaceCreate vdp_video_surface_create;
VdpVideoSurfaceDestroy vdp_video_surface_destroy;
VdpVideoSurfaceGetParameters vdp_video_surface_get_parameters;
VdpVideoSurfaceGetBitsYCbCr vdp_video_surface_get_bits_y_cb_cr;
VdpVideoSurfacePutBitsYCbCr vdp_video_surface_put_bits_y_cb_cr;
VdpVideoSurfaceQueryCapabilities vdp_video_surface_query_capabilities;
VdpVideoSurfaceQueryGetPutBitsYCbCrCapabilities vdp_video_surface_query_get_put_bits_y_cb_cr_capabilities;

VdpOutputSurfaceCreate vdp_output_surface_create;
VdpOutputSurfaceDestroy vdp_output_surface_destroy;
VdpOutputSurfaceGetParameters vdp_output_surface_get_parameters;
VdpOutputSurfaceGetBitsNative vdp_output_surface_get_bits_native;
VdpOutputSurfacePutBitsNative vdp_output_surface_put_bits_native;
VdpOutputSurfacePutBitsIndexed vdp_output_surface_put_bits_indexed;
VdpOutputSurfacePutBitsYCbCr vdp_output_surface_put_bits_y_cb_cr;
VdpOutputSurfaceRenderOutputSurface vdp_output_surface_render_output_surface;
VdpOutputSurfaceRenderBitmapSurface vdp_output_surface_render_bitmap_surface;
VdpOutputSurfaceQueryCapabilities vdp_output_surface_query_capabilities;
VdpOutputSurfaceQueryGetPutBitsNativeCapabilities vdp_output_surface_query_get_put_bits_native_capabilities;
VdpOutputSurfaceQueryPutBitsIndexedCapabilities vdp_output_surface_query_put_bits_indexed_capabilities;
VdpOutputSurfaceQueryPutBitsYCbCrCapabilities vdp_output_surface_query_put_bits_y_cb_cr_capabilities;

VdpVideoMixerCreate vdp_video_mixer_create;
VdpVideoMixerDestroy vdp_video_mixer_destroy;
VdpVideoMixerRender vdp_video_mixer_render;
VdpVideoMixerGetFeatureSupport vdp_video_mixer_get_feature_support;
VdpVideoMixerSetFeatureEnables vdp_video_mixer_set_feature_enables;
VdpVideoMixerGetFeatureEnables vdp_video_mixer_get_feature_enables;
VdpVideoMixerSetAttributeValues vdp_video_mixer_set_attribute_values;
VdpVideoMixerGetParameterValues vdp_video_mixer_get_parameter_values;
VdpVideoMixerGetAttributeValues vdp_video_mixer_get_attribute_values;
VdpVideoMixerQueryFeatureSupport vdp_video_mixer_query_feature_support;
VdpVideoMixerQueryParameterSupport vdp_video_mixer_query_parameter_support;
VdpVideoMixerQueryParameterValueRange vdp_video_mixer_query_parameter_value_range;
VdpVideoMixerQueryAttributeSupport vdp_video_mixer_query_attribute_support;
VdpVideoMixerQueryAttributeValueRange vdp_video_mixer_query_attribute_value_range;
VdpGenerateCSCMatrix vdp_generate_csc_matrix;

VdpDecoderCreate vdp_decoder_create;
VdpDecoderDestroy vdp_decoder_destroy;
VdpDecoderGetParameters vdp_decoder_get_parameters;
VdpDecoderRender vdp_decoder_render;
VdpDecoderQueryCapabilities vdp_decoder_query_capabilities;

VdpBitmapSurfaceCreate vdp_bitmap_surface_create;
VdpBitmapSurfaceDestroy vdp_bitmap_surface_destroy;
VdpBitmapSurfaceGetParameters vdp_bitmap_surface_get_parameters;
VdpBitmapSurfacePutBitsNative vdp_bitmap_surface_put_bits_native;
VdpBitmapSurfaceQueryCapabilities vdp_bitmap_surface_query_capabilities;

#endif
