#include "ppu.h"
#include "mmu.h"
#include "gb.h"
#include <string.h>

/* GB palette colors (classic green-ish screen) */
static const u32 palette_colors[4] = {
    0xFFE0F8D0, /* lightest - RGBA */
    0xFF88C070,
    0xFF346856,
    0xFF081820  /* darkest */
};

/* Get palette color index */
static inline u8 get_palette_color(u8 palette, u8 idx) {
    return (palette >> (idx * 2)) & 0x03;
}

/* Render one scanline */
static void render_scanline(GB_State *gb) {
    PPU_State *ppu = &gb->ppu;
    u8 ly = ppu->ly;
    
    if (ly >= GB_SCREEN_H) return;
    
    u32 *row = (u32 *)(ppu->framebuffer + ly * GB_SCREEN_W * 4);
    
    /* Clear line with palette color 0 */
    u8 bg_color0 = get_palette_color(ppu->bgp, 0);
    u32 clear_color = palette_colors[bg_color0];
    int x;
    for (x = 0; x < GB_SCREEN_W; x++) row[x] = clear_color;
    
    /* Background layer */
    if (ppu->lcdc & 0x01) { /* BG enabled */
        u16 tile_map = (ppu->lcdc & 0x08) ? 0x1C00 : 0x1800;
        u16 tile_data = (ppu->lcdc & 0x10) ? 0x0000 : 0x0800;
        int signed_addressing = !(ppu->lcdc & 0x10);
        
        u8 y = (u8)(ly + ppu->scy);
        
        for (x = 0; x < GB_SCREEN_W; x++) {
            u8 screen_x = (u8)(x + ppu->scx);
            u8 tile_x = screen_x / 8;
            u8 tile_y = y / 8;
            u8 px = screen_x % 8;
            u8 py = y % 8;
            
            u16 map_addr = tile_map + (u16)tile_y * 32 + tile_x;
            u8 tile_id = gb->ppu.vram[map_addr];
            
            u16 tile_addr;
            if (signed_addressing) {
                tile_addr = (u16)(0x1000 + (s8)tile_id * 16);
            } else {
                tile_addr = tile_data + (u16)tile_id * 16;
            }
            tile_addr += py * 2;
            
            u8 lo = ppu->vram[tile_addr];
            u8 hi = ppu->vram[tile_addr + 1];
            
            u8 bit = 7 - px;
            u8 color_idx = ((hi >> bit) & 1) << 1 | ((lo >> bit) & 1);
            u8 palette_idx = get_palette_color(ppu->bgp, color_idx);
            row[x] = palette_colors[palette_idx];
        }
    }
    
    /* Window layer */
    if ((ppu->lcdc & 0x20) && ly >= ppu->wy) {
        u16 tile_map = (ppu->lcdc & 0x40) ? 0x1C00 : 0x1800;
        u16 tile_data = (ppu->lcdc & 0x10) ? 0x0000 : 0x0800;
        int signed_addressing = !(ppu->lcdc & 0x10);
        
        int wx = (int)ppu->wx - 7;
        int wy_offset = ly - ppu->wy;
        
        for (x = (wx < 0 ? 0 : wx); x < GB_SCREEN_W; x++) {
            int win_x = x - wx;
            if (win_x < 0) continue;
            
            u8 tile_x = win_x / 8;
            u8 tile_y = wy_offset / 8;
            u8 px = win_x % 8;
            u8 py = wy_offset % 8;
            
            u16 map_addr = tile_map + (u16)tile_y * 32 + tile_x;
            u8 tile_id = ppu->vram[map_addr];
            
            u16 tile_addr;
            if (signed_addressing) {
                tile_addr = (u16)(0x1000 + (s8)tile_id * 16);
            } else {
                tile_addr = tile_data + (u16)tile_id * 16;
            }
            tile_addr += py * 2;
            
            u8 lo = ppu->vram[tile_addr];
            u8 hi = ppu->vram[tile_addr + 1];
            
            u8 bit = 7 - px;
            u8 color_idx = ((hi >> bit) & 1) << 1 | ((lo >> bit) & 1);
            u8 palette_idx = get_palette_color(ppu->bgp, color_idx);
            row[x] = palette_colors[palette_idx];
        }
    }
    
    /* Sprites */
    if (ppu->lcdc & 0x02) {
        int sprite_height = (ppu->lcdc & 0x04) ? 16 : 8;
        int sprite_count = 0;
        int s;
        
        for (s = 0; s < 40 && sprite_count < 10; s++) {
            u8 sy   = ppu->oam[s * 4 + 0] - 16;
            u8 sx   = ppu->oam[s * 4 + 1] - 8;
            u8 tile = ppu->oam[s * 4 + 2];
            u8 attr = ppu->oam[s * 4 + 3];
            
            if (ly < (int)sy || ly >= (int)sy + sprite_height) continue;
            sprite_count++;
            
            int row_y = ly - (int)sy;
            if (attr & 0x40) row_y = sprite_height - 1 - row_y; /* Y flip */
            
            if (sprite_height == 16) tile &= 0xFE;
            
            u16 tile_addr = (u16)tile * 16 + row_y * 2;
            u8 lo = ppu->vram[tile_addr];
            u8 hi = ppu->vram[tile_addr + 1];
            
            u8 obp = (attr & 0x10) ? ppu->obp1 : ppu->obp0;
            
            for (x = 0; x < 8; x++) {
                int screen_x = (int)sx + x;
                if (screen_x < 0 || screen_x >= GB_SCREEN_W) continue;
                
                int bit = (attr & 0x20) ? x : (7 - x); /* X flip */
                u8 color_idx = ((hi >> bit) & 1) << 1 | ((lo >> bit) & 1);
                if (color_idx == 0) continue; /* Transparent */
                
                /* Priority: bit 7 of attr = behind BG */
                if ((attr & 0x80) && (row[screen_x] != palette_colors[get_palette_color(ppu->bgp, 0)])) continue;
                
                u8 palette_idx = get_palette_color(obp, color_idx);
                row[screen_x] = palette_colors[palette_idx];
            }
        }
    }
}

