/*
	libpcfx -- A set of libraries for controlling the NEC PC-FX
                   Based on liberis by Alex Marshall

Copyright (C) 2011		Alex Marshall "trap15" <trap15@raidenii.net>
      and (C) 2024		David Shadoff  GitHub userid: dshadoff


# This code is licensed to you under the terms of the MIT license;
# see file LICENSE or http://www.opensource.org/licenses/mit-license.php
*/

#include <string.h>

#include <pcfx/types.h>
#include <pcfx/v810.h>
#include <pcfx/contrlr.h>
#include <pcfx/romfont.h>
#include <pcfx/king.h>
#include <pcfx/tetsu.h>
#include <pcfx/vdc.h>

void printch(u32 sjis, u32 kram, int tall);
void printstr(const char* str, int x, int y, int tall);

// This data is sprite encoding data for a 16x16 '+' sign
//
const uint16_t spr_data[] = {
  0x0180,0x0180,0x0180,0x0180,0x0180,0x0180,0x0180,0xFFFF,
  0xFFFF,0x0180,0x0180,0x0180,0x0180,0x0180,0x0180,0x0180,
  0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
  0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
  0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
  0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
  0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
  0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000
};

const uint16_t sprite_image_load_addr = 0x4000;

int main(int argc, char *argv[])
{
	int i, x, y, xl, yl;
	u16 microprog[16];
	u32 pad;

	vdc_init_5MHz(VDC0);
	vdc_init_5MHz(VDC1);
	vdc_set_satb_address(VDC0, 0x7000);  // just to show that it doesn't have to be 0xff00

	king_init();
	tetsu_init();
	contrlr_pad_init(0);
	
	tetsu_set_priorities(0, 0, 1, 0, 0, 0, 0);
	tetsu_set_vdc_palette(0, 0);
	tetsu_set_king_palette(0, 0, 0, 0);
	tetsu_set_rainbow_palette(0);

	king_set_bg_prio(KING_BGPRIO_3, KING_BGPRIO_HIDE, KING_BGPRIO_HIDE, KING_BGPRIO_HIDE, 0);
	king_set_bg_mode(KING_BGMODE_4_PAL, 0, 0, 0);
	king_set_kram_pages(0, 0, 0, 0);

	for(i = 0; i < 16; i++) {
		microprog[i] = KING_CODE_NOP;
	}

	microprog[0] = KING_CODE_BG0_CG_0;
	king_disable_microprogram();
	king_write_microprogram(microprog, 0, 16);
	king_enable_microprogram();

	tetsu_set_palette(0, 0x0088);
	tetsu_set_palette(1, 0xE088);
	tetsu_set_palette(2, 0xE0F0);
	tetsu_set_palette(3, 0x602C);
	tetsu_set_palette(4, 0x5080);
	tetsu_set_palette(5, 0xC422);
	tetsu_set_palette(6, 0x9999);
	tetsu_set_palette(7, 0x1234);
	tetsu_set_video_mode(TETSU_LINES_262, 0, TETSU_DOTCLOCK_5MHz, TETSU_COLORS_16,
				TETSU_COLORS_16, 0, 1, 1, 0, 0, 0, 0);
	king_set_bat_cg_addr(KING_BG0, 0, 0);
	king_set_bat_cg_addr(KING_BG0SUB, 0, 0);
	king_set_scroll(KING_BG0, 0, 0);
	king_set_bg_size(KING_BG0, KING_BGSIZE_256, KING_BGSIZE_256, KING_BGSIZE_256, KING_BGSIZE_256);

        vdc_setreg(VDC0, VDC_REG_CR, VDC_CR_SB);
        vdc_setreg(VDC0, VDC_REG_MWR, VDC_MWR_SCREEN_32x32);
	vdc_set_scroll(VDC0, 0, 0);

	king_set_kram_read(0, 1);
	king_set_kram_write(0, 1);

	// Clear BG0's RAM
	for(i = 0; i < 0x1E00; i++) {
		king_kram_write(0);
	}

	// load sprite data
	// -> Place at VRAM address set by sprite_image_load_addr
	//
	vdc_set_vram_write(VDC0, sprite_image_load_addr);
	for(i = 0; i < 8*4; i++) {
		vdc_vram_write(VDC0, spr_data[i]); /* sprite is plus sign */
	}

	vdc_set(VDC0);
	vdc_spr_set(0);  // Use sprite #0, but could be any (0 - 63)
	vdc_spr_create(0, 0, VDC_SPR_PATTERN(sprite_image_load_addr), 0);

	king_set_kram_write(0, 1);
	printstr("VDC sprite example", 7, 0x10, 1);

	x = 0x40;  // note that X-offsets are not 0-relative (0x20 is the left edge of the screen)
	y = 0x60;  // note that Y-offsets are not 0-relative (0x40 is the top edge of the screen)

	xl = yl = 1;
	for(;;) {
		vdc_spr_xy(x, y);

		/* crude way to make a delay */
	        /* Following examples should wait for VBlank */
		for(i = 0; i < 0x6000; i++) {
			asm("mov r0, r0");
		}
		
		pad = contrlr_pad_read(0);

		if((yl != 1) && (pad & JOY_DOWN))
			y++;
		else if((yl != -1) && (pad & JOY_UP))
			y--;
		if((xl != 1) && (pad & JOY_RIGHT))
			x++;
		else if((xl != -1) && (pad & JOY_LEFT))
			x--;
		if(x > 256+32-16) xl = 1;
		else if(x < 32)   xl = -1;
		else              xl = 0;
		if(y > 238+64-16) yl = 1;
		else if(y < 64+2) yl = -1;
		else              yl = 0;
	}

	return 0;
}

void printstr(const char* str, int x, int y, int tall)
{
	int i;
	u32 tempstr;

	u32 kram = x + (y << 5);
	int len = strlen(str);
	for(i = 0; i < len; i++) {
		tempstr = str[i];
		printch(tempstr, kram + i, tall);
	}
}

void printch(u32 sjis, u32 kram, int tall)
{
	u16 px;
	int x, y;
	u8* glyph = romfont_get(sjis, tall ? ROMFONT_ANK_8x16 : ROMFONT_ANK_8x8);
	for(y = 0; y < (tall ? 16 : 8); y++) {
		king_set_kram_write(kram + (y << 5), 1);
		px = 0;
		for(x = 0; x < 8; x++) {
			if((glyph[y] >> x) & 1) {
				px |= 1 << (x << 1);
			}
		}
		king_kram_write(px);
	}
}

