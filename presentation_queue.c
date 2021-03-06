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
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>


int AllocMemPg(queue_target_ctx_t *qt, Bool init)
{
    int ret=-1;

    if(!init && qt->FbAllCnt >= MEMPG_MAX_CNT)
    	goto err;

    mem_fb_t *FbPtr = calloc(1, sizeof(mem_fb_t));

    if (!FbPtr)
    	goto err;

    FbPtr->pMemBuf = OvlAllocMemPg(qt->MemPgSize, 0);
    if(!FbPtr->pMemBuf){
    	ret =-2;
    	goto err1;
    }

    FbPtr->pMapMemBuf = OvlMapBufMem(FbPtr->pMemBuf);
    if(!FbPtr->pMapMemBuf){
    	ret =-3;
    	goto err2;
    }

    VDPAU_DBG(3, "ClrMemPg:%d", OvlClrMemPg(FbPtr->pMemBuf));

//    FbPtr->pYUVMapMemBuf = FbPtr->pMapMemBuf + OvlGetYUVoffsetMemPg(FbPtr->pMemBuf);
    FbPtr->UVoffset = OvlGetUVoffsetMemPg(FbPtr->pMemBuf);
    FbPtr->PhyAddr = OvlGetPhyAddrMemPg(FbPtr->pMemBuf);

    qt->FbAllCnt++;
    if(init){
    	FbPtr->Next = FbPtr;
    	qt->PutFbPtr = FbPtr;
    	qt->DispFbPtr = FbPtr;
//    	qt->WorkFbPtr = FbPtr;
    	qt->FbAllCnt = 1;
    	qt->FbFilledCnt = 2;//for display (one render + one prepare)
    }
    else{
        FbPtr->Next = qt->PutFbPtr->Next;
        qt->PutFbPtr->Next = FbPtr;
    }

    VDPAU_DBG(2, "init:%d Alloc new buf: %d %p", init,qt->FbAllCnt, FbPtr);
    return 0;

err2:
    OvlFreeMemPg(FbPtr->pMemBuf);
err1:
    free(FbPtr);
err:
    return ret;
}

void FreeAllMemPg(queue_target_ctx_t *qt)
{
    mem_fb_t *tmpFbPtr,*NextFbPtr;

    if(!qt->PutFbPtr)
	return;

    NextFbPtr = qt->PutFbPtr->Next;
    qt->PutFbPtr->Next = NULL;

    while(NextFbPtr){
	OvlFreeMemPg(NextFbPtr->pMemBuf);
	tmpFbPtr = NextFbPtr->Next;
        free(NextFbPtr);
	NextFbPtr = tmpFbPtr;
	qt->FbAllCnt--;
    }

    qt->PutFbPtr = NULL;
    VDPAU_DBG(2, "FbAllCnt: %d ", qt->FbAllCnt);
//    qt->FbAllCnt = 0;

    return;
}

static pthread_mutex_t out_buf_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t out_buf_cv = PTHREAD_COND_INITIALIZER;

mem_fb_t *GetMemBlkForPut(queue_target_ctx_t *qt)
{
	pthread_mutex_lock(&out_buf_mutex);
	while(qt->FbFilledCnt >= qt->FbAllCnt && !qt->drop_fl)
		pthread_cond_wait(&out_buf_cv, &out_buf_mutex);

	if(qt->drop_fl)
		qt->drop_fl--;
//	else
	if(qt->FbFilledCnt < qt->FbAllCnt){
	if( qt->PutFbPtr->Next !=  qt->DispFbPtr){
		qt->PutFbPtr = qt->PutFbPtr->Next;
		qt->FbFilledCnt++;
/*    if(qt->FbFilledCnt < qt->FbAllCnt || !AllocPhyMemPg(qt)){
    	if( qt->PutFbPtr->Next !=  qt->DispFbPtr){
    		qt->PutFbPtr = qt->PutFbPtr->Next;
    		qt->FbFilledCnt++;
    	}
    }else{
    	VDPAU_DBG(3, "Drop Pic");
    	*/
    }

    VDPAU_DBG(4, "FbFilledCnt:%d, drop_fl:%d, %p ",qt->FbFilledCnt, qt->drop_fl, qt->PutFbPtr);
	}

    pthread_mutex_unlock(&out_buf_mutex);

    return qt->PutFbPtr;
}

