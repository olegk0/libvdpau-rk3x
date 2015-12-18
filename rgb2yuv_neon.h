/*****************************************************************************
 * 2015 olegk0 <olegvedi@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include <stdint.h>

struct index_pack
{
    uint8_t  *I8;
    uint8_t  *A8;
    uint32_t *clr_tblYUV;
    size_t pitch;
};

struct img_pack
{
    void *data;
    size_t pitch;
};

struct img_a8_pack
{
    uint8_t *A8;
    uint32_t Color;
    size_t pitch;
};

struct y_uv_plan
{
    uint8_t *y;
    uint16_t *uv;
    size_t pitch;
};

/* copy bytes width >3 */
void copy_img(struct img_pack *const out, const struct img_pack *const in,
		    int width, int height) asm("copy_bytes_asm");

/* Unpack indexed image */
void iargb_argb(struct img_pack *const out,
                     const struct index_pack *const in,
                     int width, int height) asm("iargb_argb_asm");

/* Convert color table from XRGB32 to XYVU. */
void ct_rgb2yuv(uint32_t *const outYUV,
		const uint32_t *const inRGB, int size) asm("ct_rgb2yuv_neon");

/* Mix two YUV sources , store to background. (yuv420)*/
void mix_yuv_semiplanar(struct y_uv_plan *const bg,
                     const struct img_pack *const in,
                     int width, int height) asm("mix_yuv_semiplanar_neon");

/* Mix YUV and A8 bitmap sources , store to YUV background. (yuv420)*/
void mix_a8_yuv_semiplanar(struct y_uv_plan *const bg,
                     const struct img_a8_pack *const in,
                     int width, int height) asm("mix_a2yuv_semiplanar_neon");

