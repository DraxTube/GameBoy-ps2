#include "mmu.h"
#include "gb.h"
#include <stdio.h>

u8 mmu_read(GB_State *gb, u16 addr) {
    /* ROM Bank 0 */
    if (addr < 0x8000) {
        return gb->mmu.rom[addr];
    }
    /* VRAM */
    if (addr < 0xA000) {
        return gb->ppu.vram[addr - 0x8000];
    }
    /* External RAM (not implemented for simple ROMs) */
    if (addr < 0xC000) {
        return 0xFF;
    }
    /* WRAM */
    if (addr < 0xE000) {
        return gb->mmu.wram[addr - 0xC000];
    }
    /* Echo RAM */
    if (addr < 0xFE00) {
        return gb->mmu.wram[addr - 0xE000];
    }
    /* OAM */
    if (addr < 0xFEA0) {
        return gb->ppu.oam[addr - 0xFE00];
    }
    /* Unusable */
    if (addr < 0xFF00) {
        return 0xFF;
    }
    /* I/O Registers */
    if (addr < 0xFF80) {
        u8 reg = addr & 0xFF;
        switch (reg) {
            case 0x00: { /* Joypad */
                u8 sel = gb->mmu.joypad_select;
                if (!(sel & 0x20)) return 0xD0 | gb->mmu.joypad_dpad;
                if (!(sel & 0x10)) return 0xE0 | gb->mmu.joypad_buttons;
                return 0xFF;
            }
            case 0x04: return (u8)(gb->mmu.div_counter >> 8);
            case 0x05: return gb->mmu.tima;
            case 0x06: return gb->mmu.tma;
            case 0x07: return gb->mmu.tac;
            case 0x0F: return gb->mmu.if_;
            case 0x40: return gb->ppu.lcdc;
            case 0x41: return gb->ppu.stat;
            case 0x42: return gb->ppu.scy;
            case 0x43: return gb->ppu.scx;
            case 0x44: return gb->ppu.ly;
            case 0x45: return gb->ppu.lyc;
            case 0x47: return gb->ppu.bgp;
            case 0x48: return gb->ppu.obp0;
            case 0x49: return gb->ppu.obp1;
            case 0x4A: return gb->ppu.wy;
            case 0x4B: return gb->ppu.wx;
            default:   return 0xFF;
        }
    }
    /* HRAM */
    if (addr < 0xFFFF) {
        return gb->mmu.hram[addr - 0xFF80];
    }
    /* IE */
    if (addr == 0xFFFF) {
        return gb->mmu.ie;
    }
    return 0xFF;
}

void mmu_write(GB_State *gb, u16 addr, u8 val) {
    if (addr < 0x8000) {
        /* ROM write (MBC control - ignored for no-MBC) */
        return;
    }
    if (addr < 0xA000) {
        gb->ppu.vram[addr - 0x8000] = val;
        return;
    }
    if (addr < 0xC000) {
        /* External RAM - ignored */
        return;
    }
    if (addr < 0xE000) {
        gb->mmu.wram[addr - 0xC000] = val;
        return;
    }
    if (addr < 0xFE00) {
        gb->mmu.wram[addr - 0xE000] = val;
        return;
    }
    if (addr < 0xFEA0) {
        gb->ppu.oam[addr - 0xFE00] = val;
        return;
    }
    if (addr < 0xFF00) return;
    if (addr < 0xFF80) {
        u8 reg = addr & 0xFF;
        switch (reg) {
            case 0x00: gb->mmu.joypad_select = val; break;
            case 0x04: gb->mmu.div_counter = 0; break;
            case 0x05: gb->mmu.tima = val; break;
            case 0x06: gb->mmu.tma = val; break;
            case 0x07: gb->mmu.tac = val & 0x07; break;
            case 0x0F: gb->mmu.if_ = val; break;
            case 0x40: gb->ppu.lcdc = val; break;
            case 0x41: gb->ppu.stat = (gb->ppu.stat & 0x07) | (val & 0xF8); break;
            case 0x42: gb->ppu.scy = val; break;
            case 0x43: gb->ppu.scx = val; break;
            case 0x45: gb->ppu.lyc = val; break;
            case 0x46: { /* DMA Transfer */
                u16 src = (u16)val << 8;
                int i;
                for (i = 0; i < 0xA0; i++) {
                    gb->ppu.oam[i] = mmu_read(gb, src + i);
                }
                break;
            }
            case 0x47: gb->ppu.bgp = val; break;
            case 0x48: gb->ppu.obp0 = val; break;
            case 0x49: gb->ppu.obp1 = val; break;
            case 0x4A: gb->ppu.wy = val; break;
            case 0x4B: gb->ppu.wx = val; break;
        }
        return;
    }
    if (addr < 0xFFFF) {
        gb->mmu.hram[addr - 0xFF80] = val;
        return;
    }
    if (addr == 0xFFFF) {
        gb->mmu.ie = val;
    }
}

u16 mmu_read16(GB_State *gb, u16 addr) {
    return (u16)mmu_read(gb, addr) | ((u16)mmu_read(gb, addr + 1) << 8);
}

void mmu_write16(GB_State *gb, u16 addr, u16 val) {
    mmu_write(gb, addr, (u8)(val & 0xFF));
    mmu_write(gb, addr + 1, (u8)(val >> 8));
}

/* Timer */
void timer_step(GB_State *gb, int cycles) {
    gb->mmu.div_counter += cycles;
    
    if (!(gb->mmu.tac & 0x04)) return;
    
    static int timer_counter = 0;
    int threshold;
    switch (gb->mmu.tac & 0x03) {
        case 0: threshold = 1024; break;
        case 1: threshold = 16;   break;
        case 2: threshold = 64;   break;
        case 3: threshold = 256;  break;
        default: threshold = 1024;
    }
    
    timer_counter += cycles;
    while (timer_counter >= threshold) {
        timer_counter -= threshold;
        gb->mmu.tima++;
        if (gb->mmu.tima == 0) {
            gb->mmu.tima = gb->mmu.tma;
            gb->mmu.if_ |= 0x04; /* Timer interrupt */
        }
    }
}
