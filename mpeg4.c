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
#include <inc/mp4decapi.h>


typedef struct
{
    MP4DecInst mpeg4dec;

} mpeg4_private_t;

static void mpeg4_private_free(decoder_ctx_t *decoder)
{
	mpeg4_private_t *decoder_p = (mpeg4_private_t *)decoder->private;
	MP4DecRelease(decoder_p->mpeg4dec);

	free(decoder_p);
}

static void mpeg4_error(int err)
{
    char *err_name;

    switch(err){
    case MP4DEC_PARAM_ERROR:
	err_name = "MP4DEC_PARAM_ERROR";
	break;
    case MP4DEC_STRM_ERROR:
	err_name = "MP4DEC_STRM_ERROR";
	break;
    case MP4DEC_NOT_INITIALIZED:
	err_name = "MP4DEC_NOT_INITIALIZED";
	break;
    case MP4DEC_MEMFAIL:
	err_name = "MP4DEC_MEMFAIL";
	break;
    case MP4DEC_INITFAIL:
	err_name = "MP4DEC_INITFAIL";
	break;
    case MP4DEC_FORMAT_NOT_SUPPORTED:
	err_name = "MP4DEC_FORMAT_NOT_SUPPORTED";
	break;
    case MP4DEC_STRM_NOT_SUPPORTED:
	err_name = "MP4DEC_STRM_NOT_SUPPORTED";
	break;
    case MP4DEC_HW_RESERVED:
	err_name = "MP4DEC_HW_RESERVED";
	break;
    case MP4DEC_HW_TIMEOUT:
	err_name = "MP4DEC_HW_TIMEOUT";
	break;
    case MP4DEC_HW_BUS_ERROR:
	err_name = "MP4DEC_HW_BUS_ERROR";
	break;
    case MP4DEC_SYSTEM_ERROR:
	err_name = "MP4DEC_SYSTEM_ERROR";
	break;
    case MP4DEC_DWL_ERROR:
	err_name = "MP4DEC_DWL_ERROR";
	break;
    default:
	err_name = "MP4DEC_UNKNOWN_ERROR";
    }
    VDPAU_ERR("decode error (%d) %s", err, err_name);
}

static void SetFormat(video_surface_ctx_t *output, uint32_t format)
{
    switch(format){
    case MP4DEC_SEMIPLANAR_YUV420:
	output->source_format = VDP_YCBCR_FORMAT_NV12;
	break;
//    case MP4DEC_TILED_YUV420:
    }
}

