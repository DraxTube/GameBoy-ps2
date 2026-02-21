#ifndef MMU_H
#define MMU_H

#include "gb.h"

u8  mmu_read(GB_State *gb, u16 addr);
void mmu_write(GB_State *gb, u16 addr, u8 val);
u16  mmu_read16(GB_State *gb, u16 addr);
void mmu_write16(GB_State *gb, u16 addr, u16 val);
void timer_step(GB_State *gb, int cycles);

#endif
