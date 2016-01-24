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
    VDPAU_ERR("decode error (%d) %s", err, err_name);
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

void *mpeg2_decode(void *args)
{
	decoder_ctx_t *decoder = (decoder_ctx_t *)args;
    VdpPictureInfoMPEG1Or2 const *info;
    mpeg2_private_t *decoder_p = (mpeg2_private_t *)decoder->private;
    queue_target_ctx_t *qt;

    u32 picNumber = 0;
    Mpeg2DecInput decIn;
    Mpeg2DecOutput decOut;
    Mpeg2DecInfo decInfo;
    Mpeg2DecPicture decPic;

    Mpeg2DecRet decRet;
    Mpeg2DecRet infoRet;

//    int secondField = 1;

    int forceflush = 0;

/*
	if (!info->resync_marker_disable)
	{
		VDPAU_DBG("We can't decode VOPs with resync markers yet! Sorry");
		return VDP_STATUS_ERROR;
	}
*/
    VDPAU_DBG(3, "thread run");
    do{
    	in_mem_fb_t *in_buf = PopInBuf(decoder);
    	if(decoder->th_stat < 0 ){
    		return VDP_STATUS_OK;
    	}

    	if(!picNumber){
    		info = (VdpPictureInfoMPEG1Or2 const *)decoder->pic_info;
    		qt = decoder->device->queue_target;
    		decIn.skipNonReference = PROP_DEFAULT_SKIP_NON_REFERENCE;
    	}

    	VDPAU_DBG(5, "stream InLen ******* %d ********", in_buf->data_size);

    	decIn.pStream = (u8 *)decoder->streamMem.virtualAddress + in_buf->offset;
    	decIn.streamBusAddress = decoder->streamMem.busAddress + in_buf->offset;
    	decIn.dataLen = in_buf->data_size;

    	VDPAU_DBG(5, "virtualAddress:%p, offset:%d, busAddress:%X, decIn.streamBusAddress:%X",
    			decoder->streamMem.virtualAddress, in_buf->offset, decoder->streamMem.busAddress, decIn.streamBusAddress);

donext:
    	decIn.picId = picNumber;
    	decRet = Mpeg2DecDecode(decoder_p->mpeg2dec, &decIn, &decOut);

    	switch (decRet) {
    	case MPEG2DEC_HDRS_RDY:
	// read stream info 
    		infoRet = Mpeg2DecGetInfo(decoder_p->mpeg2dec, &decInfo);
    		SetFormat( decoder->vs, decInfo.outputFormat);
    		VDPAU_DBG(1, "stream, %dx%d, interlaced %d, format %x",
    				decInfo.frameWidth, decInfo.frameHeight,
					decInfo.interlacedSequence, decInfo.outputFormat);
    		decoder->dec_width = decInfo.frameWidth;
    		decoder->dec_height = decInfo.frameHeight;
    		if (decoder->pp)
    			vdpPPsetConfig(decoder, decoder->vs, decInfo.outputFormat, decInfo.interlacedSequence);
    		break;
    	case MPEG2DEC_PIC_DECODED:
	// a picture was decoded 
    		picNumber++;
doflush:
			while (Mpeg2DecNextPicture(decoder_p->mpeg2dec, &decPic, forceflush) == MPEG2DEC_PIC_RDY && decoder->th_stat >= 0) {

				if ((decPic.fieldPicture && !decPic.firstField) || !decPic.fieldPicture) {
					VDPAU_DBG(5 ,"decoded picture %d, mpeg2 timestamp:%d-%d:%d:%d",
							decPic.picId, decPic.interlaced,
							decPic.timeCode.hours, decPic.timeCode.minutes,
							decPic.timeCode.seconds);

					if(decoder->pp){
						vdpPPsetOutBuf( GetMemBlkForPut(qt), decoder);
					}else{
						uint32_t length = decoder->dec_width * decoder->dec_height;
						OvlCopyNV12SemiPlanarToFb(GetMemPgForPut(qt), decPic.pOutputPicture,
								decPic.pOutputPicture+length,
								qt->DSP_pitch, decoder->dec_width,
								decoder->dec_width, decoder->dec_height);
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
    		if(decoder->th_stat < 0 )
    			return VDP_STATUS_OK;

    	}

    	if (decOut.dataLeft > 0 && decoder->th_stat >= 0)
        {
            decIn.dataLen = decOut.dataLeft;
            decIn.pStream = decOut.pStrmCurrPos;
            decIn.streamBusAddress = decOut.strmCurrBusAddress;
            goto donext;
        }

    }while(decoder->th_stat >= 0 );

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