void *mpeg4_decode(void *args)
{
	decoder_ctx_t *decoder = (decoder_ctx_t *)args;
    VdpPictureInfoMPEG4Part2 const *info;
    mpeg4_private_t *decoder_p = (mpeg4_private_t *)decoder->private;
    queue_target_ctx_t *qt;

    u32 picNumber = 0;
    MP4DecInput decIn;
    MP4DecOutput decOut;
    MP4DecInfo decInfo;
    MP4DecPicture decPic;
    int LasttimeIncr = 0;

    MP4DecRet decRet;
    MP4DecRet infoRet;

    int secondField = 1;
//    GstBuffer *image;
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
    		info = (VdpPictureInfoMPEG4Part2 const *)decoder->pic_info;
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
    	decRet = MP4DecDecode(decoder_p->mpeg4dec, &decIn, &decOut);

    	switch (decRet) {
    	case MP4DEC_HDRS_RDY:
	// read stream info 
    		infoRet = MP4DecGetInfo(decoder_p->mpeg4dec, &decInfo);
    		SetFormat( decoder->vs, decInfo.outputFormat);
    		VDPAU_DBG(1, "MPEG4 stream, %dx%d, interlaced %d, format %x",
    				decInfo.frameWidth, decInfo.frameHeight,
					decInfo.interlacedSequence, decInfo.outputFormat);
    		decoder->dec_width = decInfo.frameWidth;
    		decoder->dec_height = decInfo.frameHeight;
    		if (decoder->pp)
    			vdpPPsetConfig(decoder, decoder->vs, decInfo.outputFormat, decInfo.interlacedSequence);

    		break;
    	case MP4DEC_PIC_DECODED:
	// a picture was decoded 
    		picNumber++;

doflush:
			while(MP4DecNextPicture(decoder_p->mpeg4dec, &decPic, forceflush) == MP4DEC_PIC_RDY  && decoder->th_stat >= 0) {

				if(decPic.timeCode.timeIncr == LasttimeIncr)
					qt->drop_fl = 1;
//					continue;
				LasttimeIncr = decPic.timeCode.timeIncr;

				if ((decPic.fieldPicture && secondField) || !decPic.fieldPicture)
				{
					VDPAU_DBG(5 ,"play_mpeg4: decoded picture %d,fieldPicture %d, secondField %d,  mpeg4 timestamp: %d:%d:%d:%d (%d)",
							picNumber, decPic.fieldPicture, secondField,
							decPic.timeCode.hours, decPic.timeCode.minutes,
							decPic.timeCode.seconds, decPic.timeCode.timeIncr,
							decPic.timeCode.timeRes);
					if (decPic.fieldPicture)
						secondField = 0;

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
				else
					if (decPic.fieldPicture)
						secondField = 1;

			}

			break;
    	case MP4DEC_NONREF_PIC_SKIPPED:
	// Skipped non-reference picture
//    		VDPAU_DBG(5 ,"MP4DEC_NONREF_PIC_SKIPPED");
    		break;
    	case MP4DEC_STRM_PROCESSED:
	// input stream processed but no picture ready
//    		VDPAU_DBG(5 ,"MP4DEC_STRM_PROCESSED");
    		break;
    	case MP4DEC_STRM_ERROR:
	// input stream processed but no picture ready
//    		VDPAU_DBG(5 ,"MP4DEC_STRM_ERROR");
    		break;
    	case MP4DEC_VOS_END:
	// end of stream 
    		decOut.dataLeft = 0;
    		break;
    	default:
	// some kind of error, decoding cannot continue 
    		mpeg4_error(decRet);
    		if(decoder->th_stat < 0 )
    			return VDP_STATUS_OK;
    		decoder->th_stat = VDP_STATUS_ERROR;
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

VdpStatus new_decoder_mpeg4(decoder_ctx_t *decoder)
{
    int ret, subtype;
    mpeg4_private_t *decoder_p = calloc(1, sizeof(mpeg4_private_t));

    if (!decoder_p)
	goto err_priv;

    switch(decoder->profile){
    case VDP_DECODER_PROFILE_MPEG4_PART2_SP:
    case VDP_DECODER_PROFILE_MPEG4_PART2_ASP:
	subtype = MP4DEC_MPEG4;
	break;
    case VDP_DECODER_PROFILE_DIVX4_QMOBILE:
    case VDP_DECODER_PROFILE_DIVX4_MOBILE:
    case VDP_DECODER_PROFILE_DIVX4_HOME_THEATER:
    case VDP_DECODER_PROFILE_DIVX4_HD_1080P:
    case VDP_DECODER_PROFILE_DIVX5_QMOBILE:
    case VDP_DECODER_PROFILE_DIVX5_MOBILE:
    case VDP_DECODER_PROFILE_DIVX5_HOME_THEATER:
    case VDP_DECODER_PROFILE_DIVX5_HD_1080P:
    default:
	subtype = MP4DEC_CUSTOM_1;
	break;
    }//MP4DEC_SORENSON

    ret = MP4DecInit(&decoder_p->mpeg4dec, subtype, PROP_DEFAULT_INTRA_FREEZE_CONCEALMENT,
                         PROP_DEFAULT_NUM_FRAME_BUFFS,
                         PROP_DEFAULT_DPB_FLAGS);
    if (ret != MP4DEC_OK){
	VDPAU_ERR("Init error:%d",ret);
	goto err_free;
    }

    decoder->decode = mpeg4_decode;
    decoder->private = decoder_p;
    decoder->private_free = mpeg4_private_free;
    decoder->DWLinstance = MPEG4ToDWLinstance(decoder_p->mpeg4dec);
    decoder->pDecInst = decoder_p->mpeg4dec;

    return VDP_STATUS_OK;

err_free:
    free(decoder_p);
err_priv:
    return VDP_STATUS_RESOURCES;
}
