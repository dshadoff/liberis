/*
	libpcfx -- A set of libraries for controlling the NEC PC-FX
                   Based on liberis by Alex Marshall

Copyright (C) 2011		Alex Marshall "trap15" <trap15@raidenii.net>
      and (C) 2024		David Shadoff  GitHub userid: dshadoff

# This code is licensed to you under the terms of the MIT license;
# see file LICENSE or http://www.opensource.org/licenses/mit-license.php
*/

//-------------------------------------------------------------------------
// This example demonstrates implementing interrupt handlers
// There is one handler for the VSync interrupt, and one for the TIMER
// Both are set for approimately the same frequency
//-------------------------------------------------------------------------

/*
The V810 back end supports these function attributes:
__attribute__ ((interrupt))
__attribute__ ((interrupt_handler))
Use these attributes to indicate that the specified function is an interrupt
handler.  The compiler generates function entry and exit sequences suitable
for use in an interrupt handler when either attribute is present

The number of registers that need to be saved depends upon whether the handler
is a leaf function (i.e. it doesn't call any other functions) or not.

If it is a leaf function, then the handler only saves the registers that are
actually used within the function itself, otherwise it must save all of the
registers that might be overwritten when calling other functions.

Note that variables accessed within an interrupt handler which are global
(or otherwise shared) should also be marked as 'volatile' to force the
optimizer to re-read the value when accessed (as an interrupt handler may
have updated it).
*/


#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <pcfx/types.h>
#include <eris/v810.h>
#include <eris/timer.h>
#include <eris/king.h>
#include <eris/tetsu.h>
#include <eris/romfont.h>

const int __attribute__ ((zda)) zda_constant         = 0xDEADBEEF;
volatile int __attribute__ ((zda)) zda_initialized   = 0x12345678;
volatile int __attribute__ ((zda)) zda_uninitialized = 0;
volatile int sda_frame_count = 0;

/* HuC6270-A's status register (RAM mapping). */
volatile uint16_t * const MEM_6270A_SR = (uint16_t *) 0x80000400;

/* Declare this "noinline" to ensure that my_timer_irq() is not a leaf. */
__attribute__ ((noinline)) void increment_zda_uninitialized (void)
{
	zda_uninitialized++;
}

/* Simple test interrupt_handler that is not a leaf. */
/* Because it is not a leaf function, it will use the full IRQ preamble. */
__attribute__ ((interrupt)) void my_timer_irq (void)
{
	eris_timer_ack_irq();

	increment_zda_uninitialized();
	zda_initialized += zda_constant; 
}

/* Simple test interrupt_handler that is a leaf. */
/* Because it is a leaf function, the IRQ preamble only saves the registers that are used. */
__attribute__ ((interrupt_handler)) void my_vblank_irq (void)
{
	uint16_t vdc_status = *MEM_6270A_SR;

	if (vdc_status & 0x20) {
		sda_frame_count++;
	}
}

void printch(u32 sjis, u32 kram, int tall);
void printstr(const char* str, int x, int y, int tall);

/* Fake "sprintf" to test varargs handling. */
int fake_sprintf(char *str, const char *fmt, ...)
{
	va_list ap;

	va_start( ap, fmt );

	int val = va_arg( ap, int );

	if (val == 0xdeadbeef)
		strcpy(str, "Got DEADBEEF!");
	else
		strcpy(str, "Not DEADBEEF!");

	va_end( ap );

	return (13);
}