OvlMemPgPtr GetMemPgForDisp(queue_target_ctx_t *qt)
{
	pthread_mutex_lock(&out_buf_mutex);

    if(qt->FbFilledCnt > 2 && qt->DispFbPtr->Next != qt->PutFbPtr){
    	qt->DispFbPtr = qt->DispFbPtr->Next;
    	qt->FbFilledCnt--;
    	if(qt->FbFilledCnt == (qt->FbAllCnt - 1))
    		pthread_cond_signal(&out_buf_cv);
    }

//    qt->WorkFbPtr = qt->DispFbPtr->Next; //TODO check

    VDPAU_DBG(4, "FbFilledCnt:%d, %p",qt->FbFilledCnt, qt->DispFbPtr);

    pthread_mutex_unlock(&out_buf_mutex);

    return qt->DispFbPtr->pMemBuf;
}

void GetMemPgFake(queue_target_ctx_t *qt, int drop_fl)
{
	if(!qt || !qt->DispFbPtr)
		return;

	pthread_mutex_lock(&out_buf_mutex);
	if(qt->FbFilledCnt >= qt->FbAllCnt){
		qt->drop_fl = drop_fl;
		pthread_cond_signal(&out_buf_cv);
		VDPAU_DBG(4, "Drop pic:%d, drop_fl:%d, %p",qt->FbFilledCnt, qt->drop_fl, qt->DispFbPtr);
	}
	pthread_mutex_unlock(&out_buf_mutex);

}

uint64_t get_time(void)
{
	struct timespec tp;

	if (clock_gettime(CLOCK_MONOTONIC, &tp) == -1)
		return 0;

	return (uint64_t)tp.tv_sec * 1000000000ULL + (uint64_t)tp.tv_nsec;
}

