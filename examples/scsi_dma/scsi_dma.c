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
#include <pcfx/contrlr.h>
#include <pcfx/king.h>
#include <pcfx/tetsu.h>

#include <eris/cd.h>
#include <eris/scsi.h>

#include "lbas.h"

void printch(u32 sjis, u32 kram, int tall);
void printstr(const char* str, int x, int y, int tall);
void printhex(void* data, int x, int y, int bytes, int addr, int tall);
char x1toa(int val);

u32 test_data_in(u8 *buf, u32 maxlen);

#define CLEAN_SCSICDB(cmd, len) { \
	scsicdb[0] = cmd; \
	for(i = 1; i < 32; i++) { \
		scsicdb[i] = 0; \
	} \
}

const u16 pornpal[] = {
0x79A7, 0x57A7, 0x35A7, 0x0088, 0xA687, 0xD987, 0x7387, 0x5187, 0x8C6A, 0xC16A, 0xDB79, 0x4F6B, 0xE779, 0xFF88, 0xA66B, 0x907C, 
};

int main(int argc, char *argv[])
{
	int i;
	u16 microprog[16];
	u32 paddata = 0;
	u32 lastpad = 0;

	king_init();
	tetsu_init();
	
	tetsu_set_priorities(0, 0, 1, 0, 0, 0, 0);
	tetsu_set_king_palette(0, 0, 0, 0);
	tetsu_set_rainbow_palette(0);

	king_set_bg_prio(KING_BGPRIO_3, KING_BGPRIO_HIDE, KING_BGPRIO_HIDE, KING_BGPRIO_HIDE, 0);
	king_set_bg_mode(KING_BGMODE_16_PAL, 0, 0, 0);
	king_set_kram_pages(0, 0, 0, 0);

	for(i = 0; i < 16; i++) {
		microprog[i] = KING_CODE_NOP;
	}

	microprog[0] = KING_CODE_BG0_CG_0;
	microprog[1] = KING_CODE_BG0_CG_1;
	king_disable_microprogram();
	king_write_microprogram(microprog, 0, 16);
	king_enable_microprogram();

	for(i = 0; i < 16; i++) {
		tetsu_set_palette(i, pornpal[i]);
	}

	tetsu_set_video_mode(TETSU_LINES_262, 0, TETSU_DOTCLOCK_5MHz, TETSU_COLORS_16,
				TETSU_COLORS_16, 0, 0, 1, 0, 0, 0, 0);
	king_set_bat_cg_addr(KING_BG0, 0, 0);
	king_set_bat_cg_addr(KING_BG0SUB, 0, 0);
	king_set_scroll(KING_BG0, 0, 0);
	king_set_bg_size(KING_BG0, KING_BGSIZE_256, KING_BGSIZE_256, KING_BGSIZE_256, KING_BGSIZE_256);

	king_set_kram_read(0, 1);
	king_set_kram_write(0, 1);
	// Clear BG0's RAM
	for(i = 0x0; i < 0x7800; i++) {
		king_kram_write(0);
	}
	king_set_kram_write(0, 1);
	contrlr_pad_init(0);

	printstr("SCSI Porn", 10, 0x8, 1);
	for(;;) {
		lastpad = paddata;
		paddata = contrlr_pad_read(0);
		if(paddata & (1 << 6) && !(lastpad & (1 << 6))) { // Select
			eris_scsi_abort();
		}
		if(paddata & (1 << 7) && !(lastpad & (1 << 7))) { // Run
			eris_scsi_reset();
		}
		if(paddata & (1 << 0) && !(lastpad & (1 << 0))) { // (I) DMA to KRAM
			eris_cd_read_kram(BINARY_LBA_PORN_BIN, 0x600, 0x6800);
		}
	}

	return 0;
}

char x1toa(int val)
{
	val &= 0xF;
	if(val >= 0xA)
		return (val - 0xA) + 'A';
	else
		return val + '0';
}

void printhex(void* data, int x, int y, int bytes, int addr, int tall)
{
	char tmpstr[256];
	int tmpptr = 0;
	int i, l;
	for(i = 0; i < bytes; i += 8) {
		tmpptr = 0;
		if(addr) {
			tmpstr[tmpptr++] = x1toa(i >> 12);
			tmpstr[tmpptr++] = x1toa(i >> 8);
			tmpstr[tmpptr++] = x1toa(i >> 4);
			tmpstr[tmpptr++] = x1toa(i);
			tmpstr[tmpptr++] = ' ';
			tmpstr[tmpptr++] = ' ';
		}
		for(l = 0; (l < 8) && ((l + i) < bytes); l++) {
			tmpstr[tmpptr++] = x1toa(((char*)data)[i + l] >> 4);
			tmpstr[tmpptr++] = x1toa(((char*)data)[i + l]);
			tmpstr[tmpptr++] = ' ';
		}
		tmpstr[tmpptr] = 0;
		printstr(tmpstr, x, y + i, tall);
	}
}

void printstr(const char* str, int x, int y, int tall)
{
	int i;
	u32 tempstr;

	u32 kram = (x + (y << 5)) << 1;
	int len = strlen(str);
	for(i = 0; i < len; i++) {
		tempstr = str[i];
		printch(tempstr, kram + (i<<1), tall);
	}
}

void printch(u32 sjis, u32 kram, int tall)
{
	u16 px;
	int x, y, l;
	u8* glyph = romfont_get(sjis, tall ? ROMFONT_ANK_8x16 : ROMFONT_ANK_8x8);
	for(y = 0; y < (tall ? 16 : 8); y++) {
		king_set_kram_write(kram + (y << 6), 1);
		for(l = 8-4; l >= 0; l -= 4) {
			px = 0;
			for(x = 0; x < 4; x++) {
				if((glyph[y] >> (x+l)) & 1) {
					px |= 1 << (x << 3);
				}
			}
			king_kram_write(px);
		}
	}
}