int main(int argc, char *argv[])
{
	int i;
	char str[256];
	u16 microprog[16];

	eris_king_init();
	eris_tetsu_init();
	
	eris_tetsu_set_priorities(0, 0, 1, 0, 0, 0, 0);
	eris_tetsu_set_king_palette(0, 0, 0, 0);
	eris_tetsu_set_rainbow_palette(0);

	eris_king_set_bg_prio(KING_BGPRIO_3, KING_BGPRIO_HIDE, KING_BGPRIO_HIDE, KING_BGPRIO_HIDE, 0);
	eris_king_set_bg_mode(KING_BGMODE_4_PAL, 0, 0, 0);
	eris_king_set_kram_pages(0, 0, 0, 0);

	for(i = 0; i < 16; i++) {
		microprog[i] = KING_CODE_NOP;
	}

	microprog[0] = KING_CODE_BG0_CG_0;
	eris_king_disable_microprogram();
	eris_king_write_microprogram(microprog, 0, 16);
	eris_king_enable_microprogram();

	eris_tetsu_set_palette(0, 0x0088);
	eris_tetsu_set_palette(1, 0xE088);
	eris_tetsu_set_palette(2, 0xE0F0);
	eris_tetsu_set_palette(3, 0x602C);
	eris_tetsu_set_video_mode(TETSU_LINES_262, 0, TETSU_DOTCLOCK_5MHz, TETSU_COLORS_16,
				TETSU_COLORS_16, 0, 0, 1, 0, 0, 0, 0);
	eris_king_set_bat_cg_addr(KING_BG0, 0, 0);
	eris_king_set_bat_cg_addr(KING_BG0SUB, 0, 0);
	eris_king_set_scroll(KING_BG0, 0, 0);
	eris_king_set_bg_size(KING_BG0, KING_BGSIZE_256, KING_BGSIZE_256, KING_BGSIZE_256, KING_BGSIZE_256);

	eris_king_set_kram_read(0, 1);
	eris_king_set_kram_write(0, 1);
	// Clear BG0's RAM
	for(i = 0x0; i < 0x1E00; i++) {
		eris_king_kram_write(0);
	}
	eris_king_set_kram_write(0, 1);

	printstr("Hello World!", 10, 0x20, 1);
	printstr("Love, NEC", 11, 0x38, 0);

	i = fake_sprintf(str, "Eat %X!", 0xdeadbeef);
	printstr(str, ((32 - i) / 2), 0x48, 0);

	// The PC-FX firmware leaves a lot of hardware actively generating
	// IRQs when a program starts, and it is only because the V810 has
	// interrupts-disabled that the firmware IRQ handlers are not run.
	//
	// You *must* mask/disable/reset the existing IRQ sources and init
	// new handlers before enabling the V810's interrupts!

	// Disable all interrupts before changing handlers.
	irq_set_mask(0x7F);

	// Replace firmware IRQ handlers for the Timer and HuC6270-A.
	//
	// This liberis function uses the V810's hardware IRQ numbering,
	// see FXGA_GA and FXGABOAD documents for more info ...
	irq_set_raw_handler(0x9, my_timer_irq);
	irq_set_raw_handler(0xC, my_vblank_irq);

	// Enable Timer and HuC6270-A interrupts.
	//
	// d6=Timer
	// d5=External
	// d4=KeyPad
	// d3=HuC6270-A
	// d2=HuC6272
	// d1=HuC6270-B
	// d0=HuC6273
	irq_set_mask(0x37);

	// Reset and start the Timer.
	eris_timer_init();
	eris_timer_set_period(23864); /* approx 1/60th of a second */
	eris_timer_start(1);

	// Allow all IRQs.
	//
	// This liberis function uses the V810's hardware IRQ numbering,
	// see FXGA_GA and FXGABOAD documents for more info ...
	irq_set_level(8);

	// Enable V810 CPU's interrupt handling.
	irq_enable();

	// Display the counts in 1/10s increments.
	while (1) {
		int frame = zda_uninitialized;
		while (frame == zda_uninitialized) {}

		i = sprintf(str, "frame count / 6 = %05d", sda_frame_count / 6);
		printstr(str, ((32 - i) / 2), 0x68, 0);
		i = sprintf(str, "timer count / 6 = %05d", zda_uninitialized / 6);
		printstr(str, ((32 - i) / 2), 0x78, 0);
	}

	// We never get here!
	return 0;
}

void printstr(const char* str, int x, int y, int tall)
{
	int i;
	u32 kram = x + (y << 5);
	int len = strlen(str);
	for(i = 0; i < len; i++) {
		printch(str[i], kram + i, tall);
	}
}

void printch(u32 sjis, u32 kram, int tall)
{
	u16 px;
	int x, y;
	u8* glyph = eris_romfont_get(sjis, tall ? ROMFONT_ANK_8x16 : ROMFONT_ANK_8x8);
	for(y = 0; y < (tall ? 16 : 8); y++) {
		eris_king_set_kram_write(kram + (y << 5), 1);
		px = 0;
		for(x = 0; x < 8; x++) {
			if((glyph[y] >> x) & 1) {
				px |= 1 << (x << 1);
			}
		}
		eris_king_kram_write(px);
	}
}
