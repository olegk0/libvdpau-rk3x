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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "vdpau_private.h"
#include <inc/mpeg2decapi.h>


typedef struct
{
    Mpeg2DecInst mpeg2dec;
    u32 picNumber;
//    int width;
//    int height;
    Mpeg2DecInput decIn;
    Mpeg2DecOutput decOut;
    Mpeg2DecInfo decInfo;
    Mpeg2DecPicture decPic;
} mpeg2_private_t;

static void mpeg2_private_free(decoder_ctx_t *decoder)
{
	mpeg2_private_t *decoder_p = (mpeg2_private_t *)decoder->private;
	Mpeg2DecRelease(decoder_p->mpeg2dec);

	free(decoder_p);
}

static void mpeg2_error(int err)
{
    char *err_name;

    switch(err){
    case MPEG2DEC_PARAM_ERROR:
	err_name = "MPEG2DEC_PARAM_ERROR";
	break;
    case MPEG2DEC_STRM_ERROR:
	err_name = "MPEG2DEC_STRM_ERROR";
	break;
    case MPEG2DEC_NOT_INITIALIZED:
	err_name = "MPEG2DEC_NOT_INITIALIZED";
	break;
    case MPEG2DEC_MEMFAIL:
	err_name = "MPEG2DEC_MEMFAIL";
	break;
    case MPEG2DEC_INITFAIL:
	err_name = "MPEG2DEC_INITFAIL";
	break;
    case MPEG2DEC_STREAM_NOT_SUPPORTED:
	err_name = "MPEG2DEC_STREAM_NOT_SUPPORTED";
	break;
    case MPEG2DEC_FORMAT_NOT_SUPPORTED:
	err_name = "MPEG2DEC_FORMAT_NOT_SUPPORTED";
	break;
    case MPEG2DEC_HW_RESERVED:
	err_name = "MPEG2DEC_HW_RESERVED";
	break;
    case MPEG2DEC_HW_TIMEOUT:
	err_name = "MPEG2DEC_HW_TIMEOUT";
	break;
    case MPEG2DEC_HW_BUS_ERROR:
	err_name = "MPEG2DEC_HW_BUS_ERROR";
	break;
    case MPEG2DEC_SYSTEM_ERROR:
	err_name = "MPEG2DEC_SYSTEM_ERROR";
	break;
    case MPEG2DEC_DWL_ERROR:
	err_name = "MPEG2DEC_DWL_ERROR";
	break;
    default:
	err_name = "MPEG2DEC_UNKNOWN_ERROR";
    }
    VDPAU_ERR("decode error (%d) %s\n", err, err_name);
}

static void SetFormat(video_surface_ctx_t *output, uint32_t format)
{
    switch(format){
    case MPEG2DEC_SEMIPLANAR_YUV420:
	output->source_format = VDP_YCBCR_FORMAT_NV12;
	break;
//    case MPEG2DEC_TILED_YUV420:
    }
}

