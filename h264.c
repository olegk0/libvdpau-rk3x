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
#include <inc/h264decapi.h>

typedef struct
{
    H264DecInst h264dec;
    SPS_t pSeqParamSet;
    PPS_t pPicParamSet;
} h264_private_t;

static void h264_private_free(decoder_ctx_t *decoder)
{
	h264_private_t *decoder_p = (h264_private_t *)decoder->private;
	H264DecRelease(decoder_p->h264dec);

	free(decoder_p);
}

static void SetFormat(video_surface_ctx_t *output, uint32_t format)
{
    switch(format){
    case H264DEC_SEMIPLANAR_YUV420:
	output->source_format = VDP_YCBCR_FORMAT_NV12;
	break;
//    case H264DEC_TILED_YUV420:
//    case H264DEC_YUV400:
    }
}

static void h264_init_header(decoder_ctx_t *decoder, VdpPictureInfoH264 const *info, video_surface_ctx_t *output)
{
    h264_private_t *decoder_p = (h264_private_t *)decoder->private;

    decoder_p->pSeqParamSet.numRefFrames = info->num_ref_frames;
//info.h264.num_ref_frames               = h->sps.ref_frame_count;
    decoder_p->pSeqParamSet.mbAdaptiveFrameFieldFlag = info->mb_adaptive_frame_field_flag;
//	if(! info->mb_adaptive_frame_field_flag)
//	    decoder_p->pSeqParamSet. = 1; //Check
//info.h264.mb_adaptive_frame_field_flag = h->sps.mb_aff && !render->info.h264.field_pic_flag;
    decoder_p->pSeqParamSet.frameMbsOnlyFlag = info->frame_mbs_only_flag;
//info.h264.frame_mbs_only_flag      = h->sps.frame_mbs_only_flag;
    decoder_p->pSeqParamSet.maxFrameNum = 1 << (info->log2_max_frame_num_minus4+4);
//info.h264.log2_max_frame_num_minus4  = h->sps.log2_max_frame_num - 4;
    decoder_p->pSeqParamSet.picOrderCntType = info->pic_order_cnt_type;
//info.h264.pic_order_cnt_type         = h->sps.poc_type;
    decoder_p->pSeqParamSet.maxPicOrderCntLsb = 1 << (info->log2_max_pic_order_cnt_lsb_minus4+4);
//info.h264.log2_max_pic_order_cnt_lsb_minus4 = h->sps.poc_type ? 0 : h->sps.log2_max_poc_lsb - 4;
    decoder_p->pSeqParamSet.deltaPicOrderAlwaysZeroFlag = info->delta_pic_order_always_zero_flag;
//info.h264.delta_pic_order_always_zero_flag = h->sps.delta_pic_order_always_zero_flag;
    decoder_p->pSeqParamSet.direct8x8InferenceFlag = info->direct_8x8_inference_flag;
//info.h264.direct_8x8_inference_flag  = h->sps.direct_8x8_inference_flag;
    decoder_p->pSeqParamSet.picWidthInMbs = (output->width+15) / 16;
//TODO check
//    if (info->frame_mbs_only_flag)
	decoder_p->pSeqParamSet.picHeightInMbs = (output->height+15) / 16;
//    else
//	decoder_p->pSeqParamSet.picHeightInMbs = ((decoder->height / 2)+15) / 16;

//TODO next lines only for test
    decoder_p->pSeqParamSet.maxDpbSize = decoder_p->pSeqParamSet.numRefFrames;
    decoder_p->pSeqParamSet.chromaFormatIdc = 1;//?
    decoder_p->pSeqParamSet.scalingMatrixPresentFlag = 0;//?

    VDPAU_DBG(1, "SPS.picWidthInMbs: %d", decoder_p->pSeqParamSet.picWidthInMbs);
    VDPAU_DBG(1, "SPS.picHeightInMbs: %d", decoder_p->pSeqParamSet.picHeightInMbs);
    VDPAU_DBG(1, "SPS.maxDpbSize: %d", decoder_p->pSeqParamSet.maxDpbSize);
    VDPAU_DBG(1, "SPS.numRefFrames: %d", decoder_p->pSeqParamSet.numRefFrames);
    VDPAU_DBG(1, "SPS.maxFrameNum: %d", decoder_p->pSeqParamSet.maxFrameNum);
    VDPAU_DBG(1, "SPS.monoChrome: %d", decoder_p->pSeqParamSet.monoChrome);
    VDPAU_DBG(1, "SPS.frameMbsOnlyFlag: %d", decoder_p->pSeqParamSet.frameMbsOnlyFlag);
    VDPAU_DBG(1, "SPS.chromaFormatIdc: %d", decoder_p->pSeqParamSet.chromaFormatIdc);
    VDPAU_DBG(1, "SPS.scalingMatrixPresentFlag: %d", decoder_p->pSeqParamSet.scalingMatrixPresentFlag);
//VDPAU_DBG("SPS.: %d", decoder_p->pSeqParamSet.);
//VDPAU_DBG("SPS.: %d", decoder_p->pSeqParamSet.);

    h264bsdStoreSeqParamSetExt(decoder_p->h264dec, &decoder_p->pSeqParamSet);

    decoder_p->pPicParamSet.constrainedIntraPredFlag = info->constrained_intra_pred_flag;
//render->info.h264.constrained_intra_pred_flag = h->pps.constrained_intra_pred;
    decoder_p->pPicParamSet.weightedPredFlag = info->weighted_pred_flag;
//render->info.h264.weighted_pred_flag          = h->pps.weighted_pred;
    decoder_p->pPicParamSet.weightedBiPredIdc = info->weighted_bipred_idc;
//render->info.h264.weighted_bipred_idc         = h->pps.weighted_bipred_idc;
    decoder_p->pPicParamSet.transform8x8Flag = info->transform_8x8_mode_flag;
//render->info.h264.transform_8x8_mode_flag     = h->pps.transform_8x8_mode;
    decoder_p->pPicParamSet.chromaQpIndexOffset = info->chroma_qp_index_offset;
//render->info.h264.chroma_qp_index_offset      = h->pps.chroma_qp_index_offset[0];
    decoder_p->pPicParamSet.chromaQpIndexOffset2 = info->second_chroma_qp_index_offset;
//render->info.h264.second_chroma_qp_index_offset  = h->pps.chroma_qp_index_offset[1];
    decoder_p->pPicParamSet.picInitQp = info->pic_init_qp_minus26 + 26;
//render->info.h264.pic_init_qp_minus26         = h->pps.init_qp - 26;
    decoder_p->pPicParamSet.numRefIdxL0Active = info->num_ref_idx_l0_active_minus1 + 1;
//render->info.h264.num_ref_idx_l0_active_minus1   = h->pps.ref_count[0] - 1;
    decoder_p->pPicParamSet.numRefIdxL1Active = info->num_ref_idx_l1_active_minus1 + 1;
//render->info.h264.num_ref_idx_l1_active_minus1   = h->pps.ref_count[1] - 1;
    decoder_p->pPicParamSet.entropyCodingModeFlag = info->entropy_coding_mode_flag;
//render->info.h264.entropy_coding_mode_flag = h->pps.cabac;
    decoder_p->pPicParamSet.picOrderPresentFlag = info->pic_order_present_flag;
//render->info.h264.pic_order_present_flag  = h->pps.pic_order_present;
    decoder_p->pPicParamSet.deblockingFilterControlPresentFlag = info->deblocking_filter_control_present_flag;
//render->info.h264.deblocking_filter_control_present_flag = h->pps.deblocking_filter_parameters_present;
    decoder_p->pPicParamSet.redundantPicCntPresentFlag = info->redundant_pic_cnt_present_flag;
//render->info.h264.redundant_pic_cnt_present_flag  = h->pps.redundant_pic_cnt_present;
//	decoder_p->pPicParamSet. = info->;
//memcpy(render->info.h264.scaling_lists_4x4, h->pps.scaling_matrix4, sizeof(render->info.h264.scaling_lists_4x4));

    memcpy(decoder_p->pPicParamSet.scalingList[0] ,info->scaling_lists_8x8[0], sizeof(info->scaling_lists_8x8[0]));
//memcpy(render->info.h264.scaling_lists_8x8[0], h->pps.scaling_matrix8[0], sizeof(render->info.h264.scaling_lists_8x8[0]));
    memcpy(decoder_p->pPicParamSet.scalingList[3] ,info->scaling_lists_8x8[1], sizeof(info->scaling_lists_8x8[1]));

    //memcpy(render->info.h264.scaling_lists_8x8[1], h->pps.scaling_matrix8[3], sizeof(render->info.h264.scaling_lists_8x8[0]));
//    uint8_t scaling_lists_8x8[2][64];

//TODO next lines only for test
    decoder_p->pPicParamSet.numSliceGroups = 1;
    decoder_p->pPicParamSet.runLength = NULL;
    decoder_p->pPicParamSet.scalingMatrixPresentFlag = 0;
//	decoder_p->pPicParamSet.

    VDPAU_DBG(1, "PPS.numSliceGroups: %d", decoder_p->pPicParamSet.numSliceGroups);
    VDPAU_DBG(1, "PPS.runLength: %p", decoder_p->pPicParamSet.runLength);
    VDPAU_DBG(1, "PPS.entropyCodingModeFlag: %d", decoder_p->pPicParamSet.entropyCodingModeFlag);
    VDPAU_DBG(1, "PPS.weightedPredFlag: %d", decoder_p->pPicParamSet.weightedPredFlag);
    VDPAU_DBG(1, "PPS.weightedBiPredIdc: %d", decoder_p->pPicParamSet.weightedBiPredIdc);
    VDPAU_DBG(1, "PPS.transform8x8Flag: %d", decoder_p->pPicParamSet.transform8x8Flag);
    VDPAU_DBG(1, "PPS.scalingMatrixPresentFlag: %d", decoder_p->pPicParamSet.scalingMatrixPresentFlag);
//VDPAU_DBG("PPS.: %d", decoder_p->pPicParamSet.);

    h264bsdStorePicParamSetExt(decoder_p->h264dec, &decoder_p->pPicParamSet);
}