VdpStatus vdp_presentation_queue_target_create_x11(VdpDevice device,
                                                   Drawable drawable,
                                                   VdpPresentationQueueTarget *target)
{
    int ret;
	VDPAU_DBG(1, "");
	if (!target || !drawable)
		return VDP_STATUS_INVALID_POINTER;

	device_ctx_t *dev = handle_get(device);
	if (!dev)
		return VDP_STATUS_INVALID_HANDLE;

	queue_target_ctx_t *qt = handle_create(sizeof(*qt), target);
	if (!qt)
		return VDP_STATUS_RESOURCES;

	qt->drawable = drawable;
	qt->device = dev;
	qt->DispFbPtr = NULL;
	ret = Open_RkLayers(0);
	if ( ret < 0)
	{
		VDPAU_ERR("Error Open_RkLayers():%d",ret);
		handle_destroy(*target);
		return VDP_STATUS_ERROR;
	}

	qt->VideoLayer = OvlAllocLay(SCALE_L, ALC_NONE_FB);
	if (qt->VideoLayer == ERROR_L)
		goto out_layer;

	qt->OSDLayer = ERROR_L;
	if (dev->osd_enabled)
	{
	    qt->OSDLayer = OvlAllocLay(UI_L, ALC_FRONT_FB);
	    if (qt->OSDLayer == ERROR_L)
		dev->osd_enabled = False;
	    else{
//		OvlSetModeFb(qt->OSDLayer, 0, 0, RKL_FORMAT_BGRA_8888);
		qt->OSD_DispMode = OvlGetModeByLay(qt->OSDLayer);
		VDPAU_DBG(2, "OSD mode:%d", qt->OSD_DispMode);
		qt->OSD_bpp = OvlGetBppByLay(qt->OSDLayer);
		qt->OSD_pitch = OvlGetVXresByLay(qt->OSDLayer) * qt->OSD_bpp;
		VDPAU_DBG(2, "OSD pitch:%d", qt->OSD_pitch);
		qt->OSDMemPg = OvlGetBufByLay(qt->OSDLayer, FRONT_FB);
		if(!qt->OSDMemPg){
		    dev->osd_enabled = False;
		    OvlFreeLay(qt->OSDLayer);
		}else{
		    qt->OSDmmap = OvlMapBufMem(qt->OSDMemPg);
		    if(!qt->OSDmmap){
			dev->osd_enabled = False;
			OvlFreeLay(qt->OSDLayer);
		    }else{
			qt->OSDdst = qt->OSDmmap;
			VDPAU_DBG(2, "OSD lay init ok");
//			OvlEnable(qt->OSDLayer, 1);
		    }
		}
	    }
	}

	qt->OSDColorKey = 0xff020202;
//	XSetWindowBackground(dev->display, drawable, 0x020202);
	XSetWindowBackground(dev->display, drawable, 0x0);

	OvlSetColorKey(qt->OSDColorKey);
//	OvlEnable(qt->VideoLayer, 1);

	qt->DSP_pitch = OvlGetVXresByLay(qt->VideoLayer);
        XGCValues gr_val;
        gr_val.function = GXcopy;
        gr_val.plane_mask = AllPlanes;
        gr_val.foreground = qt->OSDColorKey;
	gr_val.background = 0;
        qt->gr_context = XCreateGC(dev->display, qt->drawable,
	    GCFunction | GCPlaneMask | GCForeground/* | GCBackground*/, &gr_val); 

	qt->OSDRendrFlags = 0;
	qt->OSDShowFlags = 0;
	qt->drop_fl = 0;
	qt->flush_fl = 0;
	dev->queue_target = qt;
	VDPAU_DBG(2, "ok");
	return VDP_STATUS_OK;

out_fb:
	FreeAllMemPg(qt);
	OvlFreeLay(qt->VideoLayer);
out_layer:
	Close_RkLayers();
	handle_destroy(*target);
	return VDP_STATUS_RESOURCES;
}


VdpStatus vdp_presentation_queue_target_destroy(VdpPresentationQueueTarget presentation_queue_target)
{
	VDPAU_DBG(2, "");
	queue_target_ctx_t *qt = handle_get(presentation_queue_target);
	if (!qt)
		return VDP_STATUS_INVALID_HANDLE;

        if (qt->OSDLayer != ERROR_L)
	    OvlFreeLay(qt->OSDLayer);

	OvlFreeLay(qt->VideoLayer);

	FreeAllMemPg(qt);

	Close_RkLayers();

	XFreeGC(qt->device->display, qt->gr_context);

	handle_destroy(presentation_queue_target);

	return VDP_STATUS_OK;
}

VdpStatus vdp_presentation_queue_create(VdpDevice device,
                                        VdpPresentationQueueTarget presentation_queue_target,
                                        VdpPresentationQueue *presentation_queue)
{
	VDPAU_DBG(1, "");
	if (!presentation_queue)
		return VDP_STATUS_INVALID_POINTER;

	device_ctx_t *dev = handle_get(device);
	if (!dev)
		return VDP_STATUS_INVALID_HANDLE;

	queue_target_ctx_t *qt = handle_get(presentation_queue_target);
	if (!qt)
		return VDP_STATUS_INVALID_HANDLE;

	queue_ctx_t *q = handle_create(sizeof(*q), presentation_queue);
	if (!q)
		return VDP_STATUS_RESOURCES;

	q->target = qt;
	q->device = dev;

	q->DispMode = 0;
	q->Drw_x = 0;
	q->Drw_y = 0;
	q->Drw_w = 0;
	q->Drw_h = 0;

	VDPAU_DBG(2, "ok");
	return VDP_STATUS_OK;
}

VdpStatus vdp_presentation_queue_destroy(VdpPresentationQueue presentation_queue)
{
	queue_ctx_t *q = handle_get(presentation_queue);
	if (!q)
		return VDP_STATUS_INVALID_HANDLE;

	handle_destroy(presentation_queue);

	return VDP_STATUS_OK;
}

