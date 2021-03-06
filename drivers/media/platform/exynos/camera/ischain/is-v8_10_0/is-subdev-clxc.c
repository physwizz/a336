/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "is-device-ischain.h"
#include "is-subdev-ctrl.h"
#include "is-config.h"
#include "is-param.h"
#include "is-video.h"
#include "is-type.h"

static int is_ischain_clxc_cfg(struct is_subdev *subdev,
	void *device_data,
	struct is_frame *frame,
	struct is_crop *incrop,
	struct is_crop *otcrop,
	IS_DECLARE_PMAP(pmap))
{
	return 0;
}

static int is_ischain_clxc_start(struct is_device_ischain *device,
	struct is_subdev *subdev,
	struct is_frame *frame,
	struct is_queue *queue,
	struct is_crop *otcrop,
	IS_DECLARE_PMAP(pmap))
{
	int ret = 0;
	struct param_dma_output *dma_output;
	struct is_fmt *format;
	u32 votf_dst_ip = 0;
	u32 votf_dst_axi_id_m = 0;

	FIMC_BUG(!queue);
	FIMC_BUG(!queue->framecfg.format);

	format = queue->framecfg.format;

	dma_output = is_itf_g_param(device, frame, PARAM_CLH_DMA_OUTPUT);
	dma_output->cmd = DMA_OUTPUT_COMMAND_ENABLE;
	dma_output->format = format->hw_format;
	dma_output->order = format->hw_order;
	dma_output->bitwidth = format->hw_bitwidth;
	dma_output->plane = format->hw_plane;
	dma_output->msb = format->hw_bitwidth - 1;
	dma_output->width = otcrop->w;
	dma_output->height = otcrop->h;
	dma_output->dma_crop_offset_x = 0;
	dma_output->dma_crop_offset_y = 0;
	dma_output->dma_crop_width = otcrop->w;
	dma_output->dma_crop_height = otcrop->h;

	dma_output->v_otf_enable = OTF_INPUT_COMMAND_DISABLE;
	dma_output->v_otf_dst_ip = votf_dst_ip;
	dma_output->v_otf_dst_axi_id = votf_dst_axi_id_m;
	dma_output->v_otf_token_line = VOTF_TOKEN_LINE;

	subdev->output.crop = *otcrop;

	set_bit(PARAM_CLH_DMA_OUTPUT, pmap);
	set_bit(IS_SUBDEV_RUN, &subdev->state);

	return ret;
}

static int is_ischain_clxc_stop(struct is_device_ischain *device,
	struct is_subdev *subdev,
	struct is_frame *frame,
	IS_DECLARE_PMAP(pmap))
{
	int ret = 0;
	struct param_dma_output *dma_output;

	mdbgd_ischain("%s\n", device, __func__);

	dma_output = is_itf_g_param(device, frame, PARAM_CLH_DMA_OUTPUT);
	dma_output->cmd = DMA_OUTPUT_COMMAND_DISABLE;

	set_bit(PARAM_CLH_DMA_OUTPUT, pmap);
	clear_bit(IS_SUBDEV_RUN, &subdev->state);

	return ret;
}

static int is_ischain_clxc_tag(struct is_subdev *subdev,
	void *device_data,
	struct is_frame *ldr_frame,
	struct camera2_node *node)
{
	int ret = 0;
	struct is_subdev *leader;
	struct is_queue *queue;
	struct clh_param *clh_param;
	struct is_crop *otcrop, otparm;
	struct is_device_ischain *device;
	IS_DECLARE_PMAP(pmap);
	u32 pixelformat = 0;

	device = (struct is_device_ischain *)device_data;

	FIMC_BUG(!device);
	FIMC_BUG(!device->is_region);
	FIMC_BUG(!subdev);
	FIMC_BUG(!GET_SUBDEV_QUEUE(subdev));
	FIMC_BUG(!ldr_frame);
	FIMC_BUG(!ldr_frame->shot);

	mdbgs_ischain(4, "CLHC TAG(request %d)\n", device, node->request);

	IS_INIT_PMAP(pmap);
	leader = subdev->leader;
	clh_param = &device->is_region->parameter.clh;
	queue = GET_SUBDEV_QUEUE(subdev);
	if (!queue) {
		merr("queue is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (!queue->framecfg.format) {
		merr("format is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	pixelformat = queue->framecfg.format->pixelformat;

	if (node->request) {
		otcrop = (struct is_crop *)node->output.cropRegion;
		otcrop->x = 0;
		otcrop->y = 0;
		otcrop->w = clh_param->dma_input.width;
		otcrop->h = clh_param->dma_input.height;

		otparm.x = 0;
		otparm.y = 0;
		otparm.w = clh_param->dma_output.width;
		otparm.h = clh_param->dma_output.height;

		if (!COMPARE_CROP(otcrop, &otparm) ||
			!test_bit(IS_SUBDEV_RUN, &subdev->state) ||
			test_bit(IS_SUBDEV_FORCE_SET, &leader->state)) {
			ret = is_ischain_clxc_start(device,
				subdev,
				ldr_frame,
				queue,
				otcrop,
				pmap);
			if (ret) {
				merr("is_ischain_clxc_start is fail(%d)", device, ret);
				goto p_err;
			}

			mdbg_pframe("ot_crop[%d, %d, %d, %d]\n", device, subdev, ldr_frame,
				otcrop->x, otcrop->y, otcrop->w, otcrop->h);
		}

		ret = is_ischain_buf_tag(device,
			subdev,
			ldr_frame,
			pixelformat,
			otcrop->w,
			otcrop->h,
			ldr_frame->clxcTargetAddress);
		if (ret) {
			mswarn("%d frame is drop", device, subdev, ldr_frame->fcount);
			node->request = 0;
		}
	} else {
		if (test_bit(IS_SUBDEV_RUN, &subdev->state)) {
			ret = is_ischain_clxc_stop(device,
				subdev,
				ldr_frame,
				pmap);
			if (ret) {
				merr("is_ischain_clxc_stop is fail(%d)", device, ret);
				goto p_err;
			}

			mdbg_pframe(" off\n", device, subdev, ldr_frame);
		}

		ldr_frame->clxcTargetAddress[0] = 0;
		ldr_frame->clxcTargetAddress[1] = 0;
		ldr_frame->clxcTargetAddress[2] = 0;
		node->request = 0;
	}

	ret = is_itf_s_param(device, ldr_frame, pmap);
	if (ret) {
		mrerr("is_itf_s_param is fail(%d)", device, ldr_frame, ret);
		goto p_err;
	}

p_err:
	return ret;
}

const struct is_subdev_ops is_subdev_clxc_ops = {
	.bypass			= NULL,
	.cfg			= is_ischain_clxc_cfg,
	.tag			= is_ischain_clxc_tag,
};
