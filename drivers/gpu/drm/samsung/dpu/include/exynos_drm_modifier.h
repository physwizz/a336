// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS DRM modifier header file for Samsung EXYNOS DPU driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef EXYNOS_DRM_MODIFIER_H
#define EXYNOS_DRM_MODIFIER_H

#include <uapi/drm/drm_fourcc.h>

/*
 * Set to access the secure buffer
 *
 * The secure buffer is used to store DRM(Digital Right Management) contents.
 * DMA needs special authority to access the secure buffer. This modifier can
 * be set to allow the DMA to access the secure buffer. This can be used in
 * combination with another modifier.
 */
#define DRM_FORMAT_MOD_PROTECTION	fourcc_mod_code(NONE, (1ULL << 51))

/*
 * SAMSUNG DRM Modifiers
 * [63:0]	: full modifier range, [63:56] vendor name [55:0] specific modifier
 * [3:0]	: 64x32 TILE = 1, 16x16 TILE = 2, 8P2 split = X, colormap = 4
 * [8:4]	: SBWC
 * [13:9]	: VOTF_IN
 * [16:14]	: SAJC
 * [55:17]	: Reserved
 */

/*
 * The colormap uses the color data generated by hardware instead of reading
 * the data from the memory.
 *
 * It supports only solid color in BGRA8888 format. When it is used as
 * a modifier, BGRA8888 format should be used and color value is passed through
 * first handles[0].
 */
#define DRM_FORMAT_MOD_SAMSUNG_COLORMAP		fourcc_mod_code(SAMSUNG, 4)

/*
 * Samsung Virtual On The Fly (VOTF) modifier
 */
#define VOTF_IDENTIFIER				(1 << 9)
#define VOTF_BUF_IDX_MASK		(0xfULL << 10)
#define VOTF_BUF_IDX_SET(buf_idx)		\
		((buf_idx << 10) & VOTF_BUF_IDX_MASK)
#define VOTF_BUF_IDX_GET(modifier)		\
		(((modifier) & VOTF_BUF_IDX_MASK) >> 10)

#define DRM_FORMAT_MOD_SAMSUNG_VOTF(buf_idx)	\
		fourcc_mod_code(SAMSUNG,	\
		(VOTF_BUF_IDX_SET(buf_idx) | VOTF_IDENTIFIER))

/*
 * Samsung Band Width Compression (SBWC) modifier
 *
 * SBWC is a specific lossless or lossy image compression protocol and format.
 * It supports video image (YUV) compression to reduce the amount of data
 * transferred between IP blocks. This modifier is used when to decode data or
 * when to encode data through writeback.
 */
#define SBWC_IDENTIFIER				(1 << 4)
#define SBWC_FORMAT_MOD_LOSSY			(1 << 5)
#define SBWC_FORMAT_MOD_LOSSLESS		(0 << 5)
#define SBWC_FORMAT_MOD_BLK_BYTE_MASK		(0x7ULL << 6)
#define SBWC_BLK_BYTE_SET(blk_byte)		\
		((blk_byte << 6) & SBWC_FORMAT_MOD_BLK_BYTE_MASK)
#define SBWC_BLK_BYTE_GET(modifier)		\
		(((modifier) & SBWC_FORMAT_MOD_BLK_BYTE_MASK) >> 6)
#define SBWC_FORMAT_MOD_BLK_BYTENUM_32x2		(2ULL)
#define SBWC_FORMAT_MOD_BLK_BYTENUM_32x3		(3ULL)
#define SBWC_FORMAT_MOD_BLK_BYTENUM_32x4		(4ULL)
#define SBWC_FORMAT_MOD_BLK_BYTENUM_32x5		(5ULL)
#define SBWC_FORMAT_MOD_BLK_BYTENUM_32x6		(6ULL)

#define DRM_FORMAT_MOD_SAMSUNG_SBWC(blk_byte, lossy)		       \
		fourcc_mod_code(SAMSUNG, (SBWC_BLK_BYTE_SET(blk_byte) |\
			lossy | SBWC_IDENTIFIER))

/*
 * Samsung AMD Joint Compression (SAJC) modifier
 *
 * SAJC is a specific lossless image compression protocol and format.
 * It supports graphic image (RGB) compression to reduce the amount of data
 * transferred between IP blocks.
 */
#define SAJC_IDENTIFIER				(1 << 14)
#define SAJC_FORMAT_MOD_BLK_SIZE_MASK		(0x3ULL << 15)
#define SAJC_BLK_SIZE_SET(blk_size)		\
		((blk_size << 15) & SAJC_FORMAT_MOD_BLK_SIZE_MASK)
#define SAJC_BLK_SIZE_GET(modifier)		\
		(((modifier) & SAJC_FORMAT_MOD_BLK_SIZE_MASK) >> 15)
#define DRM_FORMAT_MOD_SAMSUNG_SAJC(blk_size)		       \
		fourcc_mod_code(SAMSUNG, (SAJC_BLK_SIZE_SET(blk_size) |\
					SAJC_IDENTIFIER))

/* from 52 to 55 bit are reserved for AFBC encoder source informaton */
#define AFBC_FORMAT_MOD_SOURCE_MASK	(0xfULL << 52)
#define AFBC_FORMAT_MOD_SOURCE_GPU	(1ULL << 52)
#define AFBC_FORMAT_MOD_SOURCE_G2D	(2ULL << 52)

#endif /* EXYNOS_DRM_MODIFIER_H */