VdpStatus vdp_presentation_queue_set_background_color(VdpPresentationQueue presentation_queue,
                                                      VdpColor *const background_color)
{
	if (!background_color)
		return VDP_STATUS_INVALID_POINTER;

	queue_ctx_t *q = handle_get(presentation_queue);
	if (!q)
		return VDP_STATUS_INVALID_HANDLE;

	q->background.red = background_color->red;
	q->background.green = background_color->green;
	q->background.blue = background_color->blue;
	q->background.alpha = background_color->alpha;

	return VDP_STATUS_OK;
}

VdpStatus vdp_presentation_queue_get_background_color(VdpPresentationQueue presentation_queue,
                                                      VdpColor *const background_color)
{
	if (!background_color)
		return VDP_STATUS_INVALID_POINTER;

	queue_ctx_t *q = handle_get(presentation_queue);
	if (!q)
		return VDP_STATUS_INVALID_HANDLE;

	background_color->red = q->background.red;
	background_color->green = q->background.green;
	background_color->blue = q->background.blue;
	background_color->alpha = q->background.alpha;

	return VDP_STATUS_OK;
}

VdpStatus vdp_presentation_queue_get_time(VdpPresentationQueue presentation_queue,
                                          VdpTime *current_time)
{
	queue_ctx_t *q = handle_get(presentation_queue);
	if (!q)
		return VDP_STATUS_INVALID_HANDLE;

	*current_time = get_time();
	return VDP_STATUS_OK;
}

int SetupOut(queue_target_ctx_t *qt, OvlLayoutFormatType DstFrmt, uint32_t xres, uint32_t yres)
{
    int ret = OvlSetupFb(qt->VideoLayer, DstFrmt, xres, yres);
    qt->DSP_pitch = OvlGetVXresByLay(qt->VideoLayer) * OvlGetBppByLay(qt->VideoLayer);
    VDPAU_DBG(3, "Setup Fb, w:%d h:%d dpitch:%d", xres, yres, qt->DSP_pitch);
    return ret;
}