static void h264_error(int err)
{
    char *err_name;

    switch(err){
    case H264DEC_PARAM_ERROR:
	err_name = "H264DEC_PARAM_ERROR";
	break;
    case H264DEC_STRM_ERROR:
	err_name = "H264DEC_STRM_ERROR";
	break;
    case H264DEC_NOT_INITIALIZED:
	err_name = "H264DEC_NOT_INITIALIZED";
	break;
    case H264DEC_MEMFAIL:
	err_name = "H264DEC_MEMFAIL";
	break;
    case H264DEC_INITFAIL:
	err_name = "H264DEC_INITFAIL";
	break;
    case H264DEC_HDRS_NOT_RDY:
	err_name = "H264DEC_HDRS_NOT_RDY";
	break;
    case H264DEC_STREAM_NOT_SUPPORTED:
	err_name = "H264DEC_STREAM_NOT_SUPPORTED";
	break;
    case H264DEC_HW_RESERVED:
	err_name = "H264DEC_HW_RESERVED";
	break;
    case H264DEC_HW_TIMEOUT:
	err_name = "H264DEC_HW_TIMEOUT";
	break;
    case H264DEC_HW_BUS_ERROR:
	err_name = "H264DEC_HW_BUS_ERROR";
	break;
    case H264DEC_SYSTEM_ERROR:
	err_name = "H264DEC_SYSTEM_ERROR";
	break;
    case H264DEC_DWL_ERROR:
	err_name = "H264DEC_DWL_ERROR";
	break;
    default:
	err_name = "H264DEC_UNKNOWN_ERROR";
    }
    VDPAU_ERR("decode error (%d) %s", err, err_name);
}

