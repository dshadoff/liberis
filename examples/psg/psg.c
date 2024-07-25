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
#include <pcfx/romfont.h>
#include <pcfx/king.h>
#include <pcfx/tetsu.h>

#include <pcfx/sound.h>

void printch(u32 sjis, u32 kram, int tall);
void printstr(const char* str, int x, int y, int tall);

void delay(void);

// this waveform is not quite a sine wave but
// difficult to describe. Look at the waveform
// in the mednafen debugger to see it
//
char waveform[] = {
	0x00, 0x0c, 0x06, 0x12, 0x0c, 0x18, 0x12, 0x1e,
	0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f,
	0x1e, 0x12, 0x18, 0x0c, 0x12, 0x06, 0x0c, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

int main(int argc, char *argv[])
{
	int i, l;
	u16 microprog[16];

	king_init();
	tetsu_init();
	
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
	tetsu_set_video_mode(TETSU_LINES_262, 0, TETSU_DOTCLOCK_5MHz, TETSU_COLORS_16,
				TETSU_COLORS_16, 0, 0, 1, 0, 0, 0, 0);
	king_set_bat_cg_addr(KING_BG0, 0, 0);
	king_set_bat_cg_addr(KING_BG0SUB, 0, 0);
	king_set_scroll(KING_BG0, 0, 0);
	king_set_bg_size(KING_BG0, KING_BGSIZE_256, KING_BGSIZE_256, KING_BGSIZE_256, KING_BGSIZE_256);

	king_set_kram_read(0, 1);
	king_set_kram_write(0, 1);
	// Clear BG0's RAM
	for(i = 0x0; i < 0x1E00; i++) {
		king_kram_write(0);
	}
	king_set_kram_write(0, 1);
	printstr("SoundBox PSG Example", 5, 0x10, 1);

	psg_set_main_volume(12, 12);
	for(i = 0; i < 5; i++) {
		psg_set_channel(i);
		psg_set_balance(0xF, 0xF);
		psg_set_volume(0, 0, 0);

		for(l = 0; l < 32; l++) {
			psg_waveform_data(waveform[l]);
		}
	}

	delay();

	psg_set_channel(0);
	psg_set_freq(428); /* C4 */
	psg_set_volume(0x1F, 1, 0);

	delay();

	psg_set_channel(1);
	psg_set_freq(339); /* E4 */
	psg_set_volume(0x1F, 1, 0);

	delay();

	psg_set_channel(2);
	psg_set_freq(285); /* G4 (to make major 'C' chord)  */
	psg_set_volume(0x1F, 1, 0);

	for (i = 0; i < 12; i++) {
		delay();
	}

	// stop the music
	psg_set_channel(0);
	psg_set_volume(0x00, 0, 0);
	psg_set_channel(1);
	psg_set_volume(0x00, 0, 0);
	psg_set_channel(2);
	psg_set_volume(0x00, 0, 0);

	delay();
	delay();

	// Now make some noise
	psg_set_channel(5);
	psg_set_noise(0xF, 1);
	psg_set_balance(0xF, 0xF);
	psg_set_volume(0x1F, 1, 0);

	return 0;
}


// Note: this isn't a good way to implement a delay, but
// this will suffice for a trivial example.
//
// For proper delays, look at the hello_interrupt example
//
void delay(void)
{
	int i;
	for(i = 0; i < 0x160000; i++) {
		asm("mov r0, r0");
	}
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

