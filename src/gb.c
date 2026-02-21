#include <string.h>
#include <stdio.h>
#include "gb.h"
#include "mmu.h"

void gb_init(GB_State *gb) {
    memset(gb, 0, sizeof(GB_State));
    
    /* Initial CPU state after boot ROM */
    gb->cpu.af = 0x01B0;
    gb->cpu.bc = 0x0013;
    gb->cpu.de = 0x00D8;
    gb->cpu.hl = 0x014D;
    gb->cpu.sp = 0xFFFE;
    gb->cpu.pc = 0x0100;
    gb->cpu.ime = 0;
    
    /* Initial PPU state */
    gb->ppu.lcdc = 0x91;
    gb->ppu.stat = 0x85;
    gb->ppu.bgp  = 0xFC;
    gb->ppu.obp0 = 0xFF;
    gb->ppu.obp1 = 0xFF;
    
    /* Initial MMU state */
    gb->mmu.div_counter = 0xABCC;
    gb->mmu.tac = 0xF8;
    gb->mmu.joypad_select = 0xFF;
    
    printf("GB initialized\n");
}

int gb_load_rom(GB_State *gb, const u8 *data, u32 size) {
    if (!data || size == 0) return -1;
    if (size > sizeof(gb->mmu.rom)) {
        printf("Warning: ROM too large (%u > %zu), truncating\n", size, sizeof(gb->mmu.rom));
        size = sizeof(gb->mmu.rom);
    }
    memcpy(gb->mmu.rom, data, size);
    printf("Loaded ROM: %u bytes\n", size);
    printf("Title: %.16s\n", &data[0x134]);
    return 0;
}

void gb_update_input(GB_State *gb, u32 ps2_buttons) {
    /* GB button layout:
       Buttons: A, B, Select, Start
       D-Pad: Right, Left, Up, Down */
    
    u8 btn = 0x0F;  /* all released by default */
    u8 dir = 0x0F;
    
    /* Map PS2 -> GB */
    if (ps2_buttons & PS2_CROSS)    btn &= ~(1 << 0); /* A */
    if (ps2_buttons & PS2_CIRCLE)   btn &= ~(1 << 1); /* B */
    if (ps2_buttons & PS2_SELECT)   btn &= ~(1 << 2); /* Select */
    if (ps2_buttons & PS2_START)    btn &= ~(1 << 3); /* Start */
    
    if (ps2_buttons & PS2_RIGHT)    dir &= ~(1 << 0); /* Right */
    if (ps2_buttons & PS2_LEFT)     dir &= ~(1 << 1); /* Left */
    if (ps2_buttons & PS2_UP)       dir &= ~(1 << 2); /* Up */
    if (ps2_buttons & PS2_DOWN)     dir &= ~(1 << 3); /* Down */
    
    gb->mmu.joypad_buttons = btn;
    gb->mmu.joypad_dpad    = dir;
    
    /* Request joypad interrupt if any button pressed */
    if ((btn != 0x0F || dir != 0x0F)) {
        gb->mmu.if_ |= 0x10;
    }
}