static VdpStatus mpeg2_decode(decoder_ctx_t *decoder,
                              VdpPictureInfo const *_info,
                              int *len,
                              video_surface_ctx_t *output)
{
    VdpPictureInfoMPEG1Or2 const *info = (VdpPictureInfoMPEG1Or2 const *)_info;
    mpeg2_private_t *decoder_p = (mpeg2_private_t *)decoder->private;
    queue_target_ctx_t *qt = decoder->device->queue_target;

/*    Mpeg2DecInput decIn;
    Mpeg2DecOutput decOut;
    Mpeg2DecInfo decInfo;
    Mpeg2DecPicture decPic;
*/
    Mpeg2DecRet decRet;
    Mpeg2DecRet infoRet;

//    int secondField = 1;
//    GstBuffer *image;
    int forceflush = 0, flush =0;

/*
	if (!info->resync_marker_disable)
	{
		VDPAU_DBG("We can't decode VOPs with resync markers yet! Sorry");
		return VDP_STATUS_ERROR;
	}
*/

    decoder_p->decIn.pStream = (u8 *) decoder->streamMem.virtualAddress;
    decoder_p->decIn.streamBusAddress = decoder->streamMem.busAddress;
    decoder_p->decIn.dataLen = *len;

    VDPAU_DBG(4, "MPEG2 stream len ******* %d ********\n", *len);

donext:
    decoder_p->decIn.picId = decoder_p->picNumber;
    decRet = Mpeg2DecDecode(decoder_p->mpeg2dec, &decoder_p->decIn, &decoder_p->decOut);

    switch (decRet) {
    case MPEG2DEC_HDRS_RDY:
	// read stream info 
	infoRet = Mpeg2DecGetInfo(decoder_p->mpeg2dec, &decoder_p->decInfo);
	SetFormat( output, decoder_p->decInfo.outputFormat);
	VDPAU_DBG(1, "MPEG2 stream, %dx%d, interlaced %d, format %x\n",
	    decoder_p->decInfo.frameWidth, decoder_p->decInfo.frameHeight,
	    decoder_p->decInfo.interlacedSequence, decoder_p->decInfo.outputFormat);
	decoder->dec_width = decoder_p->decInfo.frameWidth;
	decoder->dec_height = decoder_p->decInfo.frameHeight;
	if (decoder->pp)
	    vdpPPsetConfig(decoder, decoder_p->decInfo.outputFormat, decoder_p->decInfo.interlacedSequence);
	break;
    case MPEG2DEC_PIC_DECODED:
	// a picture was decoded 
	decoder_p->picNumber++;
doflush:
	while (Mpeg2DecNextPicture(
	    decoder_p->mpeg2dec, &decoder_p->decPic, forceflush) == MPEG2DEC_PIC_RDY) {

	    if ((decoder_p->decPic.fieldPicture && !decoder_p->decPic.firstField) || !decoder_p->decPic.fieldPicture) {
		qt->PicBalance++;
		VDPAU_DBG(4 ,"play_mpeg2: decoded picture %d, PicBalance:%d, mpeg2 timestamp:%d-%d:%d:%d\n",
			 decoder_p->decPic.picId, qt->PicBalance, decoder_p->decPic.interlaced,
			 decoder_p->decPic.timeCode.hours, decoder_p->decPic.timeCode.minutes,
			 decoder_p->decPic.timeCode.seconds);
		if(qt->PicBalance < (MEMPG_MAX_CNT - 2))
		{
		    if(decoder->pp){
			vdpPPsetOutBuf( GetMemBlkForPut(qt), decoder);
		    }else{
			uint32_t length = decoder->dec_width * decoder->dec_height;
			OvlCopyNV12SemiPlanarToFb(GetMemPgForPut(qt), decoder_p->decPic.pOutputPicture,\
			    decoder_p->decPic.pOutputPicture+length,
			    decoder->dec_width, decoder->device->src_width,
			    decoder->dec_width, decoder->dec_height);
		    }
		}else{
		    qt->PicBalance--;
		    VDPAU_DBG(4, "Drop pic\n");
		}
	    }
	}
	break;
//    case MPEG2DEC_NONREF_PIC_SKIPPED:
	// Skipped non-reference picture
//	break;
    case MPEG2DEC_STRM_PROCESSED:
	// input stream processed but no picture ready 
	break;
    case MPEG2DEC_STRM_ERROR:
	// input stream processed but no picture ready 
	break;
    default:
	// some kind of error, decoding cannot continue 
	mpeg2_error(decRet);
	VDPAU_DBG(4, "MPEG2 stream ERR ------ decRet:%d  out_left:%d in_len:%d\n", decRet, decoder_p->decOut.dataLeft, decoder_p->decIn.dataLen);
	return VDP_STATUS_ERROR;
    }

//    VDPAU_DBG("MPEG2 stream +++++ decRet:%d  out_left:%d in_len:%d\n", decRet, decOut.dataLeft, decIn.dataLen);

    if (decoder_p->decOut.dataLeft > 0)
    {
	decoder_p->decIn.dataLen = decoder_p->decOut.dataLeft;
	decoder_p->decIn.pStream = decoder_p->decOut.pStrmCurrPos;
	decoder_p->decIn.streamBusAddress = decoder_p->decOut.strmCurrBusAddress;
	goto donext;
    }

    if (flush && !forceflush) {
	forceflush = 1;
	goto doflush;
    }

    *len = (u8 *)decoder_p->decOut.pStrmCurrPos - (u8 *)decoder->streamMem.virtualAddress;

//    VDPAU_DBG("MPEG2 stream *+*+*+*+ decRet:%d  out_left:%d in_len:%d len:%d\n", decRet, decOut.dataLeft, decIn.dataLen, *len);

    return VDP_STATUS_OK;
}

VdpStatus new_decoder_mpeg2(decoder_ctx_t *decoder)
{
    int ret, subtype;
    mpeg2_private_t *decoder_p = calloc(1, sizeof(mpeg2_private_t));

    if (!decoder_p)
	goto err_priv;

    ret = Mpeg2DecInit(&decoder_p->mpeg2dec, PROP_DEFAULT_INTRA_FREEZE_CONCEALMENT,
                         PROP_DEFAULT_NUM_FRAME_BUFFS,
                         PROP_DEFAULT_DPB_FLAGS);
    if (ret != MPEG2DEC_OK){
	VDPAU_ERR("Init error:%d",ret);
	goto err_free;
    }

    decoder_p->picNumber = 0;

    decoder->decode = mpeg2_decode;
    decoder->private = decoder_p;
    decoder->private_free = mpeg2_private_free;
    decoder->DWLinstance = MPEG2ToDWLinstance(decoder_p->mpeg2dec);
    decoder->pDecInst = decoder_p->mpeg2dec;

    return VDP_STATUS_OK;

err_free:
    free(decoder_p);
err_priv:
    return VDP_STATUS_RESOURCES;
}