VdpStatus vdp_presentation_queue_display(VdpPresentationQueue presentation_queue,
                                         VdpOutputSurface surface,
                                         uint32_t clip_width,
                                         uint32_t clip_height,
                                         VdpTime earliest_presentation_time)
{
    queue_ctx_t *q = handle_get(presentation_queue);
    if (!q)
	return VDP_STATUS_INVALID_HANDLE;

    q->target->flush_fl = 0;

#ifdef DEBUG
int dt = (get_time() - q->device->tmr)/1000000;
    VDPAU_DBG(5, "time:%d mS", dt);
#endif
    output_surface_ctx_t *os = handle_get(surface);
    if (!os)
	return VDP_STATUS_INVALID_HANDLE;

/*    if (earliest_presentation_time != 0)
	VDPAU_DBG_ONCE("Presentation time not supported");
*/

    if (os->vs)
    {
//	int Src_w = os->video_src_rect.x1 - os->video_src_rect.x0;
//	int Src_h = os->video_src_rect.y1 - os->video_src_rect.y0;
	int Src_w = os->vs->width;
	int Src_h = os->vs->height;

	if(!q->DispMode){

	    switch (os->vs->source_format) {
	    case VDP_YCBCR_FORMAT_YUYV:
	    case VDP_YCBCR_FORMAT_UYVY:
		q->DispMode = RKL_FORMAT_YCbCr_422_SP;
		break;
	    case VDP_YCBCR_FORMAT_NV12:
	    case VDP_YCBCR_FORMAT_YV12:
		q->DispMode = RKL_FORMAT_YCrCb_NV12_SP;
		break;
	    default:
		q->DispMode = RKL_FORMAT_YCrCb_NV12_SP;
		break;
	    }
	    SetupOut(q->target, q->DispMode, Src_w, Src_h);
	    OvlEnable(q->target->VideoLayer, 1, 0);
	}

//	OvlWaitSync(q->target->VideoLayer);
	OvlLayerLinkMemPg( q->target->VideoLayer, GetMemPgForDisp(q->target));

	Window c;
	int x, y, Drw_w, Drw_h;
	Bool WinNeedClr = False;

	XTranslateCoordinates(q->device->display, q->target->drawable, RootWindow(q->device->display, q->device->screen), 0, 0, &x, &y, &c);

	x = x + os->video_dst_rect.x0;
	y = y + os->video_dst_rect.y0;
	Drw_w = os->video_dst_rect.x1 - os->video_dst_rect.x0;
	Drw_h = os->video_dst_rect.y1 - os->video_dst_rect.y0;

	if(q->Drw_x != x || q->Drw_y != y || q->Drw_w != Drw_w || q->Drw_h != Drw_h){

	    VDPAU_DBG(3, "changed... x:%d y:%d drw_w:%d drw_h:%d", x, y, Drw_w, Drw_h);
	    WinNeedClr = True;
	    XClearWindow(q->device->display, q->target->drawable);

	    if(q->device->osd_enabled){
		q->target->OSDdst = q->target->OSDmmap + y * q->target->OSD_pitch + x * q->target->OSD_bpp;
	    }

	    OvlSetupDrw(q->target->VideoLayer, x, y, Drw_w, Drw_h);

	    q->Drw_x = x;
	    q->Drw_y = y;
	    q->Drw_w = Drw_w;
	    q->Drw_h = Drw_h;

	    q->target->OSDRendrFlags = 0xffffffff;
	}

	if(q->device->osd_enabled){
	    if(q->target->OSDShowFlags & q->target->OSDRendrFlags){
//		OvlClrMemPg(q->target->OSDMemPg);
		WinNeedClr = True;
		VDPAU_DBG(5, "OSDShowFlags:%X OSDRendrFlags:%X", q->target->OSDShowFlags, q->target->OSDRendrFlags);
		q->target->OSDRendrFlags = 0;
	    }else
		q->target->OSDRendrFlags = ~q->target->OSDShowFlags;
	    q->target->OSDShowFlags = 0xffffffff;
	}

	if(WinNeedClr){

	    XFillRectangle(q->device->display, q->target->drawable, q->target->gr_context,
		os->video_dst_rect.x0, os->video_dst_rect.y0, Drw_w, Drw_h);
	}
    }
    else
    {
//		OvlEnable(q->device->VideoLayer, 0);
    }

#ifdef DEBUG
uint64_t ltmr = get_time();
dt = (ltmr - q->device->tmr)/1000000;
    VDPAU_DBG(5, "--- time:%d mS", dt);
q->device->tmr = ltmr;
#endif
	return VDP_STATUS_OK;
}

VdpStatus vdp_presentation_queue_block_until_surface_idle(VdpPresentationQueue presentation_queue,
                                                          VdpOutputSurface surface,
                                                          VdpTime *first_presentation_time)
{
	queue_ctx_t *q = handle_get(presentation_queue);
	if (!q)
		return VDP_STATUS_INVALID_HANDLE;

	output_surface_ctx_t *out = handle_get(surface);
	if (!out)
		return VDP_STATUS_INVALID_HANDLE;

	*first_presentation_time = get_time();

	return VDP_STATUS_OK;
}

VdpStatus vdp_presentation_queue_query_surface_status(VdpPresentationQueue presentation_queue,
                                                      VdpOutputSurface surface,
                                                      VdpPresentationQueueStatus *status,
                                                      VdpTime *first_presentation_time)
{
	queue_ctx_t *q = handle_get(presentation_queue);
	if (!q)
		return VDP_STATUS_INVALID_HANDLE;

	output_surface_ctx_t *out = handle_get(surface);
	if (!out)
		return VDP_STATUS_INVALID_HANDLE;

	*status = VDP_PRESENTATION_QUEUE_STATUS_VISIBLE;
	*first_presentation_time = get_time();

	return VDP_STATUS_OK;
}