/* PPU modes:
   Mode 0: HBlank   (204 cycles)
   Mode 1: VBlank   (456 cycles * 10 lines)
   Mode 2: OAM Scan (80 cycles)
   Mode 3: Drawing  (172 cycles)
   Total line: 456 cycles
*/
void ppu_step(GB_State *gb, int cycles) {
    PPU_State *ppu = &gb->ppu;
    
    if (!(ppu->lcdc & 0x80)) {
        /* LCD off */
        ppu->ly = 0;
        ppu->cycles = 0;
        return;
    }
    
    ppu->cycles += cycles;
    ppu->frame_ready = 0;
    
    /* Update LYC coincidence */
    if (ppu->ly == ppu->lyc) {
        ppu->stat |= 0x04;
        if (ppu->stat & 0x40) gb->mmu.if_ |= 0x02;
    } else {
        ppu->stat &= ~0x04;
    }
    
    switch (ppu->stat & 0x03) {
        case 2: /* OAM Scan */
            if (ppu->cycles >= 80) {
                ppu->cycles -= 80;
                ppu->stat = (ppu->stat & ~0x03) | 0x03;
            }
            break;
        
        case 3: /* Drawing */
            if (ppu->cycles >= 172) {
                ppu->cycles -= 172;
                /* Render current line */
                render_scanline(gb);
                /* Enter HBlank */
                ppu->stat = (ppu->stat & ~0x03) | 0x00;
                if (ppu->stat & 0x08) gb->mmu.if_ |= 0x02;
            }
            break;
        
        case 0: /* HBlank */
            if (ppu->cycles >= 204) {
                ppu->cycles -= 204;
                ppu->ly++;
                
                if (ppu->ly == 144) {
                    /* Enter VBlank */
                    ppu->stat = (ppu->stat & ~0x03) | 0x01;
                    gb->mmu.if_ |= 0x01; /* VBlank interrupt */
                    if (ppu->stat & 0x10) gb->mmu.if_ |= 0x02;
                    ppu->frame_ready = 1;
                } else {
                    /* Next line OAM scan */
                    ppu->stat = (ppu->stat & ~0x03) | 0x02;
                    if (ppu->stat & 0x20) gb->mmu.if_ |= 0x02;
                }
            }
            break;
        
        case 1: /* VBlank */
            if (ppu->cycles >= 456) {
                ppu->cycles -= 456;
                ppu->ly++;
                
                if (ppu->ly > 153) {
                    ppu->ly = 0;
                    ppu->stat = (ppu->stat & ~0x03) | 0x02;
                    if (ppu->stat & 0x20) gb->mmu.if_ |= 0x02;
                }
            }
            break;
    }
}
