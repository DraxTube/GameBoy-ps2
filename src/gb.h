#ifndef GB_H
#define GB_H

#include <tamtypes.h>

/* GB Screen dimensions */
#define GB_SCREEN_W 160
#define GB_SCREEN_H 144

/* CPU Registers */
typedef struct {
    union { struct { u8 f; u8 a; }; u16 af; };
    union { struct { u8 c; u8 b; }; u16 bc; };
    union { struct { u8 e; u8 d; }; u16 de; };
    union { struct { u8 l; u8 h; }; u16 hl; };
    u16 sp;
    u16 pc;
    u8  ime;        /* Interrupt Master Enable */
    u8  halted;
    u8  stopped;
} CPU_Regs;

/* Flags */
#define FLAG_Z 0x80
#define FLAG_N 0x40
#define FLAG_H 0x20
#define FLAG_C 0x10

/* PPU State */
typedef struct {
    u8  vram[0x2000];   /* 8KB Video RAM */
    u8  oam[0xA0];      /* Object Attribute Memory */
    u8  lcdc;           /* LCD Control */
    u8  stat;           /* LCD Status */
    u8  scy, scx;       /* Scroll Y/X */
    u8  ly;             /* Current scanline */
    u8  lyc;            /* LY Compare */
    u8  dma;            /* DMA transfer */
    u8  bgp;            /* BG Palette */
    u8  obp0, obp1;     /* OBJ Palettes */
    u8  wy, wx;         /* Window Y/X */
    int cycles;         /* PPU cycle counter */
    u8  framebuffer[GB_SCREEN_W * GB_SCREEN_H * 4]; /* RGBA */
    u8  frame_ready;
} PPU_State;

/* MMU State */
typedef struct {
    u8  rom[0x8000];    /* 32KB ROM (simple no-MBC) */
    u8  wram[0x2000];   /* 8KB Work RAM */
    u8  hram[0x7F];     /* High RAM */
    u8  ie;             /* Interrupt Enable */
    u8  if_;            /* Interrupt Flag */
    /* Timer */
    u16 div_counter;
    u8  tima;
    u8  tma;
    u8  tac;
    /* Joypad */
    u8  joypad_select;
    u8  joypad_buttons; /* Current button state */
    u8  joypad_dpad;
} MMU_State;

/* Full GB State */
typedef struct {
    CPU_Regs cpu;
    PPU_State ppu;
    MMU_State mmu;
    u8 boot_rom_enabled;
} GB_State;

/* Public API */
void gb_init(GB_State *gb);
int  gb_load_rom(GB_State *gb, const u8 *data, u32 size);
void gb_update_input(GB_State *gb, u32 ps2_buttons);

/* PS2 button mapping */
#define PS2_SELECT   (1 << 0)
#define PS2_L3       (1 << 1)
#define PS2_R3       (1 << 2)
#define PS2_START    (1 << 3)
#define PS2_UP       (1 << 4)
#define PS2_RIGHT    (1 << 5)
#define PS2_DOWN     (1 << 6)
#define PS2_LEFT     (1 << 7)
#define PS2_L2       (1 << 8)
#define PS2_R2       (1 << 9)
#define PS2_L1       (1 << 10)
#define PS2_R1       (1 << 11)
#define PS2_TRIANGLE (1 << 12)
#define PS2_CIRCLE   (1 << 13)
#define PS2_CROSS    (1 << 14)
#define PS2_SQUARE   (1 << 15)

#endif /* GB_H */
