
// bank-switching configuration
#define NES_MAPPER 4		// Mapper 4 (MMC3)
// XXX values other than 4 seem to fail on boot
// probably need redundant VECTORings
#define NES_PRG_BANKS 4		// # of 16KB PRG banks
#define NES_CHR_BANKS 1		// # of 8KB CHR banks

#include <stdlib.h>
#include <peekpoke.h>
#include <string.h>
#include <nes.h>
#include "neslib.h"

// "strobe" means "write any value"
#define STROBE(addr) __asm__ ("sta %w", addr)

#define MMC3_IRQ_SET_VALUE(n) POKE(0xc000, (n));
#define MMC3_IRQ_RELOAD()     STROBE(0xc001)
#define MMC3_IRQ_DISABLE()    STROBE(0xe000)
#define MMC3_IRQ_ENABLE()     STROBE(0xe001)

// link the pattern table into CHR ROM
//#link "chr_generic.s"

#define PPU_CTRL	0x2000
#define PPU_MASK	0x2001
#define PPU_STATUS	0x2002
#define OAM_ADDR	0x2003
#define OAM_DATA	0x2004
#define PPU_SCROLL	0x2005
#define PPU_ADDR	0x2006
#define PPU_DATA	0x2007

#include "tables.c"


// BCD arithmetic support
#include "bcd.h"
//#link "bcd.c"

// VRAM update buffer
#include "vrambuf.h"
//#link "vrambuf.c"

byte i;
byte sine_xo; // origin point of screen
byte sine_yo;
byte sine_xc; // offset point for interupt
byte sine_yc;
byte next_x;
byte next_y;


/*{pal:"nes",layout:"nes"}*/
const char PALETTE[32] = { 
  0x03,0x11,0x30,0x27, // backgrounds
  0x00,0x1c,0x20,0x2c,
  0x00,0x00,0x10,0x20,
  0x00,0x06,0x16,0x26,
  0x00,0x16,0x35,0x24, // sprites
  0x00,0x00,0x37,0x25,
  0x00,0x0d,0x2d,0x3a,
  0x00,0x0d,0x27,0x2a
};

void __fastcall__ sine_pos_next(void) {
    // advance to next scroll value
    sine_xc += 11;
    next_x = (sine[sine_xc] >> 2) - 52;
    //if (sine_xc & 0x07) sine_yc++;
    sine_yc += 7;
    next_y = sine[sine_yc] >> 6;
}

void __fastcall__ irq_nmi_callback(void) {
  // check high bit of A to see if this is an IRQ
  if (__A__ & 0x80) {
    // it's an IRQ from the MMC3 mapper
    // change PPU scroll registers
//    scroll(sine[irq_count], sine[irq_count]);
    //PPU.scroll = next_x;
    //PPU.scroll = next_y;
    //scroll(next_x, next_y);
    //PEEK(PPU_STATUS);
    POKE(PPU_ADDR, 0x20);
    //POKE(PPU_ADDR, 0xc0);
    POKE(PPU_SCROLL, next_x);
    POKE(PPU_SCROLL, next_y);
    /*
	bit PPU_STATUS
        lda scroll_x_hi
        sta PPU_SCROLL
        lda scroll_y
        sta PPU_SCROLL
        ; Vertical Scroll Solution
        ;lda #$26
        ;sta PPU_ADDR
        ;lda #$c0
        ;sta PPU_ADDR
        ;lda #$00
        ;sta PPU_SCROLL
        ;sta PPU_SCROLL
        */
    
    sine_pos_next();
    // acknowledge interrupt
    MMC3_IRQ_DISABLE();
    MMC3_IRQ_ENABLE();
  } else {
    // this is a NMI
    // reload IRQ counter
    MMC3_IRQ_RELOAD();
    // reset scroll counter
    sine_xo++;
    sine_xc = sine_xo;
    if (sine_xo & 0x01) sine_yo++;
    sine_yc = sine_yo;
    sine_pos_next();
  }
}

void main(void) {
  sine_xo = 0;
  sine_yo = 0x40;
  // clear sprites
  oam_clear();
  // set palette colors
  pal_all(PALETTE);
  // More accurate NES emulators simulate the mapper's
  // monitoring of the A12 line, so the background and
  // sprite pattern tables must be different.
  // https://forums.nesdev.com/viewtopic.php?f=2&t=19686#p257380
  set_ppu_ctrl_var(get_ppu_ctrl_var() | 0x08);
  // Enable Work RAM
  POKE(0xA001, 0x80);
  // Mirroring - horizontal
  POKE(0xA000, 0x01);
  // set up MMC3 IRQs every 8 scanlines
  MMC3_IRQ_SET_VALUE(31);
  MMC3_IRQ_RELOAD();
  MMC3_IRQ_ENABLE();
  // enable CPU IRQ
  __asm__ ("cli");
  // set IRQ callback
  nmi_set_callback(irq_nmi_callback);
  // fill vram
  vram_adr(NTADR_A(0,0));
  vram_fill('-', 32*30);
  vram_adr(NTADR_C(0,0));
  vram_fill('-', 32*30);
  // draw message  
  for (i = 0; i < 30; i += 2) {
    vram_adr(NTADR_A(0, i));
    vram_write("HELLO, WORLD! ", 14);
    vram_write("HELLO, WORLD! ", 14);
    vram_adr(NTADR_C(0, i));
    vram_write("HELLO, WORLD! ", 14);
    vram_write("HELLO, WORLD! ", 14);
  }
  // enable rendering
  ppu_on_all();
  // infinite loop
  while(1) {
    //ppu_wait_frame();
  }
}
