/*
	libpcfx -- A set of libraries for controlling the NEC PC-FX
                   Based on liberis by Alex Marshall

Copyright (C) 2011		Alex Marshall "trap15" <trap15@raidenii.net>
      and (C) 2024		David Shadoff  GitHub userid: dshadoff

# This code is licensed to you under the terms of the MIT license;
# see file LICENSE or http://www.opensource.org/licenses/mit-license.php
*/

//-------------------------------------------------------------------------
// This is a simple example which demontrates the use of C++
// on the PC-FX, including how to compile and link.
//-------------------------------------------------------------------------

extern "C" {
#include <string.h>

#include <pcfx/types.h>
#include <pcfx/v810.h>
#include <pcfx/romfont.h>
#include <pcfx/king.h>
#include <pcfx/tetsu.h>
}

class test {
  private:
  int  mInternal;
  public:
  int  mExternal;

  test ();
 ~test ();
  void hello (int iStorage);
};

test                       cTestGP;
test __attribute__ ((zda)) cTestR0;

void printch(u32 sjis, u32 kram, int tall);
void printstr(const char* str, int x, int y, int tall);

int main(int argc, char *argv[])
{
	int i;
	u16 microprog[16];

	king_init();
	tetsu_init();
	
	tetsu_set_priorities(0, 0, 1, 0, 0, 0, 0);
	tetsu_set_king_palette(0, 0, 0, 0);
	tetsu_set_rainbow_palette(0);

	king_set_bg_prio(KING_BGPRIO_3, KING_BGPRIO_HIDE, KING_BGPRIO_HIDE, KING_BGPRIO_HIDE, 0);
	king_set_bg_mode(KING_BGMODE_4_PAL, KING_BGMODE_NONE, KING_BGMODE_NONE, KING_BGMODE_NONE);
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
	printstr("Hello World!", 10, 0x20, 1);
	printstr("Love, NEC", 11, 0x38, 0);

	test * pTest = new test;
	pTest->hello(0);;
	delete pTest;

	cTestGP.hello(1);

	cTestR0.hello(2);

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

test::test ()
{
  mInternal = 2;
  mExternal = 3;
}

test::~test ()
{
  mInternal = 0;
  mExternal = 0;
}

void test::hello (int iStorage)
{
//	u32 str[256];
	int xpos, ypos;

	switch (iStorage)
	{
	default:
	case 0:
		xpos=5; ypos=0x48;
		printstr("With dynamic C++ class", xpos, ypos, 0);
		break;
	case 1:
		xpos=3; ypos=0x58;
		printstr("With static C++ class (GP)", xpos, ypos, 0);
		break;
	case 2:
		xpos=3; ypos=0x68;
		printstr("With static C++ class (R0)", xpos, ypos, 0);
		break;
	}
}
