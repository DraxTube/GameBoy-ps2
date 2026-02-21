#include "display.h"
#include "gb.h"
#include <gsKit.h>
#include <gsToolkit.h>
#include <string.h>
#include <malloc.h>

/* We'll render the GB screen scaled up to fit PS2 display */
/* GB: 160x144, PS2: 640x448 -> scale ~4x */
#define SCALE_X 4
#define SCALE_Y 3

/* Center the scaled image */
#define OFFSET_X ((640 - GB_SCREEN_W * SCALE_X) / 2)
#define OFFSET_Y ((448 - GB_SCREEN_H * SCALE_Y) / 2)

static GSTEXTURE *gb_texture = NULL;

void display_init(GSGLOBAL *gsGlobal) {
    gb_texture = (GSTEXTURE *)memalign(128, sizeof(GSTEXTURE));
    if (!gb_texture) return;
    
    gb_texture->Width  = GB_SCREEN_W;
    gb_texture->Height = GB_SCREEN_H;
    gb_texture->PSM    = GS_PSM_CT32;
    gb_texture->Filter = GS_FILTER_NEAREST;
    gb_texture->Mem    = (u32 *)memalign(128, GB_SCREEN_W * GB_SCREEN_H * 4);
    
    gsKit_texture_upload(gsGlobal, gb_texture);
}

void display_render(GSGLOBAL *gsGlobal, const u8 *framebuffer) {
    if (!gb_texture || !gb_texture->Mem) return;
    
    /* Copy framebuffer to texture memory */
    /* Framebuffer is RGBA, PS2 gsKit uses RGBA8888 in little endian */
    int i;
    u32 *dst = (u32 *)gb_texture->Mem;
    const u32 *src = (const u32 *)framebuffer;
    
    for (i = 0; i < GB_SCREEN_W * GB_SCREEN_H; i++) {
        /* Convert RGBA -> ABGR for PS2 */
        u32 pixel = src[i];
        u8 r = (pixel >> 24) & 0xFF;
        u8 g = (pixel >> 16) & 0xFF;
        u8 b = (pixel >> 8)  & 0xFF;
        u8 a = pixel & 0xFF;
        /* PS2 alpha is 0x80 = fully opaque */
        dst[i] = ((u32)(a >> 1) << 24) | ((u32)b << 16) | ((u32)g << 8) | r;
    }
    
    /* Upload to VRAM */
    gsKit_texture_upload(gsGlobal, gb_texture);
    
    /* Clear screen */
    gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(0x10, 0x10, 0x10, 0x80, 0));
    
    /* Draw the GB screen stretched on PS2 display */
    gsKit_prim_sprite_texture(gsGlobal, gb_texture,
        (float)OFFSET_X, (float)OFFSET_Y,
        0.0f, 0.0f,
        (float)(OFFSET_X + GB_SCREEN_W * SCALE_X),
        (float)(OFFSET_Y + GB_SCREEN_H * SCALE_Y),
        (float)GB_SCREEN_W, (float)GB_SCREEN_H,
        1,
        GS_SETREG_RGBAQ(0x80, 0x80, 0x80, 0x80, 0));
    
    /* Draw border / UI */
    /* Top bar */
    gsKit_prim_sprite(gsGlobal,
        0.0f, 0.0f,
        640.0f, (float)OFFSET_Y,
        1, GS_SETREG_RGBAQ(0x20, 0x20, 0x40, 0x80, 0));
    /* Bottom bar */
    gsKit_prim_sprite(gsGlobal,
        0.0f, (float)(OFFSET_Y + GB_SCREEN_H * SCALE_Y),
        640.0f, 448.0f,
        1, GS_SETREG_RGBAQ(0x20, 0x20, 0x40, 0x80, 0));
}
