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

/* copy img 1bpp */
void copy_img_1b(struct img_pack *const out, const struct img_pack *const in,
		    int width, int height) asm("copy_bytes_asm");

/* copy img 4bpp */
void copy_img_4b(struct img_pack *const out, const struct img_pack *const in,
		    int width, int height) asm("copy_words_asm");

/* Unpack indexed image*/
void iargb_argb(struct img_pack *const out,
                     const struct index_pack *const in,
                     int width, int height) asm("iargb_argb_asm");

/* Unpack indexed image with aplha replace*/
void iargb_argb_arp(struct img_a8_pack *const out,
                     const struct index_pack *const in,
                     int width, int height) asm("iargb_argb_arp_asm");

/* Unpack a8 image. width and src pitch - align by 2 */
void a8_argb(struct img_pack *const out,
                     const struct img_a8_pack *const in,
                     int width, int height) asm("a8_argb_asm");

/* Unpack a8 image with aplha replace. width and src pitch - align by 2 */
void a8_argb_arp(struct img_a8_pack *const out,
                     const struct img_a8_pack *const in,
                     int width, int height) asm("a8_argb_arp_asm");