void *h264_decode(void *args)
{
	decoder_ctx_t *decoder = (decoder_ctx_t *)args;
    VdpPictureInfoH264 const *info;
    h264_private_t *decoder_p = (h264_private_t *)decoder->private;
    queue_target_ctx_t *qt;

    H264DecRet decRet;
    H264DecRet infoRet;
    u32 picNumber = 0;
    Bool FirstTime = True;
    H264DecInput decIn;
    H264DecOutput decOut;
    H264DecInfo decInfo;
    H264DecPicture decPic;

    int forceflush = 0;

    VDPAU_DBG(3, "thread run");
    do{
    	in_mem_fb_t *in_buf = PopInBuf(decoder);
    	if(decoder->th_stat < 0 ){
    		return VDP_STATUS_OK;
    	}

    	if(FirstTime){
    		info = (VdpPictureInfoH264 const *)decoder->pic_info;
    		qt = decoder->device->queue_target;
    		h264_init_header(decoder, info, decoder->vs);
    		FirstTime = False;
    		decIn.skipNonReference = PROP_DEFAULT_SKIP_NON_REFERENCE;
    		decIn.picId = 0;
    	}

    	VDPAU_DBG(5, "stream InLen ******* %d ********", in_buf->data_size);

    	decIn.pStream = (u8 *)decoder->streamMem.virtualAddress + in_buf->offset;
    	decIn.streamBusAddress = decoder->streamMem.busAddress + in_buf->offset;
    	decIn.dataLen = in_buf->data_size;

    	VDPAU_DBG(5, "virtualAddress:%p, offset:%d, busAddress:%X, decIn.streamBusAddress:%X",
    			decoder->streamMem.virtualAddress, in_buf->offset, decoder->streamMem.busAddress, decIn.streamBusAddress);

donext:
    	decIn.picId = picNumber;
    	decRet = H264DecDecode(decoder_p->h264dec, &decIn, &decOut);

    	switch (decRet) {
    	case H264DEC_HDRS_RDY:
/* read stream info */
    		infoRet = H264DecGetInfo(decoder_p->h264dec, &decInfo);
    		SetFormat(decoder->vs, decInfo.outputFormat);
    		VDPAU_DBG(1, "H264 stream, %dx%d format %x",
    				decInfo.picWidth, decInfo.picHeight, decInfo.outputFormat);
    		decoder->dec_width = decInfo.picWidth;
    		decoder->dec_height = decInfo.picHeight;
    		if (decoder->pp)
    			vdpPPsetConfig(decoder, decoder->vs, decInfo.outputFormat, 0);
    		break;
    	case H264DEC_ADVANCED_TOOLS:/* the decoder has to reallocate ressources */
    	case H264DEC_NONREF_PIC_SKIPPED:
    		break;
    	case H264DEC_PIC_DECODED:
/* a picture was decoded */
    		picNumber++;
doflush:
			while (H264DecNextPicture(decoder_p->h264dec, &decPic, forceflush) == H264DEC_PIC_RDY && decoder->th_stat >= 0)
			{

				if(decoder->pp){
					vdpPPsetOutBuf( GetMemBlkForPut(qt), decoder);
				}else{
					uint32_t length = decoder->dec_width * decoder->dec_height;
					OvlCopyNV12SemiPlanarToFb(GetMemPgForPut(qt), decPic.pOutputPicture,
							(uint8_t *)decPic.pOutputPicture+length,
							qt->DSP_pitch, decoder->dec_width,
							decoder->dec_width, decoder->dec_height);
				}

			}
			break;
    	case H264DEC_STRM_PROCESSED:
/* input stream processed but no picture ready */
//	VDPAU_DBG("H264DEC_STRM_PROCESSED\n");
    		break;
    	case H264DEC_STRM_ERROR:
/* input stream processed but no picture ready */
//	VDPAU_DBG("H264DEC_STRM_ERROR\n");
    		break;
    	default:
/* some kind of error, decoding cannot continue */
    		h264_error(decRet);
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

VdpStatus new_decoder_h264(decoder_ctx_t *decoder)
{
    int ret;
    h264_private_t *decoder_p = calloc(1, sizeof(h264_private_t));

    if (!decoder_p)
    	goto err_priv;

    ret = H264DecInit(&decoder_p->h264dec, PROP_DEFAULT_DISABLE_OUTPUT_REORDERING,
    		PROP_DEFAULT_INTRA_FREEZE_CONCEALMENT,
            PROP_DEFAULT_USE_DISPLAY_SMOOTHING,
            PROP_DEFAULT_DPB_FLAGS);

    if (ret != H264DEC_OK){
    	VDPAU_ERR("Init error:%d",ret);
    	goto err_free;
    }

    decoder->decode = h264_decode;
    decoder->private = decoder_p;
    decoder->private_free = h264_private_free;
    decoder->DWLinstance = H264ToDWLinstance(decoder_p->h264dec);
    decoder->pDecInst = decoder_p->h264dec;

    return VDP_STATUS_OK;

err_free:
    free(decoder_p);
err_priv:
    return VDP_STATUS_RESOURCES;
}
